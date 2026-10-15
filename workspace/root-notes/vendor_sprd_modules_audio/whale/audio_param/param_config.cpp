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
#define LOG_TAG "audio_hw_paramconfig"

#define LOG_NDEBUG 0
#include "audio_hw.h"
#include "fcntl.h"
#include "audio_control.h"
#include "audio_xml_utils.h"
#include "audio_param/audio_param.h"
#include "tinyalsa_util.h"
#include <sys/types.h>
#include <sys/stat.h>
#include "stdint.h"
#include "tinyxml.h"
#include "audio_debug.h"
#include <stdio.h>
#include <sys/stat.h>
#include "stdint.h"
#include "fcntl.h"
#include <stdlib.h>

extern int get_audio_param_id_frome_name(const char *name);

static struct audiotester_param_config_handle *alloc_paramconfig(struct audiotester_config_handle *config)
{
    struct audiotester_param_config_handle *new_config;
    new_config = (struct audiotester_param_config_handle *)realloc(config->param_config,
                (config->param_config_num + 1) * sizeof(struct audiotester_param_config_handle));
    if (new_config == NULL) {
        LOG_E("alloc paramconfig failed");
        return NULL;
    } else {
        config->param_config = new_config;
    }

    config->param_config[config->param_config_num].usecase = 0;
    config->param_config[config->param_config_num].indevice = 0;
    config->param_config[config->param_config_num].outdevice = 0;
    config->param_config[config->param_config_num].paramid = -1;
    config->param_config[config->param_config_num].modename  = NULL;
    config->param_config[config->param_config_num].usecasename  = NULL;

   return &config->param_config[config->param_config_num++];
}

static struct audiotester_shareparam_config_handle *alloc_shareparamconfig(struct audiotester_config_handle *config)
{
    struct audiotester_shareparam_config_handle *new_config;
    new_config = (struct audiotester_shareparam_config_handle *)realloc(config->shareparam_config,
                (config->shareparam_config_num + 1) * sizeof(struct audiotester_shareparam_config_handle));
    if (new_config == NULL) {
        LOG_E("alloc alloc_shareparamconfig failed");
        return NULL;
    } else {
        config->shareparam_config = new_config;
    }

    config->shareparam_config[config->shareparam_config_num ].paramid = -1;
    config->shareparam_config[config->shareparam_config_num ].shareparamid = -1;
    config->shareparam_config[config->shareparam_config_num ].type = 0;

   return &config->shareparam_config[config->shareparam_config_num++];
}


static int get_audiotester_paramconfig_devicemode(const char * mode_str){

    if(NULL==mode_str){
        LOG_I("line:%d",__LINE__);
        return AUDIO_DEVICE_NONE;
    }

    if(0 == strncmp(mode_str,"Audio\\RCV",strlen("Audio\\RCV"))){
        return AUDIO_DEVICE_OUT_EARPIECE;
    }else if(0 == strncmp(mode_str,"Audio\\SPK",strlen("Audio\\SPK"))){
        return AUDIO_DEVICE_OUT_SPEAKER;
    }else if(0 == strncmp(mode_str,"Audio\\HP",strlen("Audio\\HP"))){
        return AUDIO_DEVICE_OUT_WIRED_HEADPHONE;
    }else if(0 == strncmp(mode_str,"Audio\\BTHSNREC",strlen("Audio\\BTHSNREC"))){
        return AUDIO_DEVICE_OUT_BLUETOOTH_SCO_HEADSET;
    }else if(0 == strncmp(mode_str,"Audio\\BTHS",strlen("Audio\\BTHS"))){
        return AUDIO_DEVICE_OUT_BLUETOOTH_SCO_HEADSET;
    }else if(0 == strncmp(mode_str,"Audio\\TypeC_Digital",strlen("Audio\\TypeC_Digital"))){
        return AUDIO_DEVICE_OUT_USB_HEADSET;
    }else if(0 == strncmp(mode_str,"Music\\SPK",strlen("Music\\SPK"))){
        return AUDIO_DEVICE_OUT_SPEAKER;
    }else if(0 == strncmp(mode_str,"Music\\TypeC_DigitalPlayback",strlen("Music\\TypeC_DigitalPlayback"))){
        return AUDIO_DEVICE_OUT_USB_HEADSET;
    }else if(0 == strncmp(mode_str,"Music\\TypeC_DigitalFm",strlen("Music\\TypeC_DigitalFm"))){
        return AUDIO_DEVICE_OUT_USB_HEADSET;
    }else if(0 == strncmp(mode_str,"Music\\RCV",strlen("Music\\RCV"))){
        return AUDIO_DEVICE_OUT_EARPIECE;
    }else if(0 == strncmp(mode_str,"Music\\HP,SPK",strlen("Music\\HP,SPK"))){
        return AUDIO_DEVICE_OUT_SPEAKER|AUDIO_DEVICE_OUT_WIRED_HEADPHONE;
    }else if(0 == strncmp(mode_str,"Music\\HP",strlen("Music\\HP"))){
        return AUDIO_DEVICE_OUT_WIRED_HEADPHONE;
    }else if(0 == strncmp(mode_str,"Loopback\\RCV",strlen("Loopback\\RCV"))){
        return AUDIO_DEVICE_OUT_EARPIECE;
    }else if(0 == strncmp(mode_str,"Loopback\\SPK",strlen("Loopback\\SPK"))){
        return AUDIO_DEVICE_OUT_SPEAKER;
    }else if(0 == strncmp(mode_str,"Loopback\\HP",strlen("Loopback\\HP"))){
        return AUDIO_DEVICE_OUT_WIRED_HEADPHONE;
    }else if(0 == strncmp(mode_str,"Music\\HeadsetFm",strlen("Music\\HeadsetFm"))){
        return AUDIO_DEVICE_OUT_WIRED_HEADSET|AUDIO_DEVICE_OUT_WIRED_HEADPHONE;
    }else if(0 == strncmp(mode_str,"Music\\HandsfreeFm",strlen("Music\\HandsfreeFm"))){
        return AUDIO_DEVICE_OUT_SPEAKER;
    }
    return AUDIO_DEVICE_NONE;
}

static int get_audiotester_paramconfig_uscase(const char * usecase_str){
    int usecase=0;
    char *tmp=NULL;
    char *tmp1;

    if(NULL==usecase_str){
        LOG_I("line:%d",__LINE__);
        return 0;
    }

    tmp=strdup(usecase_str);
    tmp1 = strtok(tmp, ",");
    while(tmp1!=NULL){
        if(0 == strncmp(tmp1,"GSM",strlen("GSM"))){
            usecase |=1<<AUDIOTESTER_USECASE_GSM;
        }else if(0 == strncmp(tmp1,"TD",strlen("TD"))){
            usecase |=1<<AUDIOTESTER_USECASE_TDMA;
        }else if(0 == strncmp(tmp1,"WCDMA_NB",strlen("WCDMA_NB"))){
            usecase |=1<<AUDIOTESTER_USECASE_WCDMA_NB;
        }else if(0 == strncmp(tmp1,"VOLTE_NB",strlen("VOLTE_NB"))){
            usecase |=1<<AUDIOTESTER_USECASE_VOLTE_NB;
        }else if(0 == strncmp(tmp1,"VOWIFI_NB",strlen("VOWIFI_NB"))){
            usecase |=1<<AUDIOTESTER_USECASE_VOWIFI_NB;
        }else if(0 == strncmp(tmp1,"CDMA2000",strlen("CDMA2000"))){
            usecase |=1<<AUDIOTESTER_USECASE_CDMA2000;
        }else if(0 == strncmp(tmp1,"WCDMA_WB",strlen("WCDMA_WB"))){
            usecase |=1<<AUDIOTESTER_USECASE_WCDMA_WB;
        }else if(0 == strncmp(tmp1,"VOLTE_WB",strlen("VOLTE_WB"))){
            usecase |=1<<AUDIOTESTER_USECASE_VOLTE_WB;
        }else if(0 == strncmp(tmp1,"VOWIFI_WB",strlen("VOWIFI_WB"))){
            usecase |=1<<AUDIOTESTER_USECASE_VOWIFI_WB;
        }else if(0 == strncmp(tmp1,"VOLTE_SWB",strlen("VOLTE_SWB"))){
            usecase |=1<<AUDIOTESTER_USECASE_VOLTE_SWB;
        }else if(0 == strncmp(tmp1,"VOLTE_FB",strlen("VOLTE_FB"))){
            usecase |=1<<AUDIOTESTER_USECASE_VOLTE_FB;
        }else if(0 == strncmp(tmp1,"VOIP",strlen("VOIP"))){
            usecase |=1<<AUDIOTESTER_USECASE_VOIP;
        }else if(0 == strncmp(tmp1,"Playback",strlen("Playback"))){
            usecase |=1<<AUDIOTESTER_USECASE_Playback;
        }else if(0 == strncmp(tmp1,"Record",strlen("Record"))){
            usecase |=1<<AUDIOTESTER_USECASE_Record;
        }else if(0 == strncmp(tmp1,"UnprocessRecord",strlen("UnprocessRecord"))){
            usecase |=1<<AUDIOTESTER_USECASE_UnprocessRecord;
        }else if(0 == strncmp(tmp1,"VideoRecord",strlen("VideoRecord"))){
            usecase |=1<<AUDIOTESTER_USECASE_VideoRecord;
        }else if(0 == strncmp(tmp1,"VoiceRecognition",strlen("VoiceRecognition"))){
            usecase |=1<<AUDIOTESTER_USECASE_RecognitionRecord;
        }else if(0 == strncmp(tmp1,"Fm",strlen("Fm"))){
            usecase |=1<<AUDIOTESTER_USECASE_Fm;
        }else if(0 == strncmp(tmp1,"Loop",strlen("Loop"))){
            usecase |=1<<AUDIOTESTER_USECASE_Loop;
        }else if(0 == strncmp(tmp1,"NONE",strlen("NONE"))){
            LOG_V("unused usecase:%s",tmp1);
        }else{
            LOG_W("get_audiotester_paramconfig_uscase error usecase:%s",tmp1);
        }

        tmp1 = strtok(NULL, ",");
    }

    if(tmp!=NULL){
        free(tmp);
        tmp=NULL;
    }

    return usecase;
}

