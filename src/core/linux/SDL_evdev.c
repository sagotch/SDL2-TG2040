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
#include "../../SDL_internal.h"

#ifdef SDL_INPUT_LINUXEV

/* This is based on the linux joystick driver */
/* References: https://www.kernel.org/doc/Documentation/input/input.txt 
 *             https://www.kernel.org/doc/Documentation/input/event-codes.txt
 *             /usr/include/linux/input.h
 *             The evtest application is also useful to debug the protocol
 */

#include "SDL_evdev.h"
#include "SDL_evdev_kbd.h"

#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/input.h>

#include "SDL.h"
#include "SDL_endian.h"
#include "SDL_scancode.h"
#include "../../events/SDL_events_c.h"
#include "../../events/SDL_scancode_tables_c.h"
#include "../../core/linux/SDL_evdev_capabilities.h"
#include "../../core/linux/SDL_udev.h"

/* These are not defined in older Linux kernel headers */
#ifndef SYN_DROPPED
#define SYN_DROPPED 3
#endif
#ifndef ABS_MT_SLOT
#define ABS_MT_SLOT         0x2f
#define ABS_MT_POSITION_X   0x35
#define ABS_MT_POSITION_Y   0x36
#define ABS_MT_TRACKING_ID  0x39
#define ABS_MT_PRESSURE     0x3a
#endif
#ifndef REL_WHEEL_HI_RES
#define REL_WHEEL_HI_RES    0x0b
#define REL_HWHEEL_HI_RES   0x0c
#endif

typedef struct SDL_evdevlist_item
{
    char *path;
    int fd;

    /* TODO: use this for every device, not just touchscreen */
    SDL_bool out_of_sync;

    /* TODO: expand on this to have data for every possible class (mouse,
       keyboard, touchpad, etc.). Also there's probably some things in here we
       can pull out to the SDL_evdevlist_item i.e. name */
    SDL_bool is_touchscreen;
    struct {
        char* name;

        int min_x, max_x, range_x;
        int min_y, max_y, range_y;
        int min_pressure, max_pressure, range_pressure;

        int max_slots;
        int current_slot;
        struct {
            enum {
                EVDEV_TOUCH_SLOTDELTA_NONE = 0,
                EVDEV_TOUCH_SLOTDELTA_DOWN,
                EVDEV_TOUCH_SLOTDELTA_UP,
                EVDEV_TOUCH_SLOTDELTA_MOVE
            } delta;
            int tracking_id;
            int x, y, pressure;
        } * slots;

    } * touchscreen_data;

    /* Mouse state */
    SDL_bool high_res_wheel;
    SDL_bool high_res_hwheel;
    SDL_bool relative_mouse;
    int mouse_x, mouse_y;
    int mouse_wheel, mouse_hwheel;

    struct SDL_evdevlist_item *next;
} SDL_evdevlist_item;

typedef struct SDL_EVDEV_PrivateData
{
    int ref_count;
    int num_devices;
    SDL_evdevlist_item *first;
    SDL_evdevlist_item *last;
    SDL_EVDEV_keyboard_state *kbd;
} SDL_EVDEV_PrivateData;

#undef _THIS
#define _THIS SDL_EVDEV_PrivateData *_this
static _THIS = NULL;

static SDL_Scancode SDL_EVDEV_translate_keycode(int keycode);
static void SDL_EVDEV_sync_device(SDL_evdevlist_item *item);
static int SDL_EVDEV_device_removed(const char *dev_path);

int
SDL_EVDEV_Init(void)
{
    if (_this == NULL) {
        _this = (SDL_EVDEV_PrivateData*)SDL_calloc(1, sizeof(*_this));
        if (_this == NULL) {
            return SDL_OutOfMemory();
        }

        {
            /* Allow the user to specify a list of devices explicitly of
               the form:
                  deviceclass:path[,deviceclass:path[,...]]
               where device class is an integer representing the
               SDL_UDEV_deviceclass and path is the full path to
               the event device. */
            const char* devices = SDL_getenv("SDL_EVDEV_DEVICES");
            if (devices) {
                /* Assume this is the old use of the env var and it is not in
                   ROM. */
                char* rest = (char*) devices;
                char* spec;
                while ((spec = strtok_r(rest, ",", &rest))) {
                    char* endofcls = 0;
                    long cls = strtol(spec, &endofcls, 0);
                    if (endofcls)
                        SDL_EVDEV_device_added(endofcls + 1, cls);
                }
            }
            else {
                /* TODO: Scan the devices manually, like a caveman */
            }
        }

        _this->kbd = SDL_EVDEV_kbd_init();
    }

    _this->ref_count += 1;

    return 0;
}

void
SDL_EVDEV_Quit(void)
{
    if (_this == NULL) {
        return;
    }

    _this->ref_count -= 1;

    if (_this->ref_count < 1) {

        SDL_EVDEV_kbd_quit(_this->kbd);

        /* Remove existing devices */
        while(_this->first != NULL) {
            SDL_EVDEV_device_removed(_this->first->path);
        }

        SDL_assert(_this->first == NULL);
        SDL_assert(_this->last == NULL);
        SDL_assert(_this->num_devices == 0);

        SDL_free(_this);
        _this = NULL;
    }
}

