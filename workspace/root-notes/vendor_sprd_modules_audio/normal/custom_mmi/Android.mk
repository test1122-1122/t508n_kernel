LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)


LOCAL_SRC_FILES   :=  AudioCustom_Mmi.c

ifneq ($(filter $(strip $(PLATFORM_VERSION)),P 9 Q 10),)
LOCAL_CFLAGS += -DAUDIOHAL_V4
LOCAL_C_INCLUDES  := system/media/audio/include
else
LOCAL_C_INCLUDES  := system/media/audio_utils/include
endif

LOCAL_SHARED_LIBRARIES := libcutils liblog libutils

LOCAL_MODULE:= libAudioCustomMmi
LOCAL_MODULE_TAGS := optional

ifneq ($(filter $(strip $(PLATFORM_VERSION)),O 8.0.0 8.1.0 P 9 Q 10),)
LOCAL_CFLAGS += -DFLAG_VENDOR_ETC
LOCAL_PROPRIETARY_MODULE := true
endif

include $(BUILD_SHARED_LIBRARY)