static int get_audiotester_paramconfig_outdevice(const char *device_str){
    int devices=0;
    char *tmp=NULL;
    char *tmp1=NULL;

    if(NULL==device_str){
        return 0;
    }

    tmp=strdup(device_str);
    tmp1 = strtok(tmp, ",");
    while(tmp1!=NULL){

        if(0 == strncmp(tmp1,"Earpiece",strlen("Earpiece"))){
            devices |=AUDIO_DEVICE_OUT_EARPIECE;
        }else if(0 == strncmp(tmp1,"Speaker",strlen("Speaker"))){
            devices |=AUDIO_DEVICE_OUT_SPEAKER;
        }else if(0 == strncmp(tmp1,"Headset4P",strlen("Headset4P"))){
            devices |=AUDIO_DEVICE_OUT_WIRED_HEADSET;
        }else if(0 == strncmp(tmp1,"Headset3P",strlen("Headset3P"))){
            devices |=AUDIO_DEVICE_OUT_WIRED_HEADPHONE;
        }else if(0 == strncmp(tmp1,"Sco",strlen("Sco"))){
            devices |=(AUDIO_DEVICE_OUT_BLUETOOTH_SCO|AUDIO_DEVICE_OUT_BLUETOOTH_SCO_HEADSET|AUDIO_DEVICE_OUT_BLUETOOTH_SCO_CARKIT);
        }else if(0 == strncmp(tmp1,"UsbHeadset",strlen("UsbHeadset"))){
            devices |=AUDIO_DEVICE_OUT_ALL_USB;
        }else if(0 == strncmp(tmp1,"A2dp",strlen("A2dp"))){
            devices |=(AUDIO_DEVICE_OUT_BLUETOOTH_A2DP|AUDIO_DEVICE_OUT_BLUETOOTH_A2DP_HEADPHONES|AUDIO_DEVICE_OUT_BLUETOOTH_A2DP_SPEAKER);
        }else if(0 == strncmp(tmp1,"NONE",strlen("NONE"))){
            LOG_V("unused outdevice:%s",device_str);
        }else{
            LOG_W("Invalid outdevice:%s",device_str);
        }

        tmp1 = strtok(NULL, ",");
    }

    if(tmp!=NULL){
        free(tmp);
        tmp=NULL;
    }

    return devices;
}

