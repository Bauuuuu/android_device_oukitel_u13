/*
 * Copyright (C) 2016 The M.A.D. Team
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */


#define LOG_TAG "lights"

#include <cutils/log.h>

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>

#include <sys/ioctl.h>
#include <sys/types.h>

#include <hardware/lights.h>

/******************************************************************************/

static pthread_once_t g_init = PTHREAD_ONCE_INIT;
static pthread_mutex_t g_lock = PTHREAD_MUTEX_INITIALIZER;
static struct light_state_t g_attention;
static struct light_state_t g_notification;
static struct light_state_t g_battery;

/* Red LED */
char const*const RED_LED_FILE
        = "/sys/class/leds/red/brightness";

char const*const RED_TRIGGER_FILE
        = "/sys/class/leds/red/trigger";

char const*const RED_DELAY_ON_FILE
        = "/sys/class/leds/red/delay_on";

char const*const RED_DELAY_OFF_FILE
        = "/sys/class/leds/red/delay_off";

/* Green LED */
char const*const GREEN_LED_FILE
        = "/sys/class/leds/green/brightness";

char const*const GREEN_TRIGGER_FILE
        = "/sys/class/leds/green/trigger";

char const*const GREEN_DELAY_ON_FILE
        = "/sys/class/leds/green/delay_on";

char const*const GREEN_DELAY_OFF_FILE
        = "/sys/class/leds/green/delay_off";

/* Blue LED */
char const*const BLUE_LED_FILE
        = "/sys/class/leds/blue/brightness";

char const*const BLUE_TRIGGER_FILE
        = "/sys/class/leds/blue/trigger";

char const*const BLUE_DELAY_ON_FILE
        = "/sys/class/leds/blue/delay_on";

char const*const BLUE_DELAY_OFF_FILE
        = "/sys/class/leds/blue/delay_off";

char const*const LCD_FILE
        = "/sys/class/leds/lcd-backlight/brightness";

char const*const BUTTONS_FILE
        = "/sys/class/leds/button-backlight/brightness";

/**
 * device methods
 */

void init_globals(void)
{
    // init the mutex
    pthread_mutex_init(&g_lock, NULL);
}

static int
write_int(char const* path, int value)
{
    int fd;
    static int already_warned = 0;

    fd = open(path, O_RDWR);
    if (fd >= 0) {
        char buffer[20];
        int bytes = snprintf(buffer, sizeof(buffer), "%d\n", value);
        ssize_t amt = write(fd, buffer, (size_t)bytes);
        close(fd);
        return amt == -1 ? -errno : 0;
    } else {
        if (already_warned == 0) {
            ALOGE("write_int failed to open %s\n", path);
            already_warned = 1;
        }
        return -errno;
    }
}

static int
write_str(char const* path, char *str)
{
    int fd;
    static int already_warned = 0;

    fd = open(path, O_RDWR);
    if (fd >= 0) {
        char buffer[20];
        int bytes = snprintf(buffer, sizeof(buffer), "%s\n", str);
        ssize_t amt = write(fd, buffer, (size_t)bytes);
        close(fd);
        return amt == -1 ? -errno : 0;
    } else {
        if (already_warned == 0) {
            ALOGE("write_int failed to open %s\n", path);
            already_warned = 1;
        }
        return -errno;
    }
}

static int
is_lit(struct light_state_t const* state)
{
    return state->color & 0x00ffffff;
}

static int
rgb_to_brightness(struct light_state_t const* state)
{
    int color = state->color & 0x00ffffff;
    return ((77*((color>>16)&0x00ff))
            + (150*((color>>8)&0x00ff)) + (29*(color&0x00ff))) >> 8;
}

static int
set_light_backlight(struct light_device_t* dev,
        struct light_state_t const* state)
{
    if (!dev) {
        return -1;
    }
    int err = 0;
    int brightness = rgb_to_brightness(state);
    pthread_mutex_lock(&g_lock);
    err = write_int(LCD_FILE, brightness);
    pthread_mutex_unlock(&g_lock);
    return err;
}

static int
set_light_buttons(struct light_device_t* dev,
        struct light_state_t const* state)
{
    if (!dev) {
        return -1;
    }
    int err = 0;
    int on = is_lit(state);
    pthread_mutex_lock(&g_lock);
    err = write_int(BUTTONS_FILE, on ? 255 : 0);
    pthread_mutex_unlock(&g_lock);
    return err;
}

static int
set_speaker_light_locked(struct light_device_t* dev,
        struct light_state_t const* state)
{
    int red, green, blue, blink;
    int onMS, offMS;
    unsigned int colorRGB;

    if (!dev) {
        return -1;
    }

    switch (state->flashMode) {
        case LIGHT_FLASH_TIMED:
            onMS = state->flashOnMS;
            offMS = state->flashOffMS;
            break;
        case LIGHT_FLASH_NONE:
        default:
            onMS = 0;
            offMS = 0;
            break;
    }

    colorRGB = state->color;

