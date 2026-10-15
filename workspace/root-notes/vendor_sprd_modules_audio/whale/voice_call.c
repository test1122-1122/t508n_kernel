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
#define LOG_TAG "audio_hw_voice"

#include "audio_hw.h"
#include "audio_control.h"
/*
 Extern function declaration
*/
extern struct tiny_stream_out * get_output_stream(struct tiny_audio_device *adev,AUDIO_HW_APP_T audio_app_type);

/*
 Function implement
*/
static int do_voice_out_standby(void *dev,void *out,UNUSED_ATTR AUDIO_HW_APP_T audio_app_type){
    struct tiny_audio_device *adev=(struct tiny_audio_device *)dev;
    struct tiny_stream_out *voice_out=(struct tiny_stream_out*)out;
    struct audio_control *actl=adev->dev_ctl;
    LOG_I("do_voice_out_standby:%p, standby:%d",voice_out, voice_out->standby);
    if(voice_out->standby) {
        return -1;
    }
    set_mdg_mute(adev->dev_ctl,UC_CALL,true);

    if(voice_out->pcm_modem_ul) {
        LOG_I("do_voice_out_standby close:%p",voice_out->pcm_modem_ul);
        pcm_close(voice_out->pcm_modem_ul);
        voice_out->pcm_modem_ul = NULL;
    }
    if(voice_out->pcm_modem_dl) {
        LOG_I("do_voice_out_standby close:%p",voice_out->pcm_modem_dl);
        pcm_close(voice_out->pcm_modem_dl);
        voice_out->pcm_modem_dl = NULL;
    }


    voice_out->standby=true;

    set_usecase(actl, UC_CALL, false);
    set_audioparam(adev->dev_ctl,PARAM_USECASE_CHANGE,NULL,false);
    LOG_I("do_voice_out_standby exit");
    return 0;
}

int voice_open(void *dev){
    struct tiny_audio_device *adev=(struct tiny_audio_device *)dev;
    struct tiny_stream_out *voice_out=NULL;

    voice_out=(struct tiny_stream_out *)calloc(1,
                                sizeof(struct tiny_stream_out));
    voice_out->pcm=NULL;
    voice_out->dev=adev;
    voice_out->standby=true;
    voice_out->audio_app_type=AUDIO_HW_APP_CALL;
    voice_out->standby_fun = do_voice_out_standby;

    voice_out->pcm_modem_ul=NULL;
    voice_out->pcm_modem_dl=NULL;

    if (pthread_mutex_init(&(voice_out->lock), NULL) != 0) {
        LOG_E("Failed pthread_mutex_init voice_out->lock,errno:%u,%s",
              errno, strerror(errno));
        goto err;
    }

    audio_add_output(adev,voice_out);

    return 0;

err:

    if(NULL!=voice_out){
        free(voice_out);
    }
    return -1;
}

int start_voice_call(struct audio_control *actl)
{
    struct tiny_audio_device *adev=(struct tiny_audio_device *)actl->adev;
    struct tiny_stream_out *voice_out=NULL;
    int ret=0;

    if(true==is_usecase(adev->dev_ctl,UC_CALL)){
        LOG_W("start_voice_call aready open");
        return 0;
    }
    pthread_mutex_lock(&adev->lock);
    voice_out=get_output_stream(adev,AUDIO_HW_APP_CALL);
    if(NULL==voice_out){
        set_voice_status(&adev->hw_device,VOICE_INVALID_STATUS);
        pthread_mutex_unlock(&adev->lock);
        return -1;
    }

    set_voice_status(&adev->hw_device,VOICE_PRE_START_STATUS);
    pthread_mutex_unlock(&adev->lock);
    force_out_standby(adev, AUDIO_HW_APP_VOIP);
    force_out_standby(adev, AUDIO_HW_APP_FM);
    force_in_standby(adev,AUDIO_HW_APP_VOIP_RECORD);
    force_in_standby(adev,AUDIO_HW_APP_NORMAL_RECORD);
    force_in_standby(adev,AUDIO_HW_APP_FM_RECORD);
    if(true==is_usecase(adev->dev_ctl,UC_BT_RECORD)){
        force_in_standby(adev, AUDIO_HW_APP_BT_RECORD);
    }

    pthread_mutex_lock(&voice_out->lock);

    ret=set_usecase(actl, UC_CALL, true);
    if(ret<0){
        LOG_W("set_usecase failed");
        set_voice_status(&adev->hw_device,VOICE_INVALID_STATUS);
        pthread_mutex_unlock(&voice_out->lock);
        return -1;
    }

    switch_vbc_route(adev->dev_ctl,voice_out->devices);

    if(true==is_usbmic_connected(adev->dev_ctl) &&
        (voice_out->devices & ALL_USB_OUTPUT_DEVICES)){
        start_usb_channel(&adev->usb_ctl,false);
        switch_vbc_route(adev->dev_ctl,AUDIO_DEVICE_IN_USB_HEADSET);
    }else{
        if(adev->dev_ctl->in_devices==0){
            switch_vbc_route(adev->dev_ctl,AUDIO_DEVICE_IN_BUILTIN_MIC);
        }else{
            switch_vbc_route(adev->dev_ctl,adev->dev_ctl->in_devices);
        }
    }

    LOG_I("voice open pcm_open start dl pcm device:%d config rate:%d channels:%d format:%d %d %d %d",actl->pcm_handle.playback_devices[AUD_PCM_MODEM_DL],
        actl->pcm_handle.play[AUD_PCM_MODEM_DL].rate,actl->pcm_handle.play[AUD_PCM_MODEM_DL].channels,
        actl->pcm_handle.play[AUD_PCM_MODEM_DL].format,actl->pcm_handle.play[AUD_PCM_MODEM_DL].period_size,
        actl->pcm_handle.play[AUD_PCM_MODEM_DL].start_threshold,actl->pcm_handle.play[AUD_PCM_MODEM_DL].stop_threshold);

    voice_out->pcm_modem_dl = pcm_open(adev->dev_ctl->cards.s_tinycard, actl->pcm_handle.playback_devices[AUD_PCM_MODEM_DL], PCM_OUT,
                                    &(actl->pcm_handle.play[AUD_PCM_MODEM_DL]));
    if (!pcm_is_ready(voice_out->pcm_modem_dl)) {
        LOG_E("voice:cannot open pcm_modem_dl failed:%s",
              pcm_get_error(voice_out->pcm_modem_dl));
        goto Err;
    }

    LOG_I("voice open pcm_open start ul pcm device:%d config rate:%d channels:%d format:%d %d %d %d",actl->pcm_handle.record_devices[AUD_RECORD_PCM_VOICE_UL],
        actl->pcm_handle.record[AUD_RECORD_PCM_VOICE_UL].rate,actl->pcm_handle.record[AUD_RECORD_PCM_VOICE_UL].channels,
        actl->pcm_handle.record[AUD_RECORD_PCM_VOICE_UL].format,actl->pcm_handle.record[AUD_RECORD_PCM_VOICE_UL].period_size,
        actl->pcm_handle.record[AUD_RECORD_PCM_VOICE_UL].start_threshold,actl->pcm_handle.record[AUD_RECORD_PCM_VOICE_UL].stop_threshold);

    voice_out->pcm_modem_ul = pcm_open(adev->dev_ctl->cards.s_tinycard, actl->pcm_handle.record_devices[AUD_RECORD_PCM_VOICE_UL], PCM_IN,
                                    &(actl->pcm_handle.record[AUD_RECORD_PCM_VOICE_UL]));
    if (!pcm_is_ready(voice_out->pcm_modem_ul)) {
        LOG_E("voice:cannot open pcm_modem_ul failed:%s",
              pcm_get_error(voice_out->pcm_modem_ul));
        goto Err;
    }

    if(voice_out->devices!=actl->out_devices){
        LOG_W("%s devices error, voice_out devices:0x%x 0x%x",
            __func__,voice_out->devices,actl->out_devices);
    }

    if(voice_out->devices!=0){
        select_devices_new(actl,voice_out->audio_app_type,voice_out->devices,false,false,true,true);
    }else{
        if(actl->out_devices==0){
            LOG_W("use default device AUDIO_DEVICE_OUT_EARPIECE");
            select_devices_new(actl,voice_out->audio_app_type,AUDIO_DEVICE_OUT_EARPIECE,false,false,true,true);
        }else{
            select_devices_new(actl,voice_out->audio_app_type,actl->out_devices,false,false,true,true);
        }
    }
    LOG_I("Start pcm_modem_dl start");
    if( 0 != pcm_start(voice_out->pcm_modem_dl)) {
        LOG_E("pcm dl start unsucessfully err:%s",pcm_get_error(voice_out->pcm_modem_dl));
        goto Err;
    }
    LOG_I("Start pcm_modem_ul start");
    if( 0 != pcm_start(voice_out->pcm_modem_ul)) {
        LOG_E("pcm ul start unsucessfully err:%s",pcm_get_error(voice_out->pcm_modem_ul));
        goto Err;
    }
    LOG_I("pcm_start success");
    voice_out->standby=false;
    set_mdg_mute(adev->dev_ctl,UC_CALL,false);
    pthread_mutex_unlock(&voice_out->lock);
    pthread_mutex_lock(&adev->lock);
    set_voice_status(&adev->hw_device,VOICE_START_STATUS);
    set_dsp_volume(adev->dev_ctl,adev->voice_volume);

    if(adev->dsp_mic_mute!=adev->mic_mute){
        ret=set_voice_ul_mute(adev->dev_ctl,adev->mic_mute);
        if(ret==0){
            LOG_I("set_mic_mute:%d success",adev->mic_mute);
            adev->dsp_mic_mute = adev->mic_mute;
        }else{
            LOG_W("set_mic_mute failed:%d :%d",adev->dsp_mic_mute,adev->mic_mute);
        }
    }
    pthread_mutex_unlock(&adev->lock);

    set_audioparam(adev->dev_ctl,PARAM_USECASE_DEVICES_CHANGE,NULL,true);
#ifdef AUDIO_DEBUG
    debug_dump_start(&adev->debugdump,VOICE_REG_DUMP_COUNT);
#endif
    LOG_I("start_voice_call success,out:%p",voice_out);
    return 0;

Err:

    LOG_E("start_voice_call failed");
    if(NULL!=voice_out->pcm_modem_ul){
        pcm_close(voice_out->pcm_modem_ul);
        voice_out->pcm_modem_ul=NULL;
    }
    if(NULL!=voice_out->pcm_modem_dl){
        pcm_close(voice_out->pcm_modem_dl);
        voice_out->pcm_modem_dl=NULL;
    }

    voice_out->standby=true;
    set_voice_status(&adev->hw_device,VOICE_INVALID_STATUS);
    set_usecase(actl, UC_CALL, false);
    set_audioparam(adev->dev_ctl,PARAM_USECASE_CHANGE,NULL,false);
    pthread_mutex_unlock(&voice_out->lock);

    return -1;
}

int stop_voice_call(struct audio_control *ctl)
{
    struct tiny_stream_out * out = NULL;
    struct tiny_audio_device *adev=(struct tiny_audio_device *)ctl->adev;

    out = get_output_stream(adev,AUDIO_HW_APP_CALL);
    if(out) {
        pthread_mutex_lock(&adev->lock);
        set_voice_status(&adev->hw_device,VOICE_PRE_STOP_STATUS);
        pthread_mutex_unlock(&adev->lock);
        force_in_standby(adev,AUDIO_HW_APP_CALL_RECORD);
        do_output_standby(out);
        pthread_mutex_lock(&ctl->lock);
        ctl->param_res.net_mode=AUDIO_NET_UNKNOWN;
        pthread_mutex_unlock(&ctl->lock);
        pthread_mutex_lock(&adev->lock);
        set_voice_status(&adev->hw_device,VOICE_STOP_STATUS);
        pthread_mutex_unlock(&adev->lock);
    }else{
        LOG_W("stop_voice_call failed");
    }

    return 0;
}

bool is_bt_voice(void *dev){
    struct tiny_stream_out * out = NULL;
    struct tiny_audio_device *adev=(struct tiny_audio_device *)dev;
    bool ret=false;

    if(is_call_active_unlock(adev)){
        out = get_output_stream(adev,AUDIO_HW_APP_CALL);
        if(out) {
            pthread_mutex_lock(&out->lock);
            if(out->devices&AUDIO_DEVICE_OUT_ALL_SCO){
                ret=true;
            }
            pthread_mutex_unlock(&out->lock);
        }
    }
    return ret;
}