static int get_audiotester_paramconfig_indevice(const char *device_str){
    int devices=0;
    char *tmp=NULL;
    char *tmp1=NULL;

    if(NULL==device_str){
        return 0;
    }

    tmp=strdup(device_str);
    tmp1 = strtok(tmp, ",");
    while(tmp1!=NULL){

        if(0 == strncmp(tmp1,"MainMic",strlen("MainMic"))){
            devices |=AUDIO_DEVICE_IN_BUILTIN_MIC;
        }else if(0 == strncmp(tmp1,"AuxMic",strlen("AuxMic"))){
            devices |=AUDIO_DEVICE_IN_BACK_MIC;
        }else if(0 == strncmp(tmp1,"HeadsetMic",strlen("HeadsetMic"))){
            devices |=AUDIO_DEVICE_IN_WIRED_HEADSET;
        }else if(0 == strncmp(tmp1,"Sco",strlen("Sco"))){
            devices |= AUDIO_DEVICE_IN_BLUETOOTH_SCO_HEADSET;
        }else if(0 == strncmp(tmp1,"UsbHeadsetIn",strlen("UsbHeadsetIn"))){
            devices |= AUDIO_DEVICE_IN_ALL_USB;
        }else if(0 == strncmp(tmp1,"NONE",strlen("NONE"))){
            LOG_V("unused indevice:%s",device_str);
        }else{
            LOG_W("Invalid indevice:%s",device_str);
        }

        tmp1 = strtok(NULL, ",");
    }

    if(tmp!=NULL){
        free(tmp);
        tmp=NULL;
    }
    return devices;
}

static int get_smartamp_usecase(const char *use_str){
    int usecase=0;
    char *tmp=NULL;
    char *tmp1=NULL;

    if(NULL==use_str){
        return 0;
    }

    tmp=strdup(use_str);
    tmp1 = strtok(tmp, ",");
    while(tmp1!=NULL){

        if(0 == strncmp(tmp1,"call",strlen("call"))){
            usecase |=UC_CALL;
        }else if(0 == strncmp(tmp1,"voip",strlen("voip"))){
            usecase |=UC_VOIP;
        }else if(0 == strncmp(tmp1,"fm",strlen("fm"))){
            usecase |=UC_FM;
        }else if(0 == strncmp(tmp1,"playback",strlen("playback"))){
            usecase |=UC_NORMAL_PLAYBACK|UC_OFFLOAD_PLAYBACK;
        }else{
            LOG_W("get_smartamp_usecase Invalid usecase:%s",use_str);
        }
        tmp1 = strtok(NULL, ",");
    }

    if(tmp!=NULL){
        free(tmp);
        tmp=NULL;
    }

    return usecase;
}

static int parse_audiotester_paramconfig(param_group_t parm,struct audiotester_config_handle *config
    ,bool is_tunning){
    TiXmlElement * ele_tmp;
    struct audiotester_param_config_handle *param_config=NULL;

    const char *tmp;

    ele_tmp=(TiXmlElement *)parm;
    while(ele_tmp!=NULL){

        param_config = alloc_paramconfig(config);
        if(NULL==param_config){
            LOG_I("%s line:%d",__func__,__LINE__);
            goto next;
        }

        tmp=ele_tmp->Attribute("name");
        if(NULL==tmp){
            LOG_I("%s line:%d",__func__,__LINE__);
            goto next;
        }
        param_config->paramid=get_audio_param_id_frome_name(tmp);
        if((param_config->paramid<PROFILE_AUDIO_MODE_START)||(param_config->paramid>=PROFILE_MODE_MAX)){
            LOG_E("invalid param config %s",tmp);
        }

        tmp=ele_tmp->Attribute("Usecase");
        if(NULL==tmp){
            LOG_I("%s line:%d",__func__,__LINE__);
            goto next;
        }
        param_config->usecase=get_audiotester_paramconfig_uscase(tmp);
        LOG_V("usecase:%d str:%s",param_config->usecase,tmp);
        param_config->usecasename=strdup(tmp);

        tmp=ele_tmp->Attribute("Path");
        if(NULL==tmp){
            LOG_I("%s line:%d",__func__,__LINE__);
            goto next;
        }

        param_config->hwOutDevice=get_audiotester_paramconfig_devicemode(tmp);
        if(true==is_tunning){
            param_config->modename=strdup(tmp);

             LOG_V("modename:%s usecasename:%s %p"
                 ,param_config->modename,param_config->usecasename,param_config);
        }

        tmp=ele_tmp->Attribute("OutDevice");
        if(NULL!=tmp){
            param_config->outdevice=get_audiotester_paramconfig_outdevice(tmp);
        }

        tmp=ele_tmp->Attribute("InDevice");
        if(NULL!=tmp){
            param_config->indevice=get_audiotester_paramconfig_indevice(tmp);
        }

        LOG_D("AudioTester Param Config:usecase:0x%x outdevice:0x%x indevice:0x%x paramid:%d param_config_num:%d",
        param_config->usecase,param_config->outdevice,param_config->indevice
        ,param_config->paramid,config->param_config_num);
next:
        ele_tmp=(TiXmlElement *)XML_get_next_sibling_group(ele_tmp);
    }
    return 0;
}

