# Copyright (C) 2011 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

ifeq ($(strip $(USE_AUDIO_WHALE_HAL)),true)
ifeq ($(strip $(BOARD_USES_TINYALSA_AUDIO)),true)

LOCAL_PATH := $(call my-dir)

#TinyAlsa audio

include $(CLEAR_VARS)

LOCAL_MODULE := audio.primary.$(TARGET_BOARD_PLATFORM)
LOCAL_MODULE_RELATIVE_PATH := hw
LOCAL_32_BIT_ONLY := true
LOCAL_CFLAGS := -D_POSIX_SOURCE -Wno-multichar -g

ifeq ($(TARGET_BUILD_VARIANT),userdebug)
LOCAL_CFLAGS += -DAUDIO_DEBUG
endif

ifeq ($(strip $(USE_AWINIC_EFFECT_HAL)),true)
LOCAL_CFLAGS += -DAWINIC_EFFECT_SUPPORT
endif

ifneq ($(filter $(strip $(PLATFORM_VERSION)),5.0 5.1),)
 LOCAL_CFLAGS += -DANDROID_VERSIO_5_X
endif

ifeq ($(AUDIO_24BITS_OUTPUT), 1)
LOCAL_CFLAGS += -DAUDIO_24BIT_PLAYBACK_SUPPORT
endif

ifeq ($(strip $(USE_AUDIO_HIFI)),audiohifi)
LOCAL_CFLAGS += -DSPRD_AUDIO_HIFI_SUPPORT
endif

LOCAL_CFLAGS += -DLOCAL_SOCKET_SERVER

	LOCAL_SRC_FILES := audio_hw.c audio_monitor.c audio_control.cpp tinyalsa_util.cpp \
	audio_xml_utils.cpp agdsp.c fm.c aaudio.c  alsa_pcm_util.c \
	audio_parse.cpp voice_call.c  audio_offload.c audio_register.c ring_buffer.c \
	audio_param/audio_param.cpp audio_param/dsp_control.c audio_param/param_config.cpp \
	record_process/aud_proc_config.c record_process/aud_filter_calc.c record_process/record_nr_api.c \
	audiotester/audiotester_server.c audiotester/audio_param_handler.cpp audiotester/local_socket.c \
	HuaWeiLaoHua/audio_dev_Laohua_test.c \
	debug/endpoint_test.c debug/dsp_loop.c debug/audio_hw_dev_test.c \
	debug/audio_debug.cpp debug/vbc_pcm_dump.c debug/ext_control.c \
        smartamp/smartamp.c

LOCAL_C_INCLUDES += \
        $(LOCAL_PATH)/preprocessing \
        $(LOCAL_PATH)/smartamp

LOCAL_SHARED_LIBRARIES := \
	liblog libcutils libtinyalsa libaudioutils \
	libexpat libdl \
	libutils \
	libhardware_legacy libtinycompress libatci libtinycompress_util \
	libunisocpreprocessing
LOCAL_STATIC_LIBRARIES := libSprdRecordNrProcess HuaweiApkAudioTest libISCcali

ifneq ($(filter $(strip $(PLATFORM_VERSION)),O 8.0.0 8.1.0 P 9 Q 10),)
LOCAL_PROPRIETARY_MODULE := true
LOCAL_CFLAGS += -DFLAG_VENDOR_ETC
else
LOCAL_SHARED_LIBRARIES += libmedia libutils
LOCAL_SRC_FILES += debug/audio_hw_system_test.cpp
LOCAL_CFLAGS += -DSUPPORT_OLD_AUDIO_TEST_INTERFACE
endif

ifneq ($(filter $(strip $(PLATFORM_VERSION)),P 9 Q 10),)
LOCAL_CFLAGS += -DAUDIOHAL_V4
LOCAL_C_INCLUDES += \
        external/expat/lib
LOCAL_CFLAGS += -DAUDIOHAL_V4
LOCAL_HEADER_LIBRARIES += libhardware_headers libaudio_system_headers
LOCAL_SRC_FILES +=audio_platform.cpp
LOCAL_CFLAGS += -DSPRD_AUDIO_HIDL_CLIENT
LOCAL_CFLAGS += -DSPRD_AUDIO_SMARTAMP
else
LOCAL_C_INCLUDES += \
	system/media/audio_utils/include \
	system/media/audio_effects/include \
	system/media/alsa_utils/include
endif

LOCAL_C_INCLUDES += \
	external/tinyalsa/include \
	external/expat/lib \
	$(LOCAL_PATH)/record_process \
	$(LOCAL_PATH)/debug \
	$(LOCAL_PATH)/HuaWeiLaoHua \
	$(LOCAL_PATH)/record_nr \
	$(LOCAL_PATH)/audio_param \
	$(LOCAL_PATH)/tinycompress_util \
	$(LOCAL_PATH)/preprocessing \
	external/tinyxml \
	external/tinycompress/include \
	vendor/sprd/modules/libatci

LOCAL_SHARED_LIBRARIES += libexpat libalsautils

ifneq ($(filter $(strip $(PLATFORM_VERSION)),Q 10),)
LOCAL_SHARED_LIBRARIES += libprocessgroup libvendortinyxml
LOCAL_REQUIRED_MODULES := audio_vbc_eq libsprdvoiceprocessing
else
LOCAL_SHARED_LIBRARIES += libtinyxml
endif

LOCAL_MODULE_TAGS := optional

include $(BUILD_SHARED_LIBRARY)

ifeq ($(filter $(strip $(PLATFORM_VERSION)),O 8.0.0 8.1.0 P 9 Q 10),)
include $(CLEAR_VARS)

LOCAL_SRC_FILES:= pipe.c audiotester/local_socket.c

LOCAL_C_INCLUDES += debug audiotester

LOCAL_CFLAGS += -DLOCAL_SOCKET_CLIENT

LOCAL_MODULE:= libsprd_audio

LOCAL_SHARED_LIBRARIES := \
	liblog libcutils


ifneq ($(filter $(strip $(PLATFORM_VERSION)),P 9 Q 10),)
LOCAL_CFLAGS += -DAUDIOHAL_V4
LOCAL_HEADER_LIBRARIES += libhardware_headers
endif

ifneq ($(filter $(strip $(PLATFORM_VERSION)),O 8.0.0 8.1.0 P 9 Q 10),)
LOCAL_PROPRIETARY_MODULE := true
LOCAL_CFLAGS += -DFLAG_VENDOR_ETC
endif

include $(BUILD_SHARED_LIBRARY)
endif

include $(call all-makefiles-under,$(LOCAL_PATH))
endif
endif
