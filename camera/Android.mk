# Currently only for apexqtmo and expressatt
ifeq ($(TARGET_PROVIDES_CAMERA_HAL),true)

ifeq ($(filter apexqtmo expressatt,$(VARIENT_MODEL)),)
    LOCAL_CFLAGS += -DENABLE_ZSL
endif

LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

LOCAL_SRC_FILES := \
    CameraWrapper.cpp

LOCAL_SHARED_LIBRARIES := \
    libhardware liblog libcamera_client libutils

LOCAL_MODULE_PATH := $(TARGET_OUT_SHARED_LIBRARIES)/hw
LOCAL_MODULE := camera.MSM8960
LOCAL_MODULE_TAGS := optional

include $(BUILD_SHARED_LIBRARY)

endif
