ifeq ($(BOARD_HAVE_BLUETOOTH),true)

LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

LOCAL_SRC_FILES:= \
	btconfig.c

LOCAL_MODULE_PATH := $(TARGET_OUT_OPTIONAL_EXECUTABLES)
LOCAL_MODULE_TAGS := debug
LOCAL_MODULE:=btconfig

LOCAL_SHARED_LIBRARIES += libcutils   \
                          libutils

include $(BUILD_EXECUTABLE)
endif
