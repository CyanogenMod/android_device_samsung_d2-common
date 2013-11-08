# Copyright (C) 2009 The CyanogenMod Project
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

TARGET_SPECIFIC_HEADER_PATH := device/samsung/d2-common/include

# Kernel
TARGET_KERNEL_SOURCE        := kernel/samsung/d2
BOARD_KERNEL_CMDLINE        := androidboot.hardware=qcom user_debug=31 zcache
BOARD_KERNEL_BASE           := 0x80200000
BOARD_MKBOOTIMG_ARGS := --ramdisk_offset 0x01300000
BOARD_KERNEL_PAGESIZE       := 2048
TARGET_KERNEL_VARIANT_CONFIG := cyanogen_d2_defconfig
TARGET_KERNEL_SELINUX_CONFIG := m2selinux_defconfig

TARGET_BOOTLOADER_BOARD_NAME := MSM8960

# Recovery
BOARD_CUSTOM_RECOVERY_KEYMAPPING := ../../device/samsung/d2-common/recovery/recovery_keys.c
BOARD_USE_CUSTOM_RECOVERY_FONT := \"roboto_15x24.h\"
BOARD_USES_MMCUTILS := true
BOARD_HAS_LARGE_FILESYSTEM := true
BOARD_HAS_NO_MISC_PARTITION := true
BOARD_HAS_NO_SELECT_BUTTON := true
TARGET_RECOVERY_FSTAB := device/samsung/d2-common/rootdir/etc/fstab.qcom

TARGET_USERIMAGES_USE_EXT4 := true
BOARD_BOOTIMAGE_PARTITION_SIZE := 0x00A00000
BOARD_RECOVERYIMAGE_PARTITION_SIZE := 0x00A00000
BOARD_SYSTEMIMAGE_PARTITION_SIZE := 1572864000
BOARD_USERDATAIMAGE_PARTITION_SIZE := 28651290624
BOARD_FLASH_BLOCK_SIZE := 131072

# bluetooth
BOARD_BLUETOOTH_BDROID_BUILDCFG_INCLUDE_DIR := device/samsung/d2-common/bluetooth
BOARD_BLUEDROID_VENDOR_CONF := device/samsung/d2-common/bluetooth/vnd_d2.txt

# Disable initlogo, Samsungs framebuffer is weird
TARGET_NO_INITLOGO := true

# Use Audience A2220 chip
BOARD_HAVE_AUDIENCE_A2220 := true

# Use USB Dock Audio
BOARD_HAVE_DOCK_USBAUDIO := true

#camera abi compatiblily
TARGET_DISPLAY_INSECURE_MM_HEAP := true
COMMON_GLOBAL_CFLAGS += -DQCOM_BSP_CAMERA_ABI_HACK

# Separate audio devices for VOIP
BOARD_USES_SEPERATED_VOIP := true

# SELinux
BOARD_SEPOLICY_DIRS += \
        device/samsung/d2-common/sepolicy

BOARD_SEPOLICY_UNION += \
        file_contexts \
        app.te \
        bluetooth.te \
        device.te \
        domain.te \
        drmserver.te \
        file.te \
        hci_init.te \
        healthd.te \
        init.te \
        init_shell.te \
        keystore.te \
        kickstart.te \
        mediaserver.te \
        nfc.te \
        rild.te \
        surfaceflinger.te \
        system.te \
        ueventd.te \
        wpa.te \
        wpa_socket.te
