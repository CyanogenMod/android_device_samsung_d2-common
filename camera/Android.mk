# Currently only for apexqtmo and expressatt
ifeq ($(TARGET_PROVIDES_CAMERA_HAL),true)

LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

# Add wrappers for other sensors here
ifneq ($(filter apexqtmo expressatt,$(TARGET_DEVICE)),)
LOCAL_SRC_FILES := \
    isx012-db8131m/CameraWrapper.cpp
# Wrapper skeleton? Other devices may benefit from one.
#else
#LOCAL_SRC_FILES := \
#    CameraWrapperSkeleton.cpp
endif

LOCAL_SHARED_LIBRARIES := \
    libhardware liblog libcamera_client libutils

LOCAL_MODULE_PATH := $(TARGET_OUT_SHARED_LIBRARIES)/hw
LOCAL_MODULE := camera.MSM8960
LOCAL_MODULE_TAGS := optional

include $(BUILD_SHARED_LIBRARY)

endif
