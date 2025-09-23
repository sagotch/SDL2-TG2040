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

/* Simple log messages in SDL */

#include "SDL_error.h"
#include "SDL_log.h"
#include "SDL_mutex.h"
#include "SDL_log_c.h"

#if HAVE_STDIO_H
#include <stdio.h>
#endif

#include "stdlib/SDL_vacopy.h"


/* The size of the stack buffer to use for rendering log messages. */
#define SDL_MAX_LOG_MESSAGE_STACK 256

#define DEFAULT_PRIORITY                SDL_LOG_PRIORITY_CRITICAL
#define DEFAULT_ASSERT_PRIORITY         SDL_LOG_PRIORITY_WARN
#define DEFAULT_APPLICATION_PRIORITY    SDL_LOG_PRIORITY_INFO
#define DEFAULT_TEST_PRIORITY           SDL_LOG_PRIORITY_VERBOSE

typedef struct SDL_LogLevel
{
    int category;
    SDL_LogPriority priority;
    struct SDL_LogLevel *next;
} SDL_LogLevel;

/* The default log output function */
static void SDLCALL SDL_LogOutput(void *userdata, int category, SDL_LogPriority priority, const char *message);

static SDL_LogLevel *SDL_loglevels;
static SDL_LogPriority SDL_default_priority = DEFAULT_PRIORITY;
static SDL_LogPriority SDL_assert_priority = DEFAULT_ASSERT_PRIORITY;
static SDL_LogPriority SDL_application_priority = DEFAULT_APPLICATION_PRIORITY;
static SDL_LogPriority SDL_test_priority = DEFAULT_TEST_PRIORITY;
static SDL_LogOutputFunction SDL_log_function = SDL_LogOutput;
static void *SDL_log_userdata = NULL;
static SDL_mutex *log_function_mutex = NULL;

static const char *SDL_priority_prefixes[SDL_NUM_LOG_PRIORITIES] = {
    NULL,
    "VERBOSE",
    "DEBUG",
    "INFO",
    "WARN",
    "ERROR",
    "CRITICAL"
};

void
SDL_LogInit(void)
{
    if (!log_function_mutex) {
        /* if this fails we'll try to continue without it. */
        log_function_mutex = SDL_CreateMutex();
    }
}

void
SDL_LogQuit(void)
{
    SDL_LogResetPriorities();
    if (log_function_mutex) {
        SDL_DestroyMutex(log_function_mutex);
        log_function_mutex = NULL;
    }
}

void
SDL_LogSetAllPriority(SDL_LogPriority priority)
{
    SDL_LogLevel *entry;

    for (entry = SDL_loglevels; entry; entry = entry->next) {
        entry->priority = priority;
    }
    SDL_default_priority = priority;
    SDL_assert_priority = priority;
    SDL_application_priority = priority;
}

void
SDL_LogSetPriority(int category, SDL_LogPriority priority)
{
    SDL_LogLevel *entry;

    for (entry = SDL_loglevels; entry; entry = entry->next) {
        if (entry->category == category) {
            entry->priority = priority;
            return;
        }
    }

    /* Create a new entry */
    entry = (SDL_LogLevel *)SDL_malloc(sizeof(*entry));
    if (entry) {
        entry->category = category;
        entry->priority = priority;
        entry->next = SDL_loglevels;
        SDL_loglevels = entry;
    }
}

SDL_LogPriority
SDL_LogGetPriority(int category)
{
    SDL_LogLevel *entry;

    for (entry = SDL_loglevels; entry; entry = entry->next) {
        if (entry->category == category) {
            return entry->priority;
        }
    }

    if (category == SDL_LOG_CATEGORY_TEST) {
        return SDL_test_priority;
    } else if (category == SDL_LOG_CATEGORY_APPLICATION) {
        return SDL_application_priority;
    } else if (category == SDL_LOG_CATEGORY_ASSERT) {
        return SDL_assert_priority;
    } else {
        return SDL_default_priority;
    }
}

void
SDL_LogResetPriorities(void)
{
    SDL_LogLevel *entry;

    while (SDL_loglevels) {
        entry = SDL_loglevels;
        SDL_loglevels = entry->next;
        SDL_free(entry);
    }

    SDL_default_priority = DEFAULT_PRIORITY;
    SDL_assert_priority = DEFAULT_ASSERT_PRIORITY;
    SDL_application_priority = DEFAULT_APPLICATION_PRIORITY;
    SDL_test_priority = DEFAULT_TEST_PRIORITY;
}

void
SDL_Log(SDL_PRINTF_FORMAT_STRING const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    SDL_LogMessageV(SDL_LOG_CATEGORY_APPLICATION, SDL_LOG_PRIORITY_INFO, fmt, ap);
    va_end(ap);
}

