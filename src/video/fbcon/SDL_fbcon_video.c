#include "../../SDL_internal.h"

#ifndef SDL_FBCON_VIDEO
#define SDL_FBCON_VIDEO

#include "../../SDL_internal.h"
#include "../SDL_sysvideo.h"

#include "SDL_events.h"
#include "SDL_loadso.h"
#include "SDL_stdinc.h"
#include "SDL_syswm.h"
#include "SDL_version.h"
#include "SDL_video.h"
#include "../SDL_blit.h"
#include "SDL_pixels.h"
#include "../../core/linux/SDL_evdev_capabilities.h"
#include "../../core/linux/SDL_evdev.h"

#include <fcntl.h>
#include <linux/fb.h>
#include <linux/vt.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/mman.h>

#include <arm_neon.h>

#define FBCON_DRIVER_NAME "fbcon"

/* /mnt/SDCARD # fbset                              */
/*                                                  */
/* mode "240x320-60"                                */
/*     # D: 17.000 MHz, H: 20.757 kHz, V: 60.166 Hz */
/*     geometry 240 320 240 640 16                  */
/*     timings 58823 52 525 16 7 2 2                */
/*     accel false                                  */
/*     rgba 0/0,0/0,0/0,0/0                         */
/* endmode                                          */

#define TG2040_SCREEN_BITS_PER_PIXEL_16 16
#define TG2040_SCREEN_BYTES_PER_PIXEL_2 \
    TG2040_SCREEN_BITS_PER_PIXEL_16 / 8
#define TG2040_SCREEN_WIDTH_240 240
#define TG2040_SCREEN_HEIGHT_320 320
#define TG2040_SCREEN_VIRTUAL_HEIGHT_640 640
#define TG2040_SCREEN_VIRTUAL_WIDTH_240 240
#define TG2040_SCREEN_VIRTUAL_PITCH_480 \
    TG2040_SCREEN_VIRTUAL_WIDTH_240 *TG2040_SCREEN_BYTES_PER_PIXEL_2
#define TG2040_SCREEN_REFRESH_RATE_60 60
#define TG2040_PIXELFORMAT_RGB565 SDL_PIXELFORMAT_RGB565

#endif /* SDL_FBCON_VIDEO */

static void
FB_DeleteDevice(_THIS)
{
    SDL_free(_this);
}

static int FB0_FD;
static int FB0_MMAP_LENGTH = TG2040_SCREEN_VIRTUAL_HEIGHT_640 * TG2040_SCREEN_VIRTUAL_WIDTH_240 * TG2040_SCREEN_BYTES_PER_PIXEL_2;
static char *FB0_MMAP = NULL;
static struct fb_var_screeninfo FB0_VINFO;
static int FB0_CURRENT_BUFFER = 0;
static int BUFFER_LENGTH = TG2040_SCREEN_HEIGHT_320 * TG2040_SCREEN_WIDTH_240 * TG2040_SCREEN_BYTES_PER_PIXEL_2;
static char *BUFFER = NULL;

void FBCon_Clean()
{
    if (BUFFER != NULL)
    {
        SDL_free(BUFFER);
        BUFFER = NULL;
    }
    if (FB0_MMAP != NULL)
    {
        munmap(FB0_MMAP, FB0_MMAP_LENGTH);
        FB0_MMAP = NULL;
    }
    if (FB0_FD >= 0)
    {
        close(FB0_FD);
        FB0_FD = -1;
    }

    SDL_EVDEV_Quit();
}


