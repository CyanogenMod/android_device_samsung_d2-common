#
# Copyright (C) 2011 The CyanogenMod Project
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

$(call inherit-product, $(SRC_TARGET_DIR)/product/languages_full.mk)

## The gps config appropriate for this device
PRODUCT_COPY_FILES += device/common/gps/gps.conf_US_SUPL:system/etc/gps.conf

## (2) Also get non-open-source specific aspects if available
$(call inherit-product-if-exists, vendor/samsung/d2-common/d2-common-vendor.mk)

## overlays
DEVICE_PACKAGE_OVERLAYS += device/samsung/d2-common/overlay

# Device uses high-density artwork where available
PRODUCT_AAPT_CONFIG := normal hdpi xhdpi
PRODUCT_AAPT_PREF_CONFIG := xhdpi

# Audio configuration
PRODUCT_COPY_FILES += \
	device/samsung/d2-common/audio/snd_soc_msm_2x:system/etc/snd_soc_msm/snd_soc_msm_2x \
	device/samsung/d2-common/audio/audio_policy.conf:system/etc/audio_policy.conf

# Ramdisk
PRODUCT_COPY_FILES += \
    device/samsung/d2-common/ramdisk/init.qcom.rc:root/init.qcom.rc \
    device/samsung/d2-common/ramdisk/init.qcom.usb.rc:root/init.qcom.usb.rc \
    device/samsung/d2-common/ramdisk/init.qcom.sh:root/init.qcom.sh \
    device/samsung/d2-common/ramdisk/init.qcom.usb.sh:root/init.qcom.usb.sh \
    device/samsung/d2-common/ramdisk/init.target.rc:root/init.target.rc \
    device/samsung/d2-common/fstab.qcom:root/fstab.qcom \
    device/samsung/d2-common/ramdisk/initlogo.rle:root/initlogo.rle \
    device/samsung/d2-common/ramdisk/ueventd.rc:root/ueventd.rc \
	device/samsung/d2-common/ramdisk/init.emmc.rc:root/init.emmc.rc \
	device/samsung/d2-common/ramdisk/init.bt.rc:root/init.bt.rc \
	device/samsung/d2-common/ramdisk/init.sensor.rc:root/init.sensor.rc

# Vold configuration
PRODUCT_COPY_FILES += \
    device/samsung/d2-common/vold.fstab:system/etc/vold.fstab

# Set default USB interface
PRODUCT_DEFAULT_PROPERTY_OVERRIDES += \
    persist.sys.usb.config=mtp

# Lights
PRODUCT_PACKAGES += lights.msm8960 power.msm8960

# Increase the HWUI font cache since we have tons of RAM
PRODUCT_PROPERTY_OVERRIDES += \
    ro.hwui.text_cache_width=2048

# QC Perf
PRODUCT_PROPERTY_OVERRIDES += \
    ro.vendor.extension_library=/system/lib/libqc-opt.so

# QRNGD
PRODUCT_PACKAGES += qrngd

# common msm8960
$(call inherit-product, device/samsung/msm8960-common/msm8960.mk)

$(call inherit-product, frameworks/native/build/phone-xhdpi-1024-dalvik-heap.mk)

