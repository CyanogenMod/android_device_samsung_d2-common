/*
 * Copyright (C) 2011 Samsung
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

#include <fcntl.h>
#include <errno.h>
#include <math.h>
#include <poll.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/select.h>
#include <cutils/log.h>
#include <pthread.h>

#include "LightSensor.h"

LightSensor::LightSensor()
    : SamsungSensorBase("light_sensor", ABS_MISC)
{
    mPendingEvent.sensor = ID_L;
    mPendingEvent.type = SENSOR_TYPE_LIGHT;
}

int LightSensor::readEvents(sensors_event_t* data, int count)
{
    if (count < 1)
        return -EINVAL;

    pthread_mutex_lock(&mLock);
    int numEventReceived = 0;

    if (mHasPendingEvent) {
        mHasPendingEvent = false;
        if (mEnabled) {
            mPendingEvent.timestamp = getTimestamp();
            *data = mPendingEvent;
            numEventReceived++;
        }
        goto done;
    }

    input_event const* event;
    while (count && mInputReader.readEvent(data_fd, &event)) {
        int type = event->type;
        if (type == EV_REL) {
            if (event->code == mSensorCode) {
                    mPendingEvent.light = event->value;
            }
        } else if (type == EV_SYN) {
            mPendingEvent.timestamp = timevalToNano(event->time);
            if (mEnabled) {
                *data++ = mPendingEvent;
                count--;
                numEventReceived++;
            }
        }
        mInputReader.next();
    }

done:
    pthread_mutex_unlock(&mLock);
    return numEventReceived;
}