int FBCon_VideoInit(_THIS)
{
    FB0_FD = open("/dev/fb0", O_RDWR);
    if (FB0_FD < 0)
    {
        FBCon_Clean();
        return SDL_SetError("fbcon: unable to open /dev/fb0");
    }

    if (ioctl(FB0_FD, FBIOGET_VSCREENINFO, &FB0_VINFO) < 0)
    {
        FBCon_Clean();
        return SDL_SetError("fbcon: unable to get screen info with ioctl");
    }

    FB0_VINFO.yoffset = 0;
    FB0_VINFO.activate = FB_ACTIVATE_VBL; // Ask the driver to wait for V-blank before panning
    if (ioctl(FB0_FD, FBIOPAN_DISPLAY, &FB0_VINFO) < 0)
    {
        FBCon_Clean();
        return SDL_SetError(SDL_LOG_CATEGORY_VIDEO, "fbcon: FBIOPAN_DISPLAY failed");
    }

    FB0_MMAP = mmap(NULL, FB0_MMAP_LENGTH, PROT_READ | PROT_WRITE, MAP_SHARED, FB0_FD, 0);
    if (FB0_MMAP == (char *)-1)
    {
        FBCon_Clean();
        FB0_MMAP = NULL;
        return SDL_SetError("Unable to memory map the video hardware");
    }

    BUFFER = SDL_calloc(1, BUFFER_LENGTH);
    if (BUFFER == NULL)
    {
        FBCon_Clean();
        munmap(FB0_MMAP, FB0_MMAP_LENGTH);
        FB0_MMAP = NULL;
        return SDL_SetError("Unable to allocate temporary video buffer");
    }

    SDL_DisplayMode display_mode;
    SDL_zero(display_mode);
    // TG2040 screen is 240x320 screen rotated 90deg clockwise
    display_mode.w = TG2040_SCREEN_HEIGHT_320;
    display_mode.h = TG2040_SCREEN_WIDTH_240;
    display_mode.refresh_rate = TG2040_SCREEN_REFRESH_RATE_60;
    display_mode.format = TG2040_PIXELFORMAT_RGB565;

    SDL_VideoDisplay display;
    SDL_zero(display);
    display.desktop_mode = display_mode;
    display.current_mode = display_mode;

    SDL_AddVideoDisplay(&display, SDL_FALSE);

    _this->checked_texture_framebuffer = SDL_TRUE;

    if (SDL_EVDEV_Init() < 0)
    {
        FBCon_Clean();
        return SDL_SetError("Unable to SDL_EVDEV_Init");
    }
    SDL_EVDEV_device_added("/dev/input/event0", SDL_UDEV_DEVICE_KEYBOARD);

    return 0;
}

void FBCon_VideoQuit(_THIS)
{
    FBCon_Clean();
}

void FBCon_GetDisplayModes(_THIS, SDL_VideoDisplay *display)
{
}

int FBCon_SetDisplayMode(_THIS, SDL_VideoDisplay *display, SDL_DisplayMode *mode)
{
    return 0;
}

int FBCon_CreateWindow(_THIS, SDL_Window *window)
{
    /* Always fullscreen */
    window->flags |= SDL_WINDOW_FULLSCREEN;
    window->flags |= SDL_WINDOW_SHOWN;
    window->is_hiding = SDL_TRUE;

    // TG2040 screen is 240x320 screen rotated 90deg clockwise
    window->w = TG2040_SCREEN_HEIGHT_320;
    window->h = TG2040_SCREEN_WIDTH_240;

    SDL_PixelFormat *format = (SDL_PixelFormat *)SDL_calloc(1, sizeof(SDL_PixelFormat));
    format->format = TG2040_PIXELFORMAT_RGB565;
    format->BitsPerPixel = TG2040_SCREEN_BITS_PER_PIXEL_16;
    format->BytesPerPixel = TG2040_SCREEN_BYTES_PER_PIXEL_2;
    format->Rmask = 0xF800;
    format->Gmask = 0x07E0;
    format->Bmask = 0x001F;

    // The surface will write in the buffer.
    // Display on actual device will we handheld in FBCon_UpdateWindowFramebuffer.
    SDL_Surface *surface = (SDL_Surface *)SDL_calloc(1, sizeof(SDL_Surface));
    surface->format = format;
    // TG2040 screen is 240x320 screen rotated 90deg clockwise
    surface->w = TG2040_SCREEN_HEIGHT_320;
    surface->h = TG2040_SCREEN_WIDTH_240;
    surface->pixels = BUFFER;
    surface->clip_rect.x = 0;
    surface->clip_rect.y = 0;
    surface->clip_rect.w = TG2040_SCREEN_HEIGHT_320;
    surface->clip_rect.h = TG2040_SCREEN_WIDTH_240;
    surface->pitch = TG2040_SCREEN_HEIGHT_320 * TG2040_SCREEN_BYTES_PER_PIXEL_2;
    surface->map = (SDL_BlitMap *)SDL_calloc(1, sizeof(SDL_BlitMap));

    window->surface = surface;
    window->surface_valid = SDL_TRUE;

    /* Window has been successfully created */
    printf("FBCon_CreateWindow done.\n");
    return 0;
}

