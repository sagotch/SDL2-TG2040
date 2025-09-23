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

#ifndef SDL_main_h_
#define SDL_main_h_

#include "SDL_stdinc.h"

/**
 *  \file SDL_main.h
 *
 *  Redefine main() on some platforms so that it is called by SDL.
 */

#ifndef SDL_MAIN_HANDLED
#if   defined(__PSP__)
/* On PSP SDL provides a main function that sets the module info,
   activates the GPU and starts the thread required to be able to exit
   the software.

   If you provide this yourself, you may define SDL_MAIN_HANDLED
 */
#define SDL_MAIN_AVAILABLE

#endif
#endif /* SDL_MAIN_HANDLED */

#ifndef SDLMAIN_DECLSPEC
#define SDLMAIN_DECLSPEC
#endif

/**
 *  \file SDL_main.h
 *
 *  The application's main() function must be called with C linkage,
 *  and should be declared like this:
 *  \code
 *  #ifdef __cplusplus
 *  extern "C"
 *  #endif
 *  int main(int argc, char *argv[])
 *  {
 *  }
 *  \endcode
 */

#if defined(SDL_MAIN_NEEDED) || defined(SDL_MAIN_AVAILABLE)
#define main    SDL_main
#endif

#include "begin_code.h"
#ifdef __cplusplus
extern "C" {
#endif

/**
 *  The prototype for the application's main() function
 */
typedef int (*SDL_main_func)(int argc, char *argv[]);
extern SDLMAIN_DECLSPEC int SDL_main(int argc, char *argv[]);


/**
 * Circumvent failure of SDL_Init() when not using SDL_main() as an entry
 * point.
 *
 * This function is defined in SDL_main.h, along with the preprocessor rule to
 * redefine main() as SDL_main(). Thus to ensure that your main() function
 * will not be changed it is necessary to define SDL_MAIN_HANDLED before
 * including SDL.h.
 *
 * \since This function is available since SDL 2.0.0.
 *
 * \sa SDL_Init
 */
extern DECLSPEC void SDLCALL SDL_SetMainReady(void);


#ifdef __cplusplus
}
#endif
#include "close_code.h"

#endif /* SDL_main_h_ */

/* vi: set ts=4 sw=4 expandtab: */