static int get_audiotester_shareparamconfig_type(const char *device_str){
    int type=0;
    char *tmp=NULL;
    char *tmp1=NULL;

    if(NULL==device_str){
        return 0;
    }

    tmp=strdup(device_str);
    tmp1 = strtok(tmp, ",");
    while(tmp1!=NULL){

        if(0 == strncmp(tmp1,"dsp_vbc",strlen("dsp_vbc"))){
            type |= 1<<SND_AUDIO_PARAM_DSP_VBC_PROFILE_DSP;
        }else if(0 == strncmp(tmp1,"audio_structure",strlen("audio_structure"))){
            type |=1<<SND_AUDIO_PARAM_AUDIO_STRUCTURE_PROFILE;
        }else if(0 == strncmp(tmp1,"cvs",strlen("cvs"))){
            type |=1<<SND_AUDIO_PARAM_CVS_PROFILE;
        }else if(0 == strncmp(tmp1,"audio_pga",strlen("audio_pga"))){
            type |= 1<<SND_AUDIO_PARAM_PGA_PROFILE;
        }else if(0 == strncmp(tmp1,"audio_process",strlen("audio_process"))){
            type |=1<<SND_AUDIO_PARAM_RECORD_PROCESS_PROFILE;
        }else if(0 == strncmp(tmp1,"codec",strlen("codec"))){
            type |= 1<<SND_AUDIO_PARAM_CODEC_PROFILE;
        }else if(0 == strncmp(tmp1,"dsp_smartamp",strlen("dsp_smartamp"))){
            type |=1<<SND_AUDIO_PARAM_SMARTAMP_PROFILE;
        }else{
            LOG_E("Invalid config type");
        }

        tmp1 = strtok(NULL, ",");
    }

    if(tmp!=NULL){
        free(tmp);
        tmp=NULL;
    }

    return type;
}

static int parse_audiotester_customparamconfig(param_group_t parm,struct audiotester_config_handle *config){
    TiXmlElement * ele_tmp;
    struct audiotester_shareparam_config_handle *param_config=NULL;

    const char *tmp;
    ele_tmp=(TiXmlElement *)parm;
    while(ele_tmp!=NULL){

        param_config = alloc_shareparamconfig(config);
        if(NULL==param_config){
            goto next;
        }

        tmp=ele_tmp->Attribute("Type");
        if(NULL==tmp){
            goto next;
        }
        param_config->type=get_audiotester_shareparamconfig_type(tmp);

        tmp=ele_tmp->Attribute("name");
        if(NULL==tmp){
            goto next;
        }
        param_config->paramid=get_audio_param_id_frome_name(tmp);

        tmp=ele_tmp->Attribute("include");
        if(NULL==tmp){
            goto next;
        }
        param_config->shareparamid=get_audio_param_id_frome_name(tmp);


        LOG_I("AudioTester ShareParam Config:type:%d paramid:0x%x shareparamid:%d",
        param_config->type,param_config->paramid,param_config->shareparamid);
next:
        ele_tmp=(TiXmlElement *)XML_get_next_sibling_group(ele_tmp);
    }
    return 0;
}

static int parse_smartamp_mode(const char *mode_str){
    if(0 == strncmp(mode_str,"fbsmartamp",strlen("fbsmartamp"))){
        return SND_AUDIO_FB_SMARTAMP_MODE;
    }else if(0 == strncmp(mode_str,"ffsmartamp",strlen("ffsmartamp"))){
        return SND_AUDIO_FF_SMARTAMP_MODE;
    }else{
        return SND_AUDIO_UNSUPPORT_SMARTAMP_MODE;
    }
}