void FBCon_DestroyWindow(_THIS, SDL_Window *window)
{
    if (window->surface != NULL)
    {
        SDL_free(window->surface->map);
        window->surface->map = NULL;
        SDL_free(window->surface->format);
        window->surface->format = NULL;
        SDL_free(window->surface);
        window->surface = NULL;
    }
}

void FBCon_SetWindowTitle(_THIS, SDL_Window *window)
{
}

void FBCon_SetWindowPosition(_THIS, SDL_Window *window)
{
}

void FBCon_SetWindowSize(_THIS, SDL_Window *window)
{
}

void FBCon_ShowWindow(_THIS, SDL_Window *window)
{
}

void FBCon_HideWindow(_THIS, SDL_Window *window)
{
}

/*****************************************************************************/
/* SDL Window Manager function                                               */
/*****************************************************************************/
SDL_bool
FBCon_GetWindowWMInfo(_THIS, SDL_Window *window, struct SDL_SysWMinfo *info)
{
    if (info->version.major <= SDL_MAJOR_VERSION)
    {
        return SDL_TRUE;
    }
    else
    {
        SDL_SetError("application not compiled with SDL %d.%d\n",
                     SDL_MAJOR_VERSION, SDL_MINOR_VERSION);
    }

    /* Failed to get window manager information */
    return SDL_FALSE;
}

void FBCon_PumpEvents(_THIS)
{
    SDL_EVDEV_Poll();
}

// Portable "vtrnq_u64" for ARMv7
static inline uint64x2x2_t vtrnq_u64_compat(uint64x2_t a, uint64x2_t b)
{
    uint64x2x2_t res;
    res.val[0] = vcombine_u64(vget_low_u64(a), vget_low_u64(b));
    res.val[1] = vcombine_u64(vget_high_u64(a), vget_high_u64(b));
    return res;
}

// Transpose 8x8 block of 16-bit values
static inline void transpose8x8_u16(
    uint16x8_t r0, uint16x8_t r1, uint16x8_t r2, uint16x8_t r3,
    uint16x8_t r4, uint16x8_t r5, uint16x8_t r6, uint16x8_t r7,
    uint16x8_t out[8])
{
    // Step 1: 16-bit interleave
    uint16x8x2_t t0 = vtrnq_u16(r0, r1);
    uint16x8x2_t t1 = vtrnq_u16(r2, r3);
    uint16x8x2_t t2 = vtrnq_u16(r4, r5);
    uint16x8x2_t t3 = vtrnq_u16(r6, r7);

    // Step 2: 32-bit interleave
    uint32x4x2_t s0 = vtrnq_u32(vreinterpretq_u32_u16(t0.val[0]), vreinterpretq_u32_u16(t1.val[0]));
    uint32x4x2_t s1 = vtrnq_u32(vreinterpretq_u32_u16(t0.val[1]), vreinterpretq_u32_u16(t1.val[1]));
    uint32x4x2_t s2 = vtrnq_u32(vreinterpretq_u32_u16(t2.val[0]), vreinterpretq_u32_u16(t3.val[0]));
    uint32x4x2_t s3 = vtrnq_u32(vreinterpretq_u32_u16(t2.val[1]), vreinterpretq_u32_u16(t3.val[1]));

    // Step 3: 64-bit interleave (manual implementation)
    uint64x2x2_t u0 = vtrnq_u64_compat(vreinterpretq_u64_u32(s0.val[0]), vreinterpretq_u64_u32(s2.val[0]));
    uint64x2x2_t u1 = vtrnq_u64_compat(vreinterpretq_u64_u32(s0.val[1]), vreinterpretq_u64_u32(s2.val[1]));
    uint64x2x2_t u2 = vtrnq_u64_compat(vreinterpretq_u64_u32(s1.val[0]), vreinterpretq_u64_u32(s3.val[0]));
    uint64x2x2_t u3 = vtrnq_u64_compat(vreinterpretq_u64_u32(s1.val[1]), vreinterpretq_u64_u32(s3.val[1]));

    // Collect 8 transposed rows
    out[0] = vreinterpretq_u16_u64(u0.val[0]);
    out[1] = vreinterpretq_u16_u64(u2.val[0]);
    out[2] = vreinterpretq_u16_u64(u1.val[0]);
    out[3] = vreinterpretq_u16_u64(u3.val[0]);
    out[4] = vreinterpretq_u16_u64(u0.val[1]);
    out[5] = vreinterpretq_u16_u64(u2.val[1]);
    out[6] = vreinterpretq_u16_u64(u1.val[1]);
    out[7] = vreinterpretq_u16_u64(u3.val[1]);
}

