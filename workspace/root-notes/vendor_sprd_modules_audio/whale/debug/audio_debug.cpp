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
#define LOG_TAG "audio_debug"

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
#include <cutils/properties.h>

extern int get_audio_param_id_frome_name(const char *name);

int log_level=3;
static const char *default_audiohardware_name="sprd";
static const char *default_audioparamconfig_name=AUDIO_PARAM_CONFIG_PATH;

static const char * audiopcmdumpname[PCMDUMP_MAX] = {
    "/data/vendor/local/media/playback_vbc",
    "/data/vendor/local/media/playback_hal",
    "/data/vendor/local/media/playback_offload",

    "/data/vendor/local/media/record_vbc",
    "/data/vendor/local/media/record_hal",
    "/data/vendor/local/media/record_process",
    "/data/vendor/local/media/record_nr",

    "/data/vendor/local/media/voip_playback",
    "/data/vendor/local/media/voip_record",

    "/data/vendor/local/media/loop_playback",
    "/data/vendor/local/media/loop_record",

    "/data/vendor/local/media/dump_voice_tx.pcm",
    "/data/vendor/local/media/dump_voice_rx.pcm",

};

void audiohal_pcmdump(void *ctl,int flag,void *buffer,int size,int dumptype){
    struct audio_control *dev_ctl=(struct audio_control *)ctl;
    if((dumptype<0)||(dumptype>=PCMDUMP_MAX)){
        LOG_W("audiohal_pcmdump Inavlid dumptype:%d",dumptype);
        return;
    }

    if(dev_ctl->pcmdumpflag&(1<<dumptype)){
        if(dev_ctl->pcmdumpfd[dumptype]<=0){
            char file_name[128]={0};
            snprintf(file_name,sizeof(file_name)-1,"%s_%d.pcm",audiopcmdumpname[dumptype],flag);
            dev_ctl->pcmdumpfd[dumptype]=open(file_name, O_RDWR | O_CREAT |O_TRUNC,
                S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
            if(dev_ctl->pcmdumpfd[dumptype] < 0) {
                LOG_E("audiohal_pcmdump open %s failed err:%s\n"
                    ,file_name,strerror(errno));
            }
        }

        if((dev_ctl->pcmdumpfd[dumptype]>0)
            &&(buffer!=NULL)&&(size>0)){
            write(dev_ctl->pcmdumpfd[dumptype],buffer,size);
        }
    }else{
        if(dev_ctl->pcmdumpfd[dumptype]>0){
            close(dev_ctl->pcmdumpfd[dumptype]);
            dev_ctl->pcmdumpfd[dumptype]=-1;
        }
    }
}

static int parse_audionr_config(param_group_t parm,struct audio_config_handle *config){
    TiXmlElement * ele_tmp;
    const char *tmp;

    ele_tmp=(TiXmlElement *)XML_get_first_sub_group(parm);
    while(ele_tmp!=NULL){
        tmp = ele_tmp->Value();
        if(0 == strncmp(tmp,"nr_support",strlen("nr_support"))){
            tmp=ele_tmp->Attribute("rate");
            if(NULL!=tmp){
                int rate=strtoul(tmp,NULL,0);
                int nr_mask=RECORD_UNSUPPORT_RATE_NR;
                switch(rate){
                    case 48000:
                        nr_mask=RECORD_48000_NR;
                        break;
                    case 44100:
                        nr_mask=RECORD_44100_NR;
                        break;
                    case 16000:
                        nr_mask=RECORD_16000_NR;
                        break;
                    case 8000:
                        nr_mask=RECORD_8000_NR;
                        break;
                    default:
                        nr_mask=RECORD_UNSUPPORT_RATE_NR;
                        break;
                }

                tmp=ele_tmp->Attribute("enable");
                if(NULL!=tmp){
                    if(strcmp(tmp, "true") == 0){
                        config->nr_mask |=1<<nr_mask;
                    }else{
                        config->nr_mask &=~(1<<nr_mask);
                    }
                }
            }

            tmp=ele_tmp->Attribute("other_rate");
            if(NULL!=tmp){
                tmp=ele_tmp->Attribute("enable");
                if(NULL!=tmp){
                    if(strcmp(tmp, "true") == 0){
                        config->enable_other_rate_nr=true;
                    }else{
                        config->enable_other_rate_nr=false;
                    }
                }
            }

        }
        ele_tmp = (TiXmlElement *)XML_get_next_sibling_group(ele_tmp);
    }

    LOG_I("parse_audionr_config nr_mask:0x%x enable_other_rate_nr:%d",
       config->nr_mask,config->enable_other_rate_nr);
    return 0;
}

static int parse_audio_log_level(param_group_t parm){
    TiXmlElement * ele_tmp;
    const char *tmp;
    int ret=4;
    ele_tmp=(TiXmlElement *)parm;
    if(ele_tmp!=NULL){
        tmp=ele_tmp->Attribute("level");
        ret=string_to_value(tmp);
        LOG_D("parse_audio_log_level level:%d",ret);
    }
    return ret;
}

static char * parse_audioparamconfig_path(param_group_t parm){
    TiXmlElement * ele_tmp;
    const char *tmp;
    const char *hardwarestr=NULL;
    char value[PROPERTY_VALUE_MAX]={0};
    if(property_get(AUDIO_HARDWARE_NAME_PROPERTY, value, "")){
        LOG_I("parse_audioparamconfig_path:%s=:%s",AUDIO_HARDWARE_NAME_PROPERTY,value);
        hardwarestr=(const char *)value;
    }else{
        hardwarestr=default_audiohardware_name;
    }

    ele_tmp=(TiXmlElement *)XML_get_first_sub_group(parm);
    while(ele_tmp!=NULL){
        tmp = ele_tmp->Value();
        if(0 == strncmp(tmp,hardwarestr,strlen(hardwarestr))){
            tmp=ele_tmp->Attribute("path");
            if(NULL!=tmp){
               return strdup(tmp);
            }
        }
        ele_tmp = (TiXmlElement *)XML_get_next_sibling_group(ele_tmp);
    }

    LOG_I("parse_audioparamconfig_path usedefault file:%s"
    ,default_audioparamconfig_name);
    return strdup(default_audioparamconfig_name);
}

static int parse_audio_mute_ctl(param_group_t parm,struct mute_control_name *mute){
    TiXmlElement * ele_tmp;
    const char *tmp;
    ele_tmp=(TiXmlElement *)XML_get_first_sub_group(parm);

    while(ele_tmp!=NULL){
        tmp = ele_tmp->Value();
        if(0 == strncmp(tmp,"spk_mute",strlen("spk_mute"))){
            tmp=ele_tmp->Attribute("ctl");
            if(NULL!=mute->spk_mute){
                free(mute->spk_mute);
                mute->spk_mute =NULL;
            }

            if(NULL!=tmp){
                mute->spk_mute=strdup(tmp);
            }
        }else if(0 == strncmp(tmp,"spk2_mute",strlen("spk2_mute"))){
            tmp=ele_tmp->Attribute("ctl");
            if(NULL!=mute->spk2_mute){
                free(mute->spk2_mute);
                mute->spk2_mute =NULL;
            }

            if(NULL!=tmp){
                mute->spk2_mute=strdup(tmp);
            }
        }else if(0 == strncmp(tmp,"handset_mute",strlen("handset_mute"))){
            tmp=ele_tmp->Attribute("ctl");
            if(NULL!=mute->handset_mute){
                free(mute->handset_mute);
                mute->handset_mute =NULL;
            }

            if(NULL!=tmp){
                mute->handset_mute=strdup(tmp);
            }
        }else if(0 == strncmp(tmp,"headset_mute",strlen("headset_mute"))){
            tmp=ele_tmp->Attribute("ctl");
            if(NULL!=mute->headset_mute){
                free(mute->headset_mute);
                mute->headset_mute =NULL;
            }

            if(NULL!=tmp){
                mute->headset_mute=strdup(tmp);
            }
        }else if(0 == strncmp(tmp,"linein_mute",strlen("linein_mute"))){
            tmp=ele_tmp->Attribute("ctl");
            if(NULL!=mute->linein_mute){
                free(mute->linein_mute);
                mute->linein_mute=NULL;
            }

            if(NULL!=tmp){
                mute->linein_mute=strdup(tmp);
            }
        }else if(0 == strncmp(tmp,"dsp_da0_mdg_mute",strlen("dsp_da0_mdg_mute"))){
            tmp=ele_tmp->Attribute("ctl");
            if(NULL!=mute->dsp_da0_mdg_mute){
                free(mute->dsp_da0_mdg_mute);
                mute->dsp_da0_mdg_mute=NULL;
            }

            if(NULL!=tmp){
                mute->dsp_da0_mdg_mute=strdup(tmp);
            }
        }else if(0 == strncmp(tmp,"dsp_da1_mdg_mute",strlen("dsp_da1_mdg_mute"))){
            tmp=ele_tmp->Attribute("ctl");
            if(NULL!=mute->dsp_da1_mdg_mute){
                free(mute->dsp_da1_mdg_mute);
                mute->dsp_da1_mdg_mute=NULL;
            }

            if(NULL!=tmp){
                mute->dsp_da1_mdg_mute=strdup(tmp);
            }
        }else if(0 == strncmp(tmp,"audio_mdg_mute",strlen("audio_mdg_mute"))){
            tmp=ele_tmp->Attribute("ctl");
            if(NULL!=mute->audio_mdg_mute){
                free(mute->audio_mdg_mute);
                mute->audio_mdg_mute=NULL;
            }

            if(NULL!=tmp){
                mute->audio_mdg_mute=strdup(tmp);
            }
        }else if(0 == strncmp(tmp,"audio_mdg_ap23_mute",strlen("audio_mdg_ap23_mute"))){
            tmp=ele_tmp->Attribute("ctl");
            if(NULL!=mute->audio_mdg23_mute){
                free(mute->audio_mdg23_mute);
                mute->audio_mdg23_mute=NULL;
            }

            if(NULL!=tmp){
                mute->audio_mdg23_mute=strdup(tmp);
            }
        }
        ele_tmp = (TiXmlElement *)XML_get_next_sibling_group(ele_tmp);
    }

    return 0;
}

#ifdef __cplusplus
extern "C" {
#endif
int set_audiohal_pcmdump(UNUSED_ATTR void *dev,UNUSED_ATTR struct str_parms *parms,
    int opt, UNUSED_ATTR char * val){
    struct tiny_audio_device *adev=(struct tiny_audio_device *)dev;
    struct audio_control *dev_ctl =adev->dev_ctl;

    dev_ctl->pcmdumpflag=opt;
    return 0;
}

int set_audiohal_musicpcmdump(UNUSED_ATTR void *dev,UNUSED_ATTR struct str_parms *parms,
    int opt, UNUSED_ATTR char * val){
    struct tiny_audio_device *adev=(struct tiny_audio_device *)dev;
    struct audio_control *dev_ctl =adev->dev_ctl;

    if(opt){
        dev_ctl->pcmdumpflag|=(1<<PCMDUMP_PRIMARY_PLAYBACK_MUSIC);
    }else{
        dev_ctl->pcmdumpflag&=~(1<<PCMDUMP_PRIMARY_PLAYBACK_MUSIC);
    }
    return 0;
}

int set_audiohal_voippcmdump(UNUSED_ATTR void *dev,UNUSED_ATTR struct str_parms *parms,
    int opt, UNUSED_ATTR char * val){
    struct tiny_audio_device *adev=(struct tiny_audio_device *)dev;
    struct audio_control *dev_ctl =adev->dev_ctl;

    if(opt){
        dev_ctl->pcmdumpflag|=((1<<PCMDUMP_VOIP_PLAYBACK_VBC)|(1<<PCMDUMP_VOIP_RECORD_VBC));
    }else{
        dev_ctl->pcmdumpflag&=~((1<<PCMDUMP_VOIP_PLAYBACK_VBC)|(1<<PCMDUMP_VOIP_RECORD_VBC));
    }
    return 0;
}

int set_audiohal_recordhalpcmdump(UNUSED_ATTR void *dev,UNUSED_ATTR struct str_parms *parms,
    int opt, UNUSED_ATTR char * val){
    struct tiny_audio_device *adev=(struct tiny_audio_device *)dev;
    struct audio_control *dev_ctl =adev->dev_ctl;

    if(opt){
        dev_ctl->pcmdumpflag|=(1<<PCMDUMP_NORMAL_RECORD_HAL);
    }else{
        dev_ctl->pcmdumpflag&=~(1<<PCMDUMP_NORMAL_RECORD_HAL);
    }
    return 0;
}

int set_audiohal_vbcpcmdump(UNUSED_ATTR void *dev,UNUSED_ATTR struct str_parms *parms,
    int opt, UNUSED_ATTR char * val){
    struct tiny_audio_device *adev=(struct tiny_audio_device *)dev;
    struct audio_control *dev_ctl =adev->dev_ctl;

    if(opt){
        dev_ctl->pcmdumpflag|=((1<<PCMDUMP_PRIMARY_PLAYBACK_VBC)|(1<<PCMDUMP_NORMAL_RECORD_VBC));
    }else{
        dev_ctl->pcmdumpflag&=~((1<<PCMDUMP_PRIMARY_PLAYBACK_VBC)|(1<<PCMDUMP_NORMAL_RECORD_VBC));
    }
    return 0;
}

int set_audiohal_looppcmdump(UNUSED_ATTR void *dev,UNUSED_ATTR struct str_parms *parms,
    int opt, UNUSED_ATTR char * val){
    struct tiny_audio_device *adev=(struct tiny_audio_device *)dev;
    struct audio_control *dev_ctl =adev->dev_ctl;

    if(opt){
        dev_ctl->pcmdumpflag|=((1<<PCMDUMP_LOOP_PLAYBACK_DSP)|(1<<PCMDUMP_LOOP_PLAYBACK_RECORD));
    }else{
        dev_ctl->pcmdumpflag&=~((1<<PCMDUMP_LOOP_PLAYBACK_DSP)|(1<<PCMDUMP_LOOP_PLAYBACK_RECORD));
    }
    return 0;
}

int _parse_audio_config(struct audio_config_handle *config,struct xml_handle * xmlhandle){
    param_group_t root_param;
    TiXmlElement * ele_tmp;
    const char *tmp=NULL;
    root_param = xmlhandle->param_root;

    if(NULL==root_param){
        return -1;
    }

    if(NULL!=root_param){
        ele_tmp=(TiXmlElement *)XML_get_first_sub_group(root_param);
        while(ele_tmp!=NULL){
            tmp = ele_tmp->Value();
            if(strncmp(tmp,"log",strlen("log"))==0){
                config->log_level=parse_audio_log_level(ele_tmp);
                log_level=config->log_level;
            }else if(strncmp(tmp,"card_name",strlen("card_name"))==0){
                config->card_name=strdup(ele_tmp->Attribute("val"));
            }else if(strncmp(tmp,"support_24bits_output",strlen("support_24bits_output"))==0){
                tmp=ele_tmp->Attribute("val");
                if(NULL!=tmp){
                    config->support_24bits=string_to_value(tmp);
                }else{
                    config->support_24bits=false;
                }
            }else if(strncmp(tmp,"nr",strlen("nr"))==0){
                parse_audionr_config(ele_tmp,config);
            }else if(strncmp(tmp,"mic_config",strlen("mic_config"))==0){
                bool enable=false;
                tmp=ele_tmp->Attribute("main_mic");
                if(NULL!=tmp){
                    enable=string_to_value(tmp);
                    if(enable){
                        config->mic_switch |= 1<<0;
                    }else{
                        config->mic_switch &= ~(1<<0);
                    }
                }else{
                        config->mic_switch &= ~(1<<0);
                }

                tmp=ele_tmp->Attribute("aux_mic");
                if(NULL!=tmp){
                    enable=string_to_value(tmp);
                    if(enable){
                        config->mic_switch |= 1<<1;
                    }else{
                        config->mic_switch &= ~(1<<1);
                    }
                }else{
                    config->mic_switch &= ~(1<<1);
                }
            }else if(strncmp(tmp,"mute",strlen("mute"))==0){
                parse_audio_mute_ctl(ele_tmp,&(config->mute));
            }
            if (strcmp(tmp, "audioparam") == 0) {
                if(NULL!=config->audioparampath){
                   free(config->audioparampath);
                   config->audioparampath=NULL;
                }
                config->audioparampath=parse_audioparamconfig_path(ele_tmp);
            }
#ifdef AUDIOHAL_V4
            if (strcmp(tmp, "microphone_characteristics") == 0) {
                memset(&config->audio_data, 0, sizeof(struct audio_platform_data));
                parse_microphone_characteristic(ele_tmp,&config->audio_data);
            }
#endif
            ele_tmp = (TiXmlElement *)XML_get_next_sibling_group(ele_tmp);
        }
    }
    return 0;
}

int parse_audio_config(struct audio_config_handle *config){
    param_group_t root_param;
    struct xml_handle xmlhandle;

    if(NULL!=config->audioparampath){
       free(config->audioparampath);
       config->audioparampath=NULL;
    }

    if(NULL!=config->card_name){
       free(config->card_name);
       config->card_name=NULL;
    }

    LOG_D("parse_audio_config");
    if(access(AUDIO_DEBUG_CONFIG_TUNNING_PATH, R_OK) == 0){
        load_xml_handle(&xmlhandle, AUDIO_DEBUG_CONFIG_TUNNING_PATH);
        root_param = xmlhandle.param_root;
        if(NULL==root_param){
            load_xml_handle(&xmlhandle, AUDIO_DEBUG_CONFIG_PATH);
        }
    }else{
        load_xml_handle(&xmlhandle, AUDIO_DEBUG_CONFIG_PATH);
    }

    if((xmlhandle.param_doc==NULL)
        ||(xmlhandle.param_root==NULL)){
        LOG_E("parse_audio_config open xml failed");
        return -1;
    }

    _parse_audio_config(config,&xmlhandle);
    release_xml_handle(&xmlhandle);

    parse_audioparam_config(config,false);

    LOG_I("parse_audio_config log_level:%d mic_switch:%d",
        config->log_level,config->mic_switch);
    return 0;
}

void free_audio_config(struct audio_config_handle *config){

    if(NULL!=config->audioparampath){
       free(config->audioparampath);
       config->audioparampath=NULL;
    }

    free_audioparam_config(&config->audiotester_config);

    if(NULL!=config->card_name){
       free(config->card_name);
       config->card_name=NULL;
    }

    if(NULL!=config->mute.spk_mute){
       free(config->mute.spk_mute);
       config->mute.spk_mute=NULL;
    }

    if(NULL!=config->mute.spk2_mute){
       free(config->mute.spk2_mute);
       config->mute.spk2_mute=NULL;
    }

    if(NULL!=config->mute.handset_mute){
       free(config->mute.handset_mute);
       config->mute.handset_mute=NULL;
    }

    if(NULL!=config->mute.headset_mute){
       free(config->mute.headset_mute);
       config->mute.headset_mute=NULL;
    }

    if(NULL!=config->mute.linein_mute){
       free(config->mute.linein_mute);
       config->mute.linein_mute=NULL;
    }

    if(NULL!=config->mute.dsp_da0_mdg_mute){
       free(config->mute.dsp_da0_mdg_mute);
       config->mute.dsp_da0_mdg_mute=NULL;
    }

    if(NULL!=config->mute.dsp_da1_mdg_mute){
       free(config->mute.dsp_da1_mdg_mute);
       config->mute.dsp_da1_mdg_mute=NULL;
    }

    if(NULL!=config->mute.audio_mdg23_mute){
       free(config->mute.audio_mdg23_mute);
       config->mute.audio_mdg23_mute=NULL;
    }

}

TiXmlElement *_audio_config_ctrl(param_group_t group, const char *param_name,char *val_str)
{
    char *sub_name;
    const char *tmp=NULL;
    TiXmlElement *pre_param = NULL;

    if (group == NULL)
    {
        LOG_E("the root is %p, goup_name is %s ", group, param_name);
        return NULL;
    }

    if (param_name == NULL)
    {
        return (TiXmlElement *)group;
    }

    char *name = strdup(param_name);

    sub_name = strtok(name, "/");

    LOG_I("%s %d values:%s %s",__func__,__LINE__,((TiXmlElement *)group)->Value(),sub_name);
    TiXmlElement *param = ((TiXmlElement *)group)->FirstChildElement(sub_name);
    if (param == NULL)
    {
        LOG_E("can not find the param group %s %s, %s",((TiXmlElement *)group)->Value(),sub_name, param_name);
        free(name);
        return NULL;
    }

    do
    {
        sub_name = strtok(NULL, "/");
        LOG_I("sub_name:%s",sub_name);
        if (sub_name == NULL) {
            LOG_E("_audio_config_ctrl %s %d",__func__,__LINE__);
            break;
        }

        pre_param = param;
        LOG_I("private_get_param %s find %s",param->Value(),sub_name);
        param = param->FirstChildElement(sub_name);

        if (param == NULL){
            tmp = pre_param->Attribute(sub_name);
            if(NULL!=tmp){
                pre_param->SetAttribute(sub_name,val_str);
                param = pre_param;
                break;
            }else{
                param=NULL;
                LOG_E("_audio_config_ctrl failed");
            }
        }
    }while (param != NULL);

    free(name);

    return param;
}

int audio_config_ctrl(void *dev,UNUSED_ATTR struct str_parms *paramin,UNUSED_ATTR int opt, char * kvpair){
    struct tiny_audio_device *adev = (struct tiny_audio_device *)dev;
    struct xml_handle xmlhandle;
    struct xml_handle *xml=NULL;
    struct socket_handle *tunning=NULL;
    int ret=0;

    char *eq = strchr(kvpair, ',');
    char *svalue = NULL;
    char *key = NULL;
    TiXmlElement *tmpgroup = NULL;
    if (eq) {
        key = strndup(kvpair, eq - kvpair);
        if (*(++eq)) {
            svalue = strdup(eq);
        }
    }

    LOG_I("audio_config_ctrl kvpair:%s key:%s valuse:%s\n\n",
        kvpair,key,svalue);

    if((adev->dev_ctl==NULL)||(adev->dev_ctl->audio_param==NULL)){
        LOG_E("audio_config_ctrl failed");
        ret=-1;
        if(NULL!=key){
            free(key);
            key=NULL;
        }

        if(NULL!=svalue){
            free(svalue);
            svalue=NULL;
        }
        return ret;
    }

    tunning=&(adev->dev_ctl->audio_param->tunning);

    if(tunning->wire_connected==true){
        if((adev->dev_ctl!=NULL)
            &&(adev->dev_ctl->audio_param!=NULL)){
            xml=(struct xml_handle *)&tunning->audio_config_xml;
        }
    }else{
        xml=&xmlhandle;
        xml->first_name=NULL;
        xml->param_root=NULL;
        xml->param_doc=NULL;
    }

    if(NULL==xml->param_root){
        LOG_I("audio_config_ctrl load_xml_handle");
        if(access(AUDIO_DEBUG_CONFIG_TUNNING_PATH, R_OK) == 0){
            load_xml_handle(xml, AUDIO_DEBUG_CONFIG_TUNNING_PATH);
            if(NULL==xml->param_root){
                load_xml_handle(xml, AUDIO_DEBUG_CONFIG_PATH);
            }
        }else{
            load_xml_handle(xml, AUDIO_DEBUG_CONFIG_PATH);
        }
    }

    if(NULL!=xml->param_root){
        tmpgroup = (TiXmlElement *)_audio_config_ctrl(xml->param_root, key, svalue);
    }else{
        LOG_E("audio_config_ctrl param_root is null");\
        ret=-1;
        goto exit;
    }


    if(NULL == tmpgroup){
        LOG_E("audio_config_ctrl ERR key:%s values;%s",key,svalue);
        ret=-1;
    }else{
        free_audio_config(&adev->dev_ctl->config);
        _parse_audio_config(&adev->dev_ctl->config,xml);
    }

exit:
    if(tunning->wire_connected==false){
        release_xml_handle(xml);
    }

    if(NULL!=key){
        free(key);
        key=NULL;
    }

    if(NULL!=svalue){
        free(svalue);
        svalue=NULL;
    }

    return ret;
}


long getCurrentTimeUs(void)
{
   struct timeval tv;
   gettimeofday(&tv,NULL);
   return tv.tv_sec* 1000000 + tv.tv_usec;
}

unsigned int getCurrentTimeMs(void)
{
   struct timeval tv;
   gettimeofday(&tv,NULL);
   return tv.tv_sec* 1000 + tv.tv_usec/1000;
}

UNUSED_ATTR static unsigned char* hex_to_string(unsigned char *data, int size){
    unsigned char* str=NULL;
    int str_size=0;
    int i=0;
    if(size<=0){
        return NULL;
    }
    str_size=size*2+1;
    str=(unsigned char*)malloc(str_size);
    if(NULL==str){
        return NULL;
    }

    for(i=0;i<size;i++){
        sprintf((char *)(str + i*2), "%02x", data[i]);
    }
    return str;
}

int string_to_hex(unsigned char * dst,const  char *str, int max_size){
    int size=0;
    const  char *tmp=str;
    unsigned char data=0;
    unsigned char char_tmp=0;
    while(1){
        char_tmp=(unsigned char)*tmp;
        if(char_tmp=='\0'){
            break;
        }
        if((char_tmp>='0') && (char_tmp<='9')){
            data= (char_tmp-'0')<<4;
        }else if((char_tmp>='a') && (char_tmp<='f')){
            data= ((char_tmp-'a')+10)<<4;
        }else if((char_tmp>='A') && (char_tmp<='F')){
            data= ((char_tmp-'A')+10)<<4;
        }else{
            break;
        }

        char_tmp=(unsigned char)*(tmp+1);
        if(char_tmp=='\0'){
            break;
        }

        if((char_tmp>='0') && (char_tmp<='9')){
            data |= (char_tmp-'0');
        }else if((char_tmp>='a') && (char_tmp<='f')){
            data |= ((char_tmp-'a')+10);
        }else if((char_tmp>='A') && (char_tmp<='F')){
            data |= (char_tmp-'A')+10;
        }else{
            break;
        }

        tmp+=2;
        dst[size++]=data;

        if(size>=max_size){
            break;
        }

        data=0;
    }
    return size;
}

void wirte_buffer_to_log(void *buffer,int size){
    char *line=NULL;
    char *tmpstr;
    char *data_buf=(char *)malloc(size+1);
    if(NULL!=data_buf){
        memcpy(data_buf, buffer, size);
        *(data_buf + size) = '\0';
        line = strtok_r(data_buf, REG_SPLIT, &tmpstr);
        while (line != NULL) {
            ALOGI("%s\n",line);
            line = strtok_r(NULL, REG_SPLIT, &tmpstr);
        }
        free(data_buf);
    }else{
        ALOGW("wirte_buffer_to_log malloc buffer failed");
    }
}

#ifdef __cplusplus
}
#endif
