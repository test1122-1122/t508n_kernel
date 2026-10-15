LOCAL_PATH:= $(call my-dir)

# audio preprocessing wrapper
include $(CLEAR_VARS)

LOCAL_MODULE:= libunisocpreprocessing
LOCAL_MODULE_TAGS := optional
LOCAL_PROPRIETARY_MODULE := true

LOCAL_SRC_FILES:= \
    DspCommandCtl.c \
    DspPreProcessing.c

LOCAL_C_INCLUDES += \
    $(LOCAL_PATH)/../audio_param \
    $(LOCAL_PATH)/../record_process \
    $(LOCAL_PATH)/..

LOCAL_SHARED_LIBRARIES := \
    liblog \
    libcutils

include $(BUILD_SHARED_LIBRARY)