int FBCon_UpdateWindowFramebuffer(_THIS, SDL_Window *window, const SDL_Rect *rects, int numrects)
{
    int next_buffer = 1 - FB0_CURRENT_BUFFER;
    int offset = next_buffer * (TG2040_SCREEN_HEIGHT_320 * TG2040_SCREEN_VIRTUAL_PITCH_480);

    // Source: 320x240 pixels (16-bit)
    Uint16 *src = (Uint16 *)window->surface->pixels;
    // Destination: 240x320 pixels (16-bit)
    Uint16 *dst = (Uint16 *)((char *)FB0_MMAP + offset);

    int src_w = TG2040_SCREEN_HEIGHT_320;
    int src_w_idx = src_w - 1;
    int src_h = TG2040_SCREEN_WIDTH_240;
    int dst_w = TG2040_SCREEN_WIDTH_240;

    // Process 8x8 tiles: x across width, y across height
    for (int y = 0; y < src_h; y += 8)
    {
        Uint16 *base_dst = dst + y;
        for (int x = 0; x < src_w; x += 8)
        {
            Uint16 *base_src = src + x;
            uint16x8_t r0 = vld1q_u16(base_src + (y + 0) * src_w);
            uint16x8_t r1 = vld1q_u16(base_src + (y + 1) * src_w);
            uint16x8_t r2 = vld1q_u16(base_src + (y + 2) * src_w);
            uint16x8_t r3 = vld1q_u16(base_src + (y + 3) * src_w);
            uint16x8_t r4 = vld1q_u16(base_src + (y + 4) * src_w);
            uint16x8_t r5 = vld1q_u16(base_src + (y + 5) * src_w);
            uint16x8_t r6 = vld1q_u16(base_src + (y + 6) * src_w);
            uint16x8_t r7 = vld1q_u16(base_src + (y + 7) * src_w);

            uint16x8_t out[8];
            transpose8x8_u16(r0, r1, r2, r3, r4, r5, r6, r7, out);

            int base_src_w_idx = src_w_idx - x;
            // Store each column into rotated framebuffer
            for (int j = 0; j < 8; j++)
            {
                vst1q_u16(base_dst + (base_src_w_idx - j) * dst_w, out[j]);
            }
        }
    }

    FB0_VINFO.yoffset = next_buffer * TG2040_SCREEN_HEIGHT_320;
    ioctl(FB0_FD, FBIOPAN_DISPLAY, &FB0_VINFO);
    FB0_CURRENT_BUFFER = next_buffer;

    return 0;
}

static SDL_VideoDevice *
FBCon_CreateDevice(void)
{
    SDL_VideoDevice *device;

    /* Initialize SDL_VideoDevice structure */
    device = (SDL_VideoDevice *)SDL_calloc(1, sizeof(SDL_VideoDevice));
    if (device == NULL)
    {
        SDL_OutOfMemory();
        return NULL;
    }

    /* Setup amount of available displays and current display */
    device->num_displays = 0;

    /* Set device free function */
    device->free = FB_DeleteDevice;

    /* Setup all functions which we can handle */
    device->VideoInit = FBCon_VideoInit;
    device->VideoQuit = FBCon_VideoQuit;
    device->GetDisplayModes = FBCon_GetDisplayModes;
    device->SetDisplayMode = FBCon_SetDisplayMode;
    device->CreateSDLWindow = FBCon_CreateWindow;
    device->SetWindowTitle = FBCon_SetWindowTitle;
    device->SetWindowPosition = FBCon_SetWindowPosition;
    device->SetWindowSize = FBCon_SetWindowSize;
    device->ShowWindow = FBCon_ShowWindow;
    device->HideWindow = FBCon_HideWindow;
    device->DestroyWindow = FBCon_DestroyWindow;
    device->GetWindowWMInfo = FBCon_GetWindowWMInfo;
    device->PumpEvents = FBCon_PumpEvents;
    device->UpdateWindowFramebuffer = FBCon_UpdateWindowFramebuffer;

    return device;
}

VideoBootStrap FBCon_bootstrap = {
    FBCON_DRIVER_NAME, "SDL fbcon video driver",
    FBCon_CreateDevice};