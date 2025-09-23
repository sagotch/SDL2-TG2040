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
#include "./SDL_internal.h"

#include <unistd.h> /* _exit(), etc. */

/* this checks for HAVE_DBUS_DBUS_H internally. */
#include "core/linux/SDL_dbus.h"

/* Initialization code for SDL */

#include "SDL.h"
#include "SDL_bits.h"
#include "SDL_revision.h"
#include "SDL_assert_c.h"
#include "SDL_log_c.h"
#include "events/SDL_events_c.h"

/* Initialization/Cleanup routines */
#if !SDL_TIMERS_DISABLED
# include "timer/SDL_timer_c.h"
#endif

#ifdef SDL_BUILD_MAJOR_VERSION
SDL_COMPILE_TIME_ASSERT(SDL_BUILD_MAJOR_VERSION,
                        SDL_MAJOR_VERSION == SDL_BUILD_MAJOR_VERSION);
SDL_COMPILE_TIME_ASSERT(SDL_BUILD_MINOR_VERSION,
                        SDL_MINOR_VERSION == SDL_BUILD_MINOR_VERSION);
SDL_COMPILE_TIME_ASSERT(SDL_BUILD_MICRO_VERSION,
                        SDL_PATCHLEVEL == SDL_BUILD_MICRO_VERSION);
#endif

SDL_COMPILE_TIME_ASSERT(SDL_MAJOR_VERSION_min, SDL_MAJOR_VERSION >= 0);
/* Limited only by the need to fit in SDL_version */
SDL_COMPILE_TIME_ASSERT(SDL_MAJOR_VERSION_max, SDL_MAJOR_VERSION <= 255);

SDL_COMPILE_TIME_ASSERT(SDL_MINOR_VERSION_min, SDL_MINOR_VERSION >= 0);
/* Limited only by the need to fit in SDL_version */
SDL_COMPILE_TIME_ASSERT(SDL_MINOR_VERSION_max, SDL_MINOR_VERSION <= 255);

SDL_COMPILE_TIME_ASSERT(SDL_PATCHLEVEL_min, SDL_PATCHLEVEL >= 0);
/* Limited by its encoding in SDL_VERSIONNUM and in the ABI versions */
SDL_COMPILE_TIME_ASSERT(SDL_PATCHLEVEL_max, SDL_PATCHLEVEL <= 99);

/* This is not declared in any header, although it is shared between some
    parts of SDL, because we don't want anything calling it without an
    extremely good reason. */
extern SDL_NORETURN void SDL_ExitProcess(int exitcode);
SDL_NORETURN void SDL_ExitProcess(int exitcode)
{
#if   defined(HAVE__EXIT) /* Upper case _Exit() */
    _Exit(exitcode);
#else
    _exit(exitcode);
#endif
}


/* The initialized subsystems */
#ifdef SDL_MAIN_NEEDED
static SDL_bool SDL_MainIsReady = SDL_FALSE;
#else
static SDL_bool SDL_MainIsReady = SDL_TRUE;
#endif
static SDL_bool SDL_bInMainQuit = SDL_FALSE;
static Uint8 SDL_SubsystemRefCount[ 32 ];

/* Private helper to increment a subsystem's ref counter. */
static void
SDL_PrivateSubsystemRefCountIncr(Uint32 subsystem)
{
    const int subsystem_index = SDL_MostSignificantBitIndex32(subsystem);
    SDL_assert((subsystem_index < 0) || (SDL_SubsystemRefCount[subsystem_index] < 255));
    if (subsystem_index >= 0) {
        ++SDL_SubsystemRefCount[subsystem_index];
    }
}

/* Private helper to decrement a subsystem's ref counter. */
static void
SDL_PrivateSubsystemRefCountDecr(Uint32 subsystem)
{
    const int subsystem_index = SDL_MostSignificantBitIndex32(subsystem);
    if ((subsystem_index >= 0) && (SDL_SubsystemRefCount[subsystem_index] > 0)) {
        --SDL_SubsystemRefCount[subsystem_index];
    }
}

/* Private helper to check if a system needs init. */
static SDL_bool
SDL_PrivateShouldInitSubsystem(Uint32 subsystem)
{
    const int subsystem_index = SDL_MostSignificantBitIndex32(subsystem);
    SDL_assert((subsystem_index < 0) || (SDL_SubsystemRefCount[subsystem_index] < 255));
    return ((subsystem_index >= 0) && (SDL_SubsystemRefCount[subsystem_index] == 0)) ? SDL_TRUE : SDL_FALSE;
}

/* Private helper to check if a system needs to be quit. */
static SDL_bool
SDL_PrivateShouldQuitSubsystem(Uint32 subsystem) {
    const int subsystem_index = SDL_MostSignificantBitIndex32(subsystem);
    if ((subsystem_index >= 0) && (SDL_SubsystemRefCount[subsystem_index] == 0)) {
        return SDL_FALSE;
    }

    /* If we're in SDL_Quit, we shut down every subsystem, even if refcount
     * isn't zero.
     */
    return (((subsystem_index >= 0) && (SDL_SubsystemRefCount[subsystem_index] == 1)) || SDL_bInMainQuit) ? SDL_TRUE : SDL_FALSE;
}

void
SDL_SetMainReady(void)
{
    SDL_MainIsReady = SDL_TRUE;
}

int
SDL_InitSubSystem(Uint32 flags)
{
    Uint32 flags_initialized = 0;

    if (!SDL_MainIsReady) {
        return SDL_SetError("Application didn't initialize properly, did you include SDL_main.h in the file containing your main() function?");
    }

    SDL_LogInit();

    /* Clear the error message */
    SDL_ClearError();

#if SDL_USE_LIBDBUS
    SDL_DBus_Init();
#endif

    if ((flags & SDL_INIT_GAMECONTROLLER)) {
        /* game controller implies joystick */
        flags |= SDL_INIT_JOYSTICK;
    }

    if ((flags & (SDL_INIT_VIDEO|SDL_INIT_JOYSTICK|SDL_INIT_AUDIO))) {
        /* video or joystick or audio implies events */
        flags |= SDL_INIT_EVENTS;
    }

#if !SDL_TIMERS_DISABLED
    SDL_TicksInit();
#endif

    /* Initialize the event subsystem */
    if ((flags & SDL_INIT_EVENTS)) {
#if !SDL_EVENTS_DISABLED
        if (SDL_PrivateShouldInitSubsystem(SDL_INIT_EVENTS)) {
            if (SDL_EventsInit() < 0) {
                goto quit_and_error;
            }
        }
        SDL_PrivateSubsystemRefCountIncr(SDL_INIT_EVENTS);
        flags_initialized |= SDL_INIT_EVENTS;
#else
        SDL_SetError("SDL not built with events support");
        goto quit_and_error;
#endif
    }

    /* Initialize the timer subsystem */
    if ((flags & SDL_INIT_TIMER)){
#if !SDL_TIMERS_DISABLED && !SDL_TIMER_DUMMY
        if (SDL_PrivateShouldInitSubsystem(SDL_INIT_TIMER)) {
            if (SDL_TimerInit() < 0) {
                goto quit_and_error;
            }
        }
        SDL_PrivateSubsystemRefCountIncr(SDL_INIT_TIMER);
        flags_initialized |= SDL_INIT_TIMER;
#else
        SDL_SetError("SDL not built with timer support");
        goto quit_and_error;
#endif
    }

    /* Initialize the video subsystem */
    if ((flags & SDL_INIT_VIDEO)){
        if (SDL_PrivateShouldInitSubsystem(SDL_INIT_VIDEO)) {
            if (SDL_VideoInit(NULL) < 0) {
                goto quit_and_error;
            }
        }
        SDL_PrivateSubsystemRefCountIncr(SDL_INIT_VIDEO);
        flags_initialized |= SDL_INIT_VIDEO;
    }

    /* Initialize the audio subsystem */
    if ((flags & SDL_INIT_AUDIO)){
#if !SDL_AUDIO_DISABLED
        if (SDL_PrivateShouldInitSubsystem(SDL_INIT_AUDIO)) {
            if (SDL_AudioInit(NULL) < 0) {
                goto quit_and_error;
            }
        }
        SDL_PrivateSubsystemRefCountIncr(SDL_INIT_AUDIO);
        flags_initialized |= SDL_INIT_AUDIO;
#else
        SDL_SetError("SDL not built with audio support");
        goto quit_and_error;
#endif
    }

    (void) flags_initialized;  /* make static analysis happy, since this only gets used in error cases. */

    return (0);

quit_and_error:
    SDL_QuitSubSystem(flags_initialized);
    return (-1);
}

int
SDL_Init(Uint32 flags)
{
    return SDL_InitSubSystem(flags);
}

