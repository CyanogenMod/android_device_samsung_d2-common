/*
 * Copyright (C) 2008 The Android Open Source Project
 * Copyright (C) 2012 The CyanogenMod Project
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

#define LOG_NDEBUG 0
#define LOG_TAG "lights"
#include <cutils/log.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <hardware/lights.h>
#include <linux/leds-an30259a.h>

static pthread_once_t g_init = PTHREAD_ONCE_INIT;
static pthread_mutex_t g_lock = PTHREAD_MUTEX_INITIALIZER;

char const *const LCD_FILE = "/sys/class/leds/lcd-backlight/brightness";
char const *const LED_BLINK_FILE = "/sys/class/sec/led/led_blink";
char const *const LED_PATTERN_FILE = "/sys/class/sec/led/led_pattern";
char const *const BUTTON_FILE = "/sys/class/leds/button-backlight/brightness";

#define BATTERY_CHARGING 1
#define BATTERY_ERROR 2
#define BATTERY_LOW 4
#define BATTERY_CHARGED 5

void init_g_lock(void)
{
	pthread_mutex_init(&g_lock, NULL);
}

static int write_int(char const *path, int value)
{
	int fd;
	static int already_warned;

	already_warned = 0;

	ALOGV("write_int: path %s, value %d", path, value);
	fd = open(path, O_RDWR);

	if (fd >= 0) {
		char buffer[20];
		int bytes = sprintf(buffer, "%d\n", value);
		int amt = write(fd, buffer, bytes);
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

static int write_rgb(int value, int on, int off)
{
    int fd;
    static int already_warned;

    int color = value & 0x00ffffff;

    already_warned = 0;


    ALOGV("write_rgb: value %d, on %d, off %d", value, on, off);
    fd = open(LED_BLINK_FILE, O_RDWR);

    if (fd >= 0) {
        char buffer[20];
        int bytes = sprintf(buffer, "0x%x %d %d\n", (on == 0 && off == 0) ? 0 : color, on, off);
        int amt = write(fd, buffer, bytes);
        close(fd);
        return amt == -1 ? -errno : 0;
    } else {
        if (already_warned == 0) {
            ALOGE("write_rgb failed to open %s\n", LED_BLINK_FILE);
            already_warned = 1;
        }
        return -errno;
    }
}

static int is_lit(struct light_state_t const* state)
{
    return state->color & 0x00ffffff;
}

static int rgb_to_brightness(struct light_state_t const *state)
{
	int color = state->color & 0x00ffffff;

	return ((77*((color>>16) & 0x00ff))
		+ (150*((color>>8) & 0x00ff)) + (29*(color & 0x00ff))) >> 8;
}

static int set_light_backlight(struct light_device_t *dev,
			struct light_state_t const *state)
{
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
    int err = 0;
    int on = is_lit(state);

    pthread_mutex_lock(&g_lock);
    err = write_int(BUTTON_FILE, on?255:0);
    pthread_mutex_unlock(&g_lock);

    return err;

}

static int close_lights(struct light_device_t *dev)
{
	ALOGV("close_light is called");
	if (dev)
		free(dev);

	return 0;
}

static int set_light_leds(struct light_state_t const *state)
{
    int rc = 0;

    ALOGV("%s: flashMode=%d, flashOnMS=%d, flashOffMs=%d, color=%d", __func__,
                    state->flashMode, state->flashOnMS, state->flashOffMS, state->color);

    switch (state->flashMode) {
    case LIGHT_FLASH_NONE:
        rc = write_rgb(0, 0, 0);
        break;
    case LIGHT_FLASH_TIMED:
        rc = write_rgb(state->color, state->flashOnMS, state->flashOffMS);
        break;
    case LIGHT_FLASH_HARDWARE:
        ALOGV("hardware flash, do nothing");
        break;
    default:
        rc = -EINVAL;
    }

    if (rc != 0)
        ALOGE("set color failed rc = %d\n", rc);

    return rc;
}

static int
set_light_battery(struct light_device_t* dev,
        struct light_state_t const* state)
{
    int rc = 0;

    ALOGV("%s mode=%d color=0x%08x",
            __func__,state->flashMode, state->color);

    rc = write_int(LED_PATTERN_FILE, state->color);

    if (rc != 0)
        ALOGE("set color failed rc = %d\n", rc);

    return rc;
}

static int set_light_leds_notifications(struct light_device_t *dev,
			struct light_state_t const *state)
{
	return set_light_leds(state);
}

static int set_light_leds_attention(struct light_device_t *dev,
			struct light_state_t const *state)
{
	return set_light_leds(state);
}

static int open_lights(const struct hw_module_t *module, char const *name,
						struct hw_device_t **device)
{
	int (*set_light)(struct light_device_t *dev,
		struct light_state_t const *state);

	if (0 == strcmp(LIGHT_ID_BACKLIGHT, name))
		set_light = set_light_backlight;
    else if (0 == strcmp(LIGHT_ID_BUTTONS, name))
        set_light = set_light_buttons;
	else if (0 == strcmp(LIGHT_ID_NOTIFICATIONS, name))
		set_light = set_light_leds_notifications;
	else if (0 == strcmp(LIGHT_ID_ATTENTION, name))
		set_light = set_light_leds_attention;
#if 0
    else if (0 == strcmp(LIGHT_ID_BATTERY, name))
         set_light = set_light_battery;
#endif
	else
		return -EINVAL;

	pthread_once(&g_init, init_g_lock);

	struct light_device_t *dev = malloc(sizeof(struct light_device_t));
	memset(dev, 0, sizeof(*dev));

	dev->common.tag = HARDWARE_DEVICE_TAG;
	dev->common.version = 0;
	dev->common.module = (struct hw_module_t *)module;
	dev->common.close = (int (*)(struct hw_device_t *))close_lights;
	dev->set_light = set_light;

	*device = (struct hw_device_t *)dev;

	return 0;
}

static struct hw_module_methods_t lights_module_methods = {
	.open =  open_lights,
};

struct hw_module_t HAL_MODULE_INFO_SYM = {
	.tag = HARDWARE_MODULE_TAG,
	.version_major = 1,
	.version_minor = 0,
	.id = LIGHTS_HARDWARE_MODULE_ID,
	.name = "D2 Lights Module",
	.author = "The CyanogenMod Project",
	.methods = &lights_module_methods,
};
