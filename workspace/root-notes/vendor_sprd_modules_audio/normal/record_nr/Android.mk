LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)

ifeq ($(strip $(TARGET_ARCH)), arm)
LOCAL_MODULE := libSprdRecordNrProcess
LOCAL_MODULE_CLASS := STATIC_LIBRARIES
LOCAL_MULTILIB := 32
LOCAL_MODULE_STEM_32 := libSprdRecordNrProcess.a
LOCAL_SRC_FILES_32 := arm/32/libSprdRecordNrProcess.a
ifneq ($(filter $(strip $(PLATFORM_VERSION)),O 8.0.0 8.1.0 P 9 Q 10),)
LOCAL_PROPRIETARY_MODULE := true
LOCAL_CFLAGS += -DFLAG_VENDOR_ETC
endif
LOCAL_MODULE_TAGS := optional
include $(BUILD_PREBUILT)
endif

ifeq ($(strip $(TARGET_ARCH)), arm64)
LOCAL_MODULE := libSprdRecordNrProcess
LOCAL_MODULE_CLASS := STATIC_LIBRARIES
ifeq ($(strip $(AUDIOSERVER_MULTILIB)),)
LOCAL_MULTILIB := 32
LOCAL_MODULE_STEM_32 := libSprdRecordNrProcess.a
LOCAL_SRC_FILES_32 := arm/32/libSprdRecordNrProcess.a
else
LOCAL_MULTILIB := $(AUDIOSERVER_MULTILIB)
LOCAL_MODULE_STEM_64 := libSprdRecordNrProcess.a
LOCAL_SRC_FILES_64 := arm/64/libSprdRecordNrProcess.a
endif
ifneq ($(filter $(strip $(PLATFORM_VERSION)),O 8.0.0 8.1.0 P 9 Q 10),)
LOCAL_PROPRIETARY_MODULE := true
LOCAL_CFLAGS += -DFLAG_VENDOR_ETC
endif
LOCAL_MODULE_TAGS := optional
include $(BUILD_PREBUILT)
endif

ifeq ($(strip $(TARGET_ARCH)), x86_64)
LOCAL_MODULE := libSprdRecordNrProcess
LOCAL_MODULE_CLASS := STATIC_LIBRARIES
LOCAL_MULTILIB := 32
LOCAL_MODULE_STEM_32 := libSprdRecordNrProcess.a
LOCAL_SRC_FILES_32 := x86/libSprdRecordNrProcess.a
ifneq ($(filter $(strip $(PLATFORM_VERSION)),O 8.0.0 8.1.0 P 9 Q 10),)
LOCAL_PROPRIETARY_MODULE := true
LOCAL_CFLAGS += -DFLAG_VENDOR_ETC
endif
LOCAL_MODULE_TAGS := optional
include $(BUILD_PREBUILT)
endif
