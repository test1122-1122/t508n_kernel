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
#define LOG_TAG "audio_hw_ext"

#include <sys/select.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdlib.h>
#include <pthread.h>
#include <stdint.h>
#ifdef AUDIOHAL_V4
#include <log/log.h>
#else
#include <cutils/log.h>
#endif
#include <cutils/str_parms.h>
#include <cutils/properties.h>
#include "audio_debug.h"
#include <errno.h>
#include <pthread.h>
#include <stdint.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/mman.h>
#include <expat.h>
#include <audio_hw.h>
#include "audio_control.h"

#ifdef SUPPORT_OLD_AUDIO_TEST_INTERFACE
extern int audPlayerOpen( int outdev, int sampleRate, int stereo, int sync );
extern int audPlayerPlay( const unsigned char * data, int size );
extern int audPlayerStop( void );
extern int audPlayerClose( void );
extern int audRcrderOpen( int indev, int sampleRate );
extern int audRcrderRecord( unsigned char * data, int size );
extern int audRcrderStop( void );
extern int audRcrderClose( void );
extern int sprd_audiorecord_test_start(int devices);
extern int sprd_audiorecord_test_read(int size);
extern int sprd_audiorecord_test_stop(void);
extern int sprd_audiotrack_test_start(int channel,int devices, char *buf, int size);
extern int sprd_audiotrack_test_stop(void);
extern int sprd_setStreamVolume(audio_stream_type_t stream,int level);
#endif
extern int audio_out_devices_test(void * adev,struct str_parms *parms,int is_start, UNUSED_ATTR char * val);
extern int audio_in_devices_test(void * adev,struct str_parms *parms,int is_start, UNUSED_ATTR char * val);
extern int sprd_audioloop_test(void * dev,struct str_parms *parms,int is_start, UNUSED_ATTR char * val);
extern int sprd_audio_dev_laohua_test(void * adev,struct str_parms *parms,int is_start, UNUSED_ATTR char * val);
extern int audio_dsp_loop(void * arg,UNUSED_ATTR struct str_parms * params,int opt,UNUSED_ATTR char *val);
extern int agdsp_send_msg_test(void * arg,UNUSED_ATTR struct str_parms * params,int opt,UNUSED_ATTR char *val);
extern int agdsp_check_status_test(void * arg,UNUSED_ATTR struct str_parms * params,int opt,UNUSED_ATTR char *val);
extern int audio_agdsp_reset(void * dev,UNUSED_ATTR struct str_parms *parms,int is_reset, UNUSED_ATTR char * val);
extern int ext_setVoiceMode(void * dev,struct str_parms *parms,int mode,UNUSED_ATTR char * val);
extern int agdsp_log_set(void * arg,UNUSED_ATTR struct str_parms * params,int opt,UNUSED_ATTR char *val);
extern int agdsp_timeout_dump_set(void * arg,UNUSED_ATTR struct str_parms * params,int opt,UNUSED_ATTR char *val);
extern int agdsp_auto_reset(void * arg,UNUSED_ATTR struct str_parms * params,int opt,UNUSED_ATTR char *val);
extern int agdsp_autoreset_property_set(UNUSED_ATTR void * arg,UNUSED_ATTR struct str_parms *parms,int opt,UNUSED_ATTR char * val);
extern int agdsp_force_assert_notify(void * arg,UNUSED_ATTR struct str_parms * params,int opt,UNUSED_ATTR char *val);
extern int agdsp_log_set(void * arg,UNUSED_ATTR struct str_parms * params,int opt,UNUSED_ATTR char *val);
extern int agdsp_pcmdump_set(void * arg,struct str_parms * params,int opt,UNUSED_ATTR char * val);
extern int agdsp_reboot(UNUSED_ATTR void * arg,UNUSED_ATTR struct str_parms * params,int opt,UNUSED_ATTR char *val);
extern int vbc_playback_dump(void *dev,struct str_parms *parms,int opt,UNUSED_ATTR char * val);
extern int audio_config_ctrl(void *dev,UNUSED_ATTR struct str_parms *paramin,UNUSED_ATTR int opt, char * kvpair);
extern int set_audiohal_pcmdump(void *dev,struct str_parms *parms,int opt, char * val);
extern int set_audiohal_musicpcmdump(UNUSED_ATTR void *dev,UNUSED_ATTR struct str_parms *parms,
    int opt, UNUSED_ATTR char * val);
extern int set_audiohal_voippcmdump(UNUSED_ATTR void *dev,UNUSED_ATTR struct str_parms *parms,
    int opt, UNUSED_ATTR char * val);
extern int set_audiohal_recordhalpcmdump(UNUSED_ATTR void *dev,UNUSED_ATTR struct str_parms *parms,
    int opt, UNUSED_ATTR char * val);

extern int set_audiohal_vbcpcmdump(UNUSED_ATTR void *dev,UNUSED_ATTR struct str_parms *parms,
    int opt, UNUSED_ATTR char * val);
extern int set_audiohal_looppcmdump(UNUSED_ATTR void *dev,UNUSED_ATTR struct str_parms *parms,
    int opt, UNUSED_ATTR char * val);
extern void start_audio_tunning_server(struct tiny_audio_device *adev);
extern int audio_endpoint_smartamp_cali(void * dev);
extern bool get_smartapm_cali_status(void *param);
extern int audio_endpoint_test(void * dev,struct str_parms *parms,UNUSED_ATTR int opt,UNUSED_ATTR char * val);

typedef int  (*AUDIO_EXT_CONTROL_FUN)(void *dev,struct str_parms *parms,int opt, char * value);
struct ext_control_t{
    char *cmd_string;
    AUDIO_EXT_CONTROL_FUN fun;
};

int set_wb_mode(void * dev,UNUSED_ATTR struct str_parms *parms,int mode, UNUSED_ATTR char * val){
    struct tiny_audio_device *adev = (struct tiny_audio_device *)dev ;
    struct voice_net_t net_infor;
    net_infor.net_mode=(aud_net_m)adev->dev_ctl->param_res.net_mode;
    net_infor.rate_mode=mode;
    return set_audioparam(adev->dev_ctl,PARAM_NET_CHANGE,&net_infor,false);
}

int set_net_mode(void * dev,UNUSED_ATTR struct str_parms *parms,int mode,UNUSED_ATTR char * val){
    struct tiny_audio_device *adev = (struct tiny_audio_device *)dev ;
    struct voice_net_t net_infor;
    net_infor.net_mode=(aud_net_m)mode;
    net_infor.rate_mode=adev->dev_ctl->param_res.rate_mode;
    return set_audioparam(adev->dev_ctl,PARAM_NET_CHANGE,&net_infor,false);
}

static int audio_loglevel_ctrl(UNUSED_ATTR void *dev,UNUSED_ATTR struct str_parms *parms,int opt, UNUSED_ATTR char * val){
    log_level=opt;
    LOG_I("audio_loglevel_ctrl:%d",log_level);
    return 0;
}

static int audio_codec_mute(void *dev,UNUSED_ATTR struct str_parms *parms,int opt, UNUSED_ATTR char * val){
    struct tiny_audio_device *adev = (struct tiny_audio_device *)dev;
    LOG_I("audio_codec_mute:%d",opt);
    return set_codec_mute(adev->dev_ctl,opt);
}

static int audiotester_enable(void *dev,UNUSED_ATTR struct str_parms *parms,int opt, UNUSED_ATTR char * val){
    struct tiny_audio_device *adev = (struct tiny_audio_device *)dev;
    LOG_I("audio_tester_enable:%d",opt);
    pthread_mutex_lock(&adev->lock);
    if((opt) && (false == adev->audio_param.tunning.running)){
        start_audio_tunning_server(adev);
    }
    pthread_mutex_unlock(&adev->lock);
    return 0;
}

static int set_bt_samplerate(void * dev,UNUSED_ATTR struct str_parms *parms,int rate, UNUSED_ATTR char * val){
    struct tiny_audio_device *adev=(struct tiny_audio_device *)dev;
    LOG_D("set_bt_samplerate:0x%x",rate);
    return set_vbc_bt_src(adev->dev_ctl,rate);
}

static int set_bt_wbs(void * dev,UNUSED_ATTR struct str_parms *parms,UNUSED_ATTR int rate, UNUSED_ATTR char * val){
    int ret=0;
    struct tiny_audio_device *adev=(struct tiny_audio_device *)dev;
    LOG_D("set_bt_wbs:%s",val);
    if (strcmp(val, "on") == 0) {
        ret=set_vbc_bt_src(adev->dev_ctl,16000);
        adev->bt_wbs = true;
    }
    else {
        ret=set_vbc_bt_src(adev->dev_ctl,8000);
        adev->bt_wbs = false;
    }
    return ret;
}

static int set_bt_nrec(void * dev,UNUSED_ATTR struct str_parms *parms,UNUSED_ATTR int opt, UNUSED_ATTR char * val){
    int ret=0;
    struct tiny_audio_device *adev=(struct tiny_audio_device *)dev;
    LOG_D("set_bt_nrec:%s",val);
    if (strcmp(val, "on") == 0){
        ret=set_vbc_bt_nrec(adev->dev_ctl,false);//Bluetooth headset don't support nrec
    }else{
        ret=set_vbc_bt_nrec(adev->dev_ctl,true);//Bluetooth headset support nrec
    }
    return ret;
}


static int set_in_stream_route(void * dev,UNUSED_ATTR struct str_parms *parms,int devices, UNUSED_ATTR char * val){
    int ret=0;
    struct tiny_audio_device *adev=(struct tiny_audio_device *)dev;
    LOG_D("set_in_stream_route:0x%x",devices);
    pthread_mutex_lock(&adev->lock);
    pthread_mutex_lock(&adev->dev_ctl->lock);
    adev->dev_ctl->debug_in_devices=devices;
    pthread_mutex_unlock(&adev->dev_ctl->lock);
    ret=select_devices_new(adev->dev_ctl,AUDIO_HW_APP_INVALID,devices,true,true, true,true);
    pthread_mutex_unlock(&adev->lock);
    return ret;
}

static int set_out_stream_route(void * dev,UNUSED_ATTR struct str_parms *parms,int devices,UNUSED_ATTR char * val){
    int ret=0;
    struct tiny_audio_device *adev=(struct tiny_audio_device *)dev;
    LOG_D("set_out_stream_route:0x%x",devices);
    pthread_mutex_lock(&adev->lock);
    pthread_mutex_lock(&adev->dev_ctl->lock);
    adev->dev_ctl->debug_out_devices=devices;
    pthread_mutex_unlock(&adev->dev_ctl->lock);
    ret=select_devices_new(adev->dev_ctl,AUDIO_HW_APP_INVALID,devices,false,true, true,true);
    pthread_mutex_unlock(&adev->lock);
    return ret;
}

#if 0
static int connect_audio_devices(void * dev,UNUSED_ATTR struct str_parms *parms,int devices, UNUSED_ATTR char * val){
    int ret=-1;
    struct tiny_audio_device *adev=(struct tiny_audio_device *)dev;
    pthread_mutex_lock(&adev->lock);
    set_available_outputdevices(adev->dev_ctl,devices,true);
    if(0==(AUDIO_DEVICE_BIT_IN&devices)){
        ret=open_usboutput_channel(adev);
    }else{
        ret=open_usbinput_channel(adev);
    }
    pthread_mutex_unlock(&adev->lock);
    return ret;
}
#endif

static int disconnect_audio_devices(void * dev,UNUSED_ATTR struct str_parms *parms,int devices,UNUSED_ATTR char * val){
    struct tiny_audio_device *adev=(struct tiny_audio_device *)dev;
    set_available_outputdevices(adev->dev_ctl,devices,false);

    if(AUDIO_DEVICE_OUT_ALL_USB&devices){
        disconnect_usb(&adev->usb_ctl,true);
    }

    if((AUDIO_DEVICE_IN_ALL_USB&(~AUDIO_DEVICE_BIT_IN))&devices){
        disconnect_usb(&adev->usb_ctl,false);
    }
    return 0;
}
#ifdef SUPPORT_OLD_AUDIO_TEST_INTERFACE
static int sprd_audiotrack_test(void * adev,struct str_parms *parms,bool is_start,UNUSED_ATTR char * val){
    int channel=0;
    int devices=0;
    char value[AUDIO_EXT_CONTROL_PIPE_MAX_BUFFER_SIZE];
    int ret = -1;
    int val_int = 0;
    int data_size=0;
    char *data=NULL;
    LOG_D("%s %d",__func__,__LINE__);

    pthread_mutex_lock(&adev->lock);
    if(adev->boot_completed==false){
        adev->boot_completed=true;
    }
    pthread_mutex_lock(&adev->lock);

    if(true==is_start){
        ret = str_parms_get_str(parms,"autotest_outdevices", value, sizeof(value));
        if(ret >= 0){
            LOG_D("%s %d",__func__,__LINE__);
            devices = strtoul(value,NULL,0);
            LOG_D("%s %d %d",__func__,__LINE__,devices);
        }

        ret = str_parms_get_str(parms,"autotest_channels", value, sizeof(value));
        if(ret >= 0){
            channel = strtoul(value,NULL,0);
            LOG_D("%s %d %d",__func__,__LINE__,channel);
        }

        ret = str_parms_get_str(parms,"autotest_datasize", value, sizeof(value));
        if(ret >= 0){
            data_size = strtoul(value,NULL,0);
            LOG_D("%s %d %d",__func__,__LINE__,data_size);

            if(data_size>0){
                data=(char *)malloc(data_size);
                if(NULL == data){
                    LOG_E("sprd_audiotrack_test malloc failed");
                    data_size=0;
                }
            }
        }

        ret = str_parms_get_str(parms,"autotest_data", value, sizeof(value));
        if(ret >= 0){
            if(NULL==data){
                LOG_E("autotest_data NULL ERR");
            }

            int size =string_to_hex(data,value,data_size);
            if(data_size!=size){
                LOG_E("autotest_data ERR:%x %x",size,data_size);
            }
        }

        ret=sprd_audiotrack_test_start(channel,devices,data,data_size);
        if(NULL!=data){
            free(data);
        }
    }else{
        ret=sprd_audiotrack_test_stop();
    }
    return ret;
error:
    return -1;
}

static int sprd_audiorecord_test(void * adev,struct str_parms *parms,int opt,UNUSED_ATTR char * val){
    int channel=0;
    int devices=0;
    char value[128];
    int ret = 0;
    int val_int = 0;
    int data_size=0;
    LOG_D("sprd_audiorecord_test opt:%d",opt);

    pthread_mutex_lock(&adev->lock);
    if(adev->boot_completed==false){
        adev->boot_completed=true;
    }
    pthread_mutex_lock(&adev->lock);

    switch(opt){
        case 1:{
            ret = str_parms_get_str(parms,"autotest_indevices", value, sizeof(value));
            if(ret >= 0){
                LOG_D("%s %d",__func__,__LINE__);
                devices = strtoul(value,NULL,0);
                LOG_D("%s %d %d",__func__,__LINE__,devices);
            }
            ret=sprd_audiorecord_test_start(devices);
        }
            break;
        case 2:
            ret = str_parms_get_str(parms,"autotest_datasize", value, sizeof(value));
            if(ret >= 0){
                LOG_D("%s %d",__func__,__LINE__);
                data_size = strtoul(value,NULL,0);
                LOG_D("%s %d %d",__func__,__LINE__,devices);
            }
            ret=sprd_audiorecord_test_read(data_size);
            break;
        case 3:
            ret=sprd_audiorecord_test_stop();
            break;
        default:
            break;
    }
    return ret;
error:
    return -1;
}
#endif

static int ext_handleFm(void * dev,UNUSED_ATTR struct str_parms *parms,int opt,UNUSED_ATTR char * val){
    int ret=0;
    struct tiny_audio_device *adev=(struct tiny_audio_device *)dev;
    LOG_I("ext_handleFm:0x%x",opt);
    if (opt) {
        ret=start_fm(adev->dev_ctl);
        pthread_mutex_lock(&adev->lock);
        adev->fm_start = true;
        pthread_mutex_unlock(&adev->lock);
    } else {
        bool fm_record=false;
        pthread_mutex_lock(&adev->lock);
        fm_record=adev->fm_record;
        adev->fm_record=false;
        adev->fm_start = false;
        pthread_mutex_unlock(&adev->lock);
        if(true==fm_record){
            force_in_standby(adev,AUDIO_HW_APP_FM_RECORD);
        }
        ret=stop_fm(adev->dev_ctl);
    }
    return ret;
}

static int ext_FmWithDSP(void * dev,UNUSED_ATTR struct str_parms *parms,int opt,UNUSED_ATTR char * val){
    struct tiny_audio_device *adev=(struct tiny_audio_device *)dev;
    LOG_I("ext_FmWithDSP in:0x%x",opt);
    pthread_mutex_lock(&adev->lock);
    if (opt) {
        adev->fm_bydsp = true;
    } else {
        adev->fm_bydsp = false;
    }
    pthread_mutex_unlock(&adev->lock);
    LOG_I("ext_FmWithDSP out:adev->fm_bydsp:0x%x",adev->fm_bydsp);
    return 0;
}


static int ext_set_fm_volume(void * dev,UNUSED_ATTR struct str_parms *parms,int volume,UNUSED_ATTR char * val){
    int ret=-1;
    struct tiny_audio_device *adev=(struct tiny_audio_device *)dev;
    LOG_D("ext_set_fm_volume:0x%x",volume);
    if(NULL!=adev->dev_ctl){
        ret=set_fm_volume(adev->dev_ctl,volume);
    }
    return ret;
}

static int ext_handle_fmMute(void * dev,UNUSED_ATTR struct UNUSED_ATTR str_parms *parms,int opt, UNUSED_ATTR char * val){
    int ret = 0;
    struct tiny_audio_device *adev=(struct tiny_audio_device *)dev;
    LOG_I("ext_handle_fmMute, mute:%d", opt);
    if(opt){
        ret = set_fm_mute(adev->dev_ctl, true);
    } else {
        ret = set_fm_mute(adev->dev_ctl, false);
    }

    return ret;
}

static int ext_handle_fmrecord(void * dev,UNUSED_ATTR struct str_parms *parms,int opt,UNUSED_ATTR char * val){
    struct tiny_audio_device *adev=(struct tiny_audio_device *)dev;
    LOG_D("ext_handle_fmrecord:0x%s usecase:0x%x,opt:%d",val,adev->dev_ctl->usecase,opt);
    bool fm_record_start=false;

    if(opt==0){
        force_in_standby(adev,AUDIO_HW_APP_FM_RECORD);
        pthread_mutex_lock(&adev->lock);
        adev->fm_record = false;
        pthread_mutex_unlock(&adev->lock);
    } else {
        pthread_mutex_lock(&adev->lock);
        fm_record_start=adev->fm_record;
        adev->fm_record=true;
        pthread_mutex_unlock(&adev->lock);
        if(true==fm_record_start){
            force_in_standby(adev,AUDIO_HW_APP_NORMAL_RECORD);
            force_in_standby(adev,AUDIO_HW_APP_FM_RECORD);
        }
    }
    return 0;
}

static int set_screen_state(void * dev,UNUSED_ATTR struct str_parms *parms,int state, UNUSED_ATTR char * val){
    struct tiny_audio_device *adev=(struct tiny_audio_device *)dev;
    LOG_D("ext_screen_state:0x%x",state);
    pthread_mutex_lock(&adev->lock);
    if (strcmp(val, AUDIO_PARAMETER_VALUE_ON) == 0) {
        adev->low_power = false;
    } else {
        adev->low_power = true;
    }
    pthread_mutex_unlock(&adev->lock);
    return 0;
}

static int audio_voip_enable(void * dev,UNUSED_ATTR struct str_parms *parms,UNUSED_ATTR int opt, UNUSED_ATTR char * val){
    struct tiny_audio_device *adev=(struct tiny_audio_device *)dev;
    LOG_D("audio_voip_enable:0x%s",val);

    pthread_mutex_lock(&adev->lock);

    if(strcmp(val, "false") == 0){
        LOG_I("audio_voip_enable voip_stop")
        adev->voip_start = false;
        //wait stop voip and than resume a2dp playback
        force_out_standby(adev, AUDIO_HW_APP_VOIP);
    }

    if(strcmp(val, "true") == 0){
        LOG_I("audio_voip_enable voip_start")
        adev->voip_start=true;
    }

    pthread_mutex_unlock(&adev->lock);
    return 0;
}

/*
int set_net_mode(void * dev,struct str_parms *parms,int mode, char * val){
    int net_mode=0;
    int stream_status=0;
    bool mode_change=false;
    struct tiny_audio_device *adev = (struct tiny_audio_device *)dev ;
    struct dsp_control_t * dsp_ctl=adev->dev_ctl->agdsp_ctl;

    pthread_mutex_lock(&adev->dev_ctl->lock);
    net_mode = adev->dev_ctl->param_res.net_mode;
    if(net_mode!=mode){
        mode_change=true;
        adev->dev_ctl->param_res.net_mode = mode;
    }
    pthread_mutex_unlock(&adev->dev_ctl->lock);

    if(true==mode_change){
        pthread_mutex_lock(&adev->lock);
        stream_status =adev->stream_status;
        pthread_mutex_unlock(&adev->lock);

        if(stream_status & (1<<AUDIO_HW_APP_CALL)){
            send_cmd_to_dsp_thread(dsp_ctl,RIL_NET_MODE_CHANGE,NULL);
        }else{
            LOG_D("set_net_mode:not calling,stream_status:0x%x",stream_status);
        }
    }

    return 0;
}
*/

static int ext_process_exit(void * dev,UNUSED_ATTR struct str_parms *parms,int opt,UNUSED_ATTR char * val){
    struct tiny_audio_device *adev=(struct tiny_audio_device *)dev;
    if (opt) {
        pthread_mutex_lock(&adev->lock);
        adev->pipe_process_exit=true;
        pthread_mutex_unlock(&adev->lock);
    } else {
        pthread_mutex_lock(&adev->lock);
        adev->pipe_process_exit=false;
        pthread_mutex_unlock(&adev->lock);
    }
    return 0;
}

static int ext_audiocalibration(void * dev,UNUSED_ATTR struct str_parms *parms,int opt,UNUSED_ATTR char * val){
    int ret=-1;
    if(opt){
        ret=audio_endpoint_smartamp_cali(dev);
    }
    return ret;
}

static int usb_audio_ctrl(void * dev,UNUSED_ATTR struct str_parms *parms,int opt,UNUSED_ATTR char * val){
    struct tiny_audio_device *adev=(struct tiny_audio_device *)dev;
    LOG_I("usb_audio_ctrl in:0x%x",opt);
    pthread_mutex_lock(&adev->lock);
    if (opt) {
        start_usb_channel(&adev->usb_ctl,true);
    } else {
        stop_usb_channel(&adev->usb_ctl,true);
    }
    pthread_mutex_unlock(&adev->lock);
    LOG_I("usb_audio_ctrl out:adev->usb_ctl.output_status:0x%x",adev->usb_ctl.output_status);
    return 0;
}

static int fm_set_speaker(void * dev,UNUSED_ATTR struct str_parms *parms,int opt,UNUSED_ATTR char * val){
    struct tiny_audio_device *adev=(struct tiny_audio_device *)dev;
    struct audio_control *control = (struct audio_control *)adev->dev_ctl;
    LOG_I("set_fm_speaker in:0x%x",opt);
    pthread_mutex_lock(&adev->lock);
    set_fm_speaker(control,opt);
    pthread_mutex_unlock(&adev->lock);
    return 0;
}

static int fm_set_prestop(void * dev,UNUSED_ATTR struct str_parms *parms,int opt,UNUSED_ATTR char * val){
    struct tiny_audio_device *adev=(struct tiny_audio_device *)dev;
    struct audio_control *control = (struct audio_control *)adev->dev_ctl;
    LOG_I("set_fm_prestop in:0x%x",opt);
    pthread_mutex_lock(&adev->lock);
    set_fm_prestop(control,opt);
    pthread_mutex_unlock(&adev->lock);
    return 0;
}

static int fm_policy_mute(void * dev,UNUSED_ATTR struct str_parms *parms,int opt,UNUSED_ATTR char * val){
    struct tiny_audio_device *adev=(struct tiny_audio_device *)dev;
    struct audio_control *control = (struct audio_control *)adev->dev_ctl;
    LOG_I("fm_policy_mute:0x%x",opt);
    set_fm_policy_mute(control,opt);
    return 0;
}

static int bt_sco_state(void * dev,UNUSED_ATTR struct str_parms *parms,int state, UNUSED_ATTR char * val){
    struct tiny_audio_device *adev=(struct tiny_audio_device *)dev;
    LOG_I("bt_sco_state:0x%x",state);
    pthread_mutex_lock(&adev->lock);
    if (strcmp(val, "on") == 0) {
        adev->bt_sco_status = true;
        LOG_I("bt_sco_state1");
    } else if (strcmp(val, "off") == 0) {
        adev->bt_sco_status = false;
        output_sync(adev,AUDIO_HW_APP_PRIMARY);
        output_sync(adev,AUDIO_HW_APP_VOIP);
        LOG_I("bt_sco_state2");
    }
    pthread_mutex_unlock(&adev->lock);
    return 0;
}

static int sprd_aec_enable(void * dev,UNUSED_ATTR struct str_parms *parms,int opt, UNUSED_ATTR char * val){
    struct tiny_audio_device *adev=(struct tiny_audio_device *)dev;
    LOG_I("sprd_aec_enable:0x%x",opt);
    pthread_mutex_lock(&adev->lock);
    if (opt == 1) {
        adev->sprd_aec_on= true;
    } else {
        adev->sprd_aec_on = false;
    }
    LOG_I("sprd_aec_on:%d", adev->sprd_aec_on);
    pthread_mutex_unlock(&adev->lock);
    return 0;
}

static int sprd_aec_effect_valid(void * dev,UNUSED_ATTR struct str_parms *parms,int opt, UNUSED_ATTR char * val){
    struct tiny_audio_device *adev=(struct tiny_audio_device *)dev;
    LOG_I("sprd_aec_effect_valid in:0x%x",opt);
    pthread_mutex_lock(&adev->lock);
    if (opt == 1) {
        adev->sprd_aec_effect_valid++;
    } else {
        if (adev->sprd_aec_effect_valid) {
            adev->sprd_aec_effect_valid--;
        }
    }
    LOG_I("sprd_aec_effect_valid:%d", adev->sprd_aec_effect_valid);
    pthread_mutex_unlock(&adev->lock);
    return 0;
}

static int set_usbaudio_support(void * dev,UNUSED_ATTR struct str_parms *parms,int state, UNUSED_ATTR char * val){
    struct tiny_audio_device *adev=(struct tiny_audio_device *)dev;
    pthread_mutex_lock(&adev->lock);
    if (state) {
        adev->issupport_usb = true;
    } else{
        adev->issupport_usb = false;
    }
    pthread_mutex_unlock(&adev->lock);
    return 0;
}

static int ext_agdsp_assert(void * dev,UNUSED_ATTR struct str_parms *parms,int opt,UNUSED_ATTR char * val){
    struct tiny_audio_device *adev=(struct tiny_audio_device *)dev;
    struct audio_control *control = (struct audio_control *)&adev->dev_ctl;
    LOG_I("ext_agdsp_assert in:0x%x",opt);
    agdsp_send_msg_test( dev, parms,0x24,val);
    return 0;
}

const struct ext_control_t  ext_contrl_table[] = {
    {"loglevel",                        audio_loglevel_ctrl},
    {"screen_state",                    set_screen_state},
    {"sprd_voip_start",                 audio_voip_enable},
    {"test_in_stream_route",            set_in_stream_route},
    {"test_out_stream_route",           set_out_stream_route},
    {"routing",                         set_out_stream_route},
    {"test_stream_route",               set_out_stream_route},
    {"disconnect",                      disconnect_audio_devices},
#ifdef SUPPORT_OLD_AUDIO_TEST_INTERFACE
    {"autotest_audiotracktest",         sprd_audiotrack_test},
    {"autotest_audiorecordtest",        sprd_audiorecord_test},
#endif
    {"autotest_audiolooptest",          sprd_audioloop_test},
    {"huaweilaohua_audio_loop_test",    sprd_audio_dev_laohua_test},
    {"autotest_audio_input_test",       audio_in_devices_test},
    {"out_devices_test",                audio_out_devices_test},
    {"bt_samplerate",                   set_bt_samplerate},
    {"bt_headset_nrec",                 set_bt_nrec},
    {"bt_wbs",                          set_bt_wbs},
    {"handleFm",                        ext_handleFm},
    {"fm_record",                       ext_handle_fmrecord},
    {"FM_Volume",                       ext_set_fm_volume},
    {"FM_mute",                         ext_handle_fmMute},
    {"dsp_loop",                        audio_dsp_loop},
    {"agdsp_reset",                     audio_agdsp_reset},
    {"agdsp_reset_property_set",        agdsp_autoreset_property_set},
    {"agdsp_msg",                       agdsp_send_msg_test},
    {"agdsp_check",                     agdsp_check_status_test},
    {"agdsp_autoreset",                 agdsp_auto_reset},
    {"agdsp_logset",                    agdsp_log_set},
    {"agdsp_timeoutdump",               agdsp_timeout_dump_set},
    {"agdsp_dumpset",                   agdsp_pcmdump_set},
    {"agdsp_forcenotify",               agdsp_force_assert_notify},
    {"agdsp_reboot",                    agdsp_reboot},
    {"agdsp_assert",                    ext_agdsp_assert},
//  {"ril_net",                         set_net_mode},
    {"ril_net_test",                    set_net_mode},
    {"set_mode",                        ext_setVoiceMode},
    {"audio_config",                    audio_config_ctrl},
    {"audio_wb",                        set_wb_mode},
    {"audio_net",                       set_net_mode},
    {"codec_mute",                      audio_codec_mute},
    {"AudioTester_enable",              audiotester_enable},
    {"vbc_pcm_dump",                    vbc_playback_dump},
    {"set_dump_data_switch",            set_audiohal_pcmdump},
    {"dumpmusic",                       set_audiohal_musicpcmdump},
    {"dumpsco",                         set_audiohal_voippcmdump},
    {"dumpinread",                      set_audiohal_recordhalpcmdump},
    {"dumpvbc",                         set_audiohal_vbcpcmdump},
    {"dumploop",                        set_audiohal_looppcmdump},
    {"pipeprocess_exit",                ext_process_exit},
    {"SmartAmpCalibration",             ext_audiocalibration},
    {"endpoint_test",                   audio_endpoint_test},
    {"FM_WITH_DSP",                     ext_FmWithDSP},
    {"usb_audio_test",                  usb_audio_ctrl},
    {"set_fm_speaker",                  fm_set_speaker},
    {"AudioFmPreStop",                  fm_set_prestop},
    {"policy_force_fm_mute",            fm_policy_mute},
    {"BT_SCO",                          bt_sco_state},
    {"sprd_aec_on",                     sprd_aec_enable},
    {"sprd_aec_effect_valid",           sprd_aec_effect_valid},
    {"primaryusb",                      set_usbaudio_support},
};

int ext_contrtol_process(struct tiny_audio_device *adev,const char *cmd_string){
    int i=0;
    int ret=0;
    char value[AUDIO_EXT_CONTROL_PIPE_MAX_BUFFER_SIZE]={0};
    struct str_parms *parms=NULL;
    AUDIO_EXT_CONTROL_FUN fun=NULL;
    int size=sizeof(ext_contrl_table)/sizeof(struct ext_control_t);
    int val_int=-1;
    int ret_val=0;
    LOG_I("ext_contrtol_process:%s",cmd_string);
    parms = str_parms_create_str(cmd_string);
    for(i=0;i<size;i++){
        ret = str_parms_get_str(parms,ext_contrl_table[i].cmd_string,value,sizeof(value));
        if(ret>0){
            val_int =0;
            val_int = strtoul((const char *)value,NULL,0);
            fun=ext_contrl_table[i].fun;
            if(NULL!=fun){
                ret_val|=fun(adev,parms,val_int,value);
            }
            memset(value,0x00,sizeof(value));
        }
    }
    str_parms_destroy(parms);
    return ret_val;
}

static int read_noblock_l(int fd,int8_t *buf,int bytes){
    int ret = 0;
    ret = read(fd,buf,bytes);
    return ret;
}

static void empty_command_pipe(int fd){
    char buff[16];
    int ret;
    do {
        ret = read(fd, &buff, sizeof(buff));
    } while (ret > 0 || (ret < 0 && errno == EINTR));
}

static void *control_audio_loop_process(void *arg){
    int pipe_fd,max_fd;
    fd_set fds_read;
    int result;
    void* data;
    struct tiny_audio_device *adev = (struct tiny_audio_device *)arg;

    if(access(AUDIO_EXT_CONTROL_PIPE, R_OK) ==0){
        pipe_fd = open(AUDIO_EXT_CONTROL_PIPE, O_RDWR);
    }else{
        pipe_fd = open(AUDIO_EXT_DATA_CONTROL_PIPE, O_RDWR);
    }

    if(pipe_fd < 0){
        LOG_E("%s, open pipe error!! ",__func__);
        adev->pipe_process_exit=true;
        return NULL;
    }

    if(pipe_fd >= MAX_SELECT_FD){
        LOG_E("ORTIFY: FD_SET: file descriptor %d >= FD_SETSIZE %d",pipe_fd,MAX_SELECT_FD);
        close(pipe_fd);
        adev->pipe_process_exit=true;
        return NULL;
    }

    LOG_I("control_audio_loop_process:%d",pipe_fd);

    max_fd = pipe_fd + 1;
    if((fcntl(pipe_fd,F_SETFL,O_NONBLOCK)) <0){
        LOG_E("set flag RROR --------");
    }
    data = (char*)malloc(AUDIO_EXT_CONTROL_PIPE_MAX_BUFFER_SIZE);
    if(data == NULL){
        LOG_E("malloc data err");
        close(pipe_fd);
        LOG_I("control_audio_loop_process:close:%d",pipe_fd);
        adev->pipe_process_exit=true;
        return NULL;
    }
    LOG_I("begin to receive audio control message");
    while(false==adev->pipe_process_exit){
        FD_ZERO(&fds_read);
        FD_SET(pipe_fd,&fds_read);
        result = select(max_fd,&fds_read,NULL,NULL,NULL);
        if(result < 0){
            LOG_E("select error ");
            continue;
        }
        if(FD_ISSET(pipe_fd,&fds_read) <= 0 ){
            LOG_E("SELECT OK BUT NO fd is set");
            continue;
        }
        memset(data,0,AUDIO_EXT_CONTROL_PIPE_MAX_BUFFER_SIZE);
        if(read_noblock_l(pipe_fd,data,1024) < 0){
            LOG_E("read data err");
            empty_command_pipe(pipe_fd);
        }else{
            ext_contrtol_process(adev,data);
        }
    }
    free(data);
    LOG_I("control_audio_loop_process:close:%d",pipe_fd);
    close(pipe_fd);
    LOG_I("control_audio_loop_process exit");
    pthread_detach(pthread_self());
    return NULL;
}

static int create_audio_pipe(const char *pipe_name){
    struct stat buf;
    int try_count=4;
    int ret=-1;
    LOG_I("create_audio_pipe:%s",pipe_name);
    if(access(pipe_name, R_OK) != 0){
        LOG_I("%s create audio fifo line:%d\n",__FUNCTION__,__LINE__);
        ret=mkfifo(pipe_name,S_IFIFO|0666);
        LOG_I("%s create audio fifo line:%d ret:%d\n",
            __FUNCTION__,__LINE__,ret);
        while((ret!=0)&&(try_count>0)){
            LOG_E("%s create audio fifo try:%d ret:%d error  %s\n",
                __FUNCTION__,try_count,ret,strerror(errno));
            try_count--;
            usleep(100*1000);
            ret=mkfifo(pipe_name,S_IFIFO|0666);
            usleep(50*1000);
        }

        if(0==ret){
            LOG_I("%s create audio fifo success\n",__FUNCTION__);
        }else{
            LOG_E("%s create audio fifo try:%d error: %s\n",
                __FUNCTION__,try_count,strerror(errno));
        }
    }

    if(access(pipe_name, R_OK) == 0){
        try_count=4;

        LOG_I("%s set audio fifo mode\n",__FUNCTION__);
        if(chmod(pipe_name, S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP| S_IROTH|S_IWOTH) != 0) {
            LOG_E("%s Cannot set RW to \"%s\": %s", __FUNCTION__, pipe_name, strerror(errno));
        }

        while(try_count>0){
            LOG_I("Check pipe mode try:%d",try_count);
            try_count--;
            memset(&buf,0,sizeof(struct stat));
            if(stat(pipe_name, &buf)!=0){
                LOG_E("%s stat audio fifo error %s\n",__FUNCTION__,strerror(errno));
                break;
            }

            if((buf.st_mode&(S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH))
                !=(S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH)){
                LOG_I("create_audio_pipe pipe mode error:0x%x",buf.st_mode);
                if(chmod(pipe_name, S_IRUSR|S_IWUSR|S_IRGRP| S_IWGRP|S_IROTH|S_IWOTH) != 0) {
                    LOG_E("%s Cannot set RW to \"%s\": %s", __FUNCTION__, pipe_name, strerror(errno));
                }
                usleep(100*1000);
            }else{
                LOG_I("create_audio_pipe pipe mode check success:0x%x",buf.st_mode);
                try_count=0;
                break;
            }
        }
    }else{
            LOG_I("create_audio_pipe can not access:%s",pipe_name);
            return -1;
    }

    return 0;
}
int ext_control_init(struct tiny_audio_device *adev){
    pthread_t control_audio_loop;
    adev->pipe_process_exit=false;

    /*Create mmi.audio.ctrl*/
    if(access(AUDIO_EXT_CONTROL_PIPE, R_OK) != 0){
        create_audio_pipe(AUDIO_EXT_DATA_CONTROL_PIPE);
    }

    create_audio_pipe(MMI_DEFAULT_PCM_FILE);

    if(pthread_create(&control_audio_loop, NULL, control_audio_loop_process, (void *)adev)) {
        LOG_E("control_audio_loop thread creating failed !!!!");
        return -2;
    }
    return 0;
}

void ext_control_close(struct tiny_audio_device *adev){
    int fd = -1;
    bool pipe_process_exit=false;

    pthread_mutex_lock(&adev->lock);
    pipe_process_exit= adev->pipe_process_exit;
    adev->pipe_process_exit=true;
    pthread_mutex_unlock(&adev->lock);

    if(true==pipe_process_exit){
        LOG_I("ext_control_close exit");
        return;
    }

    fd = open(AUDIO_EXT_CONTROL_PIPE, O_WRONLY | O_NONBLOCK);
    if (fd < 0) {
        fd = open(AUDIO_EXT_DATA_CONTROL_PIPE, O_WRONLY | O_NONBLOCK);
    }
    LOG_I("ext_control_close:%d",fd);

    if (fd < 0) {
        LOG_E("ext_control_close open:%s failed error:%s",  AUDIO_EXT_DATA_CONTROL_PIPE,strerror(errno));
        return ;
    } else {
        write(fd, "pipeprocess_exit", strlen("pipeprocess_exit"));
        close(fd);
    }

    if (fd > 0) {
        close(fd);
    }
}
