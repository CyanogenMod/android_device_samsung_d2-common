/*
 * Copyright (C) 2012 The Android Open Source Project
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
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#define LOG_TAG "D2 PowerHAL"
#include <utils/Log.h>

#include <hardware/hardware.h>
#include <hardware/power.h>

#define BOOSTPULSE_PATH "/sys/devices/system/cpu/cpufreq/ondemand/boostpulse"

struct d2_power_module {
    struct power_module base;
    pthread_mutex_t lock;
    int boostpulse_fd;
    int boostpulse_warned;
};

static void sysfs_write(char *path, char *s)
{
    char buf[80];
    int len;
    int fd = open(path, O_WRONLY);

    if (fd < 0) {
        strerror_r(errno, buf, sizeof(buf));
        ALOGE("Error opening %s: %s\n", path, buf);
        return;
    }

    len = write(fd, s, strlen(s));
    if (len < 0) {
        strerror_r(errno, buf, sizeof(buf));
        ALOGE("Error writing to %s: %s\n", path, buf);
    }

    close(fd);
}

static int boostpulse_open(struct d2_power_module *d2)
{
    char buf[80];

    pthread_mutex_lock(&d2->lock);

    if (d2->boostpulse_fd < 0) {
        d2->boostpulse_fd = open(BOOSTPULSE_PATH, O_WRONLY);

        if (d2->boostpulse_fd < 0) {
            if (!d2->boostpulse_warned) {
                strerror_r(errno, buf, sizeof(buf));
                ALOGE("Error opening %s: %s\n", BOOSTPULSE_PATH, buf);
                d2->boostpulse_warned = 1;
            }
        }
    }

    pthread_mutex_unlock(&d2->lock);
    return d2->boostpulse_fd;
}

static void d2_power_hint(struct power_module *module, power_hint_t hint,
                            void *data)
{
    struct d2_power_module *d2 = (struct d2_power_module *) module;
    char buf[80];
    int len;

    switch (hint) {
    case POWER_HINT_INTERACTION:
        if (boostpulse_open(d2) >= 0) {
	    len = write(d2->boostpulse_fd, "1", 1);

	    if (len < 0) {
	        strerror_r(errno, buf, sizeof(buf));
		ALOGE("Error writing to %s: %s\n", BOOSTPULSE_PATH, buf);
	    }
	}
        break;

    case POWER_HINT_VSYNC:
        break;

    default:
        break;
    }
}

static void d2_power_set_interactive(struct power_module *module, int on)
{
    return;
}

static void d2_power_init(struct power_module *module)
{
    return;
}

static struct hw_module_methods_t power_module_methods = {
    .open = NULL,
};

struct d2_power_module HAL_MODULE_INFO_SYM = {
    base: {
        common: {
            tag: HARDWARE_MODULE_TAG,
            module_api_version: POWER_MODULE_API_VERSION_0_2,
            hal_api_version: HARDWARE_HAL_API_VERSION,
            id: POWER_HARDWARE_MODULE_ID,
            name: "D2 Power HAL",
            author: "The Android Open Source Project",
            methods: &power_module_methods,
        },
       init: d2_power_init,
       setInteractive: d2_power_set_interactive,
       powerHint: d2_power_hint,
    },

    lock: PTHREAD_MUTEX_INITIALIZER,
    boostpulse_fd: -1,
    boostpulse_warned: 0,
};
