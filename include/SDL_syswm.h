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

/**
 *  \file SDL_syswm.h
 *
 *  Include file for SDL custom system window manager hooks.
 */

#ifndef SDL_syswm_h_
#define SDL_syswm_h_

#include "SDL_stdinc.h"
#include "SDL_error.h"
#include "SDL_video.h"
#include "SDL_version.h"

/**
 *  \brief SDL_syswm.h
 *
 *  Your application has access to a special type of event ::SDL_SYSWMEVENT,
 *  which contains window-manager specific information and arrives whenever
 *  an unhandled window event occurs.  This event is ignored by default, but
 *  you can enable it with SDL_EventState().
 */
struct SDL_SysWMinfo;

#if !defined(SDL_PROTOTYPES_ONLY)

/* This is the structure for custom window manager events */

#endif /* SDL_PROTOTYPES_ONLY */


#include "begin_code.h"
/* Set up for C function definitions, even when using C++ */
#ifdef __cplusplus
extern "C" {
#endif

#if !defined(SDL_PROTOTYPES_ONLY)
/**
 *  These are the various supported windowing subsystems
 */
typedef enum
{
    SDL_SYSWM_UNKNOWN,
    SDL_SYSWM_WINDOWS,
    SDL_SYSWM_X11,
    SDL_SYSWM_DIRECTFB,
    SDL_SYSWM_COCOA,
    SDL_SYSWM_UIKIT,
    SDL_SYSWM_WAYLAND,
    SDL_SYSWM_MIR,  /* no longer available, left for API/ABI compatibility. Remove in 2.1! */
    SDL_SYSWM_WINRT,
    SDL_SYSWM_ANDROID,
    SDL_SYSWM_VIVANTE,
    SDL_SYSWM_OS2,
    SDL_SYSWM_HAIKU,
    SDL_SYSWM_KMSDRM,
    SDL_SYSWM_RISCOS
} SDL_SYSWM_TYPE;

/**
 *  The custom event structure.
 */
struct SDL_SysWMmsg
{
    SDL_version version;
    SDL_SYSWM_TYPE subsystem;
    union
    {
        /* Can't have an empty union */
        int dummy;
    } msg;
};

/**
 *  The custom window manager information structure.
 *
 *  When this structure is returned, it holds information about which
 *  low level system it is using, and will be one of SDL_SYSWM_TYPE.
 */
struct SDL_SysWMinfo
{
    SDL_version version;
    SDL_SYSWM_TYPE subsystem;
    union
    {

        /* Make sure this union is always 64 bytes (8 64-bit pointers). */
        /* Be careful not to overflow this if you add a new target! */
        Uint8 dummy[64];
    } info;
};

#endif /* SDL_PROTOTYPES_ONLY */

typedef struct SDL_SysWMinfo SDL_SysWMinfo;


/**
 * Get driver-specific information about a window.
 *
 * You must include SDL_syswm.h for the declaration of SDL_SysWMinfo.
 *
 * The caller must initialize the `info` structure's version by using
 * `SDL_VERSION(&info.version)`, and then this function will fill in the rest
 * of the structure with information about the given window.
 *
 * \param window the window about which information is being requested
 * \param info an SDL_SysWMinfo structure filled in with window information
 * \returns SDL_TRUE if the function is implemented and the `version` member
 *          of the `info` struct is valid, or SDL_FALSE if the information
 *          could not be retrieved; call SDL_GetError() for more information.
 *
 * \since This function is available since SDL 2.0.0.
 */
extern DECLSPEC SDL_bool SDLCALL SDL_GetWindowWMInfo(SDL_Window * window,
                                                     SDL_SysWMinfo * info);


/* Ends C function definitions when using C++ */
#ifdef __cplusplus
}
#endif
#include "close_code.h"

#endif /* SDL_syswm_h_ */

/* vi: set ts=4 sw=4 expandtab: */