    ALOGV("set_speaker_light_locked mode %d, colorRGB=%08X, onMS=%d, offMS=%d\n",
            state->flashMode, colorRGB, onMS, offMS);

    red = (colorRGB >> 16) & 0xFF;
    green = (colorRGB >> 8) & 0xFF;
    blue = colorRGB & 0xFF;
    blink = onMS > 0 && offMS > 0;

    write_int(RED_LED_FILE, 0);
    write_str(RED_TRIGGER_FILE, "none");
    write_int(GREEN_LED_FILE, 0);
    write_str(GREEN_TRIGGER_FILE, "none");
    write_int(BLUE_LED_FILE, 0);
    write_str(BLUE_TRIGGER_FILE, "none");

    if (blink) {
        if (red >= 128) {
            write_str(RED_TRIGGER_FILE, "timer");
            write_int(RED_DELAY_ON_FILE, onMS);
            write_int(RED_DELAY_OFF_FILE, offMS);
        }
        // disable mixing green with blue or/and red
        // green led isn't blinking in sync with blue and red
        if (green >= 128 && red < 128 && blue < 128) { 
            write_str(GREEN_TRIGGER_FILE, "timer");
            write_int(GREEN_DELAY_ON_FILE, onMS);
            write_int(GREEN_DELAY_OFF_FILE, offMS);
        }
        if (blue >= 128) {
            write_str(BLUE_TRIGGER_FILE, "timer");
            write_int(BLUE_DELAY_ON_FILE, onMS);
            write_int(BLUE_DELAY_OFF_FILE, offMS);
        }
    }
    else {
        if (red >= 128) {
            write_int(RED_LED_FILE, 255);
        }
        if (green >= 128) {
            write_int(GREEN_LED_FILE, 255);
        }
        if (blue >= 128) {
            write_int(BLUE_LED_FILE, 255);
        }
    }

    return 0;
}

static void
handle_speaker_light_locked(struct light_device_t* dev)
{
    if (is_lit(&g_attention)) {
        set_speaker_light_locked(dev, &g_attention);
    } else if (is_lit(&g_notification)) {
        set_speaker_light_locked(dev, &g_notification);
    } else {
        set_speaker_light_locked(dev, &g_battery);
    }
}

static int
set_light_battery(struct light_device_t* dev,
        struct light_state_t const* state)
{
    pthread_mutex_lock(&g_lock);
    g_battery = *state;
    handle_speaker_light_locked(dev);
    pthread_mutex_unlock(&g_lock);
    return 0;
}

static int
set_light_notifications(struct light_device_t* dev,
        struct light_state_t const* state)
{
    pthread_mutex_lock(&g_lock);
    g_notification = *state;
    handle_speaker_light_locked(dev);
    pthread_mutex_unlock(&g_lock);
    return 0;
}

static int
set_light_attention(struct light_device_t* dev,
        struct light_state_t const* state)
{
    pthread_mutex_lock(&g_lock);
    g_attention = *state;
    handle_speaker_light_locked(dev);
    pthread_mutex_unlock(&g_lock);
    return 0;
}

/** Close the lights device */
static int
close_lights(struct light_device_t *dev)
{
    if (dev) {
        free(dev);
    }
    return 0;
}


/******************************************************************************/

/**
 * module methods
 */

/** Open a new instance of a lights device using name */
static int open_lights(const struct hw_module_t* module, char const* name,
        struct hw_device_t** device)
{
    int (*set_light)(struct light_device_t* dev,
            struct light_state_t const* state);

    if (0 == strcmp(LIGHT_ID_BACKLIGHT, name))
        set_light = set_light_backlight;
    else if (0 == strcmp(LIGHT_ID_BUTTONS, name))
        set_light = set_light_buttons;
    else if (0 == strcmp(LIGHT_ID_BATTERY, name))
        set_light = set_light_battery;
    else if (0 == strcmp(LIGHT_ID_NOTIFICATIONS, name))
        set_light = set_light_notifications;
    else if (0 == strcmp(LIGHT_ID_ATTENTION, name))
        set_light = set_light_attention;
    else
        return -EINVAL;

    pthread_once(&g_init, init_globals);

    struct light_device_t *dev = malloc(sizeof(struct light_device_t));

    if (!dev)
        return -ENOMEM;

    memset(dev, 0, sizeof(*dev));

    dev->common.tag = HARDWARE_DEVICE_TAG;
    dev->common.version = 0;
    dev->common.module = (struct hw_module_t*)module;
    dev->common.close = (int (*)(struct hw_device_t*))close_lights;
    dev->set_light = set_light;

    *device = (struct hw_device_t*)dev;
    return 0;
}


static struct hw_module_methods_t lights_module_methods = {
    .open =  open_lights,
};

/*
 * The lights Module
 */
struct hw_module_t HAL_MODULE_INFO_SYM = {
    .tag = HARDWARE_MODULE_TAG,
    .version_major = 1,
    .version_minor = 0,
    .id = LIGHTS_HARDWARE_MODULE_ID,
    .name = "M.A.D. Lights Module",
    .author = "M.A.D. Team",
    .methods = &lights_module_methods,
};