void 
SDL_EVDEV_Poll(void)
{
    struct input_event events[32];
    int i, len;
    SDL_evdevlist_item *item;
    SDL_Scancode scan_code;

    if (!_this) {
        return;
    }

    for (item = _this->first; item != NULL; item = item->next) {
        while ((len = read(item->fd, events, (sizeof events))) > 0) {
            len /= sizeof(events[0]);
            for (i = 0; i < len; ++i) {
                /* special handling for touchscreen, that should eventually be
                   used for all devices */
                if (item->out_of_sync && item->is_touchscreen &&
                    events[i].type == EV_SYN && events[i].code != SYN_REPORT) {
                    break;
                }

                switch (events[i].type) {
                case EV_KEY:
                    /* Probably keyboard */
                    scan_code = SDL_EVDEV_translate_keycode(events[i].code);
                    if (scan_code != SDL_SCANCODE_UNKNOWN) {
                        if (events[i].value == 0) {
                            SDL_SendKeyboardKey(SDL_RELEASED, scan_code);
                        } else if (events[i].value == 1 || events[i].value == 2 /* key repeated */) {
                            SDL_SendKeyboardKey(SDL_PRESSED, scan_code);
                        }
                    }
                    SDL_EVDEV_kbd_keycode(_this->kbd, events[i].code, events[i].value);
                    break;
                case EV_ABS:
                    break;
                case EV_REL:
                    break;
                case EV_SYN:
                    switch (events[i].code) {
                    case SYN_REPORT:
                        break;
                    case SYN_DROPPED:
                        SDL_EVDEV_sync_device(item);
                        break;
                    default:
                        break;
                    }
                    break;
                }
            }
        }    
    }
}

static SDL_Scancode
SDL_EVDEV_translate_keycode(int keycode)
{
    SDL_Scancode scancode = SDL_GetScancodeFromTable(SDL_SCANCODE_TABLE_LINUX, keycode);

#ifdef DEBUG_SCANCODES
    if (scancode == SDL_SCANCODE_UNKNOWN) {
        /* BTN_TOUCH is handled elsewhere, but we might still end up here if
           you get an unexpected BTN_TOUCH from something SDL believes is not
           a touch device. In this case, we'd rather not get a misleading
           SDL_Log message about an unknown key. */
        if (keycode != BTN_TOUCH) {
            SDL_Log("The key you just pressed is not recognized by SDL. To help "
                "get this fixed, please report this to the SDL forums/mailing list "
                "<https://discourse.libsdl.org/> EVDEV KeyCode %d", keycode);
        }
    }
#endif /* DEBUG_SCANCODES */

    return scancode;
}

