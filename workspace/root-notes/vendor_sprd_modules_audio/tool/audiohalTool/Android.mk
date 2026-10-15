LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

LOCAL_SRC_FILES:= main.c

LOCAL_32_BIT_ONLY := true
LOCAL_SHARED_LIBRARIES := \
	libcutils \
	liblog \
	libhardware

LOCAL_PROPRIETARY_MODULE := true
LOCAL_MODULE := audiohal

include $(BUILD_EXECUTABLE)
