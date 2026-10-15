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
LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

LOCAL_SRC_FILES := \
        ../../whale/tinycompress_util/compress_util.c \
        tinyalsa_util.c \
	offload_stream.c \
 	out_stream.c \
        parse.cpp

LOCAL_C_INCLUDES = \
	external/tinyalsa/include \
	external/expat/lib \
	system/media/audio_utils/include \
        system/media/alsa_utils/include \
        vendor/sprd/modules/audio/whale \
	vendor/sprd/modules/audio/whale/debug \
        vendor/sprd/modules/audio/whale/tinycompress_util \
	external/tinyxml \
	external/tinycompress/include


LOCAL_CFLAGS += -D_POSIX_SOURCE -Wno-multichar -g
LOCAL_MODULE:= libsysoffloadstream

LOCAL_SHARED_LIBRARIES := \
	liblog libcutils libtinyalsa libutils \

ifneq ($(filter $(strip $(PLATFORM_VERSION)),Q 10),)
LOCAL_SHARED_LIBRARIES += libprocessgroup libtinycompress libvendortinyxml
LOCAL_HEADER_LIBRARIES += libhardware_headers libaudio_system_headers
LOCAL_PROPRIETARY_MODULE := true
LOCAL_CFLAGS += -DFLAG_VENDOR_ETC
else
LOCAL_SHARED_LIBRARIES +=  libsystinyxml libsystinycompress
endif

include $(BUILD_SHARED_LIBRARY)
endif