static void
SDL_EVDEV_sync_device(SDL_evdevlist_item *item) 
{
#ifdef EVIOCGMTSLOTS
    int i, ret;
    struct input_absinfo abs_info;
    /*
     * struct input_mt_request_layout {
     *     __u32 code;
     *     __s32 values[num_slots];
     * };
     *
     * this is the structure we're trying to emulate
     */
    Uint32* mt_req_code;
    Sint32* mt_req_values;
    size_t mt_req_size;

    /* TODO: sync devices other than touchscreen */
    if (!item->is_touchscreen)
        return;

    mt_req_size = sizeof(*mt_req_code) +
        sizeof(*mt_req_values) * item->touchscreen_data->max_slots;

    mt_req_code = SDL_calloc(1, mt_req_size);
    if (mt_req_code == NULL) {
        return;
    }

    mt_req_values = (Sint32*)mt_req_code + 1;

    *mt_req_code = ABS_MT_TRACKING_ID;
    ret = ioctl(item->fd, EVIOCGMTSLOTS(mt_req_size), mt_req_code);
    if (ret < 0) {
        SDL_free(mt_req_code);
        return;
    }
    for(i = 0; i < item->touchscreen_data->max_slots; i++) {
        /*
         * This doesn't account for the very edge case of the user removing their
         * finger and replacing it on the screen during the time we're out of sync,
         * which'll mean that we're not going from down -> up or up -> down, we're
         * going from down -> down but with a different tracking id, meaning we'd
         * have to tell SDL of the two events, but since we wait till SYN_REPORT in
         * SDL_EVDEV_Poll to tell SDL, the current structure of this code doesn't
         * allow it. Lets just pray to God it doesn't happen.
         */
        if (item->touchscreen_data->slots[i].tracking_id < 0 &&
            mt_req_values[i] >= 0) {
            item->touchscreen_data->slots[i].tracking_id = mt_req_values[i];
            item->touchscreen_data->slots[i].delta = EVDEV_TOUCH_SLOTDELTA_DOWN;
        } else if (item->touchscreen_data->slots[i].tracking_id >= 0 &&
            mt_req_values[i] < 0) {
            item->touchscreen_data->slots[i].tracking_id = -1;
            item->touchscreen_data->slots[i].delta = EVDEV_TOUCH_SLOTDELTA_UP;
        }
    }

    *mt_req_code = ABS_MT_POSITION_X;
    ret = ioctl(item->fd, EVIOCGMTSLOTS(mt_req_size), mt_req_code);
    if (ret < 0) {
        SDL_free(mt_req_code);
        return;
    }
    for(i = 0; i < item->touchscreen_data->max_slots; i++) {
        if (item->touchscreen_data->slots[i].tracking_id >= 0 &&
            item->touchscreen_data->slots[i].x != mt_req_values[i]) {
            item->touchscreen_data->slots[i].x = mt_req_values[i];
            if (item->touchscreen_data->slots[i].delta ==
                EVDEV_TOUCH_SLOTDELTA_NONE) {
                item->touchscreen_data->slots[i].delta =
                    EVDEV_TOUCH_SLOTDELTA_MOVE;
            }
        }
    }

    *mt_req_code = ABS_MT_POSITION_Y;
    ret = ioctl(item->fd, EVIOCGMTSLOTS(mt_req_size), mt_req_code);
    if (ret < 0) {
        SDL_free(mt_req_code);
        return;
    }
    for(i = 0; i < item->touchscreen_data->max_slots; i++) {
        if (item->touchscreen_data->slots[i].tracking_id >= 0 &&
            item->touchscreen_data->slots[i].y != mt_req_values[i]) {
            item->touchscreen_data->slots[i].y = mt_req_values[i];
            if (item->touchscreen_data->slots[i].delta ==
                EVDEV_TOUCH_SLOTDELTA_NONE) {
                item->touchscreen_data->slots[i].delta =
                    EVDEV_TOUCH_SLOTDELTA_MOVE;
            }
        }
    }

    *mt_req_code = ABS_MT_PRESSURE;
    ret = ioctl(item->fd, EVIOCGMTSLOTS(mt_req_size), mt_req_code);
    if (ret < 0) {
        SDL_free(mt_req_code);
        return;
    }
    for(i = 0; i < item->touchscreen_data->max_slots; i++) {
        if (item->touchscreen_data->slots[i].tracking_id >= 0 &&
            item->touchscreen_data->slots[i].pressure != mt_req_values[i]) {
            item->touchscreen_data->slots[i].pressure = mt_req_values[i];
            if (item->touchscreen_data->slots[i].delta ==
                EVDEV_TOUCH_SLOTDELTA_NONE) {
                item->touchscreen_data->slots[i].delta =
                    EVDEV_TOUCH_SLOTDELTA_MOVE;
            }
        }
    }

    ret = ioctl(item->fd, EVIOCGABS(ABS_MT_SLOT), &abs_info);
    if (ret < 0) {
        SDL_free(mt_req_code);
        return;
    }
    item->touchscreen_data->current_slot = abs_info.value;

    SDL_free(mt_req_code);

#endif /* EVIOCGMTSLOTS */
}

int
SDL_EVDEV_device_added(const char *dev_path, int udev_class)
{
    SDL_evdevlist_item *item;

    /* Check to make sure it's not already in list. */
    for (item = _this->first; item != NULL; item = item->next) {
        if (SDL_strcmp(dev_path, item->path) == 0) {
            return -1;  /* already have this one */
        }
    }

    item = (SDL_evdevlist_item *) SDL_calloc(1, sizeof (SDL_evdevlist_item));
    if (item == NULL) {
        return SDL_OutOfMemory();
    }

    item->fd = open(dev_path, O_RDONLY | O_NONBLOCK | O_CLOEXEC);
    if (item->fd < 0) {
        SDL_free(item);
        return SDL_SetError("Unable to open %s", dev_path);
    }

    item->path = SDL_strdup(dev_path);
    if (item->path == NULL) {
        close(item->fd);
        SDL_free(item);
        return SDL_OutOfMemory();
    }

    if (_this->last == NULL) {
        _this->first = _this->last = item;
    } else {
        _this->last->next = item;
        _this->last = item;
    }

    SDL_EVDEV_sync_device(item);

    return _this->num_devices++;
}

static int
SDL_EVDEV_device_removed(const char *dev_path)
{
    SDL_evdevlist_item *item;
    SDL_evdevlist_item *prev = NULL;

    for (item = _this->first; item != NULL; item = item->next) {
        /* found it, remove it. */
        if (SDL_strcmp(dev_path, item->path) == 0) {
            if (prev != NULL) {
                prev->next = item->next;
            } else {
                SDL_assert(_this->first == item);
                _this->first = item->next;
            }
            if (item == _this->last) {
                _this->last = prev;
            }
            close(item->fd);
            SDL_free(item->path);
            SDL_free(item);
            _this->num_devices--;
            return 0;
        }
        prev = item;
    }

    return -1;
}


#endif /* SDL_INPUT_LINUXEV */

/* vi: set ts=4 sw=4 expandtab: */
