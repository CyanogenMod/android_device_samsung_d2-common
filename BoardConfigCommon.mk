# Copyright (C) 2014 The CyanogenMod Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

#
# This file sets variables that control the way modules are built
# thorughout the system. It should not be used to conditionally
# disable makefiles (the proper mechanism to control what gets
# included in a build is to use PRODUCT_PACKAGES in a product
# definition file).
#

# inherit from common msm8960
-include device/samsung/msm8960-common/BoardConfigCommon.mk

TARGET_SPECIFIC_HEADER_PATH += device/samsung/d2-common/include

# Bootloader
TARGET_BOOTLOADER_BOARD_NAME := MSM8960

# Kernel
TARGET_KERNEL_CONFIG        := cyanogen_d2_defconfig

# Audio
BOARD_HAVE_AUDIENCE_A2220 := true
BOARD_HAVE_DOCK_USBAUDIO := true
BOARD_HAVE_NEW_QCOM_CSDCLIENT := true
BOARD_USES_FLUENCE_FOR_VOIP := true
BOARD_USES_FLUENCE_INCALL := true
BOARD_USES_SEPERATED_AUDIO_INPUT := true
BOARD_USES_SEPERATED_VOIP := true

# Bluetooth
BOARD_BLUEDROID_VENDOR_CONF := device/samsung/d2-common/bluetooth/vnd_d2.txt
BOARD_BLUETOOTH_BDROID_BUILDCFG_INCLUDE_DIR := device/samsung/d2-common/bluetooth
BOARD_HAVE_BLUETOOTH_BCM := true

# Camera
TARGET_ADD_ISO_MODE_1600 := true
TARGET_NEED_CAMERA_ZSL := true
TARGET_NEED_EXPOSURE_HACK := true
TARGET_NEED_SAMSUNG_CAMERA_MODE := true

# GPS
TARGET_NO_RPC := true

# Partitions
BOARD_BOOTIMAGE_PARTITION_SIZE := 10485760
BOARD_RECOVERYIMAGE_PARTITION_SIZE := 10485760
BOARD_SYSTEMIMAGE_PARTITION_SIZE := 1572864000
BOARD_USERDATAIMAGE_PARTITION_SIZE := 13140754432
TARGET_RECOVERY_FSTAB := device/samsung/d2-common/rootdir/etc/fstab.qcom

# RIL
BOARD_RIL_CLASS := ../../../device/samsung/d2-common/ril

# Wifi
BOARD_WLAN_DEVICE := bcmdhd
BOARD_HAVE_SAMSUNG_WIFI := true
BOARD_HOSTAPD_DRIVER := NL80211
BOARD_HOSTAPD_PRIVATE_LIB := lib_driver_cmd_${BOARD_WLAN_DEVICE}
BOARD_WPA_SUPPLICANT_DRIVER := NL80211
BOARD_WPA_SUPPLICANT_PRIVATE_LIB := lib_driver_cmd_${BOARD_WLAN_DEVICE}
WPA_SUPPLICANT_VERSION := VER_0_8_X
WIFI_DRIVER_FW_PATH_PARAM   := "/sys/module/dhd/parameters/firmware_path"
WIFI_DRIVER_FW_PATH_STA     := "/system/etc/wifi/bcmdhd_sta.bin"
WIFI_DRIVER_FW_PATH_AP      := "/system/etc/wifi/bcmdhd_apsta.bin"

# Releasetools
TARGET_RELEASETOOLS_EXTENSIONS := device/samsung/d2-common

