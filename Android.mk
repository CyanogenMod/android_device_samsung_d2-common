#
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

LOCAL_PATH := $(call my-dir)

ifneq ($(filter d2att d2bst d2cri d2csp d2mtr d2spr d2tmo d2usc d2vzw,$(TARGET_DEVICE)),)
include $(call all-subdir-makefiles,$(LOCAL_PATH))

FIRMWARE_DSPS_IMAGES := \
    dsps.b00 dsps.b01 dsps.b02 dsps.b03 dsps.b04 dsps.b05 dsps.mdt

FIRMWARE_DSPS_SYMLINKS := $(addprefix $(TARGET_OUT_ETC)/firmware/,$(notdir $(FIRMWARE_DSPS_IMAGES)))
$(FIRMWARE_DSPS_SYMLINKS): $(LOCAL_INSTALLED_MODULE)
	@echo "DSPS Firmware link: $@"
	@mkdir -p $(dir $@)
	@rm -rf $@
	$(hide) ln -sf /firmware-mdm/image/$(notdir $@) $@

ALL_DEFAULT_INSTALLED_MODULES += $(FIRMWARE_DSPS_SYMLINKS)

FIRMWARE_MODEM_IMAGES := \
    modem.b00 modem.b01 modem.b02 modem.b03 modem.b04 modem.b05 modem.b06 modem.b07 modem.b08 \
    modem.b09 modem.b10 modem.mdt

FIRMWARE_MODEM_IMAGES += \
    modem_fw.b00 modem_fw.b01 modem_fw.b02 modem_fw.b03 modem_fw.b04 modem_fw.b05 modem_fw.b06 \
    modem_fw.b07 modem_fw.b08 modem_fw.b09 modem_fw.b10 modem_fw.b11 modem_fw.b12 modem_fw.b13 \
    modem_fw.b14 modem_fw.b15 modem_fw.b16 modem_fw.b17 modem_fw.b18 modem_fw.b21 modem_fw.b22 \
    modem_fw.b23 modem_fw.b24 modem_fw.b25 modem_fw.b26 modem_fw.b27 modem_fw.b28 modem_fw.b29 \
    modem_fw.b31 modem_fw.mdt

FIRMWARE_MODEM_SYMLINKS := $(addprefix $(TARGET_OUT_ETC)/firmware/,$(notdir $(FIRMWARE_MODEM_IMAGES)))
$(FIRMWARE_MODEM_SYMLINKS): $(LOCAL_INSTALLED_MODULE)
	@echo "Modem Firmware link: $@"
	@mkdir -p $(dir $@)
	@rm -rf $@
	$(hide) ln -sf /firmware-mdm/image/$(notdir $@) $@

ALL_DEFAULT_INSTALLED_MODULES += $(FIRMWARE_MODEM_SYMLINKS)

FIRMWARE_Q6_IMAGES := \
    q6.b00 q6.b01 q6.b03 q6.b04 q6.b05 q6.b06 q6.mdt

FIRMWARE_Q6_SYMLINKS := $(addprefix $(TARGET_OUT_ETC)/firmware/,$(notdir $(FIRMWARE_Q6_IMAGES)))
$(FIRMWARE_Q6_SYMLINKS): $(LOCAL_INSTALLED_MODULE)
	@echo "Q6 Firmware link: $@"
	@mkdir -p $(dir $@)
	@rm -rf $@
	$(hide) ln -sf /firmware-mdm/image/$(notdir $@) $@

ALL_DEFAULT_INSTALLED_MODULES += $(FIRMWARE_Q6_SYMLINKS)

FIRMWARE_TZ_IMAGES := \
    tzapps.b00 tzapps.b01 tzapps.b02 tzapps.b03 tzapps.mdt

FIRMWARE_TZ_SYMLINKS := $(addprefix $(TARGET_OUT_ETC)/firmware/,$(notdir $(FIRMWARE_TZ_IMAGES)))
$(FIRMWARE_TZ_SYMLINKS): $(LOCAL_INSTALLED_MODULE)
	@echo "TZ Firmware link: $@"
	@mkdir -p $(dir $@)
	@rm -rf $@
	$(hide) ln -sf /firmware-mdm/image/$(notdir $@) $@

ALL_DEFAULT_INSTALLED_MODULES += $(FIRMWARE_TZ_SYMLINKS)

FIRMWARE_VIDC_IMAGES := \
    vidc.b00 vidc.b01 vidc.b02 vidc.b03 vidc.mdt

FIRMWARE_VIDC_SYMLINKS := $(addprefix $(TARGET_OUT_ETC)/firmware/,$(notdir $(FIRMWARE_VIDC_IMAGES)))
$(FIRMWARE_VIDC_SYMLINKS): $(LOCAL_INSTALLED_MODULE)
	@echo "Vidc Firmware link: $@"
	@mkdir -p $(dir $@)
	@rm -rf $@
	$(hide) ln -sf /firmware-mdm/image/$(notdir $@) $@

ALL_DEFAULT_INSTALLED_MODULES += $(FIRMWARE_VIDC_SYMLINKS)

endif
