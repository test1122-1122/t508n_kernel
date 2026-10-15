/*
* Copyright (C) 2010 The Android Open Source Project
* Copyright (C) 2012-2015, The Linux Foundation. All rights reserved.
*
* Not a Contribution, Apache license notifications and license are retained
* for attribution purposes only.
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

#include <stdint.h>
#include <sys/types.h>
#include <utils/Timers.h>
#include <utils/Errors.h>
#include <utils/KeyedVector.h>
#include <AudioPolicyManager.h>

#ifndef ANDROID_AUDIO_POLICY_MANAGER_SPRD_H
#define ANDROID_AUDIO_POLICY_MANAGER_SPRD_H

namespace android {
class AudioPolicyManagerSPRD: public AudioPolicyManager
{
public:
    explicit AudioPolicyManagerSPRD(AudioPolicyClientInterface *clientInterface);
    virtual ~AudioPolicyManagerSPRD() {}
    virtual status_t startOutput(audio_port_handle_t portId);
    virtual status_t stopOutput(audio_port_handle_t portId);
    virtual void releaseOutput(audio_port_handle_t portId);

    virtual status_t setDeviceConnectionState(audio_devices_t device,
                                              audio_policy_dev_state_t state,
                                              const char *device_address,
                                              const char *device_name,
                                              audio_format_t encodedFormat);
    status_t setDeviceConnectionState2(audio_devices_t device,
                                              audio_policy_dev_state_t state,
                                              const char *device_address,
                                              const char *device_name,
                                              audio_format_t encodedFormat);
    status_t setDeviceConnectionState_l(audio_devices_t device,
                                              audio_policy_dev_state_t state,
                                              const char *device_address,
                                              const char *device_name,
                                              audio_format_t encodedFormat,bool is_local_thread);
    virtual bool isStreamActive(audio_stream_type_t stream, uint32_t inPastMs = 0) const;

private:
    bool isVoipSet;
    bool isFmMute;
    audio_port_handle_t fm_port;
    volatile bool mSetDone;
    bool mPolicySetDone;
    pthread_t mThread;
    int mUSBCard;
    int mUSBDevice;
    String8 mUSBCardName;
    uint32_t getActivityCount(audio_stream_type_t stream);
    audio_port_handle_t getFmPort();
    status_t startReadingThread();
    static void *ThreadWrapper(void *me);
    void threadFunc();
};

} //namespace android

#endif //ANDROID_AUDIO_POLICY_MANAGER_SPRD_H
