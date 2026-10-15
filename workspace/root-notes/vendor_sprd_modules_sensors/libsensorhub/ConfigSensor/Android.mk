# Copyright (C) 2008 The Android Open Source Project
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
ifeq ($(USE_SPRD_SENSOR_HUB),true)

include $(CLEAR_VARS)
LOCAL_SHARED_LIBRARIES := liblog libcutils libdl libutils libhardware 
include $(LOCAL_PATH)/SensorCfg.mk
LOCAL_C_INCLUDES += \
		$(LOCAL_PATH)/../include/	\
		$(LOCAL_PATH)/include/
LOCAL_MODULE := libsensorsdrvcfg
LOCAL_MODULE_TAGS := optional
LOCAL_PROPRIETARY_MODULE := true
#LOCAL_CFLAGS := -DLOG_TAG=\"SensorHubDrvConfig\"
include $(BUILD_SHARED_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE := sensors.firmware
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_CLASS := FAKE
LOCAL_MODULE_SUFFIX := -timestamp
include $(BUILD_SYSTEM)/base_rules.mk
$(LOCAL_BUILT_MODULE): $(LOCAL_PATH)/Android.mk
$(LOCAL_BUILT_MODULE):
	$(hide) mkdir -p $(dir $@)
	$(hide) mkdir -p $(dir $(TARGET_OUT_VENDOR)/firmware/)
	$(hide) rm -rf $@
	$(hide) touch $@
endif # USE_SPRD_SENSOR_HUB
