LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

LOCAL_MODULE := audio_vbc_eq
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_CLASS := FAKE
LOCAL_MODULE_SUFFIX := -timestamp

include $(BUILD_SYSTEM)/base_rules.mk
ifneq ($(filter $(strip $(PLATFORM_VERSION)),P 9 Q 10),)
$(LOCAL_BUILT_MODULE): DSP_VBC_PARAM_FILE := /data/vendor/local/media/audio_params/dsp_vbc
$(LOCAL_BUILT_MODULE): CVS_PARAM_FILE := /data/vendor/local/media/audio_params/cvs
$(LOCAL_BUILT_MODULE): AUDIO_STRUCTURE_PARAM_FILE := /data/vendor/local/media/audio_params/audio_structure
$(LOCAL_BUILT_MODULE): DSP_SMARTAMP_PARAM_FILE := /data/vendor/local/media/audio_params/dsp_smartamp
else
$(LOCAL_BUILT_MODULE): DSP_VBC_PARAM_FILE := /data/local/media/dsp_vbc
$(LOCAL_BUILT_MODULE): CVS_PARAM_FILE := /data/local/media/nxp
$(LOCAL_BUILT_MODULE): AUDIO_STRUCTURE_PARAM_FILE := /data/local/media/audio_structure
$(LOCAL_BUILT_MODULE): DSP_SMARTAMP_PARAM_FILE := /data/local/media/dsp_smartamp
endif
$(LOCAL_BUILT_MODULE): DSP_VBC_SYMLINK := $(TARGET_OUT_VENDOR)/firmware/dsp_vbc
ifneq ($(filter $(strip $(PLATFORM_VERSION)),P 9 Q 10),)
$(LOCAL_BUILT_MODULE): CVS_SYMLINK := $(TARGET_OUT_VENDOR)/firmware/cvs
else
$(LOCAL_BUILT_MODULE): CVS_SYMLINK := $(TARGET_OUT_VENDOR)/firmware/nxp
endif
$(LOCAL_BUILT_MODULE): AUDIO_STRUCTURE_SYMLINK := $(TARGET_OUT_VENDOR)/firmware/audio_structure
$(LOCAL_BUILT_MODULE): DSP_SMARTAMP_SYMLINK := $(TARGET_OUT_VENDOR)/firmware/dsp_smartamp
ifeq ($(strip $(BOARD_USES_REALTEK_CODEC)),true)
ifneq ($(filter $(strip $(PLATFORM_VERSION)),P 9 Q 10),)
$(LOCAL_BUILT_MODULE): REALTEK_CODEC_EXTEND_PARAM_FILE := /data/vendor/local/media/audio_params/realtek_codec
else
$(LOCAL_BUILT_MODULE): REALTEK_CODEC_EXTEND_PARAM_FILE := /data/local/media/realtek_codec
endif
$(LOCAL_BUILT_MODULE): REALTEK_CODEC_EXTEND_PARAM_SYMLINK := $(TARGET_OUT_VENDOR)/firmware/realtek_codec
endif
$(LOCAL_BUILT_MODULE): $(LOCAL_PATH)/Android.mk
$(LOCAL_BUILT_MODULE):
	$(hide) echo "Symlink: $(DSP_VBC_SYMLINK) -> $(DSP_VBC_PARAM_FILE)"
	$(hide) echo "Symlink: $(CVS_SYMLINK) -> $(CVS_PARAM_FILE)"
	$(hide) echo "Symlink: $(AUDIO_STRUCTURE_SYMLINK) -> $(AUDIO_STRUCTURE_PARAM_FILE)"
	$(hide) echo "Symlink: $(DSP_SMARTAMP_SYMLINK) -> $(DSP_SMARTAMP_PARAM_FILE)"
ifeq ($(strip $(BOARD_USES_REALTEK_CODEC)),true)
	$(hide) echo "Symlink: $(REALTEK_CODEC_EXTEND_PARAM_SYMLINK) -> $(REALTEK_CODEC_EXTEND_PARAM_FILE)"
endif
	$(hide) mkdir -p $(dir $@)
	$(hide) mkdir -p $(dir $(DSP_VBC_SYMLINK))
	$(hide) rm -rf $@
	$(hide) rm -rf $(DSP_VBC_SYMLINK)
	$(hide) rm -rf $(CVS_SYMLINK)
	$(hide) rm -rf $(AUDIO_STRUCTURE_SYMLINK)
ifeq ($(strip $(BOARD_USES_REALTEK_CODEC)),true)
	$(hide) rm -rf $(REALTEK_CODEC_EXTEND_PARAM_SYMLINK)
endif
	$(hide) ln -sf $(DSP_VBC_PARAM_FILE) $(DSP_VBC_SYMLINK)
	$(hide) ln -sf $(CVS_PARAM_FILE) $(CVS_SYMLINK)
	$(hide) ln -sf $(AUDIO_STRUCTURE_PARAM_FILE) $(AUDIO_STRUCTURE_SYMLINK)
	$(hide) ln -sf $(DSP_SMARTAMP_PARAM_FILE) $(DSP_SMARTAMP_SYMLINK)
ifeq ($(strip $(BOARD_USES_REALTEK_CODEC)),true)
	$(hide) ln -sf $(REALTEK_CODEC_EXTEND_PARAM_FILE) $(REALTEK_CODEC_EXTEND_PARAM_SYMLINK)
endif
	$(hide) touch $@


include $(CLEAR_VARS)


LOCAL_SRC_FILES:= \
	audio_param_tool.cpp \
	../audio_xml_utils.cpp \
	
LOCAL_32_BIT_ONLY := true

LOCAL_C_INCLUDES += \
	external/tinyalsa/include \
	external/expat/lib \
	system/media/audio_utils/include \
	system/media/audio_effects/include \
	vendor/sprd/modules/audio/whale/record_process \
	vendor/sprd/modules/audio/whale/debug \
	vendor/sprd/modules/audio/whale/record_nr \
	vendor/sprd/modules/audio/whale \
	external/tinyxml \
	external/tinycompress/include

LOCAL_SHARED_LIBRARIES := \
	 liblog libcutils libtinyalsa libaudioutils \
	 libexpat libdl \
	 libutils \
	 libhardware_legacy

ifneq ($(filter $(strip $(PLATFORM_VERSION)),10 Q),)
LOCAL_SHARED_LIBRARIES += libvendortinyxml
else
LOCAL_SHARED_LIBRARIES += libtinyxml
endif

ifneq ($(filter $(strip $(PLATFORM_VERSION)),P 9 10 Q),)
LOCAL_CFLAGS += -DAUDIOHAL_V4
endif

ifneq ($(filter $(strip $(PLATFORM_VERSION)),O 8.0.0 8.1.0 P 9 10 Q),)
LOCAL_PROPRIETARY_MODULE := true
LOCAL_CFLAGS += -DFLAG_VENDOR_ETC
endif
LOCAL_MODULE =	audio_param_tool

include $(BUILD_EXECUTABLE)
