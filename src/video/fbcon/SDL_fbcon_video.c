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
static int FB0_BUFFER_LENGTH = TG2040_SCREEN_HEIGHT_320 * TG2040_SCREEN_WIDTH_240 * TG2040_SCREEN_BYTES_PER_PIXEL_2;
static char *FB0_BUFFER = NULL;

int FBCon_VideoInit(_THIS)
{
    FB0_FD = open("/dev/fb0", O_RDWR);
    if (FB0_FD < 0)
    {
        return SDL_SetError("fbcon: unable to open /dev/fb0");
    }

    FB0_MMAP = mmap(NULL, FB0_MMAP_LENGTH, PROT_READ | PROT_WRITE, MAP_SHARED, FB0_FD, 0);
    if (FB0_MMAP == (char *)-1)
    {
        FB0_MMAP = NULL;
        return SDL_SetError("Unable to memory map the video hardware");
    }

    FB0_BUFFER = SDL_calloc(1, FB0_BUFFER_LENGTH);
    if (FB0_BUFFER == NULL)
    {
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

    return 0;
}

void FBCon_VideoQuit(_THIS)
{
    if (FB0_BUFFER != NULL)
    {
        SDL_free(FB0_BUFFER);
        FB0_BUFFER = NULL;
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
    surface->pixels = FB0_BUFFER;
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
}

int FBCon_UpdateWindowFramebuffer(_THIS, SDL_Window *window, const SDL_Rect *rects, int numrects)
{
    // Source: 320x240 pixels (16-bit)
    Uint16 *src = (Uint16 *)window->surface->pixels;
    // Destination: 240x320 pixels (16-bit)
    Uint16 *dst = (Uint16 *)FB0_MMAP;

    int src_w = TG2040_SCREEN_HEIGHT_320;
    int src_w_idx = src_w - 1;
    int src_h = TG2040_SCREEN_WIDTH_240;
    int dst_w = TG2040_SCREEN_WIDTH_240;

    for (int y = 0; y < src_h; y++)
    {
        Uint16 *src_row = src + y * src_w;
        Uint16 *dst_col = dst + y;

        int x = 0;

        // Process 8 pixels per iteration using NEON
#pragma unroll // hint compiler to unroll this loop fully
        for (; x <= src_w - 8; x += 8)
        {
            // Load 8 pixels from src (contiguous)
            uint16x8_t pixels = vld1q_u16(src_row + x);

            // Extract to scalars
            uint16_t p0 = vgetq_lane_u16(pixels, 0);
            uint16_t p1 = vgetq_lane_u16(pixels, 1);
            uint16_t p2 = vgetq_lane_u16(pixels, 2);
            uint16_t p3 = vgetq_lane_u16(pixels, 3);
            uint16_t p4 = vgetq_lane_u16(pixels, 4);
            uint16_t p5 = vgetq_lane_u16(pixels, 5);
            uint16_t p6 = vgetq_lane_u16(pixels, 6);
            uint16_t p7 = vgetq_lane_u16(pixels, 7);

            int base_dy = src_w_idx - x;

            // Scatter stores
            dst_col[(base_dy - 0) * dst_w] = p0;
            dst_col[(base_dy - 1) * dst_w] = p1;
            dst_col[(base_dy - 2) * dst_w] = p2;
            dst_col[(base_dy - 3) * dst_w] = p3;
            dst_col[(base_dy - 4) * dst_w] = p4;
            dst_col[(base_dy - 5) * dst_w] = p5;
            dst_col[(base_dy - 6) * dst_w] = p6;
            dst_col[(base_dy - 7) * dst_w] = p7;
        }
    }

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