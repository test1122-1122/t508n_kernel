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

#define LOG_TAG "audio_hw_control"
#define LOG_NDEBUG 0

#include "audio_control.h"
#include "audio_hw.h"
#include <tinyxml.h>
#ifdef AUDIOHAL_V4
#include <log/log.h>
#else
#include <cutils/log.h>
#endif
#include "fcntl.h"

#include "tinyalsa_util.h"
#include "audio_parse.h"
#include <stdio.h>
#include <stdlib.h>

#include "audio_param.h"
#include "param_config.h"
#include "smartamp.h"

#ifdef __cplusplus
extern "C" {
#endif
extern int free_dsploop_control(struct private_dsploop_control *dsploop_ctl);
extern int disable_vbc_playback_dump(struct audio_control *dev_ctl);
extern int set_dsp_volume(struct audio_control *ctl,int volume);
extern int disconnect_audiotester_process(struct socket_handle *tunning_handle);
extern int agdsp_send_msg_test(void * arg,UNUSED_ATTR struct str_parms * params,int opt,UNUSED_ATTR char *val);
static void do_select_device(struct audio_control *actl,AUDIO_HW_APP_T audio_app_type,audio_devices_t device,bool is_in,bool update_param,bool force_set);

static int do_switch_in_devices(struct audio_control *actl,AUDIO_HW_APP_T audio_app_type,audio_devices_t device);
static int set_vbc_bt_src_unlock(struct audio_control *actl, int rate);
static int select_audio_param_unlock(struct audio_control *dev_ctl,struct audio_param_res *param_res);
static uint8_t get_record_param(struct audiotester_config_handle *config,int input_source, audio_devices_t in_devices);
static int is_voip_active(int usecase);
static int is_call_active(int usecase);
static int set_voice_mic(struct audio_control *actl,AUDIO_HW_APP_T audio_app_type,audio_devices_t device);
static int set_vbc_bt_src_without_lock(struct audio_control *actl, int rate);
int dev_ctl_get_out_pcm_config(struct audio_control *dev_ctl,int app_type, int * dev, struct pcm_config *config);
extern int start_usb_channel(struct usbaudio_ctl *usb_ctl,bool is_output);
extern int stop_usb_channel(struct usbaudio_ctl *usb_ctl,bool is_output);
static bool is_usbmic_connected_unlock(struct audio_control *dev_ctl);
static bool is_usbdevice_connected_unlock(struct audio_control *dev_ctl);
static int set_fm_dg_param(struct audio_control *dev_ctl,int param_id,int volume);
int switch_vbc_route_unlock(struct audio_control *ctl,int device);
static int select_devices_set_fm_volume(struct audio_control *actl, int volume, bool sync);
static void set_all_ucp1301_param(struct ucp_1301_handle_t *ucp_1301, void *para,int max_para_size);
static void create_bt_ul_mute_timer(struct audio_control *actl,int delay,int delay_nsec);
static void bt_ul_mute_timer_handler(union sigval arg);

#define SPRD_MAIN_MIC_PATH_SWITCH (1<<0)
#define SPRD_BACK_MIC_PATH_SWITCH (1<<1)
#define SPRD_HEADSET_MIC_PATH_SWITCH (1<<2)
#define SPRD_USB_MIC_PATH_SWITCH (1<<3)
#define ALL_SPRD_MIC_SWITCH  (SPRD_MAIN_MIC_PATH_SWITCH|SPRD_BACK_MIC_PATH_SWITCH|SPRD_HEADSET_MIC_PATH_SWITCH|SPRD_USB_MIC_PATH_SWITCH)
#define BT_UL_MUTE_TIMER_DELAY_NSEC 700000000


struct ucp_1301_ctl_name {
    const char * pa_agc_en;
    const char * pa_mode;
    const char * pa_agc_gain;
    const char * pa_class_d_f;
    const char * rl;
    const char * power_limit_p2;
};

static const struct ucp_1301_ctl_name  ucp_1301_ctl_name_table[MAX_1301_NUMBER] = {
    {
        "UCP1301 AGC Enable","UCP1301 Class Mode","UCP1301 AGC Gain",
        "UCP1301 CLSD Trim","UCP1301 R Load","UCP1301 Power Limit P2"
    },

    {
        "UCP1301 AGC Enable SPK2","UCP1301 Class Mode SPK2","UCP1301 AGC Gain SPK2",
        "UCP1301 CLSD Trim SPK2","UCP1301 R Load SPK2","UCP1301 Power Limit P2 SPK2"
    },

    {
        "UCP1301 AGC Enable RCV","UCP1301 Class Mode RCV","UCP1301 AGC Gain RCV",
        "UCP1301 CLSD Trim RCV","UCP1301 R Load RCV","UCP1301 Power Limit P2 RCV"
    },
};

static const char *ucp_1301_product_id_ctl_name_table[MAX_1301_NUMBER] = {
    "UCP1301 Product ID",
    "UCP1301 Product ID SPK2",
    "UCP1301 Product ID RCV",
};

static const char *ucp_1301_i2c_index_ctl_name_table[MAX_1301_NUMBER] = {
    "UCP1301 I2C Index",
    "UCP1301 I2C Index SPK2",
    "UCP1301 I2C Index RCV",
};

static int device_access_enable(struct audio_control *actl)
{
    int ret=0;

    LOG_I("device_access_enable");
    ret = dsp_sleep_ctrl_pair(actl->agdsp_ctl,true);
    return ret;
}

static int device_access_restore(struct audio_control *actl)
{
    int ret = 0;

    LOG_I("device_access_restore");
    dsp_sleep_ctrl_pair(actl->agdsp_ctl,false);
    return ret;
}

static struct device_route *get_device_route_withname(struct device_route_handler
        *route_handler, const char *name)
{
    for (unsigned int i = 0; i < route_handler->size; i++) {
        LOG_D("get_device_route_withname[%d]:%s",i,route_handler->route[i].name);
        if((route_handler->route+i) && route_handler->route[i].name && name){
            if (strcmp(route_handler->route[i].name, name) == 0) {
                return &route_handler->route[i];
            }
        }
    }
    return NULL;
}

static struct device_route *get_usecase_device_ctl(struct device_route_handler
        *route_handler, const char *name,int device)
{
    if(NULL==name){
        return NULL;
    }

    for (unsigned int i = 0; i < route_handler->size; i++) {
        LOG_D("get_device_route_withname[%d]:%s",i,route_handler->route[i].name);
        if((route_handler->route+i) && (route_handler->route[i].name!=NULL)){
            if(strcmp(route_handler->route[i].name, name) == 0){
                if(device&AUDIO_DEVICE_BIT_IN){
                    if ((route_handler->route[i].devices&(~AUDIO_DEVICE_BIT_IN)) & (device&(~AUDIO_DEVICE_BIT_IN))){
                        return &route_handler->route[i];
                    }
                }else{
                    if (route_handler->route[i].devices & device){
                        return &route_handler->route[i];
                    }
                }
            }
        }
    }
    return NULL;
}

static struct device_route *get_device_route_withdevice(struct device_route_handler  *route_handler, int devices){
    for (unsigned int i = 0; i < route_handler->size; i++) {
        if(route_handler->route+i){
            if( (route_handler->route[i].devices!=-1) &&
                        (devices ==  route_handler->route[i].devices)){
                return &route_handler->route[i];
            }
        }
    }
    return NULL;
}

static struct vbc_device_route *get_vbc_device_route_withdevice(struct vbc_device_route_handler  *route_handler, int devices){
    struct vbc_device_route *route=NULL;
    bool is_input_device=false;
    LOG_D("get_vbc_device_route_withdevice size:%d",route_handler->size);

    if(AUDIO_DEVICE_BIT_IN&devices){
        is_input_device =true;
        devices&=~AUDIO_DEVICE_BIT_IN;
    }

    for (unsigned int i = 0; i < route_handler->size; i++) {
        if(route_handler->route+i){
            route=&route_handler->route[i];
            LOG_D("get_vbc_device_route_withdevice[%d] devices:%x %x %x %x",i,
                devices,route->devices[0],route->devices[1],route->devices[2]);
            if(NULL==route){
                break;
            }

            if((route->devices[0]==0)||(route->devices[1]==0)||(route->devices[2]==0)){
                continue;
            }

            if((true==is_input_device)&&(0==(AUDIO_DEVICE_BIT_IN&route->devices[2]))){
                continue;
            }

            if((false==is_input_device)&&(AUDIO_DEVICE_BIT_IN&route->devices[2])){
                continue;
            }

            if(0==(devices&route->devices[2])){
                continue;
            }

            if((true==is_input_device)&&(0==(AUDIO_DEVICE_BIT_IN&route->devices[1]))){
                continue;
            }

            if((false==is_input_device)&&(AUDIO_DEVICE_BIT_IN&route->devices[1])){
                continue;
            }

            if(0==(devices&route->devices[1])){
                continue;
            }

            if((true==is_input_device)&&(0==(AUDIO_DEVICE_BIT_IN&route->devices[0]))){
                continue;
            }

            if((false==is_input_device)&&(AUDIO_DEVICE_BIT_IN&route->devices[0])){
                continue;
            }

            if((devices&route->devices[0])&&(devices&route->devices[1])&&(devices&route->devices[2])){
                return route;
            }
        }
    }

    for (unsigned int i = 0; i < route_handler->size; i++) {
        if(route_handler->route+i){
            route=&route_handler->route[i];
            if(NULL==route){
                break;
            }

            if((route->devices[0]==0)||(route->devices[1]==0)){
                continue;
            }

            if((true==is_input_device)&&(0==(AUDIO_DEVICE_BIT_IN&route->devices[1]))){
                continue;
            }

            if((false==is_input_device)&&(AUDIO_DEVICE_BIT_IN&route->devices[1])){
                continue;
            }

            if(0==(devices&route->devices[1])){
                continue;
            }

            if((true==is_input_device)&&(0==(AUDIO_DEVICE_BIT_IN&route->devices[0]))){
                continue;
            }

            if((false==is_input_device)&&(AUDIO_DEVICE_BIT_IN&route->devices[0])){
                continue;
            }

            if((devices&route->devices[0])&&(devices&route->devices[1])){
                return route;
            }
        }
    }


    for (unsigned int i = 0; i < route_handler->size; i++) {
        if(route_handler->route+i){
            route=&route_handler->route[i];
            if(NULL==route){
                break;
            }

            if(route->devices[0]==0){
                continue;
            }

            if((true==is_input_device)&&(0==(AUDIO_DEVICE_BIT_IN&route->devices[0]))){
                continue;
            }

            if((false==is_input_device)&&(AUDIO_DEVICE_BIT_IN&route->devices[0])){
                continue;
            }

            if(devices&route->devices[0]){
                return route;
            }
        }
    }

    return NULL;
}

static int apply_mixer_control(struct device_control *dev_ctl, const char *info)
{
    int ret=-1;
    unsigned int i=0;
    LOG_D("apply_mixer_control %s %s ",dev_ctl->name,info);
    for (i=0; i < dev_ctl->ctl_size; i++) {
        struct mixer_control *mixer_ctl = &dev_ctl->ctl[i];
        if(mixer_ctl->strval!=NULL){
            ret = mixer_ctl_set_enum_by_string(mixer_ctl->ctl, mixer_ctl->strval);
            if (ret != 0) {
                LOG_E("Failed to set '%s' to '%s'\n",mixer_ctl->name, mixer_ctl->strval);
            } else {
                LOG_I("Set '%s' to '%s'\n",mixer_ctl->name, mixer_ctl->strval);
            }
        }else{
            if(1!=mixer_ctl->val_count){
                int val[2]={0};
                val[0]=mixer_ctl->value;
                val[1]=mixer_ctl->value;
                ret=mixer_ctl_set_array(mixer_ctl->ctl,val,2);
            }else{
                ret=mixer_ctl_set_value(mixer_ctl->ctl, 0, mixer_ctl->value);
            }
            if (ret != 0) {
                LOG_E("Failed to set '%s'to %d\n",
                        mixer_ctl->name, mixer_ctl->value);
            } else {
                LOG_I("Set '%s' to %d\n",
                        mixer_ctl->name, mixer_ctl->value);
            }
        }
    }
    return ret;
}

int stream_type_to_usecase(int audio_app_type)
{
    int usecase = UC_UNKNOWN;
     switch(audio_app_type){
        case AUDIO_HW_APP_VOIP:
            usecase = UC_VOIP;
            break;
        case AUDIO_HW_APP_PRIMARY:
            usecase = UC_NORMAL_PLAYBACK;
            break;
        case AUDIO_HW_APP_FAST:
            usecase = UC_FAST_PLAYBACK;
            break;
        case AUDIO_HW_APP_DEEP_BUFFER:
            usecase = UC_DEEP_BUFFER_PLAYBACK;
            break;
        case AUDIO_HW_APP_OFFLOAD:
            usecase = UC_OFFLOAD_PLAYBACK;
            break;
        case AUDIO_HW_APP_RECOGNITION:
            usecase = UC_RECOGNITION;
            break;
        case AUDIO_HW_APP_NORMAL_RECORD:
            usecase = UC_MM_RECORD;
            break;
        case AUDIO_HW_APP_CALL:
            usecase = UC_CALL;
            break;
        case AUDIO_HW_APP_DSP_LOOP:
            usecase = UC_LOOP;
            break;
        case AUDIO_HW_APP_FM:
            usecase = UC_FM;
            break;
        case AUDIO_HW_APP_VOIP_RECORD:
            usecase = UC_VOIP_RECORD;
            break;
        case AUDIO_HW_APP_FM_RECORD:
            usecase = UC_FM_RECORD;
            break;
        case AUDIO_HW_APP_CALL_RECORD:
            usecase = UC_VOICE_RECORD;
            break;
        case AUDIO_HW_APP_BT_RECORD:
            usecase = UC_BT_RECORD;
            break;
        case AUDIO_HW_APP_MMAP:
            usecase = UC_MMAP_PLAYBACK;
            break;
        case AUDIO_HW_APP_MMAP_RECORD:
            usecase = UC_MMAP_RECORD;
            break;
        case AUDIO_HW_APP_VOICE_TX:
            usecase = UC_VOICE_TX;
            break;
        default:
            LOG_E("start_output_stream stream type:0x%x", audio_app_type);
            break;
    }

    return usecase;
}

static int close_all_control_unlock(struct audio_control *actl)
{
    int ret=0;
    struct audio_route * route = (struct audio_route *)&actl->route;
    LOG_I("close_all_control_unlock enter usecase:0x%x",actl->usecase);
    if(NULL !=route->vbc_iis_mux_route.pre_in_ctl){
        LOG_I("IIS MUX IN DEVICE :%s Route OFF",route->vbc_iis_mux_route.pre_in_ctl->name);
        ret=apply_mixer_control(&(route->vbc_iis_mux_route.pre_in_ctl->ctl_off), "Route OFF");
        route->vbc_iis_mux_route.pre_in_ctl=NULL;
    }

    if(NULL !=route->be_switch_route.pre_in_ctl){
        LOG_I("BE IN DEVICE :%s Route OFF",route->be_switch_route.pre_in_ctl->name);
        ret=apply_mixer_control(&(route->be_switch_route.pre_in_ctl->ctl_off), "Route OFF");
        route->be_switch_route.pre_in_ctl=NULL;
    }

    pthread_mutex_lock(&actl->lock_route);
    if(NULL !=route->devices_route.pre_in_ctl){
        LOG_I("IN DEVICE :%s Route OFF",route->devices_route.pre_in_ctl->name);
        ret=apply_mixer_control(&(route->devices_route.pre_in_ctl->ctl_off), "Route OFF");
        route->devices_route.pre_in_ctl=NULL;
    }
    pthread_mutex_unlock(&actl->lock_route);

    if(NULL !=route->vbc_iis_mux_route.pre_out_ctl){
        LOG_I("IIS MUX OUT DEVICE :%s Route OFF",route->vbc_iis_mux_route.pre_out_ctl->name);
        ret=apply_mixer_control(&(route->vbc_iis_mux_route.pre_out_ctl->ctl_off), "Route OFF");
        route->vbc_iis_mux_route.pre_out_ctl=NULL;
    }

    if(NULL !=route->be_switch_route.pre_out_ctl){
        LOG_I("BE OUT DEVICE :%s Route OFF",route->be_switch_route.pre_out_ctl->name);
        ret=apply_mixer_control(&(route->be_switch_route.pre_out_ctl->ctl_off), "Route OFF");
        route->be_switch_route.pre_out_ctl=NULL;
    }

    pthread_mutex_lock(&actl->lock_route);
    if(NULL !=route->devices_route.pre_out_ctl){
        LOG_I("OUT DEVICE :%s Route OFF",route->devices_route.pre_out_ctl->name);
        ret=apply_mixer_control(&(route->devices_route.pre_out_ctl->ctl_off), "Route OFF");
        route->devices_route.pre_out_ctl=NULL;
    }
    pthread_mutex_unlock(&actl->lock_route);
    LOG_D("close_all_control_unlock exit");
    return ret;
}

int close_all_control(struct audio_control *actl)
{
    int ret=0;
    pthread_mutex_lock(&actl->lock);
    ret=close_all_control_unlock(actl);
    pthread_mutex_unlock(&actl->lock);
    return ret;
}

int close_out_control(struct audio_control *actl)
{
    int ret=0;
    struct audio_route * route = (struct audio_route *)&actl->route;
    pthread_mutex_lock(&actl->lock);
    LOG_I("close_out_control usecase:0x%x",actl->usecase);
    if(NULL !=route->vbc_iis_mux_route.pre_out_ctl){
        LOG_I("OUT DEVICE :%s Route OFF",route->vbc_iis_mux_route.pre_out_ctl->name);
        ret=apply_mixer_control(&(route->vbc_iis_mux_route.pre_out_ctl->ctl_off), "Route OFF");
        route->vbc_iis_mux_route.pre_out_ctl=NULL;
    }

    if(NULL !=route->be_switch_route.pre_out_ctl){
        LOG_I("OUT DEVICE :%s Route OFF",route->be_switch_route.pre_out_ctl->name);
        ret=apply_mixer_control(&(route->be_switch_route.pre_out_ctl->ctl_off), "Route OFF");
        route->be_switch_route.pre_out_ctl=NULL;
    }

    pthread_mutex_unlock(&actl->lock);
    pthread_mutex_lock(&actl->lock_route);
    if(NULL !=route->devices_route.pre_out_ctl){
        LOG_I("OUT DEVICE :%s Route OFF",route->devices_route.pre_out_ctl->name);
        ret=apply_mixer_control(&(route->devices_route.pre_out_ctl->ctl_off), "Route OFF");
        route->devices_route.pre_out_ctl=NULL;
    }
    pthread_mutex_unlock(&actl->lock_route);
    pthread_mutex_lock(&actl->lock);

    actl->out_devices=0;
    pthread_mutex_unlock(&actl->lock);
    return ret;
}

static void close_mic_control_unlock(struct audio_control *actl){
    struct audio_route * route = (struct audio_route *)&actl->route;
    if(NULL !=route->devices_route.pre_in_ctl){
        LOG_I("close_mic_control_unlock IN DEVICE :%s Route OFF",route->devices_route.pre_in_ctl->name);
        apply_mixer_control(&(route->devices_route.pre_in_ctl->ctl_off), "Route OFF");
        route->devices_route.pre_in_ctl=NULL;
    }
}

int close_in_control(struct audio_control *actl)
{
    int ret=0;
    struct audio_route * route = (struct audio_route *)&actl->route;
    pthread_mutex_lock(&actl->lock);
    LOG_I("close_in_control usecase:0x%x",actl->usecase);
    if(NULL !=route->vbc_iis_mux_route.pre_in_ctl){
        LOG_I("IN DEVICE :%s Route OFF",route->vbc_iis_mux_route.pre_in_ctl->name);
        ret=apply_mixer_control(&(route->vbc_iis_mux_route.pre_in_ctl->ctl_off), "Route OFF");
        route->vbc_iis_mux_route.pre_in_ctl=NULL;
    }

    if(NULL !=route->be_switch_route.pre_in_ctl){
        LOG_I("IN DEVICE :%s Route OFF",route->be_switch_route.pre_in_ctl->name);
        ret=apply_mixer_control(&(route->be_switch_route.pre_in_ctl->ctl_off), "Route OFF");
        route->be_switch_route.pre_in_ctl=NULL;
    }

    pthread_mutex_lock(&actl->lock_route);
    if(NULL !=route->devices_route.pre_in_ctl){
        LOG_I("IN DEVICE :%s Route OFF",route->devices_route.pre_in_ctl->name);
        ret=apply_mixer_control(&(route->devices_route.pre_in_ctl->ctl_off), "Route OFF");
        route->devices_route.pre_in_ctl=NULL;
    }
    pthread_mutex_unlock(&actl->lock_route);

    actl->in_devices=0;
    pthread_mutex_unlock(&actl->lock);
    return ret;
}

/**should add lock if call this function without any locks*/
static int _apply_mixer_control(struct device_control *dev_ctl,
    struct device_control *pre_dev_ctl_on,struct device_control *pre_dev_ctl_off)
{
    unsigned int i=0,j=0,ret=0;
    struct mixer_control *mixer_ctl=NULL;
    struct mixer_control *mixer_ctl_off=NULL;
    bool mixer_find=false;
    LOG_D("_apply_mixer_control ON:%s size:%d",dev_ctl->name,dev_ctl->ctl_size);
    for(i=0;i<dev_ctl->ctl_size;i++){
        mixer_ctl = &dev_ctl->ctl[i];
        mixer_find = false;
        for(j=0;j<pre_dev_ctl_on->ctl_size;j++){
            mixer_ctl_off = &pre_dev_ctl_on->ctl[j];
            if(mixer_ctl->ctl==mixer_ctl_off->ctl){
                if(mixer_ctl->strval!=NULL){
                    if(0 == strncmp(mixer_ctl_off->strval,mixer_ctl->strval,strlen(mixer_ctl->strval))){
                        mixer_find=true;
                        LOG_I("same mixer:%s same value:%s",mixer_ctl_off->name,mixer_ctl->strval);
                        break;
                    }
                }else{
                    if(mixer_ctl_off->value==mixer_ctl->value){
                        mixer_find=true;
                        LOG_I("same mixer:%s same value:%d",mixer_ctl_off->name,mixer_ctl->value);
                        break;
                    }
                }
                mixer_find=false;
                LOG_I("same mixer:%s different value",mixer_ctl_off->name);
                break;
            }
        }

        if(true==mixer_find){
            continue;
        }

        if(mixer_ctl->strval!=NULL){
            ret = mixer_ctl_set_enum_by_string(mixer_ctl->ctl, mixer_ctl->strval);
            if (ret != 0) {
                LOG_E("Failed to set '%s' to '%s'\n",mixer_ctl->name, mixer_ctl->strval);
            } else {
                LOG_I("Set '%s' to '%s'\n",mixer_ctl->name, mixer_ctl->strval);
            }
        }else{
            if(1!=mixer_ctl->val_count){
                int val[2]={0};
                val[0]=mixer_ctl->value;
                val[1]=mixer_ctl->value;
                ret=mixer_ctl_set_array(mixer_ctl->ctl,val,2);
            }else{
                ret=mixer_ctl_set_value(mixer_ctl->ctl, 0, mixer_ctl->value);
            }
            if (ret != 0) {
                LOG_E("Failed to set '%s'to %d\n",
                        mixer_ctl->name, mixer_ctl->value);
            } else {
                LOG_I("Set '%s' to %d\n",
                        mixer_ctl->name, mixer_ctl->value);
            }
        }
    }

    LOG_D("_apply_mixer_control OFF:%s size:%d",pre_dev_ctl_off->name,pre_dev_ctl_off->ctl_size);
    for(i=0;i<pre_dev_ctl_off->ctl_size;i++){
        mixer_ctl_off = &pre_dev_ctl_off->ctl[i];
        mixer_find = false;
        for(j=0;j<dev_ctl->ctl_size;j++){
            mixer_ctl= &dev_ctl->ctl[j];
            if(mixer_ctl->ctl==mixer_ctl_off->ctl){
                LOG_D("same mixer:%s",mixer_ctl_off->name);
                mixer_find=true;
                break;
            }
        }

        if(true==mixer_find){
            continue;
        }

        if(mixer_ctl_off->strval!=NULL){
            ret = mixer_ctl_set_enum_by_string(mixer_ctl_off->ctl, mixer_ctl_off->strval);
            if (ret != 0) {
                LOG_E("Failed to set '%s' to '%s'\n",mixer_ctl_off->name, mixer_ctl_off->strval);
            } else {
                LOG_I("Set '%s' to '%s'\n",mixer_ctl_off->name, mixer_ctl_off->strval);
            }
        }else{
            if (1!=mixer_ctl_off->val_count) {
                int val[2]={0};
                val[0]=mixer_ctl_off->value;
                val[1]=mixer_ctl_off->value;
                ret=mixer_ctl_set_array(mixer_ctl_off->ctl,val,2);
            } else {
                ret=mixer_ctl_set_value(mixer_ctl_off->ctl, 0, mixer_ctl_off->value);
            }
            if (ret != 0) {
                LOG_E("Failed to set '%s'to %d\n",
                        mixer_ctl_off->name, mixer_ctl_off->value);
            } else {
                LOG_I("Set '%s' to %d\n",
                        mixer_ctl_off->name, mixer_ctl_off->value);
            }
        }
    }

    return ret;
}

static int set_digital_codec_status(struct audio_control *actl,bool on_off)
{
    int ret=0;

    if(NULL!=actl->digital_codec_ctl){
        ret=mixer_ctl_set_value(actl->digital_codec_ctl, 0, on_off);
        if (ret != 0) {
            LOG_E("%s %d Failed ret:%d\n", __func__,on_off,ret);
        }else{
            LOG_I("Set 'Virt Output Switch' to '%d'\n",on_off);
        }
    }
    return ret;
}

static int switch_device_route(struct audio_control *ctl, int device,
                               bool in_device)
{
    LOG_D("switch_device_route device=0x%x,in_device:%d", device,in_device);
    struct device_route *cur = NULL;

    cur = get_device_route_withdevice(&(ctl->route.devices_route), device);

    if(NULL == cur){
        LOG_I("switch_device_route not fined the devices: 0x%x",device);
        #if 0
        if(ctl->route.devices_route.pre_out_ctl) {
            apply_mixer_control(&ctl->route.devices_route.pre_out_ctl->ctl_off, "Route OFF");
            ctl->route.devices_route.pre_out_ctl = NULL;
        }
        #endif
        return -1;
    }

    if(in_device) {
        if(cur == ctl->route.devices_route.pre_in_ctl){
            LOG_D("switch_device_route set the same devices");
            return 0;
        }

        if(NULL !=ctl->route.devices_route.pre_in_ctl){
            LOG_I("IN DEVICES %s Route OFF %s Route ON",ctl->route.devices_route.pre_in_ctl->name,cur->name);
            _apply_mixer_control(&cur->ctl_on,
                &(ctl->route.devices_route.pre_in_ctl->ctl_on),
                &(ctl->route.devices_route.pre_in_ctl->ctl_off));
        }else{
            LOG_I("IN DEVICES %s Route ON",cur->name);
            apply_mixer_control(&cur->ctl_on, "Route ON");
        }

        ctl->route.devices_route.pre_in_ctl=cur;
    }
    else {
        if(cur == ctl->route.devices_route.pre_out_ctl){
            LOG_D("switch_device_route set the same devices");
            return 0;
        }

        if(NULL !=ctl->route.devices_route.pre_out_ctl){

            set_digital_codec_status(ctl,true);

            #ifndef SPRD_AUDIO_HIFI_SUPPORT
            if(((AUDIO_DEVICE_OUT_WIRED_HEADSET==ctl->out_devices)||(AUDIO_DEVICE_OUT_WIRED_HEADPHONE==ctl->out_devices))
                    ||((AUDIO_DEVICE_OUT_WIRED_HEADSET==device)||(AUDIO_DEVICE_OUT_WIRED_HEADPHONE==device))){
                if(ctl->out_devices!=(audio_devices_t)device){
                    LOG_I("Close output device while switch to headset or switch from headset");
                        apply_mixer_control(&(ctl->route.devices_route.pre_out_ctl->ctl_off), "Route OFF");
                        ctl->route.devices_route.pre_out_ctl =NULL;
                }
            }
            #endif

            if(NULL !=ctl->route.devices_route.pre_out_ctl){
                LOG_I("OUT DEVICES %s Route OFF %s Route ON",ctl->route.devices_route.pre_out_ctl->name,cur->name);
                _apply_mixer_control(&cur->ctl_on,
                    &(ctl->route.devices_route.pre_out_ctl->ctl_on),
                    &(ctl->route.devices_route.pre_out_ctl->ctl_off));
            }else{
                LOG_I("OUT DEVICES %s Route ON",cur->name);
                apply_mixer_control(&cur->ctl_on, "Route ON");
            }

            set_digital_codec_status(ctl,false);

        }else{
            LOG_I("OUT DEVICES %s Route ON",cur->name);
            apply_mixer_control(&cur->ctl_on, "Route ON");
        }

        ctl->route.devices_route.pre_out_ctl=cur;
    }
    return 0;
}

static int apply_gain_control(struct gain_mixer_control *gain_ctl, int volume)
{
    int ctl_volume = 0;
    int volume_array[2]={0};
    if (gain_ctl->volume_size == 1 || volume == -1) {
        ctl_volume = gain_ctl->volume_value[0];
    } else if (volume < gain_ctl->volume_size) {
        ctl_volume = gain_ctl->volume_value[volume];
    } else {
        LOG_W("volume is too big ctl_volume:%d max:%d",volume,gain_ctl->volume_size);
        ctl_volume = gain_ctl->volume_value[gain_ctl->volume_size - 1];
    }
    volume_array[0]=ctl_volume;
    volume_array[1]=ctl_volume;
    mixer_ctl_set_array(gain_ctl->ctl, volume_array, 2);
    LOG_I("Apply Gain Control [%s. %d]", gain_ctl->name, ctl_volume);
    return 0;
}

static int _set_mdg_mute(struct mixer_ctrl_t *mute_ctl,bool mute){
    int ret=-1;
    int vol_array[2]={0};

    if(mute==mute_ctl->value){
        LOG_W("_set_mdg_mute:set the same value:%d",mute_ctl->value);
        return 0;
    }

    if(true==mute){
        vol_array[0]=1;
    }else{
        vol_array[0]=0;
    }
    vol_array[1]=1024;

    if(NULL!=mute_ctl){
        ret=mixer_ctl_set_array(mute_ctl->mixer, vol_array, 2);
        if (ret != 0) {
            LOG_E("_set_mdg_mute Failed :%d\n",mute);
        }else{
            LOG_I("_set_mdg_mute:%p %d",mute_ctl,mute);
            mute_ctl->value=mute;
        }
    }else{
        LOG_W("_set_mdg_mute mute_ctl is null");
    }
    return ret;
}

static int usecase_to_stream_type(int usecase){
    int stream_type = AUDIO_HW_APP_INVALID;
    switch(usecase){
        case UC_NORMAL_PLAYBACK:
            stream_type = AUDIO_HW_APP_PRIMARY;
            break;
        case UC_VOIP:
            stream_type = AUDIO_HW_APP_VOIP;
            break;
        case UC_FAST_PLAYBACK:
            stream_type = AUDIO_HW_APP_FAST;
            break;
        case UC_DEEP_BUFFER_PLAYBACK:
            stream_type = AUDIO_HW_APP_DEEP_BUFFER;
            break;
        case UC_OFFLOAD_PLAYBACK:
            stream_type = AUDIO_HW_APP_OFFLOAD;
            break;
        case UC_MM_RECORD:
            stream_type = AUDIO_HW_APP_NORMAL_RECORD;
            break;
        case UC_RECOGNITION:
            stream_type = AUDIO_HW_APP_RECOGNITION;
            break;
        case UC_CALL:
            stream_type = AUDIO_HW_APP_CALL;
            break;
        case UC_FM:
            stream_type = AUDIO_HW_APP_FM;
            break;
        case UC_LOOP:
            stream_type = AUDIO_HW_APP_DSP_LOOP;
            break;
        case UC_VOIP_RECORD:
            stream_type = AUDIO_HW_APP_VOIP_RECORD;
            break;
        case UC_FM_RECORD:
            stream_type = AUDIO_HW_APP_FM_RECORD;
            break;
        case UC_VOICE_RECORD:
            stream_type = AUDIO_HW_APP_CALL_RECORD;
            break;
        case UC_BT_RECORD:
            stream_type = AUDIO_HW_APP_BT_RECORD;
            break;
        case UC_MMAP_PLAYBACK:
            stream_type = AUDIO_HW_APP_MMAP;
            break;
        case UC_MMAP_RECORD:
            stream_type = AUDIO_HW_APP_MMAP_RECORD;
            break;
        case UC_VOICE_TX:
            stream_type = AUDIO_HW_APP_VOICE_TX;
            break;

        default:
            LOG_E("usecase_to_stream_type usecase:%x", usecase);
            break;
    }
    return stream_type;
}

static int get_stream_type_with_pcm_dev(int pcm_devices){
    int audio_app_type = AUDIO_HW_APP_INVALID;

    switch(pcm_devices){
        case AUD_PCM_VOIP:
            audio_app_type = AUDIO_HW_APP_VOIP;
            break;
        case AUD_PCM_DIGITAL_FM:
            audio_app_type = AUDIO_HW_APP_FM;
            break;
        case AUD_PCM_MM_NORMAL:
            audio_app_type = AUDIO_HW_APP_PRIMARY;
            break;
        case AUD_PCM_DEEP_BUFFER:
            audio_app_type = AUDIO_HW_APP_DEEP_BUFFER;
            break;
        case AUD_PCM_FAST:
            audio_app_type = AUDIO_HW_APP_FAST;
            break;
        case AUD_PCM_MODEM_DL:
            audio_app_type = AUDIO_HW_APP_CALL;
            break;
        case AUD_PCM_DSP_LOOP:
            audio_app_type = AUDIO_HW_APP_DSP_LOOP;
            break;
        case AUD_PCM_MMAP_NOIRQ:
            audio_app_type = AUDIO_HW_APP_MMAP;
            break;
        default:
            LOG_E("get_stream_type_with_pcm_dev stream type:%d", audio_app_type);
            break;
    }
    return audio_app_type;
}


/*
1. "VBC DAC0 DSP MDG Set":
fe fast:3
fe offload:4
fe fm dsp:15

2. "VBC DAC1 DSP MDG Set":
fe voice:5
fe voip:6
fe loop:10

3. "VBC DAC0 AUD MDG Set":
fe normal ap01:0

4. "VBC DAC0 AUD23 MDG Set":
fe normal ap23:1
*/
static struct mixer_ctrl_t * get_mdg_with_pcm_dev(struct audio_control *actl,int pcm_devices){
    struct mute_control *mute=&actl->mute;
    struct mixer_ctrl_t * mdg=NULL;

    switch(pcm_devices){
        case 3:
        case 4:
        case 15:
            mdg=&mute->dsp_da0_mdg_mute;
            break;
        case 5:
        case 6:
        case 10:
            mdg=&mute->dsp_da1_mdg_mute;
            break;
        case 0:
            mdg=&mute->audio_mdg_mute;
            break;
        case 1:
            mdg=&mute->audio_mdg23_mute;
            break;
        default:
            mdg=NULL;
    }
    return mdg;
}

static int get_usecase_with_mdg(struct audio_control *actl,struct mixer_ctrl_t * mdg){
    struct mute_control *mute = &actl->mute;
    int pcm_devices = 0;
    int i = 0;
    int usecase = 0;
    int app_type = 0;

    if(&mute->dsp_da0_mdg_mute == mdg){
        pcm_devices = (1<<3)|(1<<4)|(1<<15);
    }

    if(&mute->dsp_da1_mdg_mute == mdg){
        pcm_devices = (1<<5)|(1<<6)|(1<<10);
    }

    if(&mute->audio_mdg_mute == mdg){
        pcm_devices = (1<<0);
    }

    if(&mute->audio_mdg23_mute == mdg){
        pcm_devices = (1<<1);
    }
    for(i=0;i<AUD_PCM_MAX;i++){
        if((1<<actl->pcm_handle.playback_devices[i])&pcm_devices){
            app_type = get_stream_type_with_pcm_dev(i);
            usecase |= stream_type_to_usecase(app_type);
        }
    }
    if((1<<actl->pcm_handle.compress.devices)&pcm_devices) {
        usecase |= stream_type_to_usecase(AUDIO_HW_APP_OFFLOAD);
    }
    LOG_I("get_usecase_with_mdg, device:%x, app_type:%d, usecase:%x",pcm_devices, app_type, usecase);
    return usecase;
}

int set_mdg_mute_unlock(struct audio_control *actl,int usecase,bool on){
    int ret = -1;
    struct mute_control *mute = &actl->mute;
    int pcm_devices = 0;
    int stream_type = AUDIO_HW_APP_INVALID;
    struct mixer_ctrl_t * mdg_ctl = NULL;
    int mdg_usecase = 0;
    struct pcm_config pcm_config;

    LOG_D("set_mdg_mute_unlock:%d %p usecase:%d",on,mute,usecase);

    if(UC_UNKNOWN==actl->usecase){
        LOG_W("set_mdg_mute_unlock failed");
        goto exit;
    }

    stream_type = usecase_to_stream_type(usecase);

    if(AUDIO_HW_APP_OFFLOAD==stream_type){
        pcm_devices=actl->pcm_handle.compress.devices;
    }else{
        ret=dev_ctl_get_out_pcm_config(actl,stream_type,&pcm_devices,&pcm_config);
    }
    mdg_ctl=get_mdg_with_pcm_dev(actl,pcm_devices);
    if(mdg_ctl==NULL){
        LOG_I("get_mdg_with_devices error,devices:%d!!!",pcm_devices);
        goto exit;
    }

    if(on==false){
        ret = _set_mdg_mute(mdg_ctl,on);
    } else {
        mdg_usecase=get_usecase_with_mdg(actl,mdg_ctl);
        if(false == is_usecase_unlock(actl, mdg_usecase&(~usecase))){
            LOG_I("set_mdg_mute_unlock, usecase:%x, on:%d",usecase, on);
            ret = _set_mdg_mute(mdg_ctl,on);
        }
    }

exit:
    return ret;
}

int set_mdg_mute(struct audio_control *actl,int usecase,bool on){
    int ret = -1;
    pthread_mutex_lock(&actl->lock);
    ret = set_mdg_mute_unlock(actl,usecase,on);
    pthread_mutex_unlock(&actl->lock);
    return ret;
}

static void set_mdg_all_mute(struct audio_control *actl) {
    int ret;
    int vol_array[2]={1,1};
    int vol_array_voice[2]={1,4};

    LOG_I("set_mdg_all_mute");

    if(NULL!=actl->mute.dsp_da0_mdg_mute.mixer){
        ret=mixer_ctl_set_array(actl->mute.dsp_da0_mdg_mute.mixer, vol_array, 2);
        if (ret != 0) {
            LOG_E("set_mdg_all_mute dsp_da0_mdg Failed :%d\n",ret);
        }
    }

    if(NULL!=actl->mute.dsp_da1_mdg_mute.mixer){
        ret=mixer_ctl_set_array(actl->mute.dsp_da1_mdg_mute.mixer, vol_array_voice, 2);
        if (ret != 0) {
            LOG_E("set_mdg_all_mute dsp_da1_mdg Failed :%d\n",ret);
        }
    }

    if(NULL!=actl->mute.audio_mdg23_mute.mixer){
        ret=mixer_ctl_set_array(actl->mute.audio_mdg23_mute.mixer, vol_array, 2);
        if (ret != 0) {
            LOG_E("set_mdg_all_mute mdg23 Failed :%d\n",ret);
        }
    }

    if(NULL!=actl->mute.audio_mdg_mute.mixer){
        ret=mixer_ctl_set_array(actl->mute.audio_mdg_mute.mixer, vol_array, 2);
        if (ret != 0) {
            LOG_E("set_mdg_all_mute mdg Failed :%d\n",ret);
        }
    }
}

static void set_mdg_all_restore(struct audio_control *actl) {
    int ret;
    int vol_array[2]={0,1};
    int vol_array_voice[2]={0,4};

    if((NULL!=actl->mute.dsp_da0_mdg_mute.mixer)&&(!actl->mute.dsp_da0_mdg_mute.value)){
        ret=mixer_ctl_set_array(actl->mute.dsp_da0_mdg_mute.mixer, vol_array, 2);
        if (ret != 0) {
            LOG_E("set_mdg_all_mute dsp_da0_mdg Failed :%d\n",ret);
        }else{
            LOG_D("set_mdg_all_restore dsp_da0_mdg unmute");
        }
    }

    if((NULL!=actl->mute.dsp_da1_mdg_mute.mixer)&&(!actl->mute.dsp_da1_mdg_mute.value)){
        ret=mixer_ctl_set_array(actl->mute.dsp_da1_mdg_mute.mixer, vol_array_voice, 2);
        if (ret != 0) {
            LOG_E("set_mdg_all_mute dsp_da1_mdg Failed :%d\n",ret);
        }else{
            LOG_D("set_mdg_all_restore dsp_da1_mdg unmute");
        }
    }

    if((NULL!=actl->mute.audio_mdg23_mute.mixer)&&(!actl->mute.audio_mdg23_mute.value)){
        ret=mixer_ctl_set_array(actl->mute.audio_mdg23_mute.mixer, vol_array, 2);
        if (ret != 0) {
            LOG_E("set_mdg_all_mute mdg23 Failed :%d\n",ret);
        }else{
            LOG_D("set_mdg_all_restore mdg23 unmute");
        }
    }

    if((NULL!=actl->mute.audio_mdg_mute.mixer)&&(!actl->mute.audio_mdg_mute.value)){
        ret=mixer_ctl_set_array(actl->mute.audio_mdg_mute.mixer, vol_array, 2);
        if (ret != 0) {
            LOG_E("set_mdg_all_mute mdg Failed :%d\n",ret);
        }else{
            LOG_D("set_mdg_all_restore audio dg unmute");
        }
    }
}

int set_vdg_gain(struct device_usecase_gain *dg_gain, int param_id, int volume){
    struct device_gain * gain=NULL;

    LOG_I("UPDATE_PARAM_VDG:%s volme:%d",get_audio_param_name(param_id),volume);

    LOG_V("set_vdg_gain size:%d param_id:%d volume:%d",dg_gain->gain_size,param_id,volume);
    for(int i=0;i<dg_gain->gain_size;i++){
        LOG_V("set_vdg_gain:%s",dg_gain->dev_gain[i].name);
        gain=&(dg_gain->dev_gain[i]);
        if(gain->id==param_id){
            LOG_V("set_vdg_gain profile:%d",param_id);
            for(int j=0;j<gain->ctl_size;j++){
                apply_gain_control(&(gain->ctl[j]),volume);
            }
            return 0;
        }
    }
    LOG_E("set_vdg_gain Didn't found the Device Gain for profile_id=%d", param_id);
    return -1;
}

static int do_switch_out_devices(struct audio_control *actl,UNUSED_ATTR AUDIO_HW_APP_T audio_app_type,audio_devices_t device)
{
    int ret=0;

    ret=switch_device_route(actl,device,false);
    if(ret<0) {
        if(device &ALL_USB_OUTPUT_DEVICES) {
            LOG_I("do_switch_out_devices:usb device");
            ret = 0;
        }else {
            if(device&AUDIO_DEVICE_OUT_WIRED_HEADSET){
                device=AUDIO_DEVICE_OUT_WIRED_HEADSET;
            }else if(device&AUDIO_DEVICE_OUT_WIRED_HEADPHONE){
                device=AUDIO_DEVICE_OUT_WIRED_HEADPHONE;
            }else if(device&AUDIO_DEVICE_OUT_EARPIECE){
                device=AUDIO_DEVICE_OUT_EARPIECE;
            }else{
                device=actl->out_devices;
            }
            LOG_I("do_switch_out_devices 0x%x failed, try again switch to 0x%x",device, actl->out_devices);
            switch_device_route(actl,device,false);
        }
    }

    if(AUDIO_DEVICE_OUT_ALL_SCO&device){
        //enable bt src after turn on bt_sco
        set_vbc_bt_src_unlock(actl,actl->param_res.bt_infor.samplerate);
    }

    if(AUDIO_DEVICE_OUT_ALL_USB&device){
        if(NULL !=actl->route.devices_route.pre_in_ctl){
            apply_mixer_control(&(actl->route.devices_route.pre_in_ctl->ctl_off), "Route OFF");
            actl->route.devices_route.pre_in_ctl =NULL;
        }
    }

    return ret;
}

static int do_switch_in_devices(struct audio_control *actl,AUDIO_HW_APP_T audio_app_type,audio_devices_t device)
{
    // open fm record route
    if(audio_app_type==AUDIO_HW_APP_FM_RECORD){
        device=AUDIO_DEVICE_IN_FM_TUNER;
        LOG_I("do_switch_in_devices in_devices:%x",device);
    }
    return switch_device_route(actl,device,true);
}
static void *stream_routing_thread_entry(void *param)
{
    struct audio_control *actl = (struct audio_control *)param;
    struct listnode *item;

    setpriority(PRIO_PROCESS, 0, ANDROID_PRIORITY_AUDIO);

    prctl(PR_SET_NAME, (unsigned long)"Audio Routing Thread", 0, 0, 0);

   while(!actl->routing_mgr.is_exit)
    {
        struct switch_device_cmd *cmd = NULL;

        pthread_mutex_lock(&actl->cmd_lock);
         if (list_empty(&actl->switch_device_cmd_list)) {
            LOG_D(" switch_device_cmd_list is empty,wait");
            pthread_mutex_unlock(&actl->cmd_lock);
            sem_wait(&actl->routing_mgr.device_switch_sem);
            continue;
        }

        item = list_head(&actl->switch_device_cmd_list);
        cmd = node_to_item(item, struct switch_device_cmd, node);
        list_remove(item);
        pthread_mutex_unlock(&actl->cmd_lock);
        LOG_D("stream_routing_thread_entry process cmd in ");

         switch(cmd->cmd) {
         case SWITCH_DEVICE:
            do_select_device(actl,cmd->audio_app_type,cmd->device,cmd->is_in,cmd->update_param,cmd->is_force);
            break;
        case SET_FM_VOLUME:
            set_audioparam(actl,PARAM_FM_VOLUME_CHANGE, &cmd->param1,false);
            break;
         default:
            break;
         }
        if (cmd->is_sync)
            sem_post(&cmd->sync_sem);
        else
            free(cmd);
         LOG_D("stream_routing_thread_entry process cmd out ");
    }

     pthread_mutex_lock(&actl->cmd_lock);
     while (!list_empty(&actl->switch_device_cmd_list)) {
        struct switch_device_cmd *cmd = NULL;

        item = list_head(&actl->switch_device_cmd_list);
        cmd = node_to_item(item, struct switch_device_cmd, node);
        list_remove(item);
        pthread_mutex_unlock(&actl->cmd_lock);
        if (cmd->is_sync)
            sem_post(&cmd->sync_sem);
        else
            free(cmd);
    }
    pthread_mutex_unlock(&actl->cmd_lock);
    sem_destroy(&actl->routing_mgr.device_switch_sem);
    return NULL;
}

static void *audio_event_thread_entry(void *param)
{
    struct audio_control *actl = (struct audio_control *)param;
    struct listnode *item;

    setpriority(PRIO_PROCESS, 0, ANDROID_PRIORITY_AUDIO);

    prctl(PR_SET_NAME, (unsigned long)"Audio Routing Thread", 0, 0, 0);

    while(!actl->audio_event_mgr.is_exit){
        struct audio_event_cmd *cmd = NULL;

        pthread_mutex_lock(&actl->audio_event_lock);
        if (list_empty(&actl->audio_event_list)) {
            LOG_D("audio_event_thread_entry is empty,wait");
            pthread_mutex_unlock(&actl->audio_event_lock);
            sem_wait(&actl->audio_event_mgr.device_switch_sem);
            continue;
        }

        item = list_head(&actl->audio_event_list);
        cmd = node_to_item(item, struct audio_event_cmd, node);
        list_remove(item);
        pthread_mutex_unlock(&actl->audio_event_lock);
        LOG_D("audio_event_thread_entry process cmd in ");

        switch(cmd->cmd) {
            case SET_OUTPUT_DEVICE:
                adev_out_apm_devices_check(actl->adev,cmd->device);
                adev_out_devices_check(actl->adev,cmd->device);
                select_devices_new(actl,AUDIO_HW_APP_INVALID,cmd->device,false,true,false,false);
            break;
                default:
            break;
        }
        free(cmd);
        LOG_D("audio_event_thread_entry process cmd out ");
    }

    pthread_mutex_lock(&actl->audio_event_lock);
    while (!list_empty(&actl->audio_event_list)) {
        struct audio_event_cmd *cmd = NULL;

        item = list_head(&actl->audio_event_list);
        cmd = node_to_item(item, struct audio_event_cmd, node);
        list_remove(item);
        pthread_mutex_unlock(&actl->audio_event_lock);
        free(cmd);
    }
    pthread_mutex_unlock(&actl->audio_event_lock);

    sem_destroy(&actl->audio_event_mgr.device_switch_sem);
    return NULL;
}

static int audio_event_manager_create(struct audio_control *actl)
{
    int ret;
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);

    actl->audio_event_mgr.is_exit = false;
    /* init semaphore to signal thread */
    ret = sem_init(&actl->audio_event_mgr.device_switch_sem, 0, 0);
    if (ret) {
        LOG_E("sem_init falied, code is %s", strerror(errno));
        pthread_attr_destroy(&attr);
        return ret;
    }
    list_init(&actl->audio_event_list);
    /* create a thread to manager the device routing switch.*/
    ret = pthread_create(&actl->audio_event_mgr.routing_switch_thread, &attr,
                         audio_event_thread_entry, (void *)actl);

    if (ret) {
        LOG_E("audio_event_manager_create falied, code is %s", strerror(errno));
        sem_destroy(&actl->audio_event_mgr.device_switch_sem);
    }else{
        LOG_I("%s pthread_create:%p",__func__,&actl->audio_event_mgr.routing_switch_thread);
    }
    pthread_attr_destroy(&attr);
    return ret;
}

static void audio_event_manager_close(struct audio_control *actl)
{
    actl->audio_event_mgr.is_exit = true;
    sem_post(&actl->audio_event_mgr.device_switch_sem);
    pthread_join(actl->audio_event_mgr.routing_switch_thread, NULL);
    pthread_mutex_destroy(&actl->audio_event_lock);
}

static void * create_audio_event(struct audio_control *actl,AUDIO_SET_CMD_T cmd_param,int param1)
{
    struct audio_event_cmd *cmd = (struct audio_event_cmd *)calloc(1,
                            sizeof(struct audio_event_cmd));
    if(NULL==cmd){
        return NULL;
    }
    pthread_mutex_lock(&actl->audio_event_lock);
    cmd->cmd = cmd_param;
    if(SET_OUTPUT_DEVICE==cmd_param){
        cmd->device = param1;
    }
    list_add_tail(&actl->audio_event_list, &cmd->node);
    pthread_mutex_unlock(&actl->audio_event_lock);
    sem_post(&actl->audio_event_mgr.device_switch_sem);
    return cmd;
}

int do_set_output_device(struct audio_control *actl,audio_devices_t device)
{
    struct audio_event_cmd *cmd=(struct audio_event_cmd *)create_audio_event(actl,SET_OUTPUT_DEVICE,device);
    if(NULL==cmd){
        LOG_W("%s faile",__func__);
        return -1;
    }
    return 0;
}

int apply_private_control(struct private_control *pri, const char *priv_name)
{
    unsigned int i=0;
    LOG_I("apply_private_control [%s]", priv_name);
    for (i = 0; i < pri->size; i++) {
        if (strcmp(pri->priv[i].name, priv_name) == 0) {
            apply_mixer_control(&(pri->priv[i]),priv_name);
            return 0;
        }
    }
    LOG_E("Can not find the Private control :%s", priv_name);
    return -1;
}

int apply_dsploop_control(struct private_dsploop_control *dsploop_ctl, int type,int rate,int mode)
{
    int i=0;
    LOG_I("apply_dsploop_control type:%d mode:%d", type, mode);
    struct device_control dev_ctl;
    for ( i = 0; i < dsploop_ctl->size; i++) {
        if(type==dsploop_ctl->dsp_ctl[i].type){
            if((dsploop_ctl->dsp_ctl[i].rate!=-1)
                &&(rate!=dsploop_ctl->dsp_ctl[i].rate)){
                continue;
            }
            if ((dsploop_ctl->dsp_ctl[i].mode != -1)
                &&(mode != dsploop_ctl->dsp_ctl[i].mode)) {
                continue;
            }
            dev_ctl.ctl_size=dsploop_ctl->dsp_ctl[i].ctl_size;
            dev_ctl.ctl=dsploop_ctl->dsp_ctl[i].ctl;
            dev_ctl.name=NULL;
            apply_mixer_control(&dev_ctl,"dsploop");
            return 0;
        }
    }
    LOG_E("Can not find the dsploop control type:%x", type);
    return -1;
}

int set_vbc_dump_control(struct audio_control *ctl, const char * dump_name, bool on){
    struct device_route *cur = NULL;
    int ret=0;
    cur = get_device_route_withname(&(ctl->route.vbc_pcm_dump), dump_name);

    if(NULL == cur){
        LOG_I("set_vbc_dump_control not fined iis ctl :%s",dump_name);
        ret=-1;
        goto out;
    }

    if(true==on){
        apply_mixer_control(&cur->ctl_on, "vbc_iis on");
    }else{
        apply_mixer_control(&cur->ctl_off, "vbc_iis off");
    }

out:
    return ret;
}

int switch_vbc_iis_route(struct audio_control *ctl,USECASE uc,bool on){
    struct device_route *cur = NULL;
    int ret=0;
    switch(uc){
        case UC_CALL:
            cur = get_device_route_withname(&(ctl->route.vbc_iis), "voice_playback");
            break;
        case UC_VOIP:
            cur = get_device_route_withname(&(ctl->route.vbc_iis), "voip");
            break;
        case UC_FM:
            cur = get_device_route_withname(&(ctl->route.vbc_iis), "fm");
            break;
        case UC_DEEP_BUFFER_PLAYBACK:
        case UC_OFFLOAD_PLAYBACK:
        case UC_NORMAL_PLAYBACK:
        case UC_MMAP_PLAYBACK:
        case UC_FAST_PLAYBACK:
        case UC_VOICE_TX:
            cur = get_device_route_withname(&(ctl->route.vbc_iis), "playback");
            break;
        case UC_RECOGNITION:
        case UC_MM_RECORD:
        case UC_MMAP_RECORD:
            cur = get_device_route_withname(&(ctl->route.vbc_iis), "record");
            break;
        case UC_BT_RECORD:
            cur = get_device_route_withname(&(ctl->route.vbc_iis), "bt_record");
            break;
        case UC_LOOP:
            cur = get_device_route_withname(&(ctl->route.vbc_iis), "dsploop");
            break;
        default:
            break;
    }

    if(NULL == cur){
        LOG_I("switch_vbc_iis_route not fined iis ctl usecase:0x%x:",uc);
        ret=-1;
        goto out;
    }

    if(true==on){
        apply_mixer_control(&cur->ctl_on, "vbc_iis on");
    }else{
        apply_mixer_control(&cur->ctl_off, "vbc_iis off");
    }

out:
    return 0;
}

static int set_vbc_usecase_ctl(struct audio_control *ctl,int uc,int device){
    struct device_route *cur = NULL;

    if(uc&UC_CALL){
        cur = get_usecase_device_ctl(&(ctl->route.usecase_ctl), "voice_playback",device);
    }else if (uc&UC_VOIP){
        cur = get_usecase_device_ctl(&(ctl->route.usecase_ctl), "voip",device);
    }else if (uc&UC_LOOP){
        cur = get_usecase_device_ctl(&(ctl->route.usecase_ctl), "dsploop",device);
    }

    if(NULL != cur){
        apply_mixer_control(&cur->ctl_on, "usecase_device");
    }
    return 0;
}

static int vbc_iis_loop_enable(struct audio_control *ctl,bool on){
    int ret=0;
#ifdef NORMAL_AUDIO_PLATFORM
    return ret;
#else
    if(AUD_REALTEK_CODEC_TYPE == ctl->codec_type){
        LOG_I("do nothing vbc_iis_loop_enable\n");
        return 0;
    }
//    pthread_mutex_lock(&ctl->lock);
    if(NULL == ctl->vbc_iis_loop){
        ctl->vbc_iis_loop= mixer_get_ctl_by_name(ctl->mixer, "VBC IIS Master Setting");
    }

    if(NULL == ctl->vbc_iis_loop){
        LOG_E("vbc_iis_loop_enable:%d Failed,vbc_iis_loop is null\n",on);
        ret=-1;
        goto out;
    }

    ret = device_access_enable(ctl);
        if(ret) {
            LOG_E("vbc_iis_loop_enable:%d device_access_enable failed\n",on);
            ret=-1;
            goto out;
        }

    if(true==on){
        ret = mixer_ctl_set_enum_by_string(ctl->vbc_iis_loop,"loop");
    }else{
        ret = mixer_ctl_set_enum_by_string(ctl->vbc_iis_loop,"disable_loop");
    }

    device_access_restore(ctl);

    if (ret != 0) {
        LOG_E("vbc_iis_loop_enable:%d Failed \n",on);
        ret=-1;
    }else{
        LOG_I("vbc_iis_loop_enable:%d",on);
    }
out:
//    pthread_mutex_unlock(&ctl->lock);
#endif
    return ret;
}

static int vbc_iis_digital_top_enable(struct audio_control *ctl,bool on){
    int ret=0;
#ifdef NORMAL_AUDIO_PLATFORM
    return ret;
#else

    if(AUD_REALTEK_CODEC_TYPE == ctl->codec_type){

        if(NULL == ctl->vbc_iis_loop){
            ctl->vbc_iis_loop= mixer_get_ctl_by_name(ctl->mixer, "VBC IIS Master Setting");
        }

        if(NULL == ctl->vbc_iis_loop){
            LOG_E("vbc_iis_digital_top_enable:%d Failed,vbc_iis_loop is null\n",on);
            ret=-1;
            goto out;
        }

        ret = device_access_enable(ctl);
        if (ret) {
            LOG_E("vbc_iis_loop_enable:%d device_access_enable failed\n",on);
            ret=-1;
            goto out;
        }

        if(true==on){
            ret = mixer_ctl_set_enum_by_string(ctl->vbc_iis_loop,"iis0");
        }else{
            ret = mixer_ctl_set_enum_by_string(ctl->vbc_iis_loop,"disable_iis0");
        }

        device_access_restore(ctl);

        if (ret != 0) {
            LOG_E("vbc_iis_digital_top_enable:%d Failed \n",on);
            ret=-1;
        }else{
            LOG_D("vbc_iis_digital_top_enable:%d",on);
        }
        return 0;
    }

out:
    return ret;
#endif
}

int set_fm_volume(struct audio_control *ctl, int volume)
{
    struct device_usecase_gain *dg_gain=NULL;
    int ret = -1;
    pthread_mutex_lock(&ctl->lock);
    dg_gain=&(ctl->adev->dev_ctl->dg_gain);
    ctl->fm_volume=volume;
    if(ctl->fm_mute || ctl->fm_policy_mute) {
        LOG_I("set_fm_volume ,fm_mute:%d,ctl->fm_mute_l:%d,ctl->fm_policy_mute:%d", ctl->fm_mute, ctl->fm_mute_l, ctl->fm_policy_mute);
        pthread_mutex_unlock(&ctl->lock);
        return 0;
    }
    if(false==is_usecase_unlock(ctl,UC_FM)){
        LOG_I("set_fm_volume line:%d",__LINE__);
        pthread_mutex_unlock(&ctl->lock);
        return 0;
    }
    ret = select_devices_set_fm_volume(ctl, volume, false);
    pthread_mutex_unlock(&ctl->lock);

    return ret;
}

int set_fm_mute(struct audio_control *dev_ctl, bool mute)
{
    int ret = -1;
    pthread_mutex_lock(&dev_ctl->lock);
    LOG_I("%s, mute:%d", __func__, mute);

    if(mute) {
        int volume = 0;
        dev_ctl->fm_mute = true;
        if(false==is_usecase_unlock(dev_ctl,UC_FM)){
            pthread_mutex_unlock(&dev_ctl->lock);
            return 0;
        }
        ret = set_audioparam_unlock(dev_ctl,PARAM_FM_VOLUME_CHANGE, &volume, false);
    } else {
        dev_ctl->fm_mute = false;
        if(false==is_usecase_unlock(dev_ctl,UC_FM)){
            pthread_mutex_unlock(&dev_ctl->lock);
            return 0;
        }
        if(!dev_ctl->fm_policy_mute) {
            ret = set_audioparam_unlock(dev_ctl,PARAM_FM_VOLUME_CHANGE, &dev_ctl->fm_volume, false);
        }
    }
    pthread_mutex_unlock(&dev_ctl->lock);
    return ret;
}

int set_fm_speaker(struct audio_control *actl,bool on){
    LOG_I("fm_set_speaker:%d",on);
    pthread_mutex_lock(&actl->lock);
    actl->set_fm_speaker = on;
    pthread_mutex_unlock(&actl->lock);
    return 0;
}

int set_fm_prestop(struct audio_control *actl,bool on){
    LOG_I("fm_set_prestop:%d",on);
    pthread_mutex_lock(&actl->lock);
    actl->fm_pre_stop = on;
    pthread_mutex_unlock(&actl->lock);
    return 0;
}

int set_fm_policy_mute(struct audio_control *dev_ctl,bool mute){
    int ret = -1;
    pthread_mutex_lock(&dev_ctl->lock);
    LOG_I("%s, mute:%d", __func__, mute);

    if(mute) {
        int volume = 0;
        dev_ctl->fm_policy_mute = true;
        if(false==is_usecase_unlock(dev_ctl,UC_FM)){
            pthread_mutex_unlock(&dev_ctl->lock);
            return 0;
        }
        ret = set_audioparam_unlock(dev_ctl,PARAM_FM_VOLUME_CHANGE, &volume, false);
    } else {
        dev_ctl->fm_policy_mute = false;
        if(false==is_usecase_unlock(dev_ctl,UC_FM)){
            pthread_mutex_unlock(&dev_ctl->lock);
            return 0;
        }
        if(!dev_ctl->fm_mute) {
            ret = set_audioparam_unlock(dev_ctl,PARAM_FM_VOLUME_CHANGE, &dev_ctl->fm_volume, false);
        }
    }
    pthread_mutex_unlock(&dev_ctl->lock);
    return ret;
}


static int fm_mute_check_unlock(struct audio_control *actl,audio_devices_t device) {
    if(! (actl->usecase & UC_FM)) {
        return -1;
    }
    if(actl->fm_mute_l) {
        return 0;
    }
    if(actl->fm_pre_stop || ((actl->set_fm_speaker) && (!(device & AUDIO_DEVICE_OUT_SPEAKER)))
                || ((!actl->set_fm_speaker) && (!((device & AUDIO_DEVICE_OUT_WIRED_HEADSET)
                || (device & AUDIO_DEVICE_OUT_WIRED_HEADPHONE)
                || (device & AUDIO_DEVICE_OUT_USB_HEADSET))))
                || (device !=actl->out_devices)) {
        set_fm_dg_param(actl,actl->param_res.cur_fm_dg_id,0);
        actl->fm_mute_l = true;
        LOG_I("fm_mute_check_unlock  :%d,cur_out:%x,adev->set_fm_speaker:%d,fm_policy_mute:%d",actl->fm_mute_l,device,actl->set_fm_speaker,actl->fm_policy_mute);
    }
    return 0;
}

static int fm_unmute_check_unlock(struct audio_control *actl,audio_devices_t device) {
    if(! (actl->usecase & UC_FM)) {
        return -1;
    }
    if(!actl->fm_mute_l) {
        return -1;
    }
    if(!(actl->fm_pre_stop || ((actl->set_fm_speaker) && (!(device & AUDIO_DEVICE_OUT_SPEAKER)))
            || ((!actl->set_fm_speaker) && (!((device & AUDIO_DEVICE_OUT_WIRED_HEADSET)
            || (device & AUDIO_DEVICE_OUT_WIRED_HEADPHONE)
            || (device & AUDIO_DEVICE_OUT_USB_HEADSET)))))) {
        if(actl->fm_mute_l == true){
            actl->fm_mute_l = false;
            if((actl->fm_mute == false) &&  actl->fm_volume && (actl->fm_policy_mute == false)){
                if(actl->fm_cur_param_id != actl->param_res.cur_fm_dg_id) {
                    set_fm_dg_param(actl,actl->fm_cur_param_id,actl->fm_volume);
                } else {
                    set_fm_dg_param(actl,actl->param_res.cur_fm_dg_id,actl->fm_volume);
                }
                LOG_I("fm_unmute_check_unlock :%d,cur_out:%x,adev->set_fm_speaker:%d,fm_policy_mute:%d",actl->fm_mute_l,device,actl->set_fm_speaker, actl->fm_policy_mute);
            }
        }
    }
    return 0;
}


bool is_usecase_unlock(struct audio_control *actl, int usecase)
{
    bool on=false;
    if (actl->usecase & usecase) {
        on=true;
    } else {
        on=false;
    }
    return on;
}

int dev_ctl_iis_set(struct audio_control *ctl, int usecase,int on){
    struct audio_control *dev_ctl = ctl;

    if(on) {
        switch(usecase) {
            case UC_CALL:
                if(false==is_usecase_unlock(dev_ctl,UC_CALL)) {
                    vbc_iis_loop_enable(dev_ctl,false);
                    if((true==is_usecase_unlock(dev_ctl,UC_BT_RECORD))
                        ||(ctl->out_devices&AUDIO_DEVICE_OUT_ALL_SCO)){
                        LOG_I("VBC need switch to bt");
                    }else{
                        switch_vbc_iis_route(dev_ctl,UC_CALL,true);
                    }
                }
            break;
            case UC_VOIP_RECORD:
                    vbc_iis_loop_enable(dev_ctl,false);
                    break;
            case UC_VOIP:
                if((false==is_usecase_unlock(dev_ctl,UC_CALL))
                    &&(false==is_voip_active(dev_ctl->usecase))){
                    switch_vbc_iis_route(dev_ctl,UC_VOIP,true);
                    if(true==is_usecase_unlock(dev_ctl,UC_MM_RECORD|UC_VOIP_RECORD|UC_RECOGNITION)){
                        vbc_iis_loop_enable(dev_ctl,false);
                    }else{
                        vbc_iis_loop_enable(dev_ctl,true);
                        LOG_I("dev_ctl_iis_set voip_start  uc:%x",dev_ctl->usecase);
                    }
                }
            break;
            case UC_DEEP_BUFFER_PLAYBACK:
                if((false==is_usecase_unlock(dev_ctl,UC_CALL))
                    &&(false==is_voip_active(dev_ctl->usecase))) {
                        switch_vbc_iis_route(dev_ctl,UC_DEEP_BUFFER_PLAYBACK,true);
                }
                break;
            case UC_VOICE_TX:
                break;
            case UC_NORMAL_PLAYBACK:
                if((false==is_usecase_unlock(dev_ctl,UC_CALL))
                    &&(false==is_voip_active(dev_ctl->usecase))) {
                        switch_vbc_iis_route(dev_ctl,UC_NORMAL_PLAYBACK,true);
                }
                break;
            case UC_OFFLOAD_PLAYBACK:
                if((false==is_usecase_unlock(dev_ctl,UC_CALL))
                    &&(false==is_voip_active(dev_ctl->usecase))) {
                        switch_vbc_iis_route(dev_ctl,UC_OFFLOAD_PLAYBACK,true);
                }
            break;
            case UC_MMAP_PLAYBACK:
                if((false==is_usecase_unlock(dev_ctl,UC_CALL))
                    &&(false==is_voip_active(dev_ctl->usecase))) {
                        switch_vbc_iis_route(dev_ctl,UC_MMAP_PLAYBACK,true);
                }
                FALLTHROUGH_INTENDED;
            case UC_FAST_PLAYBACK:
                if((false==is_usecase_unlock(dev_ctl,UC_CALL))
                    &&(false==is_voip_active(dev_ctl->usecase))) {
                        switch_vbc_iis_route(dev_ctl,UC_FAST_PLAYBACK,true);
                }
            break;
            case UC_MMAP_RECORD:
            case UC_RECOGNITION:
            case UC_MM_RECORD:
                if((false==is_voip_active(dev_ctl->usecase))
                    &&(false==is_usecase_unlock(dev_ctl,UC_FM_RECORD))
                    &&(false==is_usecase_unlock(dev_ctl,UC_MM_RECORD))
                    &&(false==is_usecase_unlock(dev_ctl,UC_MMAP_RECORD))
                    &&(false==is_usecase_unlock(dev_ctl,UC_RECOGNITION))) {
                        vbc_iis_loop_enable(dev_ctl,false);
                        switch_vbc_iis_route(dev_ctl,UC_MM_RECORD,true);
                }
            break;
            case UC_BT_RECORD:
                    vbc_iis_loop_enable(dev_ctl,false);
                    switch_vbc_iis_route(dev_ctl,UC_BT_RECORD,true);
                    set_vbc_bt_src_unlock(dev_ctl,dev_ctl->param_res.bt_infor.samplerate);
            break;
            case UC_FM:
                if(false==is_usecase_unlock(dev_ctl,UC_CALL|UC_VOIP|UC_LOOP)) {
                    switch_vbc_iis_route(dev_ctl,UC_FM,true);
                }
            break;
            case UC_LOOP:
                if((false==is_usecase_unlock(dev_ctl,UC_CALL))
                    &&(false==is_voip_active(dev_ctl->usecase))
                    &&(false==is_usecase_unlock(dev_ctl,UC_LOOP))) {
                        switch_vbc_iis_route(dev_ctl,UC_LOOP,true);
                }
            break;
            case UC_FM_RECORD:
                if((false==is_usecase_unlock(dev_ctl,UC_CALL))
                    &&(false==is_usecase_unlock(dev_ctl,UC_FM))) {
                        switch_vbc_iis_route(dev_ctl,UC_FM,true);
                }
            break;
            default:
                LOG_D("dev_ctl_iis_set default usecase %d",usecase);
            break;

        }
    }else{
        if((usecase == UC_VOIP_RECORD) && (is_voip_active(dev_ctl->usecase))){
            vbc_iis_loop_enable(dev_ctl,true);
        }

        if((usecase == UC_BT_RECORD)
            && (0==(dev_ctl->out_devices&AUDIO_DEVICE_OUT_ALL_SCO))){

            /* disable src before disable bt record*/
            LOG_I("trun of bt record devices");
            set_vbc_bt_src_unlock(dev_ctl,48000);
        }
    }

    return 0;
}

static int clear_playback_param_state(struct audio_param_res *param_res){
    param_res->cur_playback_dg_id=0xff;
#ifdef SPRD_AUDIO_SMARTAMP
    param_res->cur_dsp_smartamp_id=0xff;
#endif
    LOG_I("clear_playback_param_state");
    return 0;
}

static int clear_fm_param_state_unlock(struct audio_param_res *param_res){
    param_res->cur_fm_dg_id=0xff;
    param_res->cur_fm_dg_volume=0xff;

    param_res->cur_vbc_id=0xff;
    LOG_I("clear_fm_param_state_unlock");
    return 0;
}

int clear_fm_param_state(struct audio_control *dev_ctl){
    struct audio_param_res  *param_res = NULL;

    pthread_mutex_lock(&dev_ctl->lock);
    param_res = &dev_ctl->param_res;
    clear_fm_param_state_unlock(param_res);
    pthread_mutex_unlock(&dev_ctl->lock);
    return 0;
}

static int clear_voice_param_state(struct audio_param_res *param_res){
    param_res->cur_voice_dg_id=0xff;
    param_res->cur_voice_dg_volume=0xff;

    param_res->cur_dsp_id=0xff;
    param_res->cur_dsp_volume=0xff;

    param_res->cur_vbc_id=0xff;

    /*clear voice playback param*/
    param_res->cur_vbc_playback_id=0xff;
    LOG_I("clear_voice_param_state");
    return 0;
}

static int clear_record_param_state(struct audio_param_res *param_res){
    param_res->cur_record_dg_id=0xff;
    param_res->cur_vbc_id=0xff;
    param_res->cur_codec_c_id=0xff;
    LOG_I("clear_record_param_state");
    return 0;
}

static int clear_all_vbc_dg_param_state(struct audio_param_res *param_res){
    param_res->cur_record_dg_id=0xff;
    param_res->cur_playback_dg_id=0xff;
    param_res->cur_voice_dg_id=0xff;
    param_res->cur_fm_dg_id=0xff;

    param_res->cur_vbc_playback_id=0xff;
    param_res->cur_vbc_id=0xff;

    LOG_I("clear_all_vbc_dg_param_state");
    return 0;
}

UNUSED_ATTR static int clear_audio_param_state(struct audio_param_res  *param_res)
{
    param_res->usecase =0;
    clear_playback_param_state(param_res);
    clear_fm_param_state_unlock(param_res);
    clear_voice_param_state(param_res);
    clear_record_param_state(param_res);

    param_res->cur_codec_p_id = 0xff ;
    param_res->cur_codec_p_volume = 0xff ;
    param_res->cur_codec_c_id = 0xff ;

    return 0;
}

void clear_smartamp_param_state(struct audio_param_res  *param_res){
    param_res->cur_dsp_smartamp_id=0xff;
}

static int switch_vbc_device_route(struct vbc_device_route_handler* route, int device,
    bool in_device)
{
    int ret=0;
    LOG_D("switch_vbc_device_route device=0x%x,in_device:%d", device,in_device);
    struct vbc_device_route *cur = NULL;

    cur = get_vbc_device_route_withdevice(route,device);

    if(NULL == cur){
        LOG_I("switch_vbc_device_route not fined the devices: 0x%x",device);
        ret=-1;
        goto out;
    }

    if(in_device) {
        if(cur == route->pre_in_ctl){
            LOG_D("switch_vbc_device_route set the same devices");
            goto out;
        }

        if(NULL !=route->pre_in_ctl){
            LOG_I("VBC IN DEVICES %s Route OFF %s Route ON",route->pre_in_ctl->name,cur->name);
            _apply_mixer_control(&cur->ctl_on,&(route->pre_in_ctl->ctl_on),&(route->pre_in_ctl->ctl_off));
        }else{
            LOG_I("VBC IN DEVICES %s Route ON",cur->name);
            apply_mixer_control(&cur->ctl_on, "Route ON");
        }
         route->pre_in_ctl=cur;
    } else {
        if(cur == route->pre_out_ctl){
            LOG_D("switch_vbc_device_route set the same devices");
            goto out;
        }

        if(NULL !=route->pre_out_ctl){
            LOG_I("VBC OUT DEVICES %s Route OFF %s Route ON",route->pre_out_ctl->name,cur->name);
            _apply_mixer_control(&cur->ctl_on,&(route->pre_out_ctl->ctl_on),&(route->pre_out_ctl->ctl_off));
        }else{
            LOG_I("VBC OUT DEVICES %s Route ON",cur->name);
            apply_mixer_control(&cur->ctl_on, "Route ON");
        }
        route->pre_out_ctl=cur;
    }
out:
    return ret;
}

int switch_vbc_route(struct audio_control *ctl,int device)
{
    bool is_in_device =false;
    int ret=0;
    if(device&AUDIO_DEVICE_BIT_IN){
        is_in_device=true;
    }
    if(!(device & (~AUDIO_DEVICE_BIT_IN))) {
        LOG_E("switch_vbc_route:device is 0 ");
        return -1;
    }
    pthread_mutex_lock(&ctl->lock);
    LOG_I("switch_vbc_route iis in,device:%x usecase:%x", device,ctl->usecase);
    ret= switch_vbc_device_route(&ctl->route.vbc_iis_mux_route,
        device,is_in_device);
    LOG_I("switch_vbc_route iis out be in,device:%x", device);
    ret= switch_vbc_device_route(&ctl->route.be_switch_route,
        device,is_in_device);
    LOG_I("switch_vbc_route be out,device:%x", device);
    pthread_mutex_unlock(&ctl->lock);
    return ret;
}

int switch_vbc_route_unlock(struct audio_control *ctl,int device)
{
    bool is_in_device =false;
    int ret=0;
    if(device&AUDIO_DEVICE_BIT_IN){
        is_in_device=true;
    }
    if(!(device & (~AUDIO_DEVICE_BIT_IN))) {
        LOG_E("switch_vbc_route_unlock:device is 0 ");
        return -1;
    }
    LOG_I("switch_vbc_route_unlock:%x usecase:%x",device,ctl->usecase);
    ret= switch_vbc_device_route(&ctl->route.vbc_iis_mux_route,
        device,is_in_device);

    ret= switch_vbc_device_route(&ctl->route.be_switch_route,
        device,is_in_device);
    return ret;
}

static void set_smartamp_mode(struct audio_control *actl){
    struct smart_amp_ctl *smartamp_ctl=&actl->smartamp_ctl;
    if((get_smartamp_support_usecase(&actl->smartamp_ctl)&actl->usecase)
        &&(0==(actl->usecase&(UC_VOIP|UC_CALL|UC_LOOP)))
        &&(AUDIO_DEVICE_OUT_SPEAKER==actl->out_devices)){

        pthread_mutex_lock(&smartamp_ctl->lock);
        if(true!=smartamp_ctl->smartamp_func_enable){
            enable_smartamp_func(actl->agdsp_ctl,true);
            smartamp_ctl->smartamp_func_enable=true;
        }
        pthread_mutex_unlock(&smartamp_ctl->lock);
    }else{
        pthread_mutex_lock(&smartamp_ctl->lock);
        if(false!=smartamp_ctl->smartamp_func_enable){
            enable_smartamp_func(actl->agdsp_ctl,false);
            smartamp_ctl->smartamp_func_enable=false;
        }
        pthread_mutex_unlock(&smartamp_ctl->lock);
        clear_smartamp_param_state(&actl->param_res);
    }
    return;
}

static void set_fbsmartamp_iv_mode(struct audio_control *ctl,bool on){
    struct device_route *cur = NULL;
    struct smart_amp_ctl *smartamp_ctl=&ctl->smartamp_ctl;

    pthread_mutex_lock(&smartamp_ctl->lock);
    if((on!=smartamp_ctl->iv_enable)
        &&(SND_AUDIO_FB_SMARTAMP_MODE==smartamp_ctl->smartamp_support_mode)){
        smartamp_ctl->iv_enable=on;
        cur = get_device_route_withname(&(ctl->route.fb_smartamp),"fbsmartamp");
        if(NULL == cur){
            LOG_I("set_fbsmartamp_iv_mode not fined fbsmartamp");
        }else{
            if(true==on){

                close_mic_control_unlock(ctl);

                LOG_I("Open SmartAmp IV SENSE");
                apply_mixer_control(&cur->ctl_on, "vbc_iis on");
            }else{
                LOG_I("Close SmartAmp IV SENSE");
                apply_mixer_control(&cur->ctl_off, "vbc_iis off");
            }
        }
    }
    pthread_mutex_unlock(&smartamp_ctl->lock);
    return;
}

static bool is_enable_fbsmartamp_iv(struct audio_control *ctl){
    struct smart_amp_ctl *smartamp_ctl=&ctl->smartamp_ctl;
    bool ret=false;
    pthread_mutex_lock(&smartamp_ctl->lock);
    ret=smartamp_ctl->iv_enable;
    pthread_mutex_unlock(&smartamp_ctl->lock);
    return ret;
}

static void set_fbsmartamp_mode(struct audio_control *actl){
    LOG_I("set_fbsmartamp_mode:%x %x %d",actl->usecase,get_smartamp_support_usecase(&actl->smartamp_ctl),actl->out_devices);
    if((get_smartamp_support_usecase(&actl->smartamp_ctl)&actl->usecase)
        &&(0==(actl->usecase&(UC_MM_RECORD |UC_BT_RECORD|UC_MMAP_RECORD|UC_RECOGNITION|UC_VOIP|UC_CALL|UC_LOOP|UC_FM
        |UC_VOICE_RECORD|UC_VOIP_RECORD)))
        &&(actl->out_devices==AUDIO_DEVICE_OUT_SPEAKER)){
        set_fbsmartamp_iv_mode(actl,true);
    }else{
        set_fbsmartamp_iv_mode(actl,false);
    }
}

static void set_audio_sense(struct tiny_audio_device *adev, int value)
{
    struct mixer_ctl *ctl1;
    /* set audio_sense */
    ctl1 = mixer_get_ctl_by_name(adev->mixer, "Audio Sense");
    if (!ctl1) {
        ALOGE("[%s] Unknown control 'Audio Sense'\n", __func__);
    } else {
        mixer_ctl_set_value(ctl1, 0, value);
        ALOGV("[%s] Set 'Audio Sense' %d\n", __func__, value);
    }
}

int set_usecase(struct audio_control *actl, int usecase, bool on)
{
    int ret = 0;
    LOG_I("set_usecase cur :0x%x usecase=0x%x  %s",actl->usecase, usecase, on ? "on" : "off");

    pthread_mutex_lock(&actl->lock);
    if(on) {
        ret = dsp_sleep_ctrl(actl->agdsp_ctl,true);
        if (0 != ret) {
            LOG_E("dsp_sleep_ctrl true failed %s", __func__);
            goto exit;
    }
        if(actl->usecase  & usecase) {
            goto exit;
        }
    }
    else {
        if(!(actl->usecase  & usecase)) {
            goto exit;
        }
    }

    if(actl->usecase  & UC_CALL){
        if((usecase==UC_BT_RECORD)&&(0==(actl->out_devices&AUDIO_DEVICE_OUT_ALL_SCO))){
            LOG_I("set_usecase UC_BT_RECORD Failed");
            goto exit;
        } else if((usecase==UC_MM_RECORD)&&(actl->out_devices&AUDIO_DEVICE_OUT_ALL_SCO)){
            LOG_I("set_usecase UC_MM_RECORD Failed");
            goto exit;
        } else if((usecase==UC_MMAP_RECORD)&&(actl->out_devices&AUDIO_DEVICE_OUT_ALL_SCO)){
            LOG_I("set_usecase UC_MMAP_RECORD Failed");
            goto exit;
        }
    }

    ret = dev_ctl_iis_set(actl, usecase, on);

    if (on) {
        actl->usecase |= usecase;
    } else {
        actl->usecase &= ~usecase;
        if((0==(actl->usecase & (UC_CALL|UC_VOIP|UC_LOOP)))&&
            (0==(actl->out_devices&AUDIO_DEVICE_OUT_ALL_SCO))){
            vbc_iis_digital_top_enable(actl,false);
        }

        if(0==(actl->usecase & (UC_CALL|UC_VOIP))){
            vbc_iis_loop_enable(actl,false);
        }

        if(((UC_CALL == usecase) ||(UC_VOIP == usecase)||(UC_LOOP == usecase))
            &&(UC_FM & actl->usecase)&&(0==((UC_CALL|UC_VOIP|UC_LOOP) & actl->usecase))){
            LOG_I("%s line:%d set vbc for fm",__func__,__LINE__);
            dev_ctl_iis_set(actl, UC_FM, true);
        }

        LOG_D("set_usecase usecase:%x",actl->usecase);
        if ((UC_FM == actl->usecase) &&
                actl->enable_fm_dspsleep) {
            ret = dsp_sleep_ctrl(actl->agdsp_ctl,false);
            if (0 != ret) {
                LOG_E("%s dsp_sleep_ctrl false failed", __func__);
                goto exit;
            }
        }
    }

    if(((UC_VOIP|UC_CALL|UC_NORMAL_PLAYBACK|UC_DEEP_BUFFER_PLAYBACK|UC_OFFLOAD_PLAYBACK|UC_MMAP_PLAYBACK|UC_FAST_PLAYBACK) &
        actl->usecase)==0){
        if(is_usbdevice_connected_unlock(actl)){
            stop_usb_channel(&actl->adev->usb_ctl,true);
        }
    }

    if(((UC_VOIP_RECORD|UC_MM_RECORD|UC_RECOGNITION|UC_MMAP_RECORD|UC_CALL) &
        actl->usecase)==0){
        if(is_usbmic_connected_unlock(actl)){
            stop_usb_channel(&actl->adev->usb_ctl,false);
        }
    }

    if(true==is_support_fbsmartamp(&actl->smartamp_ctl)){
        set_fbsmartamp_mode(actl);
    }

    if(true==is_support_smartamp(&actl->smartamp_ctl)){
        set_smartamp_mode(actl);
    }

    if(UC_UNKNOWN==actl->usecase){
#ifdef AUDIO_DEBUG
        debug_dump_stop(&actl->adev->debugdump);
#endif
        vbc_iis_digital_top_enable(actl,false);
        close_all_control_unlock(actl);
        ret = dsp_sleep_ctrl(actl->agdsp_ctl,false);
        if (0 != ret) {
            LOG_E("%s dsp_sleep_ctrl false failed", __func__);
            goto exit;
        }
    }

    if (UC_CALL & usecase) {
        set_audio_sense(actl->adev, on);
    }

    pthread_mutex_unlock(&actl->lock);

    return ret;

exit:
#ifdef AUDIO_DEBUG
    debug_dump_stop(&actl->adev->debugdump);
#endif
    pthread_mutex_unlock(&actl->lock);
    return -1;
}

bool is_usecase(struct audio_control *actl, int usecase)
{
    bool on=false;
    pthread_mutex_lock(&actl->lock);
    on = is_usecase_unlock(actl,usecase);
    pthread_mutex_unlock(&actl->lock);
    return on;
}

static int set_vbc_bt_src_unlock(struct audio_control *actl, int rate){
    int ret=0;
    if(NULL==actl->bt_dl_src){
        LOG_E("set_vbc_bt_src_unlock dl failed,can not get mixer:VBC_SRC_BT_DAC");
    }else{
        LOG_I("set_vbc_bt_src_unlock dl:%d",rate);
        ret=mixer_ctl_set_value(actl->bt_dl_src, 0, rate);
    }

    if(NULL==actl->bt_ul_src){
        LOG_E("set_vbc_bt_src_unlock ul failed,can not get mixer:VBC_SRC_BT_ADC");
    }else{
        LOG_I("set_vbc_bt_src_unlock ul:%d",rate);
        ret=mixer_ctl_set_value(actl->bt_ul_src, 0, rate);
    }
    return ret;
}

static int set_vbc_bt_src_without_lock(struct audio_control *actl, int rate){
    int ret=-ENOSYS;
    actl->param_res.bt_infor.samplerate=rate;

    if(AUDIO_DEVICE_OUT_ALL_SCO&actl->out_devices){
        ret=set_vbc_bt_src_unlock(actl,actl->param_res.bt_infor.samplerate);
    }else if((AUDIO_DEVICE_IN_ALL_SCO&((~AUDIO_DEVICE_BIT_IN)&actl->in_devices))
        &&(is_usecase_unlock(actl, UC_BT_RECORD))){
        ret=set_vbc_bt_src_unlock(actl,actl->param_res.bt_infor.samplerate);
    }
    return ret;
}

int set_vbc_bt_src(struct audio_control *actl, int rate){
    int ret=-ENOSYS;
    pthread_mutex_lock(&actl->lock);
    ret=set_vbc_bt_src_without_lock(actl,rate);
    pthread_mutex_unlock(&actl->lock);
    return ret;
}

int set_vbc_bt_nrec(struct audio_control *actl, bool nrec){
    struct dev_bluetooth_t bt_infor;
    bt_infor.bluetooth_nrec=nrec;
    return set_audioparam(actl,PARAM_BT_NREC_CHANGE,&bt_infor,false);
}

static int _set_codec_mute(struct mixer_ctrl_t *mute_ctl,bool mute){
    int ret=-1;

    if(NULL==mute_ctl){
        return -1;
    }

    if(mute==mute_ctl->value){
        LOG_D("_set_codec_mute:the same value");
        return 0;
    }

    if(NULL!=mute_ctl->mixer){
        ret=mixer_ctl_set_value(mute_ctl->mixer, 0, mute);
        if (ret != 0) {
            LOG_E("_set_codec_mute Failed :%d\n",mute);
        }
        mute_ctl->value=mute;
    }else{
        LOG_D("_set_codec_mute mute_ctl is null");
    }
    return ret;
}

int set_mic_mute(struct audio_control *actl, bool on){
    struct mute_control *mute=&actl->mute;
    LOG_D("set_mic_mute:%d %p",on,mute);
    return 0;
}

int set_codec_mute(struct audio_control *actl,bool on){
    int ret=0;
    struct mute_control *mute=&actl->mute;
    LOG_I("set_codec_mute:%d %p",on,mute);

    pthread_mutex_lock(&actl->lock);
    if(UC_UNKNOWN==actl->usecase){
        LOG_W("set_codec_mute failed");
        ret=-1;
        goto exit;
    }

    ret|=_set_codec_mute(&mute->spk_mute,on);
    ret|=_set_codec_mute(&mute->spk2_mute,on);
    ret|=_set_codec_mute(&mute->handset_mute,on);
    ret|=_set_codec_mute(&mute->headset_mute,on);

exit:
    pthread_mutex_unlock(&actl->lock);
    return ret;
}

static int init_mute_control(struct mixer *mixer,struct mute_control *mute,struct mute_control_name *mute_name){
    if(mixer==NULL){
        LOG_E("init_mute_control failed");
        return -1;
    }

    if((NULL==mute->spk_mute.mixer) && (NULL!= mute_name->spk_mute)){
        mute->spk_mute.mixer=mixer_get_ctl_by_name(mixer,mute_name->spk_mute);
        free(mute_name->spk_mute);
        mute_name->spk_mute=NULL;
    }
    mute->spk_mute.value=-1;

    if((NULL==mute->spk2_mute.mixer) && (NULL!= mute_name->spk2_mute)){
        mute->spk2_mute.mixer=mixer_get_ctl_by_name(mixer,mute_name->spk2_mute);
        free(mute_name->spk2_mute);
        mute_name->spk2_mute=NULL;
    }
    mute->spk2_mute.value=-1;

    if((NULL==mute->handset_mute.mixer) && (NULL!= mute_name->handset_mute)){
        mute->handset_mute.mixer=mixer_get_ctl_by_name(mixer,mute_name->handset_mute);
        free(mute_name->handset_mute);
        mute_name->handset_mute=NULL;
    }
    mute->handset_mute.value=-1;

    if((NULL==mute->headset_mute.mixer) && (NULL!= mute_name->headset_mute)){
        mute->headset_mute.mixer=mixer_get_ctl_by_name(mixer,mute_name->headset_mute);
        free(mute_name->headset_mute);
        mute_name->headset_mute=NULL;
    }
    mute->headset_mute.value=-1;

    if((NULL==mute->linein_mute.mixer) && (NULL!= mute_name->linein_mute)){
        mute->linein_mute.mixer=mixer_get_ctl_by_name(mixer,mute_name->linein_mute);
        free(mute_name->linein_mute);
        mute_name->linein_mute=NULL;
    }
    mute->linein_mute.value=-1;

    if((NULL==mute->dsp_da0_mdg_mute.mixer) && (NULL!= mute_name->dsp_da0_mdg_mute)){
        mute->dsp_da0_mdg_mute.mixer=mixer_get_ctl_by_name(mixer,mute_name->dsp_da0_mdg_mute);
        free(mute_name->dsp_da0_mdg_mute);
        mute_name->dsp_da0_mdg_mute=NULL;
    }
    mute->dsp_da0_mdg_mute.value=-1;

    if((NULL==mute->dsp_da1_mdg_mute.mixer) && (NULL!= mute_name->dsp_da1_mdg_mute)){
        mute->dsp_da1_mdg_mute.mixer=mixer_get_ctl_by_name(mixer,mute_name->dsp_da1_mdg_mute);
        free(mute_name->dsp_da1_mdg_mute);
        mute_name->dsp_da1_mdg_mute=NULL;
    }
    mute->dsp_da1_mdg_mute.value=-1;

    if((NULL==mute->audio_mdg_mute.mixer) && (NULL!= mute_name->audio_mdg_mute)){
        mute->audio_mdg_mute.mixer=mixer_get_ctl_by_name(mixer,mute_name->audio_mdg_mute);
        free(mute_name->audio_mdg_mute);
        mute_name->audio_mdg_mute=NULL;
    }
    mute->audio_mdg_mute.value=-1;

    if((NULL==mute->audio_mdg23_mute.mixer) && (NULL!= mute_name->audio_mdg23_mute)){
        mute->audio_mdg23_mute.mixer=mixer_get_ctl_by_name(mixer,mute_name->audio_mdg23_mute);
        free(mute_name->audio_mdg23_mute);
        mute_name->audio_mdg23_mute=NULL;
    }
    mute->audio_mdg23_mute.value=-1;

    if(NULL==mute->voice_ul_mute_ctl.mixer){
        mute->voice_ul_mute_ctl.mixer=mixer_get_ctl_by_name(mixer,VBC_UL_MUTE);
    }
    mute->voice_ul_mute_ctl.value=0;

    if(NULL==mute->voice_dl_mute_ctl.mixer){
        mute->voice_dl_mute_ctl.mixer=mixer_get_ctl_by_name(mixer,VBC_DL_MUTE);
    }
    mute->voice_dl_mute_ctl.value=0;
    return 0;
}

int set_voice_dl_mute(struct audio_control *actl, bool mute){
    int ret=-ENOSYS;
    pthread_mutex_lock(&actl->lock);
    if(is_usecase_unlock(actl,UC_CALL)){
        if(NULL==actl->mute.voice_dl_mute_ctl.mixer){
            LOG_E("set_voice_dl_mute failed,can not get mixer:VBC_DL_MUTE");
        }else{
            if(mute){
                ret=mixer_ctl_set_enum_by_string(actl->mute.voice_dl_mute_ctl.mixer, "enable");
                actl->mute.voice_dl_mute_ctl.value=1;
            }else{
                ret=mixer_ctl_set_enum_by_string(actl->mute.voice_dl_mute_ctl.mixer, "disable");
                actl->mute.voice_dl_mute_ctl.value=0;
            }
        }
    }
    pthread_mutex_unlock(&actl->lock);
    return ret;
}

int set_voice_ul_mute(struct audio_control *actl, bool mute){
    int ret=-ENOSYS;
    pthread_mutex_lock(&actl->lock);
    if(is_usecase_unlock(actl,UC_CALL)){
        if(NULL==actl->mute.voice_ul_mute_ctl.mixer){
            LOG_E("set_voice_ul_mute failed,can not get mixer:VBC_UL_MUTE");
        }else{
            if(mute){
                ret=mixer_ctl_set_enum_by_string(actl->mute.voice_ul_mute_ctl.mixer, "enable");
                actl->mute.voice_ul_mute_ctl.value=1;
            }
        }
    }
    if(!mute){
        ret=mixer_ctl_set_enum_by_string(actl->mute.voice_ul_mute_ctl.mixer, "disable");
        actl->mute.voice_ul_mute_ctl.value=0;
    }
    pthread_mutex_unlock(&actl->lock);
    return ret;
}
int stream_routing_manager_create(struct audio_control *actl)
{
    int ret;
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);

    actl->routing_mgr.is_exit = false;
    /* init semaphore to signal thread */
    ret = sem_init(&actl->routing_mgr.device_switch_sem, 0, 0);
    if (ret) {
        LOG_E("sem_init falied, code is %s", strerror(errno));
        pthread_attr_destroy(&attr);
        return ret;
    }
    list_init(&actl->switch_device_cmd_list);
    /* create a thread to manager the device routing switch.*/
    ret = pthread_create(&actl->routing_mgr.routing_switch_thread, &attr,
                         stream_routing_thread_entry, (void *)actl);
    if (ret) {
        LOG_E("stream_routing_manager_create falied, code is %s", strerror(errno));
        sem_destroy(&actl->routing_mgr.device_switch_sem);
    }else{
        LOG_I("%s pthread_create:%p",__func__,&actl->audio_event_mgr.routing_switch_thread);
    }
    pthread_attr_destroy(&attr);

    return ret;
}

void stream_routing_manager_close(struct audio_control *actl)
{
    actl->routing_mgr.is_exit = true;
    sem_post(&actl->routing_mgr.device_switch_sem);
    pthread_join(actl->routing_mgr.routing_switch_thread, NULL);
    pthread_mutex_destroy(&actl->cmd_lock);
}

static void do_select_device(struct audio_control *actl,AUDIO_HW_APP_T audio_app_type,audio_devices_t device,bool is_in,bool update_param,bool force_set)
{
    int ret=0;
    int mdg_mute = 0;

    pthread_mutex_lock(&actl->lock);
    LOG_I("do_select_device in device:%x,is_in:%d,force_set:%d,actl->usecase:%x,in:%x,out:%x",device,is_in,force_set,actl->usecase,actl->in_devices,actl->out_devices);
    if(actl->usecase==0){
        LOG_I("do_select_device usecase is 0");
        pthread_mutex_unlock(&actl->lock);
        return;
     }
     if(is_in) {
        if(device == actl->in_devices) {
            if(is_usbmic_connected_unlock(actl) &&
                ((ALL_USB_INPUT_DEVICES&~AUDIO_DEVICE_BIT_IN)&device) &&
                is_usecase_unlock(actl, UC_CALL|UC_VOIP|UC_LOOP|UC_MM_RECORD|
                    UC_VOICE_RECORD|UC_VOIP_RECORD|UC_RECOGNITION|UC_MMAP_RECORD)) {
                start_usb_channel(&actl->adev->usb_ctl,false);
            }
            pthread_mutex_unlock(&actl->lock);
            if(force_set==false){
                return;
            }
        }
     }
     else {
        if(device == actl->out_devices) {
            if(is_usbdevice_connected_unlock(actl) &&
                (ALL_USB_OUTPUT_DEVICES&device) &&
                is_usecase_unlock(actl, UC_CALL|UC_VOIP|UC_FM|UC_NORMAL_PLAYBACK|
                    UC_LOOP|UC_FAST_PLAYBACK|UC_DEEP_BUFFER_PLAYBACK|
                    UC_OFFLOAD_PLAYBACK|UC_MMAP_PLAYBACK)){
                start_usb_channel(&actl->adev->usb_ctl,true);
            }
            pthread_mutex_unlock(&actl->lock);
            if(force_set==false){
                return;
            }
        }
     }
    ret = device_access_enable(actl);
    if (ret) {
        LOG_E("do_select_device,device_access_enable failed");
        pthread_mutex_unlock(&actl->lock);
        return;
    }

    if(is_in) {
        if((true==is_enable_fbsmartamp_iv(actl))&&(false==is_record_active(actl->usecase))){
            LOG_I("%s fbsmartamp bypass mic",__func__);
            device_access_restore(actl);
            pthread_mutex_unlock(&actl->lock);
            return;
        }
        if(((AUDIO_HW_APP_VOIP_RECORD==audio_app_type) &&(actl->usecase&(UC_VOIP)))
            ||((AUDIO_HW_APP_INVALID==audio_app_type) &&(actl->usecase&(UC_VOIP)))
            ||(actl->usecase&(UC_CALL))){
            set_voice_mic(actl,audio_app_type,actl->out_devices);
            LOG_I("%s bypass audiopolicy input device in voip and call status",__func__);
            device_access_restore(actl);
            pthread_mutex_unlock(&actl->lock);
            return;
        }

        actl->in_devices=device;
        if((actl->usecase!=0)||(AUDIO_HW_APP_INVALID==audio_app_type)){
            LOG_I("do_select_device usecase:%x",actl->usecase);
            if((!((ALL_USB_INPUT_DEVICES&~AUDIO_DEVICE_BIT_IN)&device))
                &&(device!=AUDIO_DEVICE_IN_VOICE_CALL)){
                stop_usb_channel(&actl->adev->usb_ctl,false);
                switch_vbc_route_unlock(actl,device);
            }
            pthread_mutex_unlock(&actl->lock);
            pthread_mutex_lock(&actl->lock_route);
            do_switch_in_devices(actl,audio_app_type,device);
            pthread_mutex_unlock(&actl->lock_route);
            pthread_mutex_lock(&actl->lock);
            if((ALL_USB_INPUT_DEVICES&~AUDIO_DEVICE_BIT_IN)&device){
                switch_vbc_route_unlock(actl,device);
                start_usb_channel(&actl->adev->usb_ctl,false);
            }

            if(AUDIO_DEVICE_IN_ALL_SCO&((~AUDIO_DEVICE_BIT_IN)&device)){
                if(actl->adev->bt_wbs){
                    set_vbc_bt_src_without_lock(actl->adev->dev_ctl,16000);
                    LOG_D("set src bt record  rate  wbs  audio_app_type %d",audio_app_type);
                } else{
                    set_vbc_bt_src_without_lock(actl->adev->dev_ctl,8000);
                    LOG_D("set src bt record  rate  nbs  audio_app_type %d",audio_app_type);
                }
            }

            if(true==update_param){
                set_audioparam_unlock(actl,PARAM_INDEVICES_CHANGE,&device,false);
            }
        }
    }
    else {
        fm_mute_check_unlock(actl,device);
        if(!(ALL_USB_OUTPUT_DEVICES&device)){
            if((actl->out_devices & ALL_USB_OUTPUT_DEVICES) &&
                is_usecase_unlock(actl, UC_CALL|UC_VOIP)) {
                set_mdg_all_mute(actl);
                mdg_mute = 1;
                usleep(200000);
            }
            stop_usb_channel(&actl->adev->usb_ctl,true);
            if((AUDIO_DEVICE_OUT_ALL_SCO&device)&&(actl->usecase&(UC_CALL|UC_VOIP))){
                LOG_I("%s switch btsoc incall switch vbc route after mdg mute",__func__);
            }else{
                switch_vbc_route_unlock(actl,device);
            }
        }
        else {
            if((!(actl->out_devices & ALL_USB_OUTPUT_DEVICES)) &&
                is_usecase_unlock(actl, UC_CALL|UC_VOIP)) {
                set_mdg_all_mute(actl);
                usleep(200000);
                mdg_mute = 1;
            }
        }

        if((0==mdg_mute)&&(actl->out_devices!=device)&&(0==(actl->out_devices&device))&&(true==update_param)){
            set_mdg_all_mute(actl);
            mdg_mute = 1;
            if((AUDIO_DEVICE_OUT_ALL_SCO&device)&&(actl->usecase&(UC_CALL|UC_VOIP))){
                usleep(300000);
            }
        }

        if((actl->usecase!=0)||(AUDIO_HW_APP_INVALID==audio_app_type)){

            if((0==(AUDIO_DEVICE_OUT_ALL_SCO&device))
                &&(AUDIO_DEVICE_OUT_ALL_SCO&actl->out_devices)){
                //bt src need disable before turn off bt_sco
                set_vbc_bt_src_unlock(actl,48000);
            }

            if((audio_app_type==AUDIO_HW_APP_CALL) ||(AUDIO_HW_APP_VOIP==audio_app_type)
                ||((AUDIO_HW_APP_PRIMARY==audio_app_type) &&(actl->usecase&(UC_CALL|UC_VOIP)))
                ||((AUDIO_HW_APP_INVALID==audio_app_type) &&(actl->usecase&(UC_CALL|UC_VOIP)))){
                if(device&AUDIO_DEVICE_OUT_ALL_USB){
                    actl->out_devices=device;
                }
                set_voice_mic(actl,audio_app_type,device);
            }

            set_vbc_usecase_ctl(actl,actl->usecase,(int)device);

            pthread_mutex_unlock(&actl->lock);
            pthread_mutex_lock(&actl->lock_route);
            ret = do_switch_out_devices(actl,audio_app_type,device);
            pthread_mutex_unlock(&actl->lock_route);
            pthread_mutex_lock(&actl->lock);
            if(0 == ret)
                actl->out_devices=device;

            if(true==is_support_fbsmartamp(&actl->smartamp_ctl)){
                set_fbsmartamp_mode(actl);
            }

            if(true==is_support_smartamp(&actl->smartamp_ctl)){
                set_smartamp_mode(actl);
            }

            if(((AUDIO_DEVICE_OUT_ALL_SCO&device)&&(actl->usecase&(UC_CALL|UC_VOIP)))){
                switch_vbc_route_unlock(actl, device);
                usleep(100000);
                set_audioparam_unlock(actl,PARAM_OUTDEVICES_CHANGE,&device,true);
            }

            if(ALL_USB_OUTPUT_DEVICES&device){
                switch_vbc_route_unlock(actl,device);
                start_usb_channel(&actl->adev->usb_ctl,true);
            }

            if(true==update_param){
                set_audioparam_unlock(actl,PARAM_OUTDEVICES_CHANGE,&device,false);
            }
        } else {
            actl->out_devices=device;
        }
        fm_unmute_check_unlock(actl,device);
    }
    if(mdg_mute) {
        set_mdg_all_restore(actl);
    }
    device_access_restore(actl);
    LOG_I("do_select_device out device:%x,is_in:%d",device,is_in);
#ifdef AUDIO_DEBUG
    if((0!=actl->usecase)&&(UC_FAST_PLAYBACK!=actl->usecase)){
        debug_dump_start(&actl->adev->debugdump,DEVICE_CHANGE_REG_DUMP_COUNT);
    }
#endif
    pthread_mutex_unlock(&actl->lock);
}

static void  *select_device_cmd_send(struct audio_control *actl,AUDIO_HW_APP_T audio_app_type,SWITCH_DEVICE_CMD_T cmd_param, audio_devices_t device,
        bool is_in,bool update_param, bool sync, bool force,int param1)
{
     struct switch_device_cmd *cmd = (struct switch_device_cmd *)calloc(1,
                                    sizeof(struct switch_device_cmd));
    if(NULL==cmd){
        return cmd;
    }

    pthread_mutex_lock(&actl->cmd_lock);
    sem_init(&cmd->sync_sem, 0, 0);
    if (sync) {
        cmd->is_sync = 1;
    } else {
        cmd->is_sync = 0;
    }
    cmd->cmd = cmd_param;
    cmd->is_force = force;
    cmd->audio_app_type = audio_app_type;
    cmd->device = device;
    cmd->is_in = is_in;
    cmd->update_param = update_param;
    cmd->param1 = param1;
    LOG_D("cmd is %d, audio_app_type is %d, device is %#x", cmd->cmd, cmd->audio_app_type, cmd->device);
    list_add_tail(&actl->switch_device_cmd_list, &cmd->node);
    pthread_mutex_unlock(&actl->cmd_lock);
    sem_post(&actl->routing_mgr.device_switch_sem);

    return cmd;
}


int enable_dspsleep_for_fm(struct audio_control *actl,bool on_off)
{
    int ret = -1;
    pthread_mutex_lock(&actl->lock);
    if(actl->usecase == UC_FM) {
         LOG_I("enable_dspsleep_for_fm:on_off:%d",on_off);
         if(on_off) {
            actl->enable_fm_dspsleep= true;
            ret = dsp_sleep_ctrl(actl->agdsp_ctl,false);
        }
        else {
            actl->enable_fm_dspsleep= false;
            ret = dsp_sleep_ctrl(actl->agdsp_ctl,true);
        }
    }
    pthread_mutex_unlock(&actl->lock);
    return ret;
}

int disable_codec_dig_access(struct audio_control *actl, bool on_off){
    int ret = -1;
    pthread_mutex_lock(&actl->lock);
    if(actl->agdsp_ctl->codec_dig_access_dis) {
        ret = mixer_ctl_set_value(actl->agdsp_ctl->codec_dig_access_dis, 0, on_off);
        if (ret != 0) {
            LOG_E("%s, codec_dig_access_dis Failed\n", __func__);
        }
    }
    pthread_mutex_unlock(&actl->lock);

    return ret;
}

int select_devices_new(struct audio_control *actl, int audio_app_type, audio_devices_t device, bool is_in,
            bool update_param, bool sync, bool force)
{

    struct switch_device_cmd *cmd = NULL;

    LOG_I("select_devices_new devices 0x%x, is in %d app type:%d sync is %d,force:%d", device, is_in,audio_app_type, sync,force);

    cmd = (struct switch_device_cmd *)select_device_cmd_send(actl,(AUDIO_HW_APP_T)audio_app_type,SWITCH_DEVICE,
        device, is_in, update_param, sync, force,0);
    if (sync) {
        sem_wait(&cmd->sync_sem);
        free(cmd);
    }

    return 0;
}

static int select_devices_set_fm_volume(struct audio_control *actl, int volume, bool sync)
{
    struct switch_device_cmd *cmd = NULL;
    cmd = (struct switch_device_cmd *)select_device_cmd_send(actl,AUDIO_HW_APP_FM,SET_FM_VOLUME, 0, 0, 0,sync,0,volume);
    if(NULL==cmd){
        LOG_W("%s faile",__func__);
        return -1;
    }

    if (sync) {
        sem_wait(&cmd->sync_sem);
        free(cmd);
    }
    return 0;
}


int dev_ctl_get_in_pcm_config(struct audio_control *dev_ctl, int app_type, int * dev, struct pcm_config * config)
{
    int pcm_devices = 0;
    int ret = 0;

    if(config == NULL){
        return -1;
    }
    switch(app_type){
        case AUDIO_HW_APP_CALL_RECORD:
            LOG_I("dev_ctl_get_in_pcm_config AUDIO_HW_APP_CALL_RECORD");
            pcm_devices=dev_ctl->pcm_handle.record_devices[AUD_RECORD_PCM_VOICE_RECORD];
            memcpy(config, &dev_ctl->pcm_handle.record[AUD_RECORD_PCM_VOICE_RECORD], sizeof(struct pcm_config));
            break;
        case AUDIO_HW_APP_VOIP_RECORD:
            LOG_I("dev_ctl_get_in_pcm_config AUDIO_HW_APP_VOIP_RECORD");
            pcm_devices=dev_ctl->pcm_handle.record_devices[AUD_RECORD_PCM_VOIP];
            memcpy(config, &dev_ctl->pcm_handle.record[AUD_RECORD_PCM_VOIP], sizeof(struct pcm_config));
            config->period_size=1280;
            break;
        case AUDIO_HW_APP_FM_RECORD:
            LOG_I("dev_ctl_get_in_pcm_config AUDIO_HW_APP_FM_RECORD");
            pcm_devices=dev_ctl->pcm_handle.record_devices[AUD_RECORD_PCM_FM];
            memcpy(config, &dev_ctl->pcm_handle.record[AUD_RECORD_PCM_FM], sizeof(struct pcm_config));
            break;
        case AUDIO_HW_APP_BT_RECORD:
            LOG_I("dev_ctl_get_in_pcm_config AUDIO_HW_APP_BT_RECORD");
            pcm_devices=dev_ctl->pcm_handle.record_devices[AUD_RECORD_PCM_BT_RECORD];
            memcpy(config, &dev_ctl->pcm_handle.record[AUD_RECORD_PCM_BT_RECORD], sizeof(struct pcm_config));
            break;
        case AUDIO_HW_APP_NORMAL_RECORD:
            LOG_I("dev_ctl_get_in_pcm_config AUDIO_HW_APP_NORMAL_RECORD");
            pcm_devices=dev_ctl->pcm_handle.record_devices[AUD_RECORD_PCM_NORMAL];
            memcpy(config, &dev_ctl->pcm_handle.record[AUD_RECORD_PCM_NORMAL], sizeof(struct pcm_config));
            break;
        case AUDIO_HW_APP_MMAP_RECORD:
            LOG_I("dev_ctl_get_in_pcm_config AUDIO_HW_APP_MMAP_RECORD");
            pcm_devices=dev_ctl->pcm_handle.record_devices[AUD_RECORD_PCM_MMAP];
            memcpy(config, &dev_ctl->pcm_handle.record[AUD_RECORD_PCM_MMAP], sizeof(struct pcm_config));
            break;
        case AUDIO_HW_APP_RECOGNITION:
            LOG_I("dev_ctl_get_in_pcm_config AUDIO_HW_APP_RECOGNITION");
            pcm_devices=dev_ctl->pcm_handle.record_devices[AUD_RECORD_PCM_RECOGNITION];
            memcpy(config, &dev_ctl->pcm_handle.record[AUD_RECORD_PCM_RECOGNITION], sizeof(struct pcm_config));
            break;
        case AUDIO_HW_APP_DSP_LOOP:
            LOG_I("dev_ctl_get_in_pcm_config AUDIO_HW_APP_DSP_LOOP");
            pcm_devices=dev_ctl->pcm_handle.record_devices[AUD_RECORD_PCM_DSP_LOOP];
            memcpy(config, &dev_ctl->pcm_handle.record[AUD_RECORD_PCM_DSP_LOOP], sizeof(struct pcm_config));
            break;
        default:
            LOG_E("dev_ctl_get_in_pcm_config stream type:0x%x",app_type);
            ret = -1;
            break;
    }
    *dev = pcm_devices;
    return ret;
}


int dev_ctl_get_out_pcm_config(struct audio_control *dev_ctl,int app_type, int * dev, struct pcm_config *config)
{
    int pcm_devices = 0;
    int ret = 0;
    if(config == NULL){
        return -1;
    }
     switch(app_type){
        case AUDIO_HW_APP_CALL:
            pcm_devices=dev_ctl->pcm_handle.playback_devices[AUD_PCM_MODEM_DL];
            memcpy(config, &dev_ctl->pcm_handle.play[AUD_PCM_MODEM_DL], sizeof(struct pcm_config));
            LOG_I("dev_ctl_get_out_pcm_config UC_CALL:%p %d %d",config,config->period_count,config->period_size);
            break;
        case AUDIO_HW_APP_VOIP:
            pcm_devices=dev_ctl->pcm_handle.playback_devices[AUD_PCM_VOIP];
            memcpy(config, &dev_ctl->pcm_handle.play[AUD_PCM_VOIP], sizeof(struct pcm_config));
            config->period_size=1280;
            LOG_I("dev_ctl_get_out_pcm_config AUD_PCM_VOIP:%p %d %d",config,config->period_count,config->period_size);
            break;
        case AUDIO_HW_APP_PRIMARY:
            pcm_devices=dev_ctl->pcm_handle.playback_devices[AUD_PCM_MM_NORMAL];
            memcpy(config, &dev_ctl->pcm_handle.play[AUD_PCM_MM_NORMAL], sizeof(struct pcm_config));
#ifdef AUDIO_24BIT_PLAYBACK_SUPPORT
            if (dev_ctl->config.support_24bits)
                config->format =  PCM_FORMAT_S16_LE;
#endif
            LOG_I("dev_ctl_get_out_pcm_config AUD_PCM_MM_NORMAL:%p %d %d",config,config->period_count,config->period_size);
            break;
        case AUDIO_HW_APP_FAST:
            pcm_devices=dev_ctl->pcm_handle.playback_devices[AUD_PCM_FAST];
            memcpy(config, &dev_ctl->pcm_handle.play[AUD_PCM_FAST], sizeof(struct pcm_config));
            LOG_I("dev_ctl_get_out_pcm_config AUD_PCM_FAST:%p %d %d",config,config->period_count,config->period_size);
            break;
        case AUDIO_HW_APP_DEEP_BUFFER:
            pcm_devices=dev_ctl->pcm_handle.playback_devices[AUD_PCM_DEEP_BUFFER];
            memcpy(config, &dev_ctl->pcm_handle.play[AUD_PCM_DEEP_BUFFER], sizeof(struct pcm_config));
            LOG_I("dev_ctl_get_out_pcm_config AUDIO_HW_APP_DEEP_BUFFER:%p %d %d",config,config->period_count,config->period_size);
            break;
        case AUDIO_HW_APP_MMAP:
            pcm_devices=dev_ctl->pcm_handle.playback_devices[AUD_PCM_MMAP_NOIRQ];
            memcpy(config, &dev_ctl->pcm_handle.play[AUD_PCM_MMAP_NOIRQ], sizeof(struct pcm_config));
            LOG_I("dev_ctl_get_out_pcm_config AUDIO_HW_APP_MMAP:%p %d %d",config,config->period_count,config->period_size);
            break;
        case AUDIO_HW_APP_VOICE_TX:
            pcm_devices=dev_ctl->pcm_handle.playback_devices[AUD_PCM_VOICE_TX];
            memcpy(config, &dev_ctl->pcm_handle.play[AUD_PCM_VOICE_TX], sizeof(struct pcm_config));
            LOG_I("dev_ctl_get_out_pcm_config VOICE_TX:%p %d %d",config,config->period_count,config->period_size);
            break;
        default:
            LOG_E("dev_ctl_get_out_pcm_config stream type:0x%x",app_type);
            ret = -1;
            break;
    }
    *dev = pcm_devices;
    return ret;
}

 int set_offload_volume( struct audio_control *dev_ctl, float left,
                           float right){
    int mdg_arr[2] = {0};
    int max=0;
    int ret =0;
    pthread_mutex_lock(&dev_ctl->lock);
    if(is_usecase_unlock(dev_ctl,UC_OFFLOAD_PLAYBACK)){
        if(NULL==dev_ctl->offload_dg){
            dev_ctl->offload_dg = mixer_get_ctl_by_name(dev_ctl->mixer, "OFFLOAD DG Set");
        }

        if(dev_ctl->offload_dg) {
            max = mixer_ctl_get_range_max(dev_ctl->offload_dg);
            mdg_arr[0] = max * left;
            mdg_arr[1] = max * right;
            LOG_I("set_offload_volume left=%f,right=%f, max=%d", left, right, max);
            ret= mixer_ctl_set_array(dev_ctl->offload_dg, (void *)mdg_arr, 2);
        } else {
            LOG_E("set_offload_volume cannot get offload_dg ctrl");
        }
    }
    pthread_mutex_unlock(&dev_ctl->lock);
    return ret;
}

static  int apply_audio_profile_param_firmware(struct mixer_ctl *eq_select,int id){
    int ret=-1;

    if(NULL !=eq_select){
        ret = mixer_ctl_set_value(eq_select, 0, id);
        if (ret != 0) {
            LOG_E("apply_audio_profile_param_firmware Failed \n");
        }else{
            LOG_D("apply_audio_profile_param_firmware ret:%x val:%x",ret,id);
        }
    }else{
        LOG_E("apply_audio_profile_param_firmware failed");
    }
    return 0;
}

static int set_sprd_output_devices_param(struct sprd_codec_mixer_t *codec, struct sprd_code_param_t *param,
    int vol_index,int out_devices){
    int ret=0;

    if(param==NULL){
        return -1;
    }

    LOG_I("set_sprd_output_devices_param vol_index:%d out_devices:%d",vol_index,out_devices);

    if(out_devices & AUDIO_DEVICE_OUT_EARPIECE){
        if(NULL != codec->ear_playback_volume) {
            ret = mixer_ctl_set_value(codec->ear_playback_volume, 0,
                                      param->ear_playback_volume[vol_index]);
            if (ret != 0) {
                LOG_E("set_sprd_output_devices_param set ear_playback_volume failed");
            }else{
                LOG_I("set_sprd_output_devices_param set ear_playback_volume :0x%x",param->ear_playback_volume[vol_index]);
            }
        }

        if(NULL != codec->dac_playback_volume) {
            ret = mixer_ctl_set_value(codec->dac_playback_volume, 0,
                                  param->dacl_playback_volume[vol_index]);
            if (ret != 0) {
                LOG_E("set_sprd_output_devices_param set dacl_playback_volume failed");
            }else{
                LOG_I("set_sprd_output_devices_param set dacl_playback_volume :0x%x",param->dacl_playback_volume[vol_index]);
            }
        }
    }

    if(out_devices & AUDIO_DEVICE_OUT_SPEAKER){
        if(NULL != codec->dac_playback_volume) {
            ret = mixer_ctl_set_value(codec->dac_playback_volume, 0,
                                      param->dac_playback_volume[vol_index]);
            if (ret != 0) {
                LOG_E("set_sprd_output_devices_param set dac_playback_volume failed");
            }else{
                LOG_I("set_sprd_output_devices_param set dacs_playback_volume :0x%x",param->dac_playback_volume[vol_index]);
            }
        }

        if(NULL != codec->spkl_playback_volume) {
            ret = mixer_ctl_set_value(codec->spkl_playback_volume, 0,
                                      param->spkl_playback_volume[vol_index]);
            if (ret != 0) {
                LOG_E("set_sprd_output_devices_param set spkl_playback_volume failed");
            }else{
                LOG_I("set_sprd_output_devices_param set spkl_playback_volume :0x%x",param->spkl_playback_volume[vol_index]);
            }
        }

        if(NULL != codec->inner_pa) {
            ret = mixer_ctl_set_value(codec->inner_pa, 0,
                                      param->inter_pa_config);
            if (ret != 0) {
                LOG_E("set_sprd_output_devices_param set inner_pa failed");
            }else{
                LOG_I("set_sprd_output_devices_param set inner_pa :0x%x",param->inter_pa_config);
            }
        }
    }

    if(out_devices & (AUDIO_DEVICE_OUT_WIRED_HEADPHONE|AUDIO_DEVICE_OUT_WIRED_HEADSET)){
        if(NULL != codec->dac_playback_volume) {
            ret = mixer_ctl_set_value(codec->dac_playback_volume, 0,
                                  param->dacl_playback_volume[vol_index]);
            if (ret != 0) {
                LOG_E("set_sprd_output_devices_param set dacl_playback_volume failed");
            }else{
                LOG_I("set_sprd_output_devices_param set dacl_playback_volume :0x%x",param->dacl_playback_volume[vol_index]);
            }
        }

        if(NULL != codec->hpl_playback_volume) {
            ret = mixer_ctl_set_value(codec->hpl_playback_volume, 0,
                                      param->hpl_playback_volume[vol_index]);
            if (ret != 0) {
                LOG_E("set_sprd_output_devices_param set hpl_playback_volume failed");
            }else{
                LOG_I("set_sprd_output_devices_param set hpl_playback_volume :0x%x",param->hpl_playback_volume[vol_index]);
            }
        }
        if(NULL != codec->hpr_playback_volume) {
            ret = mixer_ctl_set_value(codec->hpr_playback_volume, 0,
                                      param->hpr_playback_volume[vol_index]);
            if (ret != 0) {
                LOG_E("set_sprd_output_devices_param set hpr_playback_volume failed");
            }else{
                LOG_I("set_sprd_output_devices_param set hpr_playback_volume :0x%x",param->hpr_playback_volume[vol_index]);
            }
        }

        if(NULL != codec->hp_inner_pa) {
            ret = mixer_ctl_set_value(codec->hp_inner_pa, 0,
                                      param->inter_hp_pa_config);
            if (ret != 0) {
                LOG_E("set_sprd_output_devices_param set hp_inner_pa failed");
            }else{
                LOG_I("set_sprd_output_devices_param set hp_inner_pa :0x%x",param->inter_hp_pa_config);
            }
        }
    }

    return ret;
}

static int set_sprd_input_devices_param( struct sprd_codec_mixer_t *codec, struct sprd_code_param_t *param,int mic_switch){
    int ret=0;

    if(0==mic_switch){
        return 0;
    }

    if(NULL != codec->adcl_capture_volume) {
        ret = mixer_ctl_set_value(codec->adcl_capture_volume, 0,
                                  param->adcl_capture_volume);
            if (ret != 0) {
                LOG_E("set_sprd_input_devices_param set adcl_capture_volume failed");
            }else{
                LOG_D("set_sprd_input_devices_param set adcl_capture_volume :0x%x",param->adcl_capture_volume);
            }
    }

    if(NULL != codec->adcr_capture_volume) {
        ret = mixer_ctl_set_value(codec->adcr_capture_volume, 0,
                                  param->adcr_capture_volume);
        if (ret != 0) {
            LOG_E("set_sprd_input_devices_param set adcr_capture_volume failed");
        }else{
            LOG_D("set_sprd_input_devices_param set adcr_capture_volume :0x%x",param->adcr_capture_volume);
        }
    }

    return 0;
}

/*volume start form 0 */
int set_dsp_volume(struct audio_control *ctl,int volume){
    LOG_D("set_dsp_volume:%d",volume);

    return set_audioparam(ctl,PARAM_VOICE_VOLUME_CHANGE, &volume,false);
}

uint8_t check_shareparam(struct audio_control *dev_ctl,int paramtype,uint8_t param_id){
    struct audiotester_config_handle *audiotester_config=&dev_ctl->config.audiotester_config;
    int i=0;

    if(false==is_audio_param_ready(dev_ctl->audio_param,paramtype)){
        LOG_W("%s param:%d is not ready",__func__,paramtype);
        return PROFILE_MODE_MAX;
    }

    if((audiotester_config->shareparam_config_num>0)
        &&(audiotester_config->shareparam_config!=NULL)){
        for(i=0;i<audiotester_config->shareparam_config_num;i++){
            if(((audiotester_config->shareparam_config[i].type)&(1<<paramtype))
                &&(audiotester_config->shareparam_config[i].paramid==param_id)){
                return audiotester_config->shareparam_config[i].shareparamid;
            }
        }
    }
    return param_id;
}

UNUSED_ATTR bool check_audioparam_exist(void *ctl,int profile,uint8_t param_id){
    int param_id_tmp=0;
    int offset=0;
    struct audio_control *dev_ctl=(struct audio_control *)ctl;

    param_id_tmp=check_shareparam(dev_ctl,profile,param_id);
    offset=(uint8_t)get_audio_param_mode(dev_ctl->audio_param,profile,param_id_tmp);
    if(AUDIO_PARAM_INVALID_8BIT_OFFSET!=offset){
        return true;
    }
    return false;
}

static void * get_ap_audio_param(AUDIO_PARAM_T *audio_param,int param_id)
{
    unsigned int offset=0;
    AUDIOVBCEQ_PARAM_T *param=&audio_param->param[SND_AUDIO_PARAM_RECORD_PROCESS_PROFILE];
    struct param_infor_t *param_infor=&audio_param->infor->data[SND_AUDIO_PARAM_RECORD_PROCESS_PROFILE];

    if((param->data==NULL)||(param_id>=PROFILE_MODE_MAX)){
        return NULL;
    }

    offset=param_infor->offset[param_id];

    if(AUDIO_PARAM_INVALID_32BIT_OFFSET==offset){
        LOG_E("get_ap_audio_param failed offset:%d %x,%d",offset,offset,param_id);
        return NULL;
    }else{
        LOG_I("get_ap_audio_param:%d %d %p param name:%s",param_id,offset,param->data,get_audio_param_name(param_id));
        return (param->data+offset);
    }
}

static int get_voice_mic_select(AUDIO_PARAM_T  *audio_param,int param_id){
    int16_t mic_swicth=0;
    int16_t * mic_param=NULL;

    param_id=check_shareparam(audio_param->dev_ctl,SND_AUDIO_PARAM_RECORD_PROCESS_PROFILE,param_id);

    mic_param=(int16_t*)get_ap_audio_param(audio_param,param_id);

    if(NULL!=mic_param){
        mic_swicth=*mic_param;
    }
    if(is_usbmic_connected_unlock(audio_param->dev_ctl)==false){
        mic_swicth&=(~SPRD_USB_MIC_PATH_SWITCH);
    }
    LOG_I("get_voice_mic_select param_id:%d %d",param_id,mic_swicth);
    return mic_swicth;
}

static int get_default_voice_mic_select(audio_devices_t device){
    int mic_select=0;

    if(device &AUDIO_DEVICE_OUT_ALL_SCO){
        mic_select =0;
    }else if(AUDIO_DEVICE_OUT_WIRED_HEADSET==device){
        mic_select = SPRD_HEADSET_MIC_PATH_SWITCH;
    }else if(device&AUDIO_DEVICE_IN_ALL_USB){
        mic_select=SPRD_USB_MIC_PATH_SWITCH;
    }else{
        mic_select = SPRD_BACK_MIC_PATH_SWITCH|SPRD_MAIN_MIC_PATH_SWITCH;
    }
    return mic_select;
}

static int micswitch_to_device(int mic_switch){
    int in_device=0;

    if(mic_switch& SPRD_USB_MIC_PATH_SWITCH){
        return AUDIO_DEVICE_IN_USB_HEADSET;
    }

    if(mic_switch& SPRD_MAIN_MIC_PATH_SWITCH){
        in_device|=AUDIO_DEVICE_IN_BUILTIN_MIC;
    }

    if(mic_switch & SPRD_BACK_MIC_PATH_SWITCH){
        in_device|=AUDIO_DEVICE_IN_BACK_MIC;
    }

    if(mic_switch & SPRD_HEADSET_MIC_PATH_SWITCH){
        in_device|=AUDIO_DEVICE_IN_WIRED_HEADSET;
    }

    if(mic_switch & SPRD_USB_MIC_PATH_SWITCH){
        in_device=SPRD_USB_MIC_PATH_SWITCH;
    }

    if(in_device==0){
        LOG_I("micswitch_to_device use default mic");
        in_device=AUDIO_DEVICE_IN_BACK_MIC;
    }
    return in_device;
}

static int get_normal_mic_select(uint32_t in_devices){
    int mic_switch=0;//bit 0:main mic, bit1:back mic, bit2:headset mic

    int in_dev = in_devices & ~AUDIO_DEVICE_BIT_IN;
    if(in_dev&AUDIO_DEVICE_IN_WIRED_HEADSET){
        mic_switch=SPRD_HEADSET_MIC_PATH_SWITCH;
    }else{
        if(in_dev&AUDIO_DEVICE_IN_BUILTIN_MIC){
            mic_switch=SPRD_MAIN_MIC_PATH_SWITCH;
        }

        if(in_dev&AUDIO_DEVICE_IN_BACK_MIC){
            mic_switch|=SPRD_BACK_MIC_PATH_SWITCH;
        }
    }
    return mic_switch;
}


int is_voice_active(int usecase)
{
    return (usecase & (UC_CALL|UC_VOIP|UC_LOOP));
}
static int is_voip_active(int usecase)
{
    return usecase & (UC_VOIP);
}
static int is_call_active(int usecase)
{
    return usecase & UC_CALL;
}
static int is_loop_active(int usecase)
{
    return usecase & UC_LOOP;
}
int is_playback_active(int usecase)
{
    return usecase & (UC_NORMAL_PLAYBACK|UC_DEEP_BUFFER_PLAYBACK|UC_OFFLOAD_PLAYBACK|UC_MMAP_PLAYBACK|UC_FAST_PLAYBACK);
}
int is_fm_active(int usecase)
{
    return usecase & UC_FM;
}
int is_record_active(int usecase)
{
    return usecase & (UC_MM_RECORD | UC_BT_RECORD | UC_MMAP_RECORD|UC_RECOGNITION);
}

static uint32_t make_param_kcontrl_value(uint8_t offset, uint8_t param_id, uint8_t dsp_case)
{
    return ((offset<<24)|(param_id<<16)|dsp_case);
}

static struct sprd_code_param_t * get_sprd_codec_param(AUDIO_PARAM_T  *audio_param,uint8_t param_id){
    AUDIOVBCEQ_PARAM_T *codec=&audio_param->param[SND_AUDIO_PARAM_CODEC_PROFILE];
    uint8_t  param_offset=get_audio_param_mode(audio_param,SND_AUDIO_PARAM_CODEC_PROFILE,param_id);

    if(AUDIO_PARAM_INVALID_8BIT_OFFSET!=param_offset){
        return (struct sprd_code_param_t * )(codec->data+(param_offset*codec->param_struct_size));
    }else{
        return NULL;
    }
}

static int get_sprd_codec_param_size(AUDIO_PARAM_T  *audio_param){
    AUDIOVBCEQ_PARAM_T *codec=&audio_param->param[SND_AUDIO_PARAM_CODEC_PROFILE];
    return codec->param_struct_size;
}

#ifdef SPRD_AUDIO_SMARTAMP
static int  set_smartamp_param(struct audio_control *dev_ctl, uint8_t param_id, uint8_t dsp_case) {
    AUDIO_PARAM_T  *audio_param = dev_ctl->audio_param;
    struct audio_param_res  *param_res = &(dev_ctl->param_res);
    int ret = 0;

    param_id=check_shareparam(dev_ctl,SND_AUDIO_PARAM_SMARTAMP_PROFILE,param_id);

    if(param_res->cur_dsp_smartamp_id != param_id){
        uint32_t param = 0;

        uint8_t offset=(uint8_t)get_audio_param_mode(dev_ctl->audio_param,SND_AUDIO_PARAM_SMARTAMP_PROFILE,param_id);
        if(AUDIO_PARAM_INVALID_8BIT_OFFSET!=offset){
            param = make_param_kcontrl_value(offset,  param_id,  dsp_case);
            LOG_I("UPDATE_PARAM_SMARTAMP:%s offset:0x%x",get_audio_param_name(param_id),offset);
            ret =apply_audio_profile_param_firmware(
                audio_param->select_mixer[SND_AUDIO_PARAM_SMARTAMP_PROFILE],
                param);
            param_res->cur_dsp_smartamp_id=param_id;

            pthread_mutex_lock(&dev_ctl->smartamp_ctl.lock);
            dev_ctl->smartamp_ctl.smartamp_func_enable=true;
            pthread_mutex_unlock(&dev_ctl->smartamp_ctl.lock);
            LOG_I("set_smartamp_param: enable SmartAmp");
        }
    }
    return ret;
}

uint8_t get_smartamp_playback_param_mode(struct audio_control *dev_ctl){
    AUDIO_PARAM_T  *audio_param = dev_ctl->audio_param;
    return get_audio_param_mode(audio_param,SND_AUDIO_PARAM_SMARTAMP_PROFILE,PROFILE_MODE_MUSIC_Handsfree_Playback);
}
#endif

static int set_vbc_param(struct audio_control *dev_ctl,uint8_t param_id, uint8_t dsp_case) {
    AUDIO_PARAM_T  *audio_param = dev_ctl->audio_param;
    struct audio_param_res  *param_res = &(dev_ctl->param_res);
    int ret = 0;
    uint32_t param = 0;
    uint8_t offset =0;

    param_id=check_shareparam(dev_ctl,SND_AUDIO_PARAM_DSP_VBC_PROFILE_DSP,param_id);

    offset=(uint8_t)get_audio_param_mode(dev_ctl->audio_param,SND_AUDIO_PARAM_DSP_VBC_PROFILE_DSP,param_id);
    if(AUDIO_PARAM_INVALID_8BIT_OFFSET==offset){
        LOG_I("set_vbc_param invalid audio param param id:%d",param_id);
        return -1;
    }

    param = make_param_kcontrl_value(offset,  param_id,  dsp_case);
    LOG_I("set_vbc_param:%d case:%d %d %d",param_id,dsp_case,param_res->cur_vbc_playback_id,param_res->cur_vbc_id);
    if((DAI_ID_NORMAL_OUTDSP_PLAYBACK==dsp_case)||(DAI_ID_FAST_P==dsp_case)
        ||(DAI_ID_OFFLOAD==dsp_case)){
        if(param_res->cur_vbc_playback_id != param_id){
            ret=apply_audio_profile_param_firmware(
                audio_param->select_mixer[SND_AUDIO_PARAM_DSP_VBC_PROFILE_DSP],
                param);
             LOG_I("UPDATE_PARAM_VBC_PLAY:%s play:%d dsp_case:%d",get_audio_param_name(param_id),param_res->cur_vbc_playback_id,dsp_case);
             param_res->cur_vbc_playback_id = param_id;
        }
    }else{
        if(param_res->cur_vbc_id != param_id) {
            ret=apply_audio_profile_param_firmware(
                audio_param->select_mixer[SND_AUDIO_PARAM_DSP_VBC_PROFILE_DSP],
                param);
            if(DAI_ID_NORMAL_OUTDSP_CAPTURE!=dsp_case){
                param_res->cur_vbc_playback_id = param_id;
            }
            LOG_I("UPDATE_PARAM_VBC_MASTER:%s play:%d dsp_case:%d",get_audio_param_name(param_id),param_res->cur_vbc_playback_id,dsp_case);
            param_res->cur_vbc_id = param_id;
        }
    }
    return ret;
}


static uint8_t get_voice_param(struct audiotester_config_handle *config,
    audio_devices_t devices, bool is_nrec,aud_net_m net_mode,aud_netrate rate_mode)
{
    int i=0;
    int mode_max=config->param_config_num;
    bool is_bt_device=false;

    if(devices&(AUDIO_DEVICE_OUT_BLUETOOTH_SCO|AUDIO_DEVICE_OUT_BLUETOOTH_SCO_HEADSET|AUDIO_DEVICE_OUT_BLUETOOTH_SCO_CARKIT)){
        is_bt_device=true;
    }

    for(i=0;i<mode_max;i++){

        if(0==(config->param_config[i].outdevice&(int)devices)){
            continue;
        }

        if(true==is_bt_device){
            if(true==is_nrec){
                if((config->param_config[i].paramid!=PROFILE_MODE_AUDIO_BTHSNREC_NB1)
                    &&(config->param_config[i].paramid!=PROFILE_MODE_AUDIO_BTHSNREC_NB2)
                    &&(config->param_config[i].paramid!=PROFILE_MODE_AUDIO_BTHSNREC_WB1)
                    &&(config->param_config[i].paramid!=PROFILE_MODE_AUDIO_BTHSNREC_WB2)
                    &&(config->param_config[i].paramid!=PROFILE_MODE_AUDIO_BTHSNREC_SWB1)
                    &&(config->param_config[i].paramid!=PROFILE_MODE_AUDIO_BTHSNREC_FB1)){
                    continue;
                }
            }else{
                if((config->param_config[i].paramid!=PROFILE_MODE_AUDIO_BTHS_NB1)
                    &&(config->param_config[i].paramid!=PROFILE_MODE_AUDIO_BTHS_NB2)
                    &&(config->param_config[i].paramid!=PROFILE_MODE_AUDIO_BTHS_WB1)
                    &&(config->param_config[i].paramid!=PROFILE_MODE_AUDIO_BTHS_WB2)
                    &&(config->param_config[i].paramid!=PROFILE_MODE_AUDIO_BTHS_SWB1)
                    &&(config->param_config[i].paramid!=PROFILE_MODE_AUDIO_BTHS_FB1)){
                    continue;
                }
            }
        }

        switch(net_mode){
            case AUDIO_NET_TDMA:
                if(rate_mode==AUDIO_NET_NB){
                    if(config->param_config[i].usecase&(1<<AUDIOTESTER_USECASE_TDMA)){
                        return config->param_config[i].paramid;
                    }
                }
                break;

            case AUDIO_NET_GSM:
                if(rate_mode==AUDIO_NET_NB){
                    if(config->param_config[i].usecase&(1<<AUDIOTESTER_USECASE_GSM)){
                        return config->param_config[i].paramid;
                    }
                }
                break;

            case AUDIO_NET_WCDMA:
                if(rate_mode==AUDIO_NET_WB){
                    if(config->param_config[i].usecase&(1<<AUDIOTESTER_USECASE_WCDMA_WB)){
                        return config->param_config[i].paramid;
                    }
                }

                if(rate_mode==AUDIO_NET_NB){
                    if(config->param_config[i].usecase&(1<<AUDIOTESTER_USECASE_WCDMA_NB)){
                        return config->param_config[i].paramid;
                    }
                }
                break;

            case AUDIO_NET_VOLTE:
                switch(rate_mode){
                     case AUDIO_NET_NB:
                         if(config->param_config[i].usecase&(1<<AUDIOTESTER_USECASE_VOLTE_NB)){
                             return config->param_config[i].paramid;
                         }
                        break;
                    case AUDIO_NET_WB:
                        if(config->param_config[i].usecase&(1<<AUDIOTESTER_USECASE_VOLTE_WB)){
                            return config->param_config[i].paramid;
                        }
                        break;
                    case AUDIO_NET_SWB:
                        if(config->param_config[i].usecase&(1<<AUDIOTESTER_USECASE_VOLTE_SWB)){
                            return config->param_config[i].paramid;
                        }
                        break;
                    case AUDIO_NET_FB:
                        if(config->param_config[i].usecase&(1<<AUDIOTESTER_USECASE_VOLTE_FB)){
                            return config->param_config[i].paramid;
                        }
                        break;

                }
                break;

            
            case AUDIO_NET_VOWIFI:
                if(rate_mode==AUDIO_NET_WB){
                    if(config->param_config[i].usecase&(1<<AUDIOTESTER_USECASE_VOWIFI_WB)){
                        return config->param_config[i].paramid;
                    }
                }
            
                if(rate_mode==AUDIO_NET_NB){
                    if(config->param_config[i].usecase&(1<<AUDIOTESTER_USECASE_VOWIFI_NB)){
                        return config->param_config[i].paramid;
                    }
                }
                break;
            
            case AUDIO_NET_CDMA2000:
                if(rate_mode==AUDIO_NET_WB){
                    if(config->param_config[i].usecase&(1<<AUDIOTESTER_USECASE_CDMA2000)){
                        return config->param_config[i].paramid;
                    }
                }
                break;

            default:
                break;
        }
    }
    return PROFILE_MODE_MAX;
}

static uint8_t get_voip_param(struct audiotester_config_handle *config,audio_devices_t devices, bool is_nrec){
    int i=0;
    int mode_max=config->param_config_num;
    bool is_bt_device=false;

    if(devices&(AUDIO_DEVICE_OUT_BLUETOOTH_SCO|AUDIO_DEVICE_OUT_BLUETOOTH_SCO_HEADSET|AUDIO_DEVICE_OUT_BLUETOOTH_SCO_CARKIT)){
        is_bt_device=true;
    }

    for(i=0;i<mode_max;i++){

        if(true==is_bt_device){
            if(true==is_nrec){
                if(config->param_config[i].paramid!=PROFILE_MODE_AUDIO_BTHSNREC_VOIP1){
                    continue;
                }
            }else{
                if(config->param_config[i].paramid!=PROFILE_MODE_AUDIO_BTHS_VOIP1){
                    continue;
                }
            }
        }

        if(0==(config->param_config[i].outdevice&(int)devices)){
            continue;
        }

        if(config->param_config[i].usecase&(1<<AUDIOTESTER_USECASE_VOIP)){
            return config->param_config[i].paramid;
        }
    }
    return PROFILE_MODE_MAX;
}

uint8_t get_loopback_param(struct audiotester_config_handle *config,audio_devices_t out_devices,audio_devices_t in_devices)
{
    int i=0;
    int mode_max=config->param_config_num;
    in_devices &=~AUDIO_DEVICE_BIT_IN;

    for(i=0;i<mode_max;i++){

        if(0==(config->param_config[i].outdevice&(int)out_devices)){
            continue;
        }

        if(0==(config->param_config[i].indevice&(int)in_devices)){
            continue;
        }

        if(config->param_config[i].usecase&(1<<AUDIOTESTER_USECASE_Loop)){
            return config->param_config[i].paramid;
        }
    }
    return PROFILE_MODE_MAX;
}

static uint8_t get_playback_param(struct audiotester_config_handle *config,audio_devices_t out_devices)
{
    int i=0;
    int mode_max=config->param_config_num;
    int device01=AUDIO_DEVICE_NONE;
    int device02=AUDIO_DEVICE_NONE;

    if(out_devices&AUDIO_DEVICE_OUT_SPEAKER){
        device01=AUDIO_DEVICE_OUT_SPEAKER;
    }

    if(out_devices&(AUDIO_DEVICE_OUT_WIRED_HEADSET|AUDIO_DEVICE_OUT_WIRED_HEADPHONE)){
        device02=AUDIO_DEVICE_OUT_WIRED_HEADSET;
    }
//    LOG_I("get_playback_param:%d mode_max:%d",out_devices,mode_max);
    for(i=0;i<mode_max;i++){
        LOG_D("get_playback_param[%d] device:%x usecase:0x%x %p",i
            ,config->param_config[i].outdevice,config->param_config[i].usecase,&config->param_config[i]);

        if((device01==AUDIO_DEVICE_OUT_SPEAKER)&&(device02==AUDIO_DEVICE_OUT_WIRED_HEADSET)){

            if(0==(config->param_config[i].outdevice&(int)device01)){
                continue;
            }

            if(0==(config->param_config[i].outdevice&(int)device02)){
                continue;
            }
        }

        if(0==(config->param_config[i].outdevice&(int)out_devices)){
            continue;
        }

        if((PROFILE_MODE_MUSIC_Headfree_Playback==config->param_config[i].paramid)
            &&((device01==AUDIO_DEVICE_NONE)||(device02==AUDIO_DEVICE_NONE))){
            LOG_I("get_playback_param return :%d is not headfree mode",config->param_config[i].paramid);
            continue;
        }

        if(config->param_config[i].usecase&(1<<AUDIOTESTER_USECASE_Playback)){
            LOG_D("get_playback_param return :%d",config->param_config[i].paramid);
            return config->param_config[i].paramid;
        }
    }
    return PROFILE_MODE_MAX;;
}

static uint8_t get_fm_param(struct audiotester_config_handle *config,audio_devices_t out_devices)
{
    int i=0;
    int mode_max=config->param_config_num;

    for(i=0;i<mode_max;i++){

        if(0==(config->param_config[i].outdevice&(int)out_devices)){
            continue;
        }

        if(config->param_config[i].usecase&(1<<AUDIOTESTER_USECASE_Fm)){
            return config->param_config[i].paramid;
        }
    }
    return PROFILE_MODE_MAX;
}

void set_record_source(struct audio_control *control, audio_source_t source){
    if(source > AUDIO_SOURCE_MAX || control== NULL){
        LOG_E("the input source is error!");
        return;
    }

    pthread_mutex_lock(&control->lock);
    control->audio_param->input_source = source;
    pthread_mutex_unlock(&control->lock);
}

static uint8_t get_record_param(struct audiotester_config_handle *config,int input_source, audio_devices_t in_devices)
{
    int i=0;
    int mode_max=config->param_config_num;
    uint8_t default_id=PROFILE_MODE_MAX;

    in_devices &=~AUDIO_DEVICE_BIT_IN;
    for(i=0;i<mode_max;i++){

        if(0==(config->param_config[i].indevice&(int)in_devices)){
            continue;
        }

        if((PROFILE_MODE_MAX==default_id)
             &&(config->param_config[i].usecase&(1<<AUDIOTESTER_USECASE_Record))){
            default_id=config->param_config[i].paramid;
        }

        if(AUDIO_SOURCE_UNPROCESSED==input_source)//input_source
        {
            if(config->param_config[i].usecase&(1<<AUDIOTESTER_USECASE_UnprocessRecord)){
                return config->param_config[i].paramid;
            }
        }else if(AUDIO_SOURCE_VOICE_RECOGNITION==input_source){
            if(config->param_config[i].usecase&(1<<AUDIOTESTER_USECASE_RecognitionRecord)){
                return config->param_config[i].paramid;
            }
        }else if(AUDIO_SOURCE_CAMCORDER ==input_source){
            if(config->param_config[i].usecase&(1<<AUDIOTESTER_USECASE_VideoRecord)){
                return config->param_config[i].paramid;
            }
        }else if(config->param_config[i].usecase&(1<<AUDIOTESTER_USECASE_Record)){
            return config->param_config[i].paramid;
        }
    }
    return default_id;
}

void * get_ap_record_param(AUDIO_PARAM_T *audio_param,audio_devices_t in_devices)
{
    uint8_t param_id =get_record_param(&audio_param->dev_ctl->config.audiotester_config,audio_param->input_source, in_devices);
    return  get_ap_audio_param(audio_param,param_id);
}

static int set_voice_dg_param(struct audio_control *dev_ctl,int param_id,int volume){

    int ret = 0;
    struct audio_param_res  *param_res = &(dev_ctl->param_res);
    param_id=check_shareparam(dev_ctl,SND_AUDIO_PARAM_PGA_PROFILE,param_id);

    LOG_D("set_voice_dg_param:%d cur_voice_dg_id:%d volume:%d",
        param_id,param_res->cur_voice_dg_id,volume);
    if ((param_id != param_res->cur_voice_dg_id)
     || (param_res->cur_voice_dg_volume != volume)) {
        ret = set_vdg_gain(&dev_ctl->dg_gain,param_id,volume);
        param_res->cur_voice_dg_id = param_id;
        param_res->cur_voice_dg_volume = volume;
    }

    return ret;
}

static int set_fm_dg_param(struct audio_control *dev_ctl,int param_id,int volume){

    int ret = 0;
    struct audio_param_res  *param_res = &(dev_ctl->param_res);

    param_id=check_shareparam(dev_ctl,SND_AUDIO_PARAM_PGA_PROFILE,param_id);

    LOG_D("set_fm_dg_param:%d cur_fm_dg_id:%d volume:%d",
        param_id,param_res->cur_fm_dg_id,volume);
    if((param_id != param_res->cur_fm_dg_id)
         || (param_res->cur_fm_dg_volume != volume)) {
        ret = set_vdg_gain(&dev_ctl->dg_gain, param_id, volume);
        param_res->cur_fm_dg_id = param_id;
        param_res->cur_fm_dg_volume = volume;
    }

    return ret;
}

static int set_record_dg_param(struct audio_control *dev_ctl,int param_id,int volume){

    int ret = 0;
    struct audio_param_res  *param_res = &(dev_ctl->param_res);

    param_id=check_shareparam(dev_ctl,SND_AUDIO_PARAM_PGA_PROFILE,param_id);

    LOG_D("set_record_dg_param:%d cur_record_dg_id:%d volume:%d",
        param_id,param_res->cur_record_dg_id,volume);
    if (param_id != param_res->cur_record_dg_id) {
        ret = set_vdg_gain(&dev_ctl->dg_gain, param_id, volume);
        param_res->cur_record_dg_id =param_id;
    }

    return ret;
}


static int set_play_dg_param(struct audio_control *dev_ctl,int param_id,int volume){

    int ret = 0;
    struct audio_param_res  *param_res = &(dev_ctl->param_res);

    param_id=check_shareparam(dev_ctl,SND_AUDIO_PARAM_PGA_PROFILE,param_id);

    LOG_D("set_play_dg_param:%d cur_playback_dg_id:%d volume:%d",
        param_id,param_res->cur_playback_dg_id,volume);
    if (param_id != param_res->cur_playback_dg_id) {
        ret = set_vdg_gain(&dev_ctl->dg_gain, param_id, volume);
        param_res->cur_playback_dg_id = param_id;
    }

    return ret;
}

static int  set_dsp_param(struct audio_control *dev_ctl, uint8_t param_id, uint8_t dsp_case,int volume) {
    AUDIO_PARAM_T  *audio_param = dev_ctl->audio_param;
    struct audio_param_res  *param_res = &(dev_ctl->param_res);
    int ret = 0;
    bool update_volume=false;
    uint8_t param_id_tmp;

    if(param_res->cur_dsp_id != param_id){
        uint32_t param = 0;
        uint8_t offset=0;

        param_id_tmp=check_shareparam(dev_ctl,SND_AUDIO_PARAM_AUDIO_STRUCTURE_PROFILE,param_id);

        offset=(uint8_t)get_audio_param_mode(dev_ctl->audio_param,SND_AUDIO_PARAM_AUDIO_STRUCTURE_PROFILE,param_id_tmp);
        if(AUDIO_PARAM_INVALID_8BIT_OFFSET!=offset){
            param = make_param_kcontrl_value(offset,  param_id_tmp,  dsp_case);
            LOG_I("UPDATE_PARAM_DSP:%s volume:%d offset:0x%x",get_audio_param_name(param_id_tmp),volume,offset);
            ret =apply_audio_profile_param_firmware(
                audio_param->select_mixer[SND_AUDIO_PARAM_AUDIO_STRUCTURE_PROFILE],
                param);
        }

        param_id_tmp=check_shareparam(dev_ctl,SND_AUDIO_PARAM_CVS_PROFILE,param_id);
        offset=(uint8_t)get_audio_param_mode(dev_ctl->audio_param,SND_AUDIO_PARAM_CVS_PROFILE,param_id_tmp);
        if(AUDIO_PARAM_INVALID_8BIT_OFFSET!=offset){
            param = make_param_kcontrl_value(offset,  param_id_tmp,  dsp_case);
            ret =apply_audio_profile_param_firmware(
                audio_param->select_mixer[SND_AUDIO_PARAM_CVS_PROFILE],
                param);
            update_volume=true;
        }
        param_res->cur_dsp_id=param_id;
    }

    LOG_D("set_dsp_param cur_dsp_volume:%d %d",param_res->cur_dsp_volume,volume);
    if((param_res->cur_dsp_volume != volume)||(true==update_volume)) {
        ret=mixer_ctl_set_value(dev_ctl->dsp_volume_ctl, 0, (volume+1));
        if (ret != 0) {
            LOG_E("set_dsp_volume Failed volume:%d\n",(volume+1));
        }else{
            LOG_I("set_dsp_param DSP_VOLUME:%d",(volume+1));
        }
        param_res->cur_dsp_volume = volume;
    }
    return ret;
}

static void set_speaker_pa_default_config(struct audio_control *dev_ctl,int default_param) {
    int ret = 0;
    int param_id =0;

    AUDIO_PARAM_T  *audio_param = dev_ctl->audio_param;
    struct sprd_code_param_t*  codec_param = NULL;

    param_id = check_shareparam(dev_ctl,SND_AUDIO_PARAM_CODEC_PROFILE,default_param);

    codec_param = get_sprd_codec_param(audio_param,param_id);

    if(NULL!=codec_param) {
        if(NULL != dev_ctl->codec.inner_pa) {
            ret = mixer_ctl_set_value(dev_ctl->codec.inner_pa, 0,
                                      codec_param->inter_pa_config);
            if (ret != 0) {
                LOG_E("set_speaker_pa_default_config set inner_pa failed");
            }else{
                LOG_I("set_speaker_pa_default_config set inner_pa :0x%x",codec_param->inter_pa_config);
            }
        }
    }
}

static int  set_codec_playback_param(struct audio_control *dev_ctl,uint8_t param_id,uint32_t out_devices,int volume) {
    int ret = 0;
    AUDIO_PARAM_T  *audio_param = dev_ctl->audio_param;
    struct audio_param_res  *param_res = &(dev_ctl->param_res);

    param_id=check_shareparam(dev_ctl,SND_AUDIO_PARAM_CODEC_PROFILE,param_id);

    LOG_D("set_codec_playback_param:0x%x out_devices:%d volume:%d",param_id,out_devices, volume);

        if(AUD_REALTEK_CODEC_TYPE == dev_ctl->codec_type){
            if(param_res->cur_codec_p_id != param_id) {
                uint8_t offset=get_audio_param_mode(audio_param,SND_AUDIO_PARAM_CODEC_PROFILE,param_id);
                if(AUDIO_PARAM_INVALID_8BIT_OFFSET!=offset){
                    LOG_I("UPDATE_PARAM_CODEC_PLAY:%s volume:%d",get_audio_param_name(param_id),volume);
                    ret =apply_audio_profile_param_firmware(
                        audio_param->select_mixer[SND_AUDIO_PARAM_CODEC_PROFILE],
                        offset);
                    param_res->cur_codec_p_id = param_id;
                }
            }
        }else{
            if((param_res->cur_codec_p_id != param_id) ||(param_res->cur_codec_p_volume != volume)) {
                struct sprd_code_param_t*  codec_param = get_sprd_codec_param(audio_param,param_id);
                if(NULL!=codec_param){
                    LOG_I("UPDATE_PARAM_CODEC_PLAY:%s cur_codec_p_volume:%d volume:%d codec_param:%p",
                        get_audio_param_name(param_id),param_res->cur_codec_p_volume,
                        volume,codec_param);
                    ret = set_sprd_output_devices_param(&dev_ctl->codec,codec_param,volume,out_devices);
                    param_res->cur_codec_p_id = param_id;
                    param_res->cur_codec_p_volume = volume;
                    if((dev_ctl->ucp_1301.count>0)
                        &&(out_devices&AUDIO_DEVICE_OUT_SPEAKER)){
                        set_all_ucp1301_param(&dev_ctl->ucp_1301,
                            (void *)((char *)codec_param+sizeof(struct sprd_code_param_t)),
                            get_sprd_codec_param_size(audio_param)-sizeof(struct sprd_code_param_t));
                    }
                }
            }
     }
    return 0;
}
static int  set_codec_record_param(struct audio_control *dev_ctl,uint8_t param_id,int mic_select) {
    int ret = 0;
    AUDIO_PARAM_T  *audio_param = dev_ctl->audio_param;
    struct audio_param_res  *param_res = &(dev_ctl->param_res);

    param_id=check_shareparam(dev_ctl,SND_AUDIO_PARAM_CODEC_PROFILE,param_id);

    if(AUD_REALTEK_CODEC_TYPE == dev_ctl->codec_type){
        if(param_res->cur_codec_c_id != param_id) {
            uint8_t offset=get_audio_param_mode(audio_param,SND_AUDIO_PARAM_CODEC_PROFILE,param_id);
            if(AUDIO_PARAM_INVALID_8BIT_OFFSET!=offset){
                LOG_I("UPDATE_PARAM CODEC_INPUT:%s",get_audio_param_name(param_id));
                ret =apply_audio_profile_param_firmware(
                    audio_param->select_mixer[SND_AUDIO_PARAM_CODEC_PROFILE],
                    offset);
                param_res->cur_codec_c_id = param_id;
            }
        }
    }else{
        struct sprd_code_param_t*  codec_param = get_sprd_codec_param(audio_param,param_id);
        LOG_D("set_codec_record_param:0x%x cur_codec_c_id:%d mic_select:0x%x",param_id,
            param_res->cur_codec_c_id,mic_select);

        if((param_res->cur_codec_c_id != param_id) && (NULL!=codec_param)){
            LOG_I("UPDATE_PARAM_CODEC_INPUT:%s mic_switch:%d",get_audio_param_name(param_id),mic_select);
            ret = set_sprd_input_devices_param(&dev_ctl->codec,codec_param,mic_select);
            param_res->cur_codec_c_id = param_id;
        }
    }
    return ret;
}

static int set_voice_mic(struct audio_control *actl,AUDIO_HW_APP_T audio_app_type,audio_devices_t device){
    int param_id=0;
    int in_device=0;
    struct audio_param_res * param_res=&actl->param_res;

    LOG_I("set_voice_mic:0x%x",device);
    if((device&AUDIO_DEVICE_OUT_ALL_USB)
        &&(true==is_usbmic_connected_unlock(actl))
        &&(true==is_usbmic_offload_supported(actl))){
        in_device=AUDIO_DEVICE_IN_USB_HEADSET;
        actl->in_devices=AUDIO_DEVICE_IN_USB_HEADSET;
        switch_vbc_route_unlock(actl,AUDIO_DEVICE_IN_USB_HEADSET);
        start_usb_channel(&actl->adev->usb_ctl,false);
        pthread_mutex_unlock(&actl->lock);
        pthread_mutex_lock(&actl->lock_route);
        do_switch_in_devices(actl,audio_app_type,in_device);
        pthread_mutex_unlock(&actl->lock_route);
        pthread_mutex_lock(&actl->lock);
        LOG_I("set_voice_mic AUDIO_DEVICE_IN_USB_HEADSET");
        return in_device;
    }else if(0==(device&AUDIO_DEVICE_OUT_ALL_USB)){
        LOG_D("set_voice_mic stop_usb_channel AUDIO_DEVICE_IN_USB_HEADSET");
        stop_usb_channel(&actl->adev->usb_ctl,false);
    }

    if(0==(device&AUDIO_DEVICE_OUT_ALL_SCO)){
        int mic_select=0;
        LOG_I("set_voice_mic with audio param");

        param_id = get_voice_param(&actl->config.audiotester_config,device, param_res->bt_infor.bluetooth_nrec,
            param_res->net_mode,param_res->rate_mode);

        if(param_id<PROFILE_MODE_MAX){
            mic_select = get_voice_mic_select(actl->audio_param, param_id);//mic config with audio param
        }

        if((mic_select&ALL_SPRD_MIC_SWITCH)==0){
            mic_select = get_default_voice_mic_select(device);
        }

        in_device=micswitch_to_device(mic_select);
    }else{
        LOG_I("set_voice_mic AUDIO_DEVICE_IN_BLUETOOTH_SCO_HEADSET");
        in_device = AUDIO_DEVICE_IN_BLUETOOTH_SCO_HEADSET;
    }

    if(in_device!=0){
        pthread_mutex_unlock(&actl->lock);
        if(((AUDIO_DEVICE_IN_ALL_SCO & (~AUDIO_DEVICE_BIT_IN)) & actl->in_devices) && (actl->in_devices!=in_device) && (actl->adev->call_start)){
            set_voice_ul_mute(actl,true);
            if(actl->bt_ul_mute_timer.created){
                ALOGI("%s ,have create timer,so we delete it",__func__);
                timer_delete(actl->bt_ul_mute_timer.timer_id);
                actl->bt_ul_mute_timer.created = false;
            }
            create_bt_ul_mute_timer(actl,0,BT_UL_MUTE_TIMER_DELAY_NSEC);
        }
        switch_vbc_route_unlock(actl,in_device);
        pthread_mutex_lock(&actl->lock);
        pthread_mutex_unlock(&actl->lock);
        pthread_mutex_lock(&actl->lock_route);
        do_switch_in_devices(actl,audio_app_type,in_device);
        pthread_mutex_unlock(&actl->lock_route);
        pthread_mutex_lock(&actl->lock);
        actl->in_devices=in_device;

#ifdef AUDIO_DSP_PLATFORM
        if (in_device == AUDIO_DEVICE_IN_BLUETOOTH_SCO_HEADSET) {
            //enable bt src after turn on bt_sco
            set_vbc_bt_src_unlock(actl, actl->param_res.bt_infor.samplerate);
        }
#endif

    }

    LOG_I("set_voice_mic in_device:0x%x exit",in_device);
    return in_device;
}

int set_available_outputdevices(struct audio_control *dev_ctl,int devices,bool avaiable){
    if(true==avaiable){
        pthread_mutex_lock(&dev_ctl->lock);
        if(0==(AUDIO_DEVICE_BIT_IN&devices)){
            dev_ctl->available_outputdevices |=devices;
        }else{
            dev_ctl->available_inputdevices |=devices;
        }
        pthread_mutex_unlock(&dev_ctl->lock);
    }else{
        pthread_mutex_lock(&dev_ctl->lock);
        if(0==(AUDIO_DEVICE_BIT_IN&devices)){
            dev_ctl->available_outputdevices &=~devices;
        }else{
            dev_ctl->available_inputdevices &=~(devices&~AUDIO_DEVICE_BIT_IN);
        }
        pthread_mutex_unlock(&dev_ctl->lock);
    }
    return 0;
}

static bool is_usbmic_connected_unlock(struct audio_control *dev_ctl){
    bool ret=false;
    if((dev_ctl->available_inputdevices&(~AUDIO_DEVICE_BIT_IN))
        &AUDIO_DEVICE_IN_ALL_USB){
        ret=true;
    }
    return ret;
}

bool is_usbdevice_connected_unlock(struct audio_control *dev_ctl){
    bool ret=false;
    if(dev_ctl->available_outputdevices&AUDIO_DEVICE_OUT_ALL_USB){
        ret=true;
    }
    return ret;
}

bool is_usbmic_connected(struct audio_control *dev_ctl){
    bool ret=false;
    pthread_mutex_lock(&dev_ctl->lock);
    ret=is_usbmic_connected_unlock(dev_ctl);
    pthread_mutex_unlock(&dev_ctl->lock);
    return ret;
}

bool is_usbdevice_connected(struct audio_control *dev_ctl){
    bool ret=false;
    pthread_mutex_lock(&dev_ctl->lock);
    ret = is_usbdevice_connected_unlock(dev_ctl);
    pthread_mutex_unlock(&dev_ctl->lock);
    return ret;
}

bool is_usbmic_offload_supported(struct audio_control *dev_ctl){
    struct usbaudio_ctl *usb_ctl=(struct usbaudio_ctl *)&dev_ctl->adev->usb_ctl;
    bool ret=false;
    if(usb_ctl->support_record){
        ret=true;
    }else{
        ret=false;
    }
    return ret;
}

void noity_usbmic_connected_for_call(struct audio_control *dev_ctl){
    struct audio_param_res  *param_res = NULL;
    param_res = &dev_ctl->param_res;

    pthread_mutex_lock(&dev_ctl->lock);
    if(is_call_active(param_res->usecase)){
        if((dev_ctl->out_devices&AUDIO_DEVICE_OUT_ALL_USB)
            &&(true==is_usbmic_connected_unlock(dev_ctl))
            &&(true==is_usbmic_offload_supported(dev_ctl))){
            LOG_I("noity_usbmic_connected_for_call start usb mic");
            switch_vbc_route_unlock(dev_ctl,AUDIO_DEVICE_IN_USB_HEADSET);
            start_usb_channel(&dev_ctl->adev->usb_ctl,false);
        }
    }
    pthread_mutex_unlock(&dev_ctl->lock);
}

int select_audio_param_unlock(struct audio_control *dev_ctl,struct audio_param_res  *param_res){
    uint8_t param_id = 0;
    int16_t dsp_case = 0;
    int volume = 0;
    uint32_t mic_select = 0;
    int ret = 0;
    if(is_voice_active(param_res->usecase)) {

        if(param_res->out_devices&AUDIO_DEVICE_OUT_ALL_SCO){
            volume=0;    //BT_SCO volume is cotrolled  by bt headset, set  audio param volume to 0
        }else{
            volume=param_res->voice_volume;
        }
        if(is_call_active(param_res->usecase)) {
            param_id = get_voice_param(&dev_ctl->config.audiotester_config,param_res->out_devices, param_res->bt_infor.bluetooth_nrec,
            param_res->net_mode,
            param_res->rate_mode);
            dsp_case=DAI_ID_VOICE;
        }
        else  if(is_voip_active(param_res->usecase)) {
            param_id = get_voip_param(&dev_ctl->config.audiotester_config,param_res->out_devices,param_res->bt_infor.bluetooth_nrec);
            dsp_case =DAI_ID_VOIP;
        }else  if(is_loop_active(param_res->usecase)) {
            param_id = get_loopback_param(&dev_ctl->config.audiotester_config,param_res->out_devices, param_res->in_devices);
            dsp_case =DAI_ID_LOOP;
            volume=0;
        }

        if(param_id>=PROFILE_MODE_MAX){
            LOG_W("get_voice_param failed,use playback param");
            if (is_playback_active(param_res->usecase)){
                int playback_param_id=0;
                playback_param_id = get_playback_param(&dev_ctl->config.audiotester_config,param_res->out_devices);
                LOG_I("get_voice_param failed,use playback param:%d",playback_param_id);
                if(playback_param_id>=PROFILE_MODE_MAX){
                    LOG_W("get_playback_param failed in call mode");
                    return -1;
                }
                set_play_dg_param(dev_ctl,playback_param_id, 0);
                set_vbc_param(dev_ctl, playback_param_id,DAI_ID_NORMAL_OUTDSP_PLAYBACK);
                set_codec_playback_param(dev_ctl, playback_param_id,
                    get_hwdevice_mode(&dev_ctl->config.audiotester_config,playback_param_id), 0);
            }
            return 0;
        }

        set_vbc_param(dev_ctl,param_id,dsp_case);

        set_voice_dg_param(dev_ctl,param_id,volume);

        set_dsp_param(dev_ctl, param_id, dsp_case,volume);
#if 0
        if(true==is_support_smartamp(&dev_ctl->smartamp_ctl)){
            ret = set_smartamp_param(dev_ctl, param_id,dsp_case);
        }
#endif
        if((is_voip_active(param_res->usecase))&&(is_fm_active(param_res->usecase))){
            uint8_t param_id_dg;
            param_id_dg = get_fm_param(&dev_ctl->config.audiotester_config,param_res->out_devices);
            if(param_id_dg>=PROFILE_MODE_MAX){
                LOG_W("get_fm_param failed");
                return -1;
            }

            if(!dev_ctl->fm_mute_l) {
                ret = set_fm_dg_param(dev_ctl,param_id_dg,param_res->fm_volume);
            }
            dev_ctl->fm_cur_param_id = param_id;
        }

        if(0==(param_res->out_devices&AUDIO_DEVICE_OUT_ALL_SCO)){
            ret = set_codec_playback_param(dev_ctl,
                param_id,get_hwdevice_mode(&dev_ctl->config.audiotester_config,param_id),volume);

            if(is_call_active(param_res->usecase)){

                if((param_res->out_devices&AUDIO_DEVICE_OUT_ALL_USB)==0){

                    mic_select = get_voice_mic_select(dev_ctl->audio_param,param_id);

                    if((mic_select&ALL_SPRD_MIC_SWITCH)==0){
                        mic_select = get_default_voice_mic_select(param_res->out_devices);
                    }
                }else{
                    mic_select=0;
                }

            }else if(is_voip_active(param_res->usecase) ||is_loop_active(param_res->usecase)){
                mic_select = get_normal_mic_select(param_res->in_devices);
            }
            ret = set_codec_record_param(dev_ctl, param_id, mic_select);
        }

        if ((is_call_active(param_res->usecase)) && ((UC_MMAP_RECORD|UC_RECOGNITION|UC_MM_RECORD)&param_res->usecase)){
            param_id = get_record_param(&dev_ctl->config.audiotester_config,dev_ctl->audio_param->input_source, param_res->in_devices);
            if(param_id>=PROFILE_MODE_MAX){
                LOG_W("get_record_param failed");
                return -1;
            }

            set_vbc_param(dev_ctl,param_id,DAI_ID_NORMAL_OUTDSP_CAPTURE);

            if(0==(param_res->out_devices&AUDIO_DEVICE_OUT_ALL_SCO)){
                mic_select = get_normal_mic_select(param_res->in_devices);
                ret = set_codec_record_param(dev_ctl, param_id, mic_select);
            }

            if (param_id != param_res->cur_record_dg_id) {
                ret = set_record_dg_param(dev_ctl, param_id, 0);
                param_res->cur_record_dg_id =param_id;
            }
        }

        if (is_playback_active(param_res->usecase)){
            param_id = get_playback_param(&dev_ctl->config.audiotester_config,param_res->out_devices);
            if(param_id>=PROFILE_MODE_MAX){
                LOG_W("get_playback_param failed");
            }else{
                ret = set_play_dg_param(dev_ctl, param_id, 0);
                set_vbc_param(dev_ctl,param_id,DAI_ID_NORMAL_OUTDSP_PLAYBACK);
            }
        }
    }else{
        if(is_fm_active(param_res->usecase)){
            volume=param_res->fm_volume;
            param_id = get_fm_param(&dev_ctl->config.audiotester_config,param_res->out_devices);
            if(param_id>=PROFILE_MODE_MAX){
                LOG_W("get_fm_param failed");
                return -1;
            }
            set_vbc_param(dev_ctl, param_id,  DAI_ID_NORMAL_OUTDSP_PLAYBACK);

            ret = set_codec_playback_param(dev_ctl, param_id,
                get_hwdevice_mode(&dev_ctl->config.audiotester_config,param_id),0);
            if(!dev_ctl->fm_mute_l) {
                ret = set_fm_dg_param(dev_ctl,param_id,param_res->fm_volume);
            }
            dev_ctl->fm_cur_param_id = param_id;

            if(true==is_support_smartamp(&dev_ctl->smartamp_ctl)){
                ret = set_smartamp_param(dev_ctl, param_id,dsp_case);
            }

            if(is_playback_active(param_res->usecase)){
                int playback_param_id= get_playback_param(&dev_ctl->config.audiotester_config,param_res->out_devices);
                if((playback_param_id>=0)&&(playback_param_id<PROFILE_MODE_MAX)){
                    ret = set_play_dg_param(dev_ctl, playback_param_id, 0);
                }
            }
        }else if(is_playback_active(param_res->usecase) ) {
            param_id = get_playback_param(&dev_ctl->config.audiotester_config,param_res->out_devices);
            if(param_id>=PROFILE_MODE_MAX){
                LOG_W("get_playback_param failed");
                return -1;
            }
            ret = set_play_dg_param(dev_ctl, param_id, 0);
            set_vbc_param(dev_ctl, param_id,  DAI_ID_NORMAL_OUTDSP_PLAYBACK);

#ifdef SPRD_AUDIO_SMARTAMP
            if(true==is_smartamp_func_enable(&dev_ctl->smartamp_ctl)){
                ret = set_smartamp_param(dev_ctl, param_id,dsp_case);
            }
#endif
            if((param_res->out_devices==AUDIO_DEVICE_OUT_BLUETOOTH_SCO)||
                (param_res->out_devices==AUDIO_DEVICE_OUT_BLUETOOTH_SCO_HEADSET)||
                (param_res->out_devices==AUDIO_DEVICE_OUT_BLUETOOTH_SCO_CARKIT)){
                LOG_I("Bt sco playback do not set codec param");
            }else{
                ret = set_codec_playback_param(dev_ctl, param_id,
                    get_hwdevice_mode(&dev_ctl->config.audiotester_config,param_id), 0);
            }
        }

        if(is_record_active(param_res->usecase)) {
            param_id = get_record_param(&dev_ctl->config.audiotester_config,dev_ctl->audio_param->input_source, param_res->in_devices);
            if(param_id>=PROFILE_MODE_MAX){
                LOG_W("get_record_param failed");
                return -1;
            }
            ret = set_vbc_param(dev_ctl, param_id,  DAI_ID_NORMAL_OUTDSP_CAPTURE);

            mic_select = get_normal_mic_select(param_res->in_devices);
            ret = set_codec_record_param(dev_ctl, param_id, mic_select);
            if (param_id != param_res->cur_record_dg_id) {
                ret = set_record_dg_param(dev_ctl, param_id, 0);
                param_res->cur_record_dg_id =param_id;
            }
        }
    }
    return 0;
}

static int audio_param_state_process(struct audio_param_res  *param_res,int usecase_off){
    if((0==(param_res->usecase&(UC_NORMAL_PLAYBACK|UC_DEEP_BUFFER_PLAYBACK|UC_OFFLOAD_PLAYBACK|UC_MMAP_PLAYBACK|UC_FAST_PLAYBACK)))
        &&(usecase_off&((UC_NORMAL_PLAYBACK|UC_DEEP_BUFFER_PLAYBACK|UC_OFFLOAD_PLAYBACK|UC_MMAP_PLAYBACK|UC_FAST_PLAYBACK))))
        {
        clear_playback_param_state(param_res);
    }

    if(0==(param_res->usecase&(UC_NORMAL_PLAYBACK|UC_DEEP_BUFFER_PLAYBACK|UC_OFFLOAD_PLAYBACK|UC_MMAP_PLAYBACK|UC_FAST_PLAYBACK|UC_FM)))
    {
        param_res->cur_vbc_playback_id=0xff;
    }

    if(((param_res->usecase&UC_FM)==0) &&(usecase_off&UC_FM)){
        clear_fm_param_state_unlock(param_res);
    }

    if(((param_res->usecase&(UC_VOIP|UC_CALL|UC_LOOP))==0)
        &&(usecase_off&((UC_VOIP|UC_CALL|UC_LOOP)))){
        clear_voice_param_state(param_res);
    }

    if(((param_res->usecase&(UC_MMAP_RECORD|UC_RECOGNITION|UC_MM_RECORD))==0)
        &&(usecase_off&(UC_MMAP_RECORD|UC_RECOGNITION|UC_MM_RECORD))){
        clear_record_param_state(param_res);
    }

    if(0==param_res->usecase){
        param_res->out_devices=0;
        param_res->in_devices=0;

        param_res->cur_codec_p_id = 0xff ;
        param_res->cur_codec_p_volume = 0xff ;
        param_res->cur_codec_c_id = 0xff ;
    }
    return 0;
}

static bool param_state_check(struct audio_control *dev_ctl,int type, void *param_change){
    bool update=false;
    struct audio_param_res  *param_res = NULL;
    param_res = &dev_ctl->param_res;

    switch(type){
        case PARAM_OUTDEVICES_CHANGE:{
                audio_devices_t *devices=(audio_devices_t *)param_change;
                if(param_res->out_devices != *devices) {
                    param_res->out_devices = *devices;
                    update=true;
                }
            }
            break;
        case PARAM_INDEVICES_CHANGE:{
                audio_devices_t *devices=(audio_devices_t *)param_change;
                if(param_res->in_devices != *devices) {
                    param_res->in_devices = *devices;
                    update=true;
                }
            }
            break;
        case PARAM_BT_NREC_CHANGE:{
                struct dev_bluetooth_t * bt_infor=(struct dev_bluetooth_t * )param_change;
                if(param_res->bt_infor.bluetooth_nrec != bt_infor->bluetooth_nrec) {
                    param_res->bt_infor.bluetooth_nrec = bt_infor->bluetooth_nrec;
                    update=true;
                }
            }
            break;
        case PARAM_NET_CHANGE:{
                struct voice_net_t * net_infor=(struct voice_net_t*)param_change;
                if((param_res->net_mode != net_infor->net_mode) || (param_res->rate_mode !=
                net_infor->rate_mode)){
                    param_res->net_mode = net_infor->net_mode;
                    param_res->rate_mode=net_infor->rate_mode;
                    update=true;
                }
            }
            break;
        case PARAM_USECASE_DEVICES_CHANGE:
                if((param_res->out_devices!=dev_ctl->out_devices)||
                    (param_res->in_devices!=dev_ctl->in_devices)){
                    update=true;
                }
                param_res->out_devices=dev_ctl->out_devices;
                param_res->in_devices=dev_ctl->in_devices;
                FALLTHROUGH_INTENDED;
        case PARAM_USECASE_CHANGE:{
                int usecase_off=0;
                if(param_res->usecase != dev_ctl->usecase){
                    usecase_off=param_res->usecase &(~dev_ctl->usecase);
                    param_res->usecase = dev_ctl->usecase;
                    audio_param_state_process(&dev_ctl->param_res,usecase_off);
                    if(0==param_res->usecase){
                        update=false;
                    }else{
                        update=true;
                    }
                }
            }
            break;
        case PARAM_VOICE_VOLUME_CHANGE:{
                int *volume=(int *)param_change;
                if(param_res->voice_volume != *volume) {
                    param_res->voice_volume=*volume;
                    update=true;
                }
            }
            break;
        case PARAM_FM_VOLUME_CHANGE:{
                int *volume=(int *)param_change;
                if(param_res->fm_volume != *volume) {
                    param_res->fm_volume=*volume;
                    update=true;
                }
            }
            break;
        case PARAM_AUDIOTESTER_CHANGE:
            /* clear playback param  */
            param_res->cur_playback_dg_id=0xff;
            param_res->cur_vbc_playback_id=0xff;
#ifdef SPRD_AUDIO_SMARTAMP
            param_res->cur_dsp_smartamp_id=0xff;
#endif
            /* clear fm param  */
            param_res->cur_fm_dg_id=0xff;
            param_res->cur_vbc_id=0xff;

            /* clear voice param  */
            param_res->cur_voice_dg_id=0xff;
            param_res->cur_dsp_id=0xff;
#ifdef SPRD_AUDIO_SMARTAMP
            param_res->cur_dsp_smartamp_id=0xff;
#endif
            /* clear record param  */
            param_res->cur_record_dg_id=0xff;

            /* clear codec param  */
            param_res->cur_codec_p_id=0xff;
            param_res->cur_codec_c_id=0xff;
            param_res->cur_codec_p_volume=0xff;
            update=true;
            break;
        default:
            LOG_W("param_state_check type:%d error",type);
            break;
    }

    return update;
}

int set_audioparam_unlock(struct audio_control *dev_ctl,int type, void *param_change,int force){
    bool update=false;
    struct audio_param_res  *param_res = NULL;
    param_res = &dev_ctl->param_res;
    int ret=-1;

    if(true==force){
        clear_all_vbc_dg_param_state(param_res);
    }
    update=param_state_check(dev_ctl,type,param_change);

    if((true==update)||(true==force)){
        ret = select_audio_param_unlock(dev_ctl,param_res);
    }
    return ret;
}

int set_audioparam(struct audio_control *dev_ctl,int type, void *param_change,int force){
    int ret=-1;
    pthread_mutex_lock(&dev_ctl->lock);
    ret=set_audioparam_unlock(dev_ctl,type, param_change,force);
    pthread_mutex_unlock(&dev_ctl->lock);
    return ret;
}

int free_audio_param(AUDIO_PARAM_T * param){
    int profile=0;

    disconnect_audiotester_process(&(param->tunning));
    for(profile=0;profile<SND_AUDIO_PARAM_PROFILE_MAX;profile++){

        if(param->param[profile].data!=NULL){
            free(param->param[profile].data);
            param->param[profile].data=NULL;
        }
        param->param[profile].num_mode=0;
        param->param[profile].param_struct_size=0;
        if(param->param[profile].xml.param_root!=NULL){
            release_xml_handle(&(param->param[profile].xml));
        }

        if(param->fd_bin[profile]>0){
            close(param->fd_bin[profile]);
            param->fd_bin[profile]=-1;
        }

        if(param->audio_param_file_table[profile].data_xml!=NULL){
            free((void *)param->audio_param_file_table[profile].data_xml);
            param->audio_param_file_table[profile].data_xml=NULL;
        }

        if(param->audio_param_file_table[profile].data_bin!=NULL){
            free((void *)param->audio_param_file_table[profile].data_bin);
            param->audio_param_file_table[profile].data_bin=NULL;
        }

        if(param->audio_param_file_table[profile].etc_xml!=NULL){
            free((void *)param->audio_param_file_table[profile].etc_xml);
            param->audio_param_file_table[profile].etc_xml=NULL;
        }

        if(param->audio_param_file_table[profile].etc_bin!=NULL){
            free((void *)param->audio_param_file_table[profile].etc_bin);
            param->audio_param_file_table[profile].etc_bin=NULL;
        }
    }

    if(param->etc_audioparam_infor!=NULL){
        free((void *)param->etc_audioparam_infor);
        param->etc_audioparam_infor=NULL;
    }

    if(param->infor!=NULL){
        free((void *)param->infor);
        param->infor=NULL;
    }
    sem_destroy(&param->sem);
    pthread_mutex_destroy(&param->audio_param_lock);

    return 0;
}

static void * audio_param_thread(void *res){
    int ret=-1;
    struct param_thread_res *thread_res=(struct param_thread_res *)res;
    AUDIO_PARAM_T *audio_param=(AUDIO_PARAM_T *)thread_res->audio_param;
    int profile=thread_res->profile;
    bool is_tunning=thread_res->is_tunning;
    LOG_I("audio_param_thread profile:%d Enter",profile);

    free(res);
    ret=load_audio_param(audio_param,is_tunning,profile);
    if(true==is_all_audio_param_ready(audio_param)){
        LOG_I("load audio param success");
        pthread_mutex_lock(&audio_param->audio_param_lock);
        if(NULL!=audio_param->infor){
            if(true==is_tunning){
                audio_param->infor->param_sn=audio_param->tunning_param_sn;
                save_audio_param_infor(audio_param->infor);
            }
        }

        if(NULL==audio_param->infor){
            audio_param->infor=(struct param_infor *)malloc(sizeof(struct param_infor));
            LOG_E("audio_param_thread param_infor is null");
            memset(audio_param->infor,0xff,(sizeof(struct param_infor)));
            load_audio_param_infor(audio_param->infor,audio_param->etc_audioparam_infor);
        }

        if(access(AUDIO_PARAM_INFOR_PATH, R_OK)!= 0){
            audio_param->backup_param_sn=audio_param->tunning_param_sn;
        }
        pthread_mutex_unlock(&audio_param->audio_param_lock);
        set_speaker_pa_default_config(audio_param->dev_ctl,
            get_playback_param(&audio_param->dev_ctl->config.audiotester_config,AUDIO_DEVICE_OUT_SPEAKER));
    }
    LOG_I("audio_param_thread profile:%d Exit load_status:0x%x",profile,audio_param->load_status);
    pthread_detach(pthread_self());
    return NULL;
}

void clear_audio_param_load_status(struct audio_control *dev_ctl){
    pthread_mutex_lock(&dev_ctl->lock);
    audio_param_clear_load_status(dev_ctl->audio_param);
    pthread_mutex_unlock(&dev_ctl->lock);
}

int init_audio_param( struct tiny_audio_device *adev)
{
    int ret = 0;
    AUDIO_PARAM_T  *audio_param=&(adev->audio_param);
    memset(audio_param,0,sizeof(AUDIO_PARAM_T));
    audio_param->agdsp_ctl=adev->dev_ctl->agdsp_ctl;
    audio_param->dev_ctl=adev->dev_ctl;
    audio_param->load_status=0;
    /*audio struct volume*/
    adev->dev_ctl->dsp_volume_ctl=mixer_get_ctl_by_name(adev->dev_ctl->mixer, "VBC_VOLUME");

    /*force digital_codec power on*/
    adev->dev_ctl->digital_codec_ctl=mixer_get_ctl_by_name(adev->dev_ctl->mixer, "Virt Output Switch");

#ifndef NORMAL_AUDIO_PLATFORM
    /* dsp vbc */
    audio_param->update_mixer[SND_AUDIO_PARAM_DSP_VBC_PROFILE_DSP]=
        mixer_get_ctl_by_name(adev->dev_ctl->mixer, VBC_DSP_PROFILE_UPDATE);

    audio_param->select_mixer[SND_AUDIO_PARAM_DSP_VBC_PROFILE_DSP]=
        mixer_get_ctl_by_name(adev->dev_ctl->mixer, VBC_DSP_PROFILE_SELECT);

    /* audio structure  */
    audio_param->update_mixer[SND_AUDIO_PARAM_AUDIO_STRUCTURE_PROFILE]=
        mixer_get_ctl_by_name(adev->dev_ctl->mixer, VBC_EQ_PROFILE_UPDATE);

    audio_param->select_mixer[SND_AUDIO_PARAM_AUDIO_STRUCTURE_PROFILE]=
        mixer_get_ctl_by_name(adev->dev_ctl->mixer, VBC_EQ_PROFILE_SELECT);

    /* NXP  */
    audio_param->update_mixer[SND_AUDIO_PARAM_CVS_PROFILE]=
        mixer_get_ctl_by_name(adev->dev_ctl->mixer, VBC_CVS_PROFILE_UPDATE);

    audio_param->select_mixer[SND_AUDIO_PARAM_CVS_PROFILE]=
        mixer_get_ctl_by_name(adev->dev_ctl->mixer, VBC_CVSPROFILE_SELECT);

    /* codec */
    if(adev->dev_ctl->codec_type==AUD_REALTEK_CODEC_TYPE){
        audio_param->update_mixer[SND_AUDIO_PARAM_CODEC_PROFILE]=
            mixer_get_ctl_by_name(adev->dev_ctl->mixer, VBC_CODEC_PROFILE_UPDATE);

        audio_param->select_mixer[SND_AUDIO_PARAM_CODEC_PROFILE]=
            mixer_get_ctl_by_name(adev->dev_ctl->mixer, VBC_CODEC_PROFILE_SELECT);
    }
#endif
    /* SmartAmp */
#ifdef SPRD_AUDIO_SMARTAMP
    audio_param->update_mixer[SND_AUDIO_PARAM_SMARTAMP_PROFILE]=
        mixer_get_ctl_by_name(adev->dev_ctl->mixer, DSP_SMARTAMP_PROFILE_UPDATE);

    audio_param->select_mixer[SND_AUDIO_PARAM_SMARTAMP_PROFILE]=
        mixer_get_ctl_by_name(adev->dev_ctl->mixer, DSP_SMARTAMP_PROFILE_SELECT);
#endif

#if defined(MINI_AUDIO)
    LOG_I("line%d test",__LINE__);
#else
    ret = sem_init(&audio_param->sem, 0, 0);
    if (ret) {
        LOG_E("init_audio_param_start sem_init falied, code is %s", strerror(errno));
    }

    pthread_mutex_init(&audio_param->audio_param_lock, NULL);

    {
        int profile[SND_AUDIO_PARAM_PROFILE_MAX]={
            SND_AUDIO_PARAM_CODEC_PROFILE,SND_AUDIO_PARAM_PGA_PROFILE,SND_AUDIO_PARAM_RECORD_PROCESS_PROFILE,
                SND_AUDIO_PARAM_DSP_VBC_PROFILE_DSP,SND_AUDIO_PARAM_CVS_PROFILE,SND_AUDIO_PARAM_AUDIO_STRUCTURE_PROFILE,
                SND_AUDIO_PARAM_SMARTAMP_PROFILE};
        int i=0;
        int err=0;
        bool is_tunning=false;

        init_smartamp(&adev->dev_ctl->smartamp_ctl);

        init_audioparam_filename(audio_param);

        audio_param->infor=(struct param_infor *)malloc(sizeof(struct param_infor));
        if(NULL==audio_param->infor){
            LOG_I("init_audio_param malloc param_infor failed");
            return -1;
        }
        memset(audio_param->infor,0xff,(sizeof(struct param_infor)));

        if(access(AUDIO_PARAM_TUNNING_FILE, R_OK)== 0){
            is_tunning = true;
        }

        for(i=0;i<SND_AUDIO_PARAM_PROFILE_MAX;i++){
            struct param_thread_res *res=NULL;

            LOG_I("init_audio_param Create audio_param_thread:%d",profile[i]);

            if((SND_AUDIO_PARAM_SMARTAMP_PROFILE==profile[i])
                &&(access(get_audioparam_filename(audio_param,SND_AUDIO_PARAM_SMARTAMP_PROFILE,AUDIO_PARAM_ETC_XML), R_OK)!=0)){
                pthread_mutex_lock(&audio_param->audio_param_lock);
                audio_param->load_status|=1<<SND_AUDIO_PARAM_SMARTAMP_PROFILE;
                pthread_mutex_unlock(&audio_param->audio_param_lock);
                continue;
            }

            res=(struct param_thread_res *)malloc(sizeof(struct param_thread_res));
            if(res==NULL){
                LOG_E("%s line:%d malloc failed",__func__,__LINE__);
                return -1;
            }

            res->audio_param=audio_param;
            res->profile=profile[i];
            res->is_tunning= is_tunning;

            err=pthread_create(&audio_param->thread[i], NULL, audio_param_thread, (void *)res);
            if(err!=0) {
                LOG_E("init_audio_param_start pthread_create failed  profile:%d!!!!(%s)",profile[i],strerror(err));
                free(res);
            }else{
                LOG_I("init_audio_param Create audio_param_thread:%d success",profile[i]);
            }
        }
    }
#endif
    return ret;
}

static void ucp1301_init_control(struct mixer *mixer,struct ucp_1301_handle_t *ucp_1301){
    int i=0;

    for(i=0;i<ucp_1301->count;i++){
        int ret=0;

        ucp_1301->ctl[i].pa_agc_en=mixer_get_ctl_by_name(mixer,ucp_1301_ctl_name_table[i].pa_agc_en);
        if(NULL==ucp_1301->ctl[i].pa_agc_en){
            LOG_E("Open [%s] Failed",ucp_1301_ctl_name_table[i].pa_agc_en);
            ret=-1;
            goto failed;
        }

        ucp_1301->ctl[i].pa_mode=mixer_get_ctl_by_name(mixer,ucp_1301_ctl_name_table[i].pa_mode);
        if(NULL==ucp_1301->ctl[i].pa_mode){
            LOG_E("Open [%s] Failed",ucp_1301_ctl_name_table[i].pa_mode);
            ret=-1;
            goto failed;
        }

        ucp_1301->ctl[i].pa_agc_gain=mixer_get_ctl_by_name(mixer,ucp_1301_ctl_name_table[i].pa_agc_gain);
        if(NULL==ucp_1301->ctl[i].pa_agc_gain){
            LOG_E("Open [%s] Failed",ucp_1301_ctl_name_table[i].pa_agc_gain);
            ret=-1;
            goto failed;
        }

        ucp_1301->ctl[i].pa_class_d_f=mixer_get_ctl_by_name(mixer,ucp_1301_ctl_name_table[i].pa_class_d_f);
        if(NULL==ucp_1301->ctl[i].pa_class_d_f){
            LOG_E("Open [%s] Failed",ucp_1301_ctl_name_table[i].pa_class_d_f);
            ret=-1;
            goto failed;
        }

        ucp_1301->ctl[i].rl=mixer_get_ctl_by_name(mixer,ucp_1301_ctl_name_table[i].rl);
        if(NULL==ucp_1301->ctl[i].rl){
            LOG_E("Open [%s] Failed",ucp_1301_ctl_name_table[i].rl);
            ret=-1;
            goto failed;
        }

        ucp_1301->ctl[i].power_limit_p2=mixer_get_ctl_by_name(mixer,ucp_1301_ctl_name_table[i].power_limit_p2);
        if(NULL==ucp_1301->ctl[i].power_limit_p2){
            LOG_E("Open [%s] Failed",ucp_1301_ctl_name_table[i].power_limit_p2);
            ret=-1;
            goto failed;
        }

failed:
        if(0!=ret){
            ucp_1301->ucp_1301_type[i]=UCP_1301_UNKNOW;
            LOG_E("%s set 1301:%d to invalid",__func__,i);
        }
    }
}

static int get_ucp1301_type(struct mixer *mixer,struct ucp_1301_handle_t *ucp_1301){
    int id=-1;
    int type=UCP_1301_UNKNOW;
    int i=0;
    int ucp1301_switch=0;

    ucp_1301->count=0;

    for(i=0;i<MAX_1301_NUMBER;i++){
        struct mixer_ctl *ctl=mixer_get_ctl_by_name(mixer, ucp_1301_i2c_index_ctl_name_table[i]);
        if(NULL==ctl){
            ucp_1301->ucp_1301_type[i]=UCP_1301_UNKNOW;
            LOG_I("get_ucp1301_type 1301[%d] type failed",i);
            continue;
        }

        id=mixer_ctl_get_value(ctl,0);
        if(id<0){
            ucp_1301->ucp_1301_type[i]=UCP_1301_UNKNOW;
            LOG_W("get_ucp1301_type 1301[%d] Failed type id:%d",i,ucp_1301->ucp_1301_type[i]);
            continue;
        }

        ctl=mixer_get_ctl_by_name(mixer, ucp_1301_product_id_ctl_name_table[i]);
        if(NULL==ctl){
            ucp_1301->ucp_1301_type[i]=UCP_1301_UNKNOW;
            continue;
        }

        id=mixer_ctl_get_value(ctl,0);
        type=UCP_1301_UNKNOW;
        switch(id){
            case 1:
                type=UCP_1301_TYPE;
                break;

            case 2:
                type=UCP_1300A_TYPE;
                break;

            case 4:
                type=UCP_1300B_TYPE;
                break;

            default:
                type=UCP_1301_UNKNOW;
                break;
        }

        ucp_1301->ucp_1301_type[i]=type;
        ucp1301_switch=1<<i;

        if(UCP_1301_UNKNOW!=ucp_1301->ucp_1301_type[i]){
            ucp_1301->count++;
        }
        LOG_I("get_ucp1301_type 1301[%d] type id:%d",i,ucp_1301->ucp_1301_type[i]);
    }

    LOG_I("get_ucp1301_type count:%d ucp1301_switch:0x%x",ucp_1301->count,ucp1301_switch);
    return ucp1301_switch;
}

void ucp1301_type_to_str(char *str,struct ucp_1301_handle_t *ucp_1301){
    int i=0;
    char tmp[128]={0};
    for(i=0;i<MAX_1301_NUMBER;i++){
        if(ucp_1301->ucp_1301_type[i]!=UCP_1301_UNKNOW){
            sprintf(tmp,"// Config\\Ucp1301\\%d\\Switch=1",i+1);
        }else{
            sprintf(tmp,"// Config\\Ucp1301\\%d\\Switch=0",i+1);
        }
        strcat(str,tmp);
        strcat(str, SPLIT);
        memset(tmp,0,sizeof(tmp));
    }
}

static int set_ucp1301_param_with_number(struct ucp_1301_handle_t *ucp_1301, void *para,int number,int max_para_size){
    int ret=0;

    LOG_I("%s %p %d %d %zd",__func__,para,number,max_para_size,sizeof(struct ucp_1301_param_t));

    if((number<0)||(number>=MAX_1301_NUMBER)){
        LOG_E("%s failed with number:%d",__func__,number);
        return -1;
    }

    if((sizeof(struct ucp_1301_param_t)*(number+1))>(unsigned int)max_para_size){
        LOG_E("%s failed with number:%d max_para_size:%d",__func__,number,max_para_size);
        return -1;
    }

    if(UCP_1301_UNKNOW!=ucp_1301->ucp_1301_type[number]){
        struct ucp_1301_param_t *ucp_1301_para=(struct ucp_1301_param_t *)((char *)para+sizeof(struct ucp_1301_param_t)*number);

        LOG_I("%s %p %d %d %d ucp_1301_para:%p",__func__,para,number,max_para_size,sizeof(struct ucp_1301_param_t),ucp_1301_para);

        LOG_I("%s pa_config:0x%x r1:0x%x power_limit_p2:0x%x",__func__,
            ucp_1301_para->pa_config,ucp_1301_para->rl,ucp_1301_para->power_limit_p2);
        if(NULL != ucp_1301->ctl[number].pa_agc_en) {
            ret = mixer_ctl_set_value(ucp_1301->ctl[number].pa_agc_en, 0,
                GET_PA_PARAM_VALUE(ucp_1301_para->pa_config,PA_AGC_EN_OFFSET,PA_AGC_EN_MASK));
            if (ret != 0) {
                LOG_E("%s %d set pa_agc_en :0x%x failed",__func__,number,
                    GET_PA_PARAM_VALUE(ucp_1301_para->pa_config,PA_AGC_EN_OFFSET,PA_AGC_EN_MASK));
            }else{
                LOG_I("%s %d set pa_agc_en :0x%x",__func__,number,
                    GET_PA_PARAM_VALUE(ucp_1301_para->pa_config,PA_AGC_EN_OFFSET,PA_AGC_EN_MASK));
            }
        }

        if(NULL != ucp_1301->ctl[number].pa_mode) {
            ret = mixer_ctl_set_value(ucp_1301->ctl[number].pa_mode, 0,
                GET_PA_PARAM_VALUE(ucp_1301_para->pa_config,PA_MODE_OFFSET,PA_MODE_MASK));
            if (ret != 0) {
                LOG_E("%s %d set pa_mode :0x%x failed",__func__,number,
                    GET_PA_PARAM_VALUE(ucp_1301_para->pa_config,PA_MODE_OFFSET,PA_MODE_MASK));
            }else{
                LOG_I("%s %d set pa_mode :0x%x",__func__,number,
                    GET_PA_PARAM_VALUE(ucp_1301_para->pa_config,PA_MODE_OFFSET,PA_MODE_MASK));
            }
        }

        if(NULL != ucp_1301->ctl[number].pa_agc_gain) {
            ret = mixer_ctl_set_value(ucp_1301->ctl[number].pa_agc_gain, 0,
                GET_PA_PARAM_VALUE(ucp_1301_para->pa_config,PA_PGA_GAIN_OFFSET,PA_PGA_GAIN_MASK));
            if (ret != 0) {
                LOG_E("%s %d set pa_agc_gain :0x%x failed",__func__,number,
                    GET_PA_PARAM_VALUE(ucp_1301_para->pa_config,PA_PGA_GAIN_OFFSET,PA_PGA_GAIN_MASK));
            }else{
                LOG_I("%s %d set pa_agc_gain :0x%x",__func__,number,
                    GET_PA_PARAM_VALUE(ucp_1301_para->pa_config,PA_PGA_GAIN_OFFSET,PA_PGA_GAIN_MASK));
            }
        }

        if(NULL != ucp_1301->ctl[number].pa_class_d_f) {
            ret = mixer_ctl_set_value(ucp_1301->ctl[number].pa_class_d_f, 0,
                GET_PA_PARAM_VALUE(ucp_1301_para->pa_config,PA_CLASS_D_F_OFFSET,PA_CLASS_D_F_MASK));
            if (ret != 0) {
                LOG_E("%s %d set pa_class_d_f :0x%x failed",__func__,number,
                    GET_PA_PARAM_VALUE(ucp_1301_para->pa_config,PA_CLASS_D_F_OFFSET,PA_CLASS_D_F_MASK));
            }else{
                LOG_I("%s %d set pa_class_d_f :0x%x",__func__,number,
                    GET_PA_PARAM_VALUE(ucp_1301_para->pa_config,PA_CLASS_D_F_OFFSET,PA_CLASS_D_F_MASK));
            }
        }

        if(NULL != ucp_1301->ctl[number].rl) {
            ret = mixer_ctl_set_value(ucp_1301->ctl[number].rl,0,ucp_1301_para->rl);
            if (ret != 0) {
                LOG_E("%s %d set rl :0x%x failed",__func__,number,ucp_1301_para->rl);
            }else{
                LOG_I("%s %d set rl :0x%x",__func__,number,ucp_1301_para->rl);
            }
        }

        if(NULL != ucp_1301->ctl[number].power_limit_p2) {
            ret = mixer_ctl_set_value(ucp_1301->ctl[number].power_limit_p2,0,ucp_1301_para->power_limit_p2);
            if (ret != 0) {
                LOG_E("%s %d set power_limit_p2 0x%x failed",__func__,number,ucp_1301_para->power_limit_p2);
            }else{
                LOG_I("%s %d set power_limit_p2 :0x%x",__func__,number,ucp_1301_para->power_limit_p2);
            }
        }
    }

    return ret;
}

static void set_all_ucp1301_param(struct ucp_1301_handle_t *ucp_1301, void *para,int max_para_size){
    int i=0;
    for(i=0;i<ucp_1301->count;i++){
        LOG_I("%s para:%p max:%d %d",__func__,para,max_para_size,i);
        set_ucp1301_param_with_number(ucp_1301,para,i,max_para_size);
    }
}

void dump_audio_control(int fd,char* buffer,UNUSED_ATTR int buffer_size,struct audio_control  *ctl){
    snprintf(buffer,(DUMP_BUFFER_MAX_SIZE-1),
        "audio control dump: "
        "%p %p %p %p %p %p %p\n",
        ctl->mixer,
        ctl->vbc_iis_loop,
        ctl->offload_dg,
        ctl->dsp_volume_ctl,
        ctl->bt_ul_src,
        ctl->bt_dl_src,
        ctl->agdsp_ctl);
    AUDIO_DUMP_WRITE_STR(fd,buffer);
    memset(buffer,0,sizeof(buffer));

    snprintf(buffer,(DUMP_BUFFER_MAX_SIZE-1),
    "audio control dump config: "
    "usecase:%d "
    "fm_volume:%d "
    "fm_mute:0x%x "
    "voice_volume:0x%x "
    "fm_mute:0x%x "
    "codec_type:0x%x "
    "music_volume:%lf\n",
    ctl->usecase,
    ctl->fm_volume,
    ctl->fm_mute,
    ctl->voice_volume,
    ctl->fm_mute,
    ctl->codec_type,
    ctl->music_volume);
    AUDIO_DUMP_WRITE_STR(fd,buffer);
    memset(buffer,0,sizeof(buffer));

    snprintf(buffer,(DUMP_BUFFER_MAX_SIZE-1),
    "audio control available outdevices:0x%x "
    "input devices:0x%x\n",
    ctl->available_outputdevices,
    ctl->available_inputdevices);
    AUDIO_DUMP_WRITE_STR(fd,buffer);
    memset(buffer,0,sizeof(buffer));

    snprintf(buffer,(DUMP_BUFFER_MAX_SIZE-1),
    "audio control dump config: "
    "device out:%d in:0x%x debug out:%d in:0x%x\n",
    ctl->out_devices,
    ctl->in_devices,
    ctl->debug_out_devices,
    ctl->debug_in_devices);
    AUDIO_DUMP_WRITE_STR(fd,buffer);
    memset(buffer,0,sizeof(buffer));

    snprintf(buffer,(DUMP_BUFFER_MAX_SIZE-1),
        "dump config: "
        "log_level:%d "
        "fm_type:%d "
        "mic_switch:0x%x "
        "support_24bits:%d\n",
        ctl->config.log_level,
        ctl->config.audiotester_config.FmType,
        ctl->config.mic_switch,
        ctl->config.support_24bits);
    AUDIO_DUMP_WRITE_STR(fd,buffer);
    memset(buffer,0,sizeof(buffer));

    dump_smartamp(fd,buffer,buffer_size,&ctl->smartamp_ctl);

}

struct audio_control *init_audio_control(struct tiny_audio_device *adev)
{
    int card_num, ret;
    int try_count=0;
    struct audio_control *control;
    struct mixer_ctl * codec_infor=NULL;
    int ucp1301_switch=0;
    int i=0;
    control = (struct audio_control *) malloc(sizeof(struct audio_control));
    if (!control) {
        LOG_E("init_audio_control malloc audio route failed");
        goto err_calloc;
    }

    memset(control,0,sizeof(struct audio_control));

    LOG_I("init_audio_control");
    pthread_mutex_init(&control->lock, NULL);
    pthread_mutex_init(&control->lock_route, NULL);
    pthread_mutex_init(&control->cmd_lock, NULL);

    control->adev = adev;
    control->usecase = UC_UNKNOWN;
    control->dg_gain.dev_gain=NULL;
    control->dg_gain.gain_size=0;
    control->fm_volume=0;
    control->voice_volume=0;
    control->music_volume=0;
    control->fm_mute = false;
    control->available_outputdevices=0;
    control->available_inputdevices=0;
    control->vbc_dump.is_exit=true;
    ret= parse_audio_config(&control->config);
    if(ret!=0){
        LOG_E("init_audio_control parse_audio_config Failed");
        goto init_failed;
    }
try_open:
    if(NULL!=control->config.card_name){
        card_num = get_snd_card_number(control->config.card_name);
    }else{
        card_num = get_snd_card_number(CARD_SPRDPHONE);
    }

    control->mixer = mixer_open(card_num);
    if (!control->mixer) {
        LOG_E("init_audio_control Unable to open the mixer, aborting.");
        try_count++;
        if(try_count<=3){
            sleep(1);
            goto try_open;
        }
        goto init_failed;
    }
    control->dg_gain.mixer=control->mixer;
    adev->mixer=control->mixer;

    ret = parse_audio_route(control);
    if(ret!=0){
        LOG_E("init_audio_control parse_audio_route Failed");
        goto init_failed;
    }

    ret = parse_audio_pcm_config(&(control->pcm_handle));
    if(ret!=0){
        LOG_E("init_audio_control parse_audio_pcm_config Failed");
        goto init_failed;
    }

    init_mute_control(control->mixer,&control->mute,&control->config.mute);
    control->cards.s_tinycard = card_num;

    ret = stream_routing_manager_create(control);
    if (ret != 0) {
        LOG_E("init_audio_control stream_routing_manager_create failed ");
        goto init_failed;
    }

    ret = audio_event_manager_create(control);
    if (ret != 0) {
        LOG_E("init_audio_control audio_event_manager_create failed ");
        goto init_failed;
    }

#ifndef NORMAL_AUDIO_PLATFORM
    control->bt_dl_src = mixer_get_ctl_by_name(control->mixer, VBC_BT_DL_SRC);
    if(NULL==control->bt_dl_src){
        LOG_E("open [%s] failed",VBC_BT_DL_SRC);
    }
    control->bt_ul_src = mixer_get_ctl_by_name(control->mixer, VBC_BT_UL_SRC);
    if(NULL==control->bt_ul_src){
        LOG_E("open [%s] failed",VBC_BT_UL_SRC);
    }

    codec_infor=mixer_get_ctl_by_name(control->mixer, CODEC_INFOR_CTL);
    control->codec_type=AUD_SPRD_2731S_CODEC_TYPE;
    if(NULL!=codec_infor){
        const char *chip_str=NULL;
        chip_str=tinymix_get_enum(codec_infor);
        if(NULL!=chip_str){
            for(i=0;i<AUD_CODEC_TYPE_MAX;i++){
                if(strncmp(chip_str,audio_codec_chip_name[i],strlen(audio_codec_chip_name[i]))==0){
                    control->codec_type=i;
                    break;
                }
            }
        }
    }
    control->codec_type = AUD_SPRD_2731S_CODEC_TYPE;
    control->param_res.codec_type=control->codec_type;
    control->param_res.bt_infor.bluetooth_nrec=false;
    control->param_res.bt_infor.samplerate=8000;

    if(AUD_SPRD_2731S_CODEC_TYPE == control->codec_type){
        ret = init_sprd_codec_mixer(&(control->codec),control->mixer);
    }
#else
    control->codec_type=AUD_SPRD_2731S_CODEC_TYPE;
#endif

    control->voice_ul_capture = mixer_get_ctl_by_name(control->mixer, VOICE_UL_CAPTURE);
    if(NULL==control->voice_ul_capture){
        LOG_E("open [%s] failed",VOICE_UL_CAPTURE);
    }

    control->voice_dl_playback = mixer_get_ctl_by_name(control->mixer, VOICE_DL_PLAYBACK);
    if(NULL==control->voice_dl_playback){
        LOG_E("open [%s] failed",VOICE_DL_PLAYBACK);
    }

    ucp1301_switch=get_ucp1301_type(control->mixer,&control->ucp_1301);
    if(ucp1301_switch>0){
        ucp1301_init_control(control->mixer,&(control->ucp_1301));
        save_1301_audioparam_config(ucp1301_switch);
    }
    control->agdsp_ctl = (dsp_control_t*)dsp_ctrl_open(control);
    control->audio_param=&adev->audio_param;

    audio_param_state_process(&control->param_res,UC_NORMAL_PLAYBACK|UC_DEEP_BUFFER_PLAYBACK
        |UC_OFFLOAD_PLAYBACK|UC_FAST_PLAYBACK|UC_FM|UC_VOIP|UC_CALL|UC_LOOP|UC_MM_RECORD|UC_RECOGNITION);

    return control;

init_failed:
    free_audio_control(control);
    free(control);
    control = NULL;
err_calloc:
    return NULL;
}


void free_audio_control(struct audio_control *control)
{
    audio_event_manager_close(control);

    disable_vbc_playback_dump(control);
    dsp_ctrl_close(control->agdsp_ctl);

    pthread_mutex_lock(&control->lock);

    freee_audio_route(&control->route);

    free_device_gain(&(control->dg_gain));

    free_audio_config(&control->config);

    if (control->mixer) {
        mixer_close(control->mixer);
        control->mixer = NULL;
    }
    pthread_mutex_unlock(&control->lock);
    pthread_mutex_destroy(&control->lock);
    pthread_mutex_destroy(&control->lock_route);
}

static void bt_ul_mute_timer_handler(union sigval arg){
   LOG_I("%s in",__func__);
   struct audio_control *actl = (struct audio_control *)arg.sival_ptr;
   pthread_mutex_lock(&actl->lock);
   timer_delete(actl->bt_ul_mute_timer.timer_id);
   actl->bt_ul_mute_timer.created = false;
   pthread_mutex_unlock(&actl->lock);
   set_voice_ul_mute(actl,false);
}

static void create_bt_ul_mute_timer(struct audio_control *actl,int delay,int delay_nsec){
    LOG_I("%s ,in",__func__);
    int status;
    struct sigevent se;
    struct itimerspec ts;

    se.sigev_notify = SIGEV_THREAD;
    se.sigev_signo = SIGALRM;
    se.sigev_value.sival_ptr = actl;
    se.sigev_notify_function = bt_ul_mute_timer_handler;
    se.sigev_notify_attributes = NULL;

    ts.it_value.tv_sec = delay;
    ts.it_value.tv_nsec = delay_nsec;
    ts.it_interval.tv_sec = 0;
    ts.it_interval.tv_nsec = 0;

    status = timer_create(CLOCK_MONOTONIC, &se,&(actl->bt_ul_mute_timer.timer_id));
    if(status == 0){
        actl->bt_ul_mute_timer.created = true;
        timer_settime(actl->bt_ul_mute_timer.timer_id, 0, &ts, 0);
    }else{
        actl->bt_ul_mute_timer.created = false;
        LOG_E("create timer err !");
    }
    LOG_I("%s ,out",__func__);
}


#ifdef __cplusplus
}
#endif

