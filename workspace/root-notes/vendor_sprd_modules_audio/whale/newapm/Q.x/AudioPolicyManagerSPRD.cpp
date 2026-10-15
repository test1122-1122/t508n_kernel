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

#define LOG_TAG "APM_AudioPolicyManagerSPRD"
#define LOG_NDEBUG 0
#include <utils/Log.h>
#include <hardware/audio.h>
#include "AudioPolicyManagerSPRD.h"
#include <media/mediarecorder.h>
#include <media/AudioParameter.h>
#include <cutils/properties.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <binder/IPCThreadState.h>
#include <system/audio.h>

namespace android {

extern "C" AudioPolicyInterface* createAudioPolicyManager(
        AudioPolicyClientInterface *clientInterface)
{
    return new AudioPolicyManagerSPRD(clientInterface);
}

extern "C" void destroyAudioPolicyManager(AudioPolicyInterface *interface)
{
    delete interface;
}

AudioPolicyManagerSPRD::AudioPolicyManagerSPRD(
        AudioPolicyClientInterface *clientInterface)
    :AudioPolicyManager(clientInterface), isVoipSet(false), isFmMute(false)
{
    ALOGV("%s", __FUNCTION__);
    fm_port = AUDIO_PORT_HANDLE_NONE;
    char systemready[PROPERTY_VALUE_MAX];
    // prop sys.boot_completed will set 1 when system ready (ActivityManagerService.java)...
    property_get("af.media.systemready.state", systemready, "");
    ALOGI("systemready:%s",systemready);
    if (strncmp("true", systemready, 1) != 0) {
        startReadingThread();
    }
    if(hasPrimaryOutput()&&(0==(mPrimaryOutput->supportedDevices().types()&AUDIO_DEVICE_OUT_ALL_USB))){
        mpClientInterface->setParameters(AUDIO_IO_HANDLE_NONE, String8("primaryusb=0"));
    }
}

audio_port_handle_t AudioPolicyManagerSPRD::getFmPort()
{
    audio_port_handle_t portId = AudioPort::getNextUniqueId();
    audio_attributes_t resultAttr=AUDIO_ATTRIBUTES_INITIALIZER;
    audio_config_base_t clientConfig=AUDIO_CONFIG_BASE_INITIALIZER;
    std::vector<wp<SwAudioOutputDescriptor>> weakSecondaryOutputDescs;

    memcpy(resultAttr.tags,"STRATEGY_FM=1",strlen("STRATEGY_FM=1"));

    audio_io_handle_t output=getOutput(AUDIO_STREAM_FM);
    sp<TrackClientDescriptor> clientDesc =
        new TrackClientDescriptor(portId, IPCThreadState::self()->getCallingUid(),AUDIO_SESSION_ALLOCATE, 
        resultAttr, clientConfig,
        false,AUDIO_STREAM_FM,
        mEngine->getProductStrategyForAttributes(resultAttr),
        toVolumeSource(resultAttr),
        AUDIO_OUTPUT_FLAG_NONE, false,
        std::move(weakSecondaryOutputDescs));
    sp<SwAudioOutputDescriptor> outputDesc = mOutputs.valueFor(output);
    outputDesc->addClient(clientDesc);
    return portId;
}

uint32_t AudioPolicyManagerSPRD::getActivityCount(audio_stream_type_t streamin)
{
    for (size_t i = 0; i < mOutputs.size(); i++) {
        const sp<SwAudioOutputDescriptor>& outputDescriptor = mOutputs[i];
        for (const sp<TrackClientDescriptor>& client : outputDescriptor->getClientIterable()) {
            sp<SwAudioOutputDescriptor> desc;
            auto clientVolSrc = client->volumeSource();
            if(client->stream()==streamin){
                int count=0;
                count = outputDescriptor->getActivityCount(clientVolSrc);
                if(count>0){
                    return count;
                }
            }
        }
    }
    return 0;
}

status_t AudioPolicyManagerSPRD::startOutput(audio_port_handle_t portId)
{
    ALOGV("%s portId %d", __FUNCTION__, portId);
    status_t status=0;

    sp<SwAudioOutputDescriptor> outputDesc = mOutputs.getOutputForClient(portId);
    if (outputDesc == 0) {
        ALOGW("startOutput() no output for client %d", portId);
        return BAD_VALUE;
    }
    sp<TrackClientDescriptor> client = outputDesc->getClient(portId);
    audio_stream_type_t stream = client->stream();

    if((!isFmMute) && (stream ==AUDIO_STREAM_ENFORCED_AUDIBLE || stream==AUDIO_STREAM_RING ||
        stream==AUDIO_STREAM_ALARM || stream == AUDIO_STREAM_NOTIFICATION || stream == AUDIO_STREAM_VOICE_CALL)){
        if((getActivityCount(AUDIO_STREAM_ENFORCED_AUDIBLE)==0) || (getActivityCount(AUDIO_STREAM_RING)==0) ||
        (getActivityCount(AUDIO_STREAM_ALARM)==0) || (getActivityCount(AUDIO_STREAM_NOTIFICATION)==0) 
        || (getActivityCount(AUDIO_STREAM_VOICE_CALL) == 0)) {
            AudioParameter param;
            param.addInt(String8("policy_force_fm_mute"), 1);
            mpClientInterface->setParameters(0, param.toString());
            isFmMute = true;
        }
    }

    status=AudioPolicyManager::startOutput(portId);

    if (status != NO_ERROR) {
        AudioParameter param;
        param.addInt(String8("policy_force_fm_mute"), 0);
        mpClientInterface->setParameters(0, param.toString());
        isFmMute = false;
    }else{
        if((!isVoipSet)&&(stream == AUDIO_STREAM_VOICE_CALL)&&(mPrimaryOutput == outputDesc)) {
            ALOGD("startOutput() outputDesc->mRefCount[AUDIO_STREAM_VOICE_CALL] %d",getActivityCount(AUDIO_STREAM_VOICE_CALL));
            AudioParameter param;
            param.add(String8("sprd_voip_start"), String8("true"));
            mpClientInterface->setParameters(0, param.toString());
            isVoipSet = true;
        }
    }
    return status;
}

status_t AudioPolicyManagerSPRD::stopOutput(audio_port_handle_t portId)
{
    ALOGI("%s portId %d", __FUNCTION__, portId);
    status_t status=0;

    sp<SwAudioOutputDescriptor> outputDesc = mOutputs.getOutputForClient(portId);
    if (outputDesc == 0) {
        ALOGW("stopOutput() no output for client %d", portId);
        return BAD_VALUE;
    }
    uint32_t delayMs = outputDesc->latency()*2;
    sp<TrackClientDescriptor> client = outputDesc->getClient(portId);
    audio_stream_type_t stream = client->stream();

    if(isVoipSet &&(stream == AUDIO_STREAM_VOICE_CALL)&&(mPrimaryOutput==outputDesc)) {
        ALOGD("stopOutput() getActivityCount[AUDIO_STREAM_VOICE_CALL] %d",getActivityCount(AUDIO_STREAM_VOICE_CALL));
        if(getActivityCount(AUDIO_STREAM_VOICE_CALL) == 1) {
            AudioParameter param;
            param.add(String8("sprd_voip_start"), String8("false"));
            mpClientInterface->setParameters(0, param.toString());
            isVoipSet = false;
        }
    }

    status=AudioPolicyManager::stopOutput(portId);

    if(isFmMute && (stream ==AUDIO_STREAM_ENFORCED_AUDIBLE || stream==AUDIO_STREAM_RING ||
        stream==AUDIO_STREAM_ALARM || stream == AUDIO_STREAM_NOTIFICATION || stream == AUDIO_STREAM_VOICE_CALL)){
        if((getActivityCount(AUDIO_STREAM_ENFORCED_AUDIBLE)==0) && (getActivityCount(AUDIO_STREAM_RING)==0) &&
        (getActivityCount(AUDIO_STREAM_ALARM)==0) && (getActivityCount(AUDIO_STREAM_NOTIFICATION)==0) 
        && (getActivityCount(AUDIO_STREAM_VOICE_CALL) == 0))
        {
            AudioParameter param;
            param.addInt(String8("policy_force_fm_mute"), 0);
            mpClientInterface->setParameters(0, param.toString(), delayMs*4);
            isFmMute = false;
        }
    }

    if (AudioPolicyManager::isStreamActive(AUDIO_STREAM_FM, 0) && (stream == AUDIO_STREAM_DTMF)) {
        AudioParameter param;
        int index = 0;
        getStreamVolumeIndex(AUDIO_STREAM_MUSIC, &index, getDevicesForStream(AUDIO_STREAM_MUSIC));
        param.addInt(String8("FM_Volume"), index);
        mpClientInterface->setParameters(0, param.toString(), delayMs*3);
    }
    return status;
}

void AudioPolicyManagerSPRD::releaseOutput(audio_port_handle_t portId)
{
    ALOGI("%s portId %d", __FUNCTION__, portId);

    sp<SwAudioOutputDescriptor> outputDesc = mOutputs.getOutputForClient(portId);
    if (outputDesc == 0) {
        // If an output descriptor is closed due to a device routing change,
        // then there are race conditions with releaseOutput from tracks
        // that may be destroyed (with no PlaybackThread) or a PlaybackThread
        // destroyed shortly thereafter.
        //
        // Here we just log a warning, instead of a fatal error.
        ALOGW("releaseOutput() no output for client %d", portId);
        return;
    }
    sp<TrackClientDescriptor> client = outputDesc->getClient(portId);

    uint32_t delayMs = 0;

    if(isVoipSet && (mPrimaryOutput == outputDesc)) {
        if(getActivityCount(AUDIO_STREAM_VOICE_CALL)==0) {
            AudioParameter param;
            param.add(String8("sprd_voip_start"), String8("false"));
            mpClientInterface->setParameters(0, param.toString());
            isVoipSet = false;
        }
    }

    if(isFmMute){
        delayMs = outputDesc->latency()*2;
        if((getActivityCount(AUDIO_STREAM_ENFORCED_AUDIBLE)==0) && (getActivityCount(AUDIO_STREAM_RING)==0) &&
                (getActivityCount(AUDIO_STREAM_ALARM)==0) && (getActivityCount(AUDIO_STREAM_NOTIFICATION)==0) &&
                (getActivityCount(AUDIO_STREAM_VOICE_CALL)==0)) {
            AudioParameter param;
            param.addInt(String8("policy_force_fm_mute"), 0);
            mpClientInterface->setParameters(0, param.toString());
            isFmMute = false;
        }
    }

    AudioPolicyManager::releaseOutput(portId);
}

status_t AudioPolicyManagerSPRD::setDeviceConnectionState(audio_devices_t device,
                                                          audio_policy_dev_state_t state,
                                                          const char *device_address,
                                                          const char *device_name,
                                                          audio_format_t encodedFormat) {

    return setDeviceConnectionState_l(device, state, device_address, device_name, encodedFormat, false);

}

status_t AudioPolicyManagerSPRD::setDeviceConnectionState2(audio_devices_t device,
                                                          audio_policy_dev_state_t state,
                                                          const char *device_address,
                                                          const char *device_name,
                                                          audio_format_t encodedFormat) {
    return setDeviceConnectionState_l(device, state, device_address, device_name, encodedFormat, true);
}

status_t AudioPolicyManagerSPRD::setDeviceConnectionState_l(audio_devices_t device,
                                                          audio_policy_dev_state_t state,
                                                          const char *device_address,
                                                          const char *device_name,
                                                          audio_format_t encodedFormat,
                                                          bool is_local_thread)
{
    int ret;
    bool isFmFirstOpen = false;
    AudioParameter param = AudioParameter();
    if (!audio_is_output_device(device) && !audio_is_input_device(device)) return BAD_VALUE;

#ifdef AUDIO_WHALE
    if ((device & AUDIO_DEVICE_OUT_ALL_USB) ||
        ((device&~AUDIO_DEVICE_BIT_IN) & AUDIO_DEVICE_IN_ALL_USB)){
        if(state&AUDIO_POLICY_DEVICE_STATE_AVAILABLE){
            if(hasPrimaryOutput()&&(0==(mPrimaryOutput->supportedDevices().types()&AUDIO_DEVICE_OUT_ALL_USB))){
                isUsboffloadSupported=0;
                isUsbRecordSupported=0;
            }else{
            AudioParameter usbparam= AudioParameter((String8(device_address)));
            const String8 key(state == AUDIO_POLICY_DEVICE_STATE_AVAILABLE ?
                        AudioParameter::keyStreamConnect : AudioParameter::keyStreamDisconnect);
            usbparam.addInt(String8("usboffloadtest"),1);
            usbparam.addInt(key, device);
            mpClientInterface->setParameters(AUDIO_IO_HANDLE_NONE, usbparam.toString());
            if (device & AUDIO_DEVICE_OUT_ALL_USB) {
                if(state&AUDIO_POLICY_DEVICE_STATE_AVAILABLE){
                    String8 reply;
                    reply = mpClientInterface->getParameters(
                                AUDIO_IO_HANDLE_NONE,
                                String8("isUsboffloadSupported"));
                    AudioParameter repliedParameters(reply);
                    repliedParameters.getInt(
                            String8("UsboffloadSupported"), isUsboffloadSupported);
                    ALOGI("UsboffloadSupported:%d",isUsboffloadSupported);
                }else{
                    isUsboffloadSupported=0;
                }
            }

            if ((device&~AUDIO_DEVICE_BIT_IN) & AUDIO_DEVICE_IN_ALL_USB) {
                if(state&AUDIO_POLICY_DEVICE_STATE_AVAILABLE){
                    String8 reply;
                    reply = mpClientInterface->getParameters(
                                AUDIO_IO_HANDLE_NONE,
                                String8("isUsbRecordSupported"));
                    AudioParameter repliedParameters(reply);
                    repliedParameters.getInt(
                            String8("UsbRecordSupported"), isUsbRecordSupported);
                    ALOGI("UsbRecordSupported:%d",isUsbRecordSupported);
                }else{
                    isUsbRecordSupported=0;
                }
            }
        }
    }
    }

    if (device & AUDIO_DEVICE_OUT_ALL_A2DP){
        if(state&AUDIO_POLICY_DEVICE_STATE_AVAILABLE){
            if (device & AUDIO_DEVICE_OUT_ALL_A2DP) {
                if(state&AUDIO_POLICY_DEVICE_STATE_AVAILABLE){
                    String8 reply;
                    reply = mpClientInterface->getParameters(
                                AUDIO_IO_HANDLE_NONE,
                                String8("isA2dpoffloadSupported"));
                    AudioParameter repliedParameters(reply);
                    repliedParameters.getInt(
                            String8("A2dpoffloadSupported"), isA2dpoffloadSupported);
                    ALOGI("A2dpoffloadSupported:%d",isA2dpoffloadSupported);
                }else{
                    isA2dpoffloadSupported=0;
                }
            }
        }
    }

#endif

    if (audio_is_output_device(device)) {
        //handle fm device connect state
//        if(device == AUDIO_DEVICE_OUT_FM_HEADSET){
        if(device == 0x10000000){
            if(state == AUDIO_POLICY_DEVICE_STATE_AVAILABLE){
                if(!AudioPolicyManager::isStreamActive(AUDIO_STREAM_FM, 0)){
                    fm_port =getFmPort();
                    status_t status=startOutput(fm_port);
                    if (status == NO_ERROR){
                        param.addInt(String8("handleFm"), 1);
                        isFmFirstOpen = true;
                        ALOGI("FM device connected");
                    }else{
                        ALOGW("%s start fm failed:%d",__func__,status);
                    }
                }
            }else {
                if (fm_port != AUDIO_PORT_HANDLE_NONE) {
                    status_t status=stopOutput(fm_port);
                    if (status == NO_ERROR){
                        param.addInt(String8("handleFm"), 0);
                        ALOGI("FM device un connected, setDeviceConnectionState() setParameters handle_fm");
                        mpClientInterface->setParameters(0, param.toString());
                    }else{
                        ALOGW("%s stop fm failed:%d",__func__,status);
                    }
                }else{
                    ALOGW("%s stop fm with invalid port:%d",__func__,fm_port);
                }
            }
        }
    }

    if (mSetDone && device == AUDIO_DEVICE_OUT_USB_HEADSET &&
        state == AUDIO_POLICY_DEVICE_STATE_AVAILABLE) {
        int usbcard = 0;
        int usbdevice = 0;
        String8 deivceaddress(device_address);
        AudioParameter repliedParameters(deivceaddress);

        if (repliedParameters.getInt(String8("card"), usbcard) == NO_ERROR &&
                repliedParameters.getInt(String8("device"), usbdevice) == NO_ERROR &&
                (mUSBCard != usbcard ||
                mUSBDevice != usbdevice ||
                strcmp(mUSBCardName.c_str(), device_name))){
                int max_address_size=200;
                char usb_device_address[max_address_size];

                snprintf(usb_device_address, max_address_size, "card=%d;device=%d;", mUSBCard, mUSBDevice);
                ALOGI("usb device not the same and disconnect the first one,usb_device_address:%s,usb_cardname.c_str():%s,usbdevice:%d, usbcard:%d",
                    usb_device_address, mUSBCardName.c_str(),usbdevice, usbcard);
                setDeviceConnectionState(AUDIO_DEVICE_OUT_USB_HEADSET, AUDIO_POLICY_DEVICE_STATE_UNAVAILABLE,
                    usb_device_address, mUSBCardName.c_str(), AUDIO_FORMAT_DEFAULT);
        }
    }
    ret = AudioPolicyManager::setDeviceConnectionState(
                    device, state, device_address, device_name,encodedFormat);

    if (device & (AUDIO_DEVICE_OUT_WIRED_HEADSET |
        AUDIO_DEVICE_OUT_WIRED_HEADPHONE |
        AUDIO_DEVICE_OUT_USB_HEADSET)) {
        if (mSetDone && ret == INVALID_OPERATION && (!is_local_thread)) {
            ret = NO_ERROR;
            ALOGI("mSetDone is true so ignore INVALID_OPERATION");
        }
        if (!is_local_thread) {
            mSetDone = false;
            mPolicySetDone = true;
        }
    }

    if (isFmFirstOpen) {
        // Change the device to headset/earpiece before handle fm if the device is connected.
        ALOGV("setDeviceConnectionState() setParameters handle_fm");
        mpClientInterface->setParameters(0, param.toString());
    }
    return ret;
}

bool AudioPolicyManagerSPRD::isStreamActive(audio_stream_type_t stream, uint32_t inPastMs) const
{
    bool active = false;
    if (AUDIO_STREAM_MUSIC == stream) {
        if (AudioPolicyManager::isStreamActive(AUDIO_STREAM_FM, inPastMs)) {
            ALOGI("FM is playing so music stream is active");
            active = true;
        } else {
            active = AudioPolicyManager::isStreamActive(stream, inPastMs);
        }
    } else {
        active = AudioPolicyManager::isStreamActive(stream, inPastMs);
    }
    return active;
}

status_t AudioPolicyManagerSPRD::startReadingThread()
{
    ALOGV("startReadingThread");
    mSetDone = false;
    mPolicySetDone = false;
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
    pthread_create(&mThread, &attr, ThreadWrapper, this);
    pthread_attr_destroy(&attr);
    return OK;
}

// static
void *AudioPolicyManagerSPRD::ThreadWrapper(void *me) {
    ALOGV("ThreadWrapper %p", me);
    AudioPolicyManagerSPRD *mBase = static_cast<AudioPolicyManagerSPRD *>(me);
    mBase->threadFunc();
    return NULL;
}


void AudioPolicyManagerSPRD::threadFunc() {
    ALOGI("threadFunc in");
    int ret;
    int preValue = 0;
    String8 reply;
    int value = 0;
    audio_devices_t connectedDevice = AUDIO_DEVICE_NONE;
    // add for bug158794 start
    char systemready[PROPERTY_VALUE_MAX];
    int max_address_size=200;
    char usb_device_address[max_address_size];
    long sleep_time = 0;

    memset(usb_device_address, 0, max_address_size);

    while (!mPolicySetDone && (sleep_time < 20000000)) {
        property_get("af.media.systemready.state", systemready, "");
        if (strncmp("true", systemready, 1) == 0 &&
            !mSetDone){
            ALOGI("threadFunc in systemready is already done");
            break;
        }
        reply = mpClientInterface->getParameters(0, String8("headphonestate"));
        AudioParameter repliedParameters(reply);
        if (repliedParameters.getInt(String8("headphonestate"), value) == NO_ERROR) {
            if (value != preValue) {
                if(preValue && mSetDone && value) {
                    ALOGI("new device found,preValue:%x, value:%x",preValue, value);
                    connectedDevice &= (~(preValue & value));
                    if (connectedDevice & AUDIO_DEVICE_OUT_WIRED_HEADSET) {
                        connectedDevice &= ~AUDIO_DEVICE_OUT_WIRED_HEADSET;
                        setDeviceConnectionState2(AUDIO_DEVICE_OUT_WIRED_HEADSET, AUDIO_POLICY_DEVICE_STATE_UNAVAILABLE, "", "", AUDIO_FORMAT_DEFAULT);
                    }
                    if (connectedDevice & AUDIO_DEVICE_OUT_WIRED_HEADPHONE) {
                        connectedDevice &= ~AUDIO_DEVICE_OUT_WIRED_HEADPHONE;
                        setDeviceConnectionState2(AUDIO_DEVICE_OUT_WIRED_HEADPHONE, AUDIO_POLICY_DEVICE_STATE_UNAVAILABLE, "", "", AUDIO_FORMAT_DEFAULT);
                    }
                    if (connectedDevice & AUDIO_DEVICE_OUT_USB_HEADSET) {
                        connectedDevice &= ~AUDIO_DEVICE_OUT_USB_HEADSET;
                        setDeviceConnectionState2(AUDIO_DEVICE_OUT_USB_HEADSET, AUDIO_POLICY_DEVICE_STATE_UNAVAILABLE, usb_device_address, mUSBCardName.c_str(), AUDIO_FORMAT_DEFAULT);
                    }
                }
                preValue = value;
                if ((value == AUDIO_DEVICE_OUT_WIRED_HEADSET ||
                    value == AUDIO_DEVICE_OUT_WIRED_HEADPHONE) &&
                    (!(connectedDevice & value))){
                    connectedDevice |= value;
                    ret = setDeviceConnectionState2(value, AUDIO_POLICY_DEVICE_STATE_AVAILABLE, "", "", AUDIO_FORMAT_DEFAULT);
                    if(ret == NO_ERROR) {
                        mSetDone = true;
                    }
                } else if ((value == AUDIO_DEVICE_OUT_USB_HEADSET) && (!(connectedDevice & value))) {
                    if (repliedParameters.getInt(String8("card"), mUSBCard) == NO_ERROR &&
                         repliedParameters.getInt(String8("device"), mUSBDevice) == NO_ERROR &&
                         repliedParameters.get(String8("cardname"), mUSBCardName) == NO_ERROR) {
                         connectedDevice |= value;
                        snprintf(usb_device_address, max_address_size, "card=%d;device=%d;", mUSBCard, mUSBDevice);
                        ALOGI("usb headphone connect usb_device_address:%s,usb_cardname.c_str():%s",usb_device_address, mUSBCardName.c_str());
                        ret = setDeviceConnectionState2(value, AUDIO_POLICY_DEVICE_STATE_AVAILABLE, usb_device_address,
                            mUSBCardName.c_str(), AUDIO_FORMAT_DEFAULT);
                        if(ret == NO_ERROR) {
                            mSetDone = true;
                        }
                    } else {
                        preValue = 0;
                    }
                }else if (value == 0) {
                    preValue = value;
                    ALOGI("value is 0,connectedDevice:%x", connectedDevice);
                    if (connectedDevice & AUDIO_DEVICE_OUT_WIRED_HEADSET) {
                        connectedDevice &= ~AUDIO_DEVICE_OUT_WIRED_HEADSET;
                        setDeviceConnectionState2(AUDIO_DEVICE_OUT_WIRED_HEADSET, AUDIO_POLICY_DEVICE_STATE_UNAVAILABLE, "", "", AUDIO_FORMAT_DEFAULT);
                    }
                    if (connectedDevice & AUDIO_DEVICE_OUT_WIRED_HEADPHONE) {
                        connectedDevice &= ~AUDIO_DEVICE_OUT_WIRED_HEADPHONE;
                        setDeviceConnectionState2(AUDIO_DEVICE_OUT_WIRED_HEADPHONE, AUDIO_POLICY_DEVICE_STATE_UNAVAILABLE, "", "", AUDIO_FORMAT_DEFAULT);
                    }
                    if (connectedDevice & AUDIO_DEVICE_OUT_USB_HEADSET) {
                        connectedDevice &= ~AUDIO_DEVICE_OUT_USB_HEADSET;
                        setDeviceConnectionState2(AUDIO_DEVICE_OUT_USB_HEADSET, AUDIO_POLICY_DEVICE_STATE_UNAVAILABLE, usb_device_address, mUSBCardName.c_str(), AUDIO_FORMAT_DEFAULT);
                    }
                    mSetDone = false;
                } else {
                    usleep(100*1000);
                    sleep_time += 100*1000;
                    continue;
                }
                DeviceVector device = getNewOutputDevices(mPrimaryOutput, false);
                checkAndSetVolume(getVolumeCurves(AUDIO_STREAM_SYSTEM), toVolumeSource(AUDIO_STREAM_SYSTEM), getVolumeCurves(AUDIO_STREAM_SYSTEM).getVolumeIndex(device.types()),
                        mPrimaryOutput, device.types(), 0,  false);
                checkAndSetVolume(getVolumeCurves(AUDIO_STREAM_ALARM), toVolumeSource(AUDIO_STREAM_SYSTEM), getVolumeCurves(AUDIO_STREAM_ALARM).getVolumeIndex(device.types()),
                        mPrimaryOutput, device.types(), 0,  false);
            }
        }
        if(mSetDone) {
            usleep(500*1000);
            sleep_time += 500*1000;
        } else {
            usleep(100*1000);
            sleep_time += 100*1000;
        }
    }
    ALOGI("threadFunc exit, sleep_time:%ld, mPolicySetDone:%d, mSetDone: %d", sleep_time, mPolicySetDone, mSetDone);
    // add for bug158794 start
    void *temp = NULL;
    pthread_exit(temp);
    // add for bug 158749 end
    return;
}


}
