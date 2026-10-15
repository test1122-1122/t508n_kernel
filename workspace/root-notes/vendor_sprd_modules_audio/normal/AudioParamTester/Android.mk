# Copyright (C) 2012 The Android Open Source Project
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


LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_CFLAGS := -D_POSIX_SOURCE -Wno-multichar -g

LOCAL_SRC_FILES := audioparam_tester.c

LOCAL_C_INCLUDES += \
    vendor/sprd/proprietories-source/engpc/sprd_fts_inc

LOCAL_SHARED_LIBRARIES := \
    liblog \
    libc \
    libnvexchange \
    libvbeffect \
    libcutils

LOCAL_MODULE := libaudioparamteser

LOCAL_MODULE_TAGS := optional

ifneq ($(filter $(strip $(PLATFORM_VERSION)),O 8.0.0 8.1.0 P 9 Q 10),)
LOCAL_PROPRIETARY_MODULE := true
LOCAL_CFLAGS += -DFLAG_VENDOR_ETC

ifneq ($(filter $(strip $(PLATFORM_VERSION)),P 9 Q 10),)
LOCAL_CFLAGS += -DAUDIOHAL_V4
ifeq ($(strip $(SPRD_AUDIO_HIDL_CLIENT_SUPPORT)), true)
LOCAL_SHARED_LIBRARIES += libsprdaudiohalv5
LOCAL_CFLAGS += -DSPRD_AUDIO_HIDL_CLIENT
endif
endif
endif

AUDIO_PARAMTESTER_FILE := /vendor/lib/libaudioparamteser.so
SYMLINK := $(TARGET_OUT_VENDOR)/lib/npidevice/libaudioparamteser.so
LOCAL_POST_INSTALL_CMD := $(hide) \
    mkdir -p $(TARGET_OUT_VENDOR)/lib/npidevice; \
    rm -rf $(SYMLINK) ;\
    ln -sf $(AUDIO_PARAMTESTER_FILE) $(SYMLINK);
include $(BUILD_SHARED_LIBRARY)

