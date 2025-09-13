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

#define TG2040_BITS_PER_PIXEL_16 16

typedef struct
{
    int width;
    int height;
    char *mapped_mem;
    Uint32 format;
} FBCon_DisplayData;

typedef struct
{
    char *mmaped_mem;
} FBCon_WindowData;

#endif /* SDL_FBCON_VIDEO */