void
SDL_QuitSubSystem(Uint32 flags)
{

#if !SDL_AUDIO_DISABLED
    if ((flags & SDL_INIT_AUDIO)) {
        /* audio implies events */
        flags |= SDL_INIT_EVENTS;

        if (SDL_PrivateShouldQuitSubsystem(SDL_INIT_AUDIO)) {
            SDL_AudioQuit();
        }
        SDL_PrivateSubsystemRefCountDecr(SDL_INIT_AUDIO);
    }
#endif

    if ((flags & SDL_INIT_VIDEO)) {
        /* video implies events */
        flags |= SDL_INIT_EVENTS;

        if (SDL_PrivateShouldQuitSubsystem(SDL_INIT_VIDEO)) {
            SDL_VideoQuit();
        }
        SDL_PrivateSubsystemRefCountDecr(SDL_INIT_VIDEO);
    }

#if !SDL_TIMERS_DISABLED && !SDL_TIMER_DUMMY
    if ((flags & SDL_INIT_TIMER)) {
        if (SDL_PrivateShouldQuitSubsystem(SDL_INIT_TIMER)) {
            SDL_TimerQuit();
        }
        SDL_PrivateSubsystemRefCountDecr(SDL_INIT_TIMER);
    }
#endif

#if !SDL_EVENTS_DISABLED
    if ((flags & SDL_INIT_EVENTS)) {
        if (SDL_PrivateShouldQuitSubsystem(SDL_INIT_EVENTS)) {
            SDL_EventsQuit();
        }
        SDL_PrivateSubsystemRefCountDecr(SDL_INIT_EVENTS);
    }
#endif
}

Uint32
SDL_WasInit(Uint32 flags)
{
    int i;
    int num_subsystems = SDL_arraysize(SDL_SubsystemRefCount);
    Uint32 initialized = 0;

    /* Fast path for checking one flag */
    if (SDL_HasExactlyOneBitSet32(flags)) {
        int subsystem_index = SDL_MostSignificantBitIndex32(flags);
        return SDL_SubsystemRefCount[subsystem_index] ? flags : 0;
    }

    if (!flags) {
        flags = SDL_INIT_EVERYTHING;
    }

    num_subsystems = SDL_min(num_subsystems, SDL_MostSignificantBitIndex32(flags) + 1);

    /* Iterate over each bit in flags, and check the matching subsystem. */
    for (i = 0; i < num_subsystems; ++i) {
        if ((flags & 1) && SDL_SubsystemRefCount[i] > 0) {
            initialized |= (1 << i);
        }

        flags >>= 1;
    }

    return initialized;
}

void
SDL_Quit(void)
{
    SDL_bInMainQuit = SDL_TRUE;

    /* Quit all subsystems */
    SDL_QuitSubSystem(SDL_INIT_EVERYTHING);

#if !SDL_TIMERS_DISABLED
    SDL_TicksQuit();
#endif

    SDL_ClearHints();
    SDL_AssertionsQuit();

#if SDL_USE_LIBDBUS
    SDL_DBus_Quit();
#endif

    SDL_LogQuit();

    /* Now that every subsystem has been quit, we reset the subsystem refcount
     * and the list of initialized subsystems.
     */
    SDL_memset( SDL_SubsystemRefCount, 0x0, sizeof(SDL_SubsystemRefCount) );

    SDL_TLSCleanup();

    SDL_bInMainQuit = SDL_FALSE;
}

/* Get the library version number */
void
SDL_GetVersion(SDL_version * ver)
{
    static SDL_bool check_hint = SDL_TRUE;
    static SDL_bool legacy_version = SDL_FALSE;

    if (!ver) {
        return;
    }

    SDL_VERSION(ver);

    if (check_hint) {
        check_hint = SDL_FALSE;
        legacy_version = SDL_GetHintBoolean("SDL_LEGACY_VERSION", SDL_FALSE);
    }

    if (legacy_version) {
        /* Prior to SDL 2.24.0, the patch version was incremented with every release */
        ver->patch = ver->minor;
        ver->minor = 0;
    }
}

/* Get the library source revision */
const char *
SDL_GetRevision(void)
{
    return SDL_REVISION;
}

/* Get the library source revision number */
int
SDL_GetRevisionNumber(void)
{
    return 0;  /* doesn't make sense without Mercurial. */
}

/* Get the name of the platform */
const char *
SDL_GetPlatform(void)
{
#if __AIX__
    return "AIX";
#elif __BSDI__
    return "BSDI";
#elif __DREAMCAST__
    return "Dreamcast";
#elif __FREEBSD__
    return "FreeBSD";
#elif __HPUX__
    return "HP-UX";
#elif __IRIX__
    return "Irix";
#elif __LINUX__
    return "Linux";
#elif __MINT__
    return "Atari MiNT";
#elif __MACOS__
    return "MacOS Classic";
#elif __NETBSD__
    return "NetBSD";
#elif __OPENBSD__
    return "OpenBSD";
#elif __OSF__
    return "OSF/1";
#elif __QNXNTO__
    return "QNX Neutrino";
#elif __TVOS__
    return "tvOS";
#elif __PSP__
    return "PlayStation Portable";
#else
    return "Unknown (see SDL_platform.h)";
#endif
}

SDL_bool
SDL_IsTablet(void)
{
    return SDL_FALSE;
}

/* vi: set sts=4 ts=4 sw=4 expandtab: */
