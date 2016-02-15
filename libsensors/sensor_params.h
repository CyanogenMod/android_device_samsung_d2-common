/*
 * Copyright (C) 2011 Invensense, Inc.
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

#ifndef INV_SENSOR_PARAMS_H
#define INV_SENSOR_PARAMS_H

/* Physical parameters of the sensors supported by Invensense MPL */
#define SENSORS_ROTATION_VECTOR_HANDLE  (ID_RV)
#define SENSORS_LINEAR_ACCEL_HANDLE     (ID_LA)
#define SENSORS_GRAVITY_HANDLE          (ID_GR)
#define SENSORS_GYROSCOPE_HANDLE        (ID_GY)
#define SENSORS_ACCELERATION_HANDLE     (ID_A)
#define SENSORS_MAGNETIC_FIELD_HANDLE   (ID_M)
#define SENSORS_ORIENTATION_HANDLE      (ID_O)
/******************************************/
//COMPASS_ID_AKM
#define COMPASS_AKM8975_RANGE       (9830.0f)
#define COMPASS_AKM8975_RESOLUTION  (0.285f)
#define COMPASS_AKM8975_POWER       (10.0f)
/*******************************************/
//ACCEL_ID_MPU6050
#define ACCEL_MPU6050_RANGE     (2.0f*GRAVITY_EARTH)
#define ACCEL_MPU6050_RESOLUTION    (0.004f*GRAVITY_EARTH)
#define ACCEL_MPU6050_POWER     (0.0f)
/******************************************/
//GYRO MPU6050
#define RAD_P_DEG (3.14159f/180.0f)
#define GYRO_MPU6050_RANGE      (2000.0f*RAD_P_DEG)
#define GYRO_MPU6050_RESOLUTION     (16.4f*RAD_P_DEG)
#define GYRO_MPU6050_POWER      (5.5f)
/******************************************/
//NINEAXIS
#define NINEAXIS_POWER (COMPASS_AKM8975_POWER + \
                        ACCEL_MPU6050_POWER + \
                        GYRO_MPU6050_POWER)

#define NINEAXIS_ORIENTATION_RANGE      (360.0f)
#define NINEAXIS_ORIENTATION_RESOLUTION (0.00001f)
#define NINEAXIS_ORIENTATION_POWER      (NINEAXIS_POWER)

#define NINEAXIS_ROTATION_VECTOR_RANGE      (1.0f)
#define NINEAXIS_ROTATION_VECTOR_RESOLUTION (0.00001f)
#define NINEAXIS_ROTATION_VECTOR_POWER      (NINEAXIS_POWER)

#define NINEAXIS_GRAVITY_RANGE      (9.81f)
#define NINEAXIS_GRAVITY_RESOLUTION (0.00001f)
#define NINEAXIS_GRAVITY_POWER      (NINEAXIS_POWER)

#define NINEAXIS_LINEAR_ACCEL_RANGE      (ACCEL_MPU6050_RANGE)
#define NINEAXIS_LINEAR_ACCEL_RESOLUTION (ACCEL_MPU6050_RESOLUTION)
#define NINEAXIS_LINEAR_ACCEL_POWER      (NINEAXIS_POWER)

#endif