void
SDL_LogVerbose(int category, SDL_PRINTF_FORMAT_STRING const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    SDL_LogMessageV(category, SDL_LOG_PRIORITY_VERBOSE, fmt, ap);
    va_end(ap);
}

void
SDL_LogDebug(int category, SDL_PRINTF_FORMAT_STRING const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    SDL_LogMessageV(category, SDL_LOG_PRIORITY_DEBUG, fmt, ap);
    va_end(ap);
}

void
SDL_LogInfo(int category, SDL_PRINTF_FORMAT_STRING const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    SDL_LogMessageV(category, SDL_LOG_PRIORITY_INFO, fmt, ap);
    va_end(ap);
}

void
SDL_LogWarn(int category, SDL_PRINTF_FORMAT_STRING const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    SDL_LogMessageV(category, SDL_LOG_PRIORITY_WARN, fmt, ap);
    va_end(ap);
}

void
SDL_LogError(int category, SDL_PRINTF_FORMAT_STRING const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    SDL_LogMessageV(category, SDL_LOG_PRIORITY_ERROR, fmt, ap);
    va_end(ap);
}

void
SDL_LogCritical(int category, SDL_PRINTF_FORMAT_STRING const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    SDL_LogMessageV(category, SDL_LOG_PRIORITY_CRITICAL, fmt, ap);
    va_end(ap);
}

void
SDL_LogMessage(int category, SDL_LogPriority priority, SDL_PRINTF_FORMAT_STRING const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    SDL_LogMessageV(category, priority, fmt, ap);
    va_end(ap);
}

void
SDL_LogMessageV(int category, SDL_LogPriority priority, const char *fmt, va_list ap)
{
    char *message = NULL;
    char stack_buf[SDL_MAX_LOG_MESSAGE_STACK];
    size_t len_plus_term;
    int len;
    va_list aq;

    /* Nothing to do if we don't have an output function */
    if (!SDL_log_function) {
        return;
    }

    /* Make sure we don't exceed array bounds */
    if ((int)priority < 0 || priority >= SDL_NUM_LOG_PRIORITIES) {
        return;
    }

    /* See if we want to do anything with this message */
    if (priority < SDL_LogGetPriority(category)) {
        return;
    }

    if (!log_function_mutex) {
        /* this mutex creation can race if you log from two threads at startup. You should have called SDL_Init first! */
        log_function_mutex = SDL_CreateMutex();
    }

    /* Render into stack buffer */
    va_copy(aq, ap);
    len = SDL_vsnprintf(stack_buf, sizeof(stack_buf), fmt, aq);
    va_end(aq);

    if (len < 0)
        return;

    /* If message truncated, allocate and re-render */
    if (len >= sizeof(stack_buf) && SDL_size_add_overflow(len, 1, &len_plus_term) == 0) {
        /* Allocate exactly what we need, including the zero-terminator */
        message = (char *)SDL_malloc(len_plus_term);
        if (!message)
            return;
        va_copy(aq, ap);
        len = SDL_vsnprintf(message, len_plus_term, fmt, aq);
        va_end(aq);
    } else {
        message = stack_buf;
    }

    /* Chop off final endline. */
    if ((len > 0) && (message[len-1] == '\n')) {
        message[--len] = '\0';
        if ((len > 0) && (message[len-1] == '\r')) {  /* catch "\r\n", too. */
            message[--len] = '\0';
        }
    }

    if (log_function_mutex) {
        SDL_LockMutex(log_function_mutex);
    }

    SDL_log_function(SDL_log_userdata, category, priority, message);

    if (log_function_mutex) {
        SDL_UnlockMutex(log_function_mutex);
    }

    /* Free only if dynamically allocated */
    if (message != stack_buf) {
        SDL_free(message);
    }
}

static void SDLCALL
SDL_LogOutput(void *userdata, int category, SDL_LogPriority priority,
              const char *message)
{
#if   defined(__PSP__) || defined(__PS2__)
    {
        FILE*        pFile;
        pFile = fopen ("SDL_Log.txt", "a");
        fprintf(pFile, "%s: %s\n", SDL_priority_prefixes[priority], message);
        fclose (pFile);
    }
#endif
#if HAVE_STDIO_H && \
    !(defined(__APPLE__) && (defined(SDL_VIDEO_DRIVER_COCOA) || defined(SDL_VIDEO_DRIVER_UIKIT)))
    fprintf(stderr, "%s: %s\n", SDL_priority_prefixes[priority], message);
#endif
}

void
SDL_LogGetOutputFunction(SDL_LogOutputFunction *callback, void **userdata)
{
    if (callback) {
        *callback = SDL_log_function;
    }
    if (userdata) {
        *userdata = SDL_log_userdata;
    }
}

void
SDL_LogSetOutputFunction(SDL_LogOutputFunction callback, void *userdata)
{
    SDL_log_function = callback;
    SDL_log_userdata = userdata;
}

/* vi: set ts=4 sw=4 expandtab: */
