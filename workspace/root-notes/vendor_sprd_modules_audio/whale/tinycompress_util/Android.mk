LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)
LOCAL_C_INCLUDES:= $(LOCAL_PATH) \
                   external/tinycompress/include
LOCAL_SRC_FILES:= compress_util.c
LOCAL_MODULE := libtinycompress_util
LOCAL_SHARED_LIBRARIES:= libcutils libutils libtinycompress
ifneq ($(filter $(strip $(PLATFORM_VERSION)),O 8.0.0 8.1.0 P 9 Q 10),)
LOCAL_PROPRIETARY_MODULE := true
LOCAL_CFLAGS += -DFLAG_VENDOR_ETC
endif
LOCAL_MODULE_TAGS := optional

include $(BUILD_SHARED_LIBRARY)