static int parse_audiotester_config(param_group_t parm,struct audiotester_config_handle 
*config,bool is_tunning){
    TiXmlElement * ele_tmp;
    TiXmlElement * sub_ele;
    int j=0;
    const char *tmp;
    int ret=0;
    struct audiotester_param_config_handle * param_config=NULL;
    LOG_I("parse_audiotester_config enter");

    if(NULL!=config){
        free_audioparam_config(config);
    }

    ele_tmp=(TiXmlElement *)parm;
    LOG_I("parse_audiotester_config Line:%d",__LINE__);
    if(ele_tmp!=NULL){
        LOG_I("parse_audiotester_config value:%s",ele_tmp->Value());

        tmp=ele_tmp->Attribute("ChipName");
        if(NULL==tmp){
            LOG_E("parse_audiotester_config failed:not find ChipName");
        }

        if(NULL!=config->ChipName){
            free((void *)config->ChipName);
            config->ChipName=NULL;
        }
        config->ChipName=strdup(tmp);

        tmp=ele_tmp->Attribute("FmType");
        if(NULL!=tmp){
            int i=0;
            for(i=0;i<AUD_TYPE_MAX;i++){
                if(0 == strncmp(tmp,fm_type_name[i],strlen(fm_type_name[i]))){
                    config->FmType=i;
                }
            }
        }

        tmp=ele_tmp->Attribute("SocketBufferSize");
        if(NULL!=tmp){
            config->SocketBufferSize=string_to_value(tmp);
        }

        tmp=ele_tmp->Attribute("DiagBufferSize");
        if(NULL!=tmp){
            config->DiagBufferSize=string_to_value(tmp);
        }

        tmp=ele_tmp->Attribute("SmartAmpSupport");
        if(NULL!=tmp){
            config->SmartAmpSupportMode=parse_smartamp_mode(tmp);
        }

        tmp=ele_tmp->Attribute("SmartAmpUseCase");
        if(NULL!=tmp){
            config->SmartAmpUsecase=get_smartamp_usecase(tmp);
        }else{
            config->SmartAmpUsecase=0;
        }

        LOG_I("SocketBufferSize:%d DiagBufferSize:%d SmartAmpSupportMode:%d SmartAmpUsecase:0x%x",
            config->SocketBufferSize,config->DiagBufferSize,config->SmartAmpSupportMode,
            config->SmartAmpUsecase);

        sub_ele=(TiXmlElement *)ele_tmp->FirstChildElement("common");
        if(NULL!=sub_ele){
            sub_ele=(TiXmlElement *)sub_ele->FirstChildElement();
            if(NULL!=sub_ele){
                parse_audiotester_paramconfig(sub_ele,config,is_tunning);
            }
        }

        sub_ele=(TiXmlElement *)ele_tmp->FirstChildElement("custom");
        if(NULL!=sub_ele){
            sub_ele=(TiXmlElement *)sub_ele->FirstChildElement();
            if(NULL!=sub_ele){
                parse_audiotester_customparamconfig(sub_ele,config);
            }
        }

    }

    LOG_I("parse_audiotester_config param_config_num:%d",config->param_config_num);

    for(j=0;j<config->param_config_num;j++){
        param_config=&config->param_config[j];
            LOG_V("parse_audiotester_config[%d] paramid:%d modename:%s usecase:%s %p %x"
                ,j,param_config->paramid,param_config->modename,param_config->usecasename,param_config,
                param_config->usecase);
    }
    LOG_I("parse_audiotester_config exit");

    return ret;
}


