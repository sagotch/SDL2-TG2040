/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2023 Sam Lantinga <slouken@libsdl.org>

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely, subject to the following restrictions:

  1. The origin of this software must not be misrepresented; you must not
     claim that you wrote the original software. If you use this software
     in a product, an acknowledgment in the product documentation would be
     appreciated but is not required.
  2. Altered source versions must be plainly marked as such, and must not be
     misrepresented as being the original software.
  3. This notice may not be removed or altered from any source distribution.
*/
#include "../SDL_internal.h"

#include "SDL_video.h"
#include "SDL_blit.h"
#include "SDL_blit_copy.h"


void
SDL_BlitCopy(SDL_BlitInfo * info)
{
    SDL_bool overlap;
    Uint8 *src, *dst;
    int w, h;
    int srcskip, dstskip;

    w = info->dst_w * info->dst_fmt->BytesPerPixel;
    h = info->dst_h;
    src = info->src;
    dst = info->dst;
    srcskip = info->src_pitch;
    dstskip = info->dst_pitch;

    /* Properly handle overlapping blits */
    if (src < dst) {
        overlap = (dst < (src + h*srcskip));
    } else {
        overlap = (src < (dst + h*dstskip));
    }
    if (overlap) {
        if ( dst < src ) {
                while ( h-- ) {
                        SDL_memmove(dst, src, w);
                        src += srcskip;
                        dst += dstskip;
                }
        } else {
                src += ((h-1) * srcskip);
                dst += ((h-1) * dstskip);
                while ( h-- ) {
                        SDL_memmove(dst, src, w);
                        src -= srcskip;
                        dst -= dstskip;
                }
        }
        return;
    }

    while (h--) {
        SDL_memcpy(dst, src, w);
        src += srcskip;
        dst += dstskip;
    }
}

/* vi: set ts=4 sw=4 expandtab: */
