ifeq ($(strip $(USE_AUDIO_WHALE_HAL)),true)
LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_SRC_FILES := \
	audio_a2dp_hw.c dsp_monitor.c

LOCAL_C_INCLUDES += \
	$(LOCAL_PATH)/ \
	$(LOCAL_PATH)/offload_stream

LOCAL_C_INCLUDES += \
	external/tinyalsa/include \
	external/expat/lib \
	system/media/audio_utils/include \
	system/media/audio_effects/include \
        system/media/alsa_utils/include \
	external/tinycompress/include 

LOCAL_SHARED_LIBRARIES := libcutils liblog libsysoffloadstream libprocessgroup

ifneq ($(filter $(strip $(PLATFORM_VERSION)),Q 10),)
LOCAL_PROPRIETARY_MODULE := true
LOCAL_CFLAGS += -DFLAG_VENDOR_ETC
LOCAL_HEADER_LIBRARIES += libhardware_headers libaudio_system_headers
LOCAL_MODULE := audio.bluetooth.$(TARGET_BOARD_PLATFORM)
else
LOCAL_MODULE := audio.a2dp.$(TARGET_BOARD_PLATFORM)
endif
LOCAL_MODULE_RELATIVE_PATH := hw

LOCAL_MODULE_TAGS := optional

include $(BUILD_SHARED_LIBRARY)

include $(call all-makefiles-under,$(LOCAL_PATH))
endif