#ifdef __cplusplus
extern "C" {
#endif

int get_hwdevice_mode(struct audiotester_config_handle *config,int param_id)
{
    int i=0;
    int mode_max=config->param_config_num;

    for(i=0;i<mode_max;i++){
        if(config->param_config[i].paramid==param_id){
            return config->param_config[i].hwOutDevice;
        }
    }
    return 0;
}

int get_smartamp_config_mode(struct audiotester_config_handle *config)
{
    return config->SmartAmpSupportMode;
}

int get_smartamp_config_usecase(struct audiotester_config_handle *config)
{
    return config->SmartAmpUsecase;
}

void free_audioparam_config(struct audiotester_config_handle *config){
    int i=0;
    struct audiotester_param_config_handle * param_config=NULL;

    if(config->ChipName!=NULL){
        free((void *)config->ChipName);
        config->ChipName=NULL;
    }

    if(config->param_config!=NULL){
        for(i=0;i<config->param_config_num;i++){
            param_config=&config->param_config[i];
            if(param_config->modename!=NULL){
                free((void *)param_config->modename);
                param_config->modename=NULL;
            }
        
            if(param_config->usecasename!=NULL){
                free((void *)param_config->usecasename);
                param_config->usecasename=NULL;
            }
        }

        free(config->param_config);
        config->param_config=NULL;
    }
    config->param_config_num=0;

    if(config->shareparam_config!=NULL){
        free((void *)config->shareparam_config);
        config->shareparam_config=NULL;
    }
    config->shareparam_config_num=0;
    config->SmartAmpSupportMode=SND_AUDIO_UNSUPPORT_SMARTAMP_MODE;
    config->SmartAmpUsecase=0;
}

int parse_audioparam_config(void *configin,bool is_tunning){
    struct audio_config_handle *config=(struct audio_config_handle *)configin;
    param_group_t root_param;
    TiXmlElement * ele_tmp;
    struct xml_handle xmlhandle;
    const char *tmp=NULL;
    char configfile[128]={0};

    if(NULL==config->audioparampath){
        load_xml_handle(&xmlhandle, AUDIO_PARAM_CONFIG_PATH);
    }else{
        snprintf(configfile,sizeof(configfile)-1,"%s/audioparam_config.xml",config->audioparampath);
        load_xml_handle(&xmlhandle,configfile);
    }
    root_param = xmlhandle.param_root;

    if(NULL!=root_param){
        ele_tmp=(TiXmlElement *)XML_get_first_sub_group(root_param);
        while(ele_tmp!=NULL){
            tmp = ele_tmp->Value();
            LOG_I("parse_audioparam_config:%s",tmp);
            if(strncmp(tmp,"AudioTester",strlen("AudioTester"))==0){
                parse_audiotester_config(ele_tmp,&(config->audiotester_config),is_tunning);
            }
            ele_tmp = (TiXmlElement *)XML_get_next_sibling_group(ele_tmp);
        }
    }else{
        LOG_I("parse_audioparam_config line:%d",__LINE__);
    }

    if(access(AUDIO_PARAM_DATA_CONFIG_PATH, R_OK)!= 0){
        TiXmlDocument *doc=NULL;
        LOG_I("parse_audioparam_config save:%s",AUDIO_PARAM_DATA_CONFIG_PATH);
        doc = (TiXmlDocument *)xmlhandle.param_doc;
        if(NULL==doc){
            LOG_E("parse_audioparam_config save:%s doc is null",AUDIO_PARAM_DATA_CONFIG_PATH);
        }else{
            doc->SaveFile(AUDIO_PARAM_DATA_CONFIG_PATH);
        }
    }

    release_xml_handle(&xmlhandle);
    return 0;
}

int save_1301_audioparam_config(int switch_1301){
    TiXmlElement *root_param;
    struct xml_handle xmlhandle;
    const char *attr=NULL;
    int old_switch_1301=0;

    LOG_I("save_1301_audioparam_config switch_1301:0x%x",switch_1301);

    load_xml_handle(&xmlhandle,AUDIO_PARAM_DATA_CONFIG_PATH);
    root_param = (TiXmlElement *)xmlhandle.param_root;

    if(NULL!=root_param){
        attr=root_param->Attribute("ucp1301_switch");
        if(NULL!=attr){
            old_switch_1301=strtoul(attr,NULL,0);
            if(old_switch_1301!=switch_1301){
                char tmp[32]={0};
                TiXmlDocument *doc=NULL;

                snprintf(tmp,sizeof(tmp)-1,"0x%x",switch_1301);
                root_param->SetAttribute("ucp1301_switch",tmp);

                doc = (TiXmlDocument *)xmlhandle.param_doc;
                if(NULL==doc){
                    LOG_E("parse_audioparam_config save:%s doc is null",AUDIO_PARAM_DATA_CONFIG_PATH);
                }else{
                    doc->SaveFile(AUDIO_PARAM_DATA_CONFIG_PATH);
                }
            }
        }
    }else{
        LOG_W("save_1301_audioparam_config load failed");
    }
    release_xml_handle(&xmlhandle);
    return 0;
}

#ifdef __cplusplus
}
#endif
