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

## (2) Also get non-open-source specific aspects if available
$(call inherit-product-if-exists, vendor/samsung/d2-common/d2-common-vendor.mk)

## overlays
DEVICE_PACKAGE_OVERLAYS += device/samsung/d2-common/overlay

# Boot animation and screen size

ifeq ($(filter apexqtmo expressatt,$(VARIENT_MODEL)),)
# Device uses high-density artwork where available
PRODUCT_AAPT_CONFIG := normal hdpi xhdpi
PRODUCT_AAPT_PREF_CONFIG := xhdpi
TARGET_SCREEN_HEIGHT := 1280
TARGET_SCREEN_WIDTH := 720
PRODUCT_PROPERTY_OVERRIDES += ro.sf.lcd_density=320
else
# These poor devices have smaller screens
PRODUCT_AAPT_CONFIG := normal hdpi
PRODUCT_AAPT_PREF_CONFIG := hdpi
TARGET_SCREEN_HEIGHT := 800
TARGET_SCREEN_WIDTH := 480
PRODUCT_PROPERTY_OVERRIDES += ro.sf.lcd_density=240
endif

# Audio configuration
PRODUCT_COPY_FILES += \
        device/samsung/d2-common/audio/snd_soc_msm_2x:system/etc/snd_soc_msm/snd_soc_msm_2x \
        device/samsung/d2-common/audio/audio_policy.conf:system/etc/audio_policy.conf

# Wifi
PRODUCT_COPY_FILES += \
        device/samsung/d2-common/wpa_supplicant_overlay.conf:system/etc/wifi/wpa_supplicant_overlay.conf \
        device/samsung/d2-common/p2p_supplicant_overlay.conf:system/etc/wifi/p2p_supplicant_overlay.conf

# Keymaps
PRODUCT_COPY_FILES += \
       device/samsung/d2-common/keylayout/fsa9485.kl:system/usr/keylayout/fsa9485.kl \
       device/samsung/d2-common/keylayout/msm8960-snd-card_Button_Jack.kl:system/usr/keylayout/msm8960-snd-card_Button_Jack.kl \
       device/samsung/d2-common/keylayout/sec_key.kl:system/usr/keylayout/sec_key.kl \
       device/samsung/d2-common/keylayout/sec_keys.kl:system/usr/keylayout/sec_keys.kl \
       device/samsung/d2-common/keylayout/sec_powerkey.kl:system/usr/keylayout/sec_powerkey.kl \
       device/samsung/d2-common/keylayout/sec_touchkey.kl:system/usr/keylayout/sec_touchkey.kl \
       device/samsung/d2-common/keylayout/sii9234_rcp.kl:system/usr/keylayout/sii9234_rcp.kl

# Media profile
PRODUCT_COPY_FILES += \
       device/samsung/d2-common/media/media_profiles.xml:system/etc/media_profiles.xml

# Ramdisk
PRODUCT_PACKAGES += \
    fstab.qcom \
    initlogo.rle \
    init.bt.rc \
    init.qcom.rc \
    init.qcom.usb.rc \
    init.target.rc \
    ueventd.qcom.rc

# GPS
PRODUCT_PACKAGES += \
    gps.msm8960

PRODUCT_COPY_FILES += \
    device/samsung/d2-common/gps/gps.conf:system/etc/gps.conf

# Torch
PRODUCT_PACKAGES += Torch

# Wifi
PRODUCT_PACKAGES += \
    libnetcmdiface \
    macloader

# Set default USB interface
PRODUCT_DEFAULT_PROPERTY_OVERRIDES += \
    persist.sys.usb.config=mtp

# Lights
PRODUCT_PACKAGES += lights.msm8960

# Increase the HWUI font cache since we have tons of RAM
PRODUCT_PROPERTY_OVERRIDES += \
    ro.hwui.text_cache_width=2048

# QC Perf
PRODUCT_PROPERTY_OVERRIDES += \
    ro.vendor.extension_library=/system/lib/libqc-opt.so

# QRNGD
PRODUCT_PACKAGES += qrngd

#common build.props
PRODUCT_PROPERTY_OVERRIDES += \
    wifi.interface=wlan0 \
    ro.ril.hsxpa=1 \
    ro.ril.gprsclass=10 \
    persist.radio.add_power_save=1 \
    persist.radio.snapshot_disabled=1 \
    com.qc.hardware=true \
    persist.radio.apm_sim_not_pwdn=1 \
    ro.ril.transmitpower=true \
    ro.opengles.version=131072 \
    persist.audio.vr.enable=false \
    persist.audio.speaker.location=high \
    persist.timed.enable=true \
    ro.emmc.sdcard.partition=17 \
    ro.use_data_netmgrd=true \
    persist.data_netmgrd_nint=16 \
    lpa.decode=true \
    rild.libpath=/system/lib/libril-qc-qmi-1.so \
    ril.subscription.types=NV,RUIM \
    ro.ril.svdo=true \
    ro.cdma.subscribe_on_ruim_ready=true \
    persist.radio.no_wait_for_card=0 \
    keyguard.no_require_sim=true \
    media.aac_51_output_enabled=true \
    persist.rild.nitz_plmn="" \
    persist.rild.nitz_long_ons_0="" \
    persist.rild.nitz_long_ons_1="" \
    persist.rild.nitz_long_ons_2="" \
    persist.rild.nitz_long_ons_3="" \
    persist.rild.nitz_short_ons_0="" \
    persist.rild.nitz_short_ons_1="" \
    persist.rild.nitz_short_ons_2="" \
    persist.rild.nitz_short_ons_3=""

ifneq ($(VARIENT_MODEL),apexqtmo)
PRODUCT_PROPERTY_OVERRIDES += \
    persist.audio.fluence.mode=endfire \
    persist.audio.handset.mic=digital \
    ro.qc.sdk.audio.fluencetype=fluence
endif

# enable repeatable keys in cwm
PRODUCT_PROPERTY_OVERRIDES += \
    ro.cwm.enable_key_repeat=true \
    ro.cwm.repeatable_keys=114,115

# NFC Support
PRODUCT_PACKAGES += \
    libnfc \
    libnfc_jni \
    Nfc \
    Tag \
    com.android.nfc_extras

# NFCEE access control
ifeq ($(TARGET_BUILD_VARIANT),user)
    NFCEE_ACCESS_PATH := device/samsung/d2-common/nfc/nfcee_access.xml
else
    NFCEE_ACCESS_PATH := device/samsung/d2-common/nfc/nfcee_access_debug.xml
endif
PRODUCT_COPY_FILES += \
    $(NFCEE_ACCESS_PATH):system/etc/nfcee_access.xml

# common msm8960
$(call inherit-product, device/samsung/msm8960-common/msm8960.mk)

ifeq ($(filter apexqtmo expressatt,$(VARIENT_MODEL)),)
    $(call inherit-product, frameworks/native/build/phone-xhdpi-2048-dalvik-heap.mk)
else
    $(call inherit-product, frameworks/native/build/phone-xhdpi-1024-dalvik-heap.mk)
endif


