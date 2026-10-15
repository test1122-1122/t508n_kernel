/*
 * Copyright (C) 2017 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#define LOG_TAG "DevicesFactoryHalHybrid"
//#define LOG_NDEBUG 0

#include <libaudiohal/4.0/DevicesFactoryHalHybrid.h>
#include "DevicesFactoryHalHidl.h"
#include <utils/Log.h>

namespace android {
namespace V4_0 {

DevicesFactoryHalHybrid::DevicesFactoryHalHybrid()
          :mHidlFactory(new DevicesFactoryHalHidl()) {
    ALOGI("DevicesFactoryHalHybrid");
}

DevicesFactoryHalHybrid::~DevicesFactoryHalHybrid() {
    ALOGI("~DevicesFactoryHalHybrid");
}

status_t DevicesFactoryHalHybrid::openDevice(const char *name, sp<DeviceHalInterface> *device) {
    status_t ret;
    if (mHidlFactory != 0 && strcmp(AUDIO_HARDWARE_MODULE_ID_A2DP, name) != 0 &&
        strcmp(AUDIO_HARDWARE_MODULE_ID_HEARING_AID, name) != 0) {
        ret= mHidlFactory->openDevice(name, device);
        ALOGI("openDevice return:%d",ret);
        return ret;
    }
    ALOGI("openDevice failed");
//    return mLocalFactory->openDevice(name, device);
    return -1;
}

} // namespace V4_0
} // namespace android
