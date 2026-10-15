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

#define LOG_TAG "audio_hw_parse"
#define LOG_NDEBUG 0
#include "audio_hw.h"
#include "audio_control.h"
#include "audio_param/audio_param.h"
#include "audio_xml_utils.h"
#include <tinyxml.h>
#ifdef AUDIOHAL_V4
#include <log/log.h>
#else
#include <cutils/log.h>
#endif

#include<stdlib.h>
#ifdef __cplusplus
extern "C" {
#endif
void freee_audio_route(struct audio_route *route);

#define SPRDPHONE_CARD "sprdphone4adnc"

extern int get_snd_card_number(const char *card_name);

static struct mixer_control *alloc_mixer_control(struct device_control *control)
{
    struct mixer_control *new_ctl;
    new_ctl = (struct mixer_control *) realloc(control->ctl,
              (control->ctl_size + 1) * sizeof(struct mixer_control));
    if (!new_ctl) {
        LOG_E("failed to realloc mixer control");
        return NULL;
    } else {
        control->ctl = new_ctl;
    }
    control->ctl[control->ctl_size].ctl = NULL;
    control->ctl[control->ctl_size].name = NULL;
    control->ctl[control->ctl_size].value = 0;
    return &control->ctl[control->ctl_size++];
}

static int parse_mixer_control(struct mixer *mixer,
                               struct device_control *control, TiXmlElement *mixer_element)
{
    const char *ctl_name =NULL;
    const char *val_conut=NULL;
    int value;
    struct mixer_control *mixer_ctl;

    const char *str=NULL;
    ctl_name = mixer_element->Attribute("name");
    if (ctl_name == NULL) {
        LOG_I("the mixer ctl :%s ctl_name is NULL",mixer_element->Value());
        return -1;
    }

    str=mixer_element->Attribute("val");
    if (str == NULL) {
        LOG_I("the mixer ctl :%s val is NULL",mixer_element->Value());
        return -1;
    }

    mixer_ctl = alloc_mixer_control(control);
    if (mixer_ctl == NULL) {
        LOG_E("%s alloc_mixer_control failed",mixer_element->Value());
        return -1;
    }
    mixer_ctl->name = strdup(ctl_name);
    mixer_ctl->strval=NULL;

    value = atoi(str);
    if (!value && strcmp(str, "0") != 0)
        mixer_ctl->strval = strdup(str);
    else
        mixer_ctl->value=value;

    mixer_element->Attribute("val", &value);
    mixer_ctl->val_count = 1;
    val_conut=mixer_element->Attribute("count");
    if(NULL!=val_conut){
        mixer_ctl->val_count=strtoul(val_conut,NULL,16);
    }
    mixer_ctl->ctl = mixer_get_ctl_by_name(mixer, ctl_name);
    LOG_D("parse mixer control, Mixer=%s, Val=%d",ctl_name, value);
    return 0;
}

static int parse_route_control(struct mixer *mixer,
                               struct device_control *control, TiXmlElement *device)
{
    TiXmlElement *ctl = device->FirstChildElement();
    for (; ctl != NULL; ctl = ctl->NextSiblingElement()) {
        parse_mixer_control(mixer, control, ctl);
    }
    return 0;
}

static struct device_route *alloc_route_device(struct device_route_handler
        *route)
{
    struct device_route *new_route;
    new_route = (struct device_route *)realloc(route->route,
                (route->size + 1) * sizeof(struct device_route));
    if (new_route == NULL) {
        LOG_E("alloc device_route failed");
        return NULL;
    } else {
        route->route = new_route;
    }
    route->route[route->size ].name = NULL;
    route->route[route->size ].ctl_on.name = NULL;
    route->route[route->size ].ctl_on.ctl = NULL;
    route->route[route->size ].ctl_on.ctl_size = 0;
    route->route[route->size ].ctl_off.name = NULL;
    route->route[route->size ].ctl_off.ctl = NULL;
    route->route[route->size ].ctl_off.ctl_size = 0;
    return &route->route[route->size++];
}

static struct vbc_device_route *alloc_vbc_route_device(struct vbc_device_route_handler
        *route)
{
    struct vbc_device_route *new_route;
    new_route = (struct vbc_device_route *)realloc(route->route,
                (route->size + 1) * sizeof(struct vbc_device_route));
    if (new_route == NULL) {
        LOG_E("alloc vbc_device_route failed");
        return NULL;
    } else {
        route->route = new_route;
    }
    route->route[route->size ].name = NULL;
    route->route[route->size ].ctl_on.name = NULL;
    route->route[route->size ].ctl_on.ctl = NULL;
    route->route[route->size ].ctl_on.ctl_size = 0;
    route->route[route->size ].ctl_off.name = NULL;
    route->route[route->size ].ctl_off.ctl = NULL;
    route->route[route->size ].ctl_off.ctl_size = 0;
    route->route[route->size ].devices[0] = 0;
    route->route[route->size ].devices[1] = 0;
    route->route[route->size ].devices[2] = 0;

    return &route->route[route->size++];
}

static int parse_route_device(struct device_route_handler *route_hanler,
                              struct mixer *mixer, TiXmlElement *device)
{
    struct device_route *route = NULL;
    const char *tmp=NULL;

    route_hanler->route = NULL;
    route_hanler->size = 0;
    route_hanler->pre_in_ctl = NULL;
    route_hanler->pre_out_ctl = NULL;
    for (; device != NULL; device = device->NextSiblingElement()){
        route = alloc_route_device(route_hanler);
        if (route == NULL) {
            LOG_E("the device route is NULL");
            return -1;
        }
        route->name = strdup(device->Value());
        tmp=device->Attribute("device");
        if(tmp!=NULL){
            route->devices = strtoul(device->Attribute("device"),NULL,16);
        }else{
            LOG_D("parse_route_device, Device=%s ERR", route->name);
            route->devices=-1;
        }

        LOG_D("parse_route_device, Device=%s", route->name);
        TiXmlElement *ctl_on = device->FirstChildElement("on");
        if (ctl_on != NULL) {
            parse_route_control(mixer, &route->ctl_on, ctl_on);
        } else {
            LOG_E("ctl on is NULL, device=%s", device->Value());
        }
        TiXmlElement *ctl_off = device->FirstChildElement("off");
        if (ctl_off != NULL) {
            parse_route_control(mixer, &route->ctl_off, ctl_off);
        } else {
            LOG_E("ctl off is NULL, device=%s", device->Value());
        }
    }
    return 0;
}


static int parse_vbc_devices_config(const char *device_str){
    int devices=0;
    char *tmp=NULL;
    char *tmp1=NULL;
    if(NULL==device_str){
        return 0;
    }

    tmp=strdup(device_str);
    tmp1 = strtok(tmp, ",");
    while(tmp1!=NULL){
        devices |=strtoul(tmp1,NULL,0);
        tmp1 = strtok(NULL, ",");
    }

    if(NULL!=tmp){
        free(tmp);
        tmp=NULL;
    }
    LOG_D("parse_vbc_devices_config devices:0x%x str(%s)",devices,device_str);
    return devices;
}

static int parse_vbc_route_device(struct vbc_device_route_handler *route_hanler,
                              struct mixer *mixer, TiXmlElement *device)
{
    struct vbc_device_route *route = NULL;
    const char *tmp=NULL;

    route_hanler->route = NULL;
    route_hanler->size = 0;
    route_hanler->pre_in_ctl = NULL;
    route_hanler->pre_out_ctl = 0;

    for (; device != NULL; device = device->NextSiblingElement()){
        route = alloc_vbc_route_device(route_hanler);
        if (route == NULL) {
            LOG_E("parse_vbc_route_device the device route is NULL");
            return -1;
        }
        route->name = strdup(device->Value());
        tmp=device->Attribute("device00");
        if(tmp!=NULL){
            route->devices[0] = parse_vbc_devices_config(tmp);
        }else{
            LOG_V("parse_vbc_route_device, Device=%s ERR", route->name);
            route->devices[0]=0;
        }

        tmp=device->Attribute("device01");
        if(tmp!=NULL){
            route->devices[1] = parse_vbc_devices_config(tmp);
        }else{
            LOG_V("parse_vbc_route_device, Device=%s ERR", route->name);
            route->devices[1]=0;
        }

        tmp=device->Attribute("device02");
        if(tmp!=NULL){
            route->devices[2] = parse_vbc_devices_config(tmp);
        }else{
            LOG_V("parse_vbc_route_device, Device=%s ERR", route->name);
            route->devices[2]=0;
        }

        LOG_V("parse_vbc_route_device, Device=%s", route->name);
        TiXmlElement *ctl_on = device->FirstChildElement("on");
        if (ctl_on != NULL) {
            parse_route_control(mixer, &route->ctl_on, ctl_on);
        } else {
            LOG_W("parse_vbc_route_device ctl on is NULL, device=%s", device->Value());
        }
        TiXmlElement *ctl_off = device->FirstChildElement("off");
        if (ctl_off != NULL) {
            parse_route_control(mixer, &route->ctl_off, ctl_off);
        } else {
            LOG_I("parse_vbc_route_device ctl off is NULL, device=%s", device->Value());
        }
    }
    return 0;
}

static struct dsploop_control *alloc_dsploop_control( struct private_dsploop_control *dsp_ctl)
{
    struct dsploop_control *new_priv_ctl;
    new_priv_ctl = (struct dsploop_control *)realloc(dsp_ctl->dsp_ctl,
                   (dsp_ctl->size + 1) * sizeof(struct dsploop_control));
    if (!new_priv_ctl) {
        LOG_E("alloc private control failed, error");
        return NULL;
    } else {
        dsp_ctl->dsp_ctl = new_priv_ctl;
    }

    dsp_ctl->dsp_ctl[dsp_ctl->size].ctl_size = 0;
    dsp_ctl->dsp_ctl[dsp_ctl->size].type = -1;
    dsp_ctl->dsp_ctl[dsp_ctl->size].rate = -1;
    dsp_ctl->dsp_ctl[dsp_ctl->size].ctl = NULL;
    return &dsp_ctl->dsp_ctl[dsp_ctl->size++];
}

static void do_parse_dsploop_control(struct private_dsploop_control *dsploop_ctl,
                                     struct mixer *mixer, TiXmlElement *priv)
{
    struct dsploop_control *dev_ctl = alloc_dsploop_control(dsploop_ctl);
    struct device_control control;
    const char *type_str=NULL;
    if (dev_ctl == NULL) {
        return;
    }

    control.ctl_size=0;
    control.ctl=NULL;
    control.name=NULL;

    type_str=priv->Attribute("val");
    if(NULL==type_str){
        LOG_E("do_parse_dsploop_control not fined %s Attribute:val",priv->Value());
        return ;
    }
    dev_ctl->type = strtoul(type_str,NULL,16);

    type_str = priv->Attribute("mode");
    if (NULL != type_str) {
        dev_ctl->mode = strtoul(type_str,NULL,0);
    } else {
        dev_ctl->mode = -1;
    }

    type_str=priv->Attribute("rate");
    if(NULL!=type_str){
        dev_ctl->rate = strtoul(type_str,NULL,0);
    }else{
        dev_ctl->rate = -1;
    }

    TiXmlElement *ctl = priv->FirstChildElement();
    for (; ctl != NULL; ctl = ctl->NextSiblingElement()) {
        parse_mixer_control(mixer, &control, ctl);
        dev_ctl->ctl_size=control.ctl_size;
        dev_ctl->ctl=control.ctl;
    }
}

static int parse_dsploop_control(struct private_dsploop_control *dsploop_ctl,
                                 struct mixer *mixer, TiXmlElement *privs)
{
    dsploop_ctl->size = 0;
    dsploop_ctl->dsp_ctl = NULL;
    TiXmlElement *ctl = privs->FirstChildElement();
    for (; ctl != NULL; ctl = ctl->NextSiblingElement()) {
        do_parse_dsploop_control(dsploop_ctl, mixer, ctl);
    }
    return 0;
}

static void free_mixer_control(struct mixer_control *ctl_mixer)
{
    if(ctl_mixer->name) {
        free(ctl_mixer->name);
        ctl_mixer->name = NULL;
    }

    if(ctl_mixer->strval) {
        free(ctl_mixer->strval);
        ctl_mixer->strval = NULL;
    }
    ctl_mixer->value=0;
    ctl_mixer->val_count=0;
    ctl_mixer->ctl=NULL;
}

int free_dsploop_control(struct private_dsploop_control *dsploop_ctl)
{
    int i=0;
    int j=0;

    for(i=0;i<dsploop_ctl->size;i++){
        if(dsploop_ctl->dsp_ctl[i].ctl!=NULL){

            for(j=0;j<dsploop_ctl->dsp_ctl[i].ctl_size;j++){
                free_mixer_control(&dsploop_ctl->dsp_ctl[i].ctl[j]);
            }
            free(dsploop_ctl->dsp_ctl[i].ctl);
            dsploop_ctl->dsp_ctl[i].ctl=NULL;
            dsploop_ctl->dsp_ctl[i].ctl_size=0;
        }
    }

    if(dsploop_ctl->dsp_ctl!=NULL){
        free(dsploop_ctl->dsp_ctl);
    }

    dsploop_ctl->dsp_ctl=NULL;
    dsploop_ctl->size=0;
    return 0;
}

static int parse_compress_config(struct _compr_config *compress, TiXmlElement *priv)
{
    const char *tmp=NULL;
    tmp=priv->Attribute("fragment_size");
    if(NULL!=tmp){
        compress->fragment_size=strtoul(tmp,NULL,16);
    }

    tmp=priv->Attribute("fragments");
    if(NULL!=tmp){
        compress->fragments=strtoul(tmp,NULL,16);
    }

    tmp=priv->Attribute("device");
    if(NULL!=tmp){
        compress->devices=strtoul(tmp,NULL,16);
    }

    return 0;
}

static int parse_pcm_config(struct pcm_config *config, int *devices, TiXmlElement *priv)
{
    TiXmlElement *pcm_tmp = NULL;
    unsigned char *tmp = NULL;
    int pcm_config_buf[AUD_PCM_ATTRIBUTE_MAX] = {0};
    int i, j;

    int config_id = -1;
    LOG_D("parse_pcm_config\n");
    pcm_tmp = NULL;
    for(i = 0; i < AUD_PCM_MAX; i++) {
        pcm_tmp = priv->FirstChildElement(pcm_config_name[i]);

        if(pcm_tmp == NULL) {
            LOG_W("parse_pcm_config find failed string:%s", pcm_config_name[i]);
            devices[i] = -1;
            continue;
        } else {
            for(j = 0; j < AUD_PCM_ATTRIBUTE_MAX; j++) {
                tmp = (unsigned char *)pcm_tmp->Attribute(pcm_config_attribute[j]);
                if(tmp != NULL) {
                    pcm_config_buf[j] = strtoul((const char *)tmp, NULL, 10);
                } else {
                    pcm_config_buf[j] = 0;
                }
            }

            config_id = i;
            config[config_id].channels = pcm_config_buf[AUD_PCM_ATTRIBUTE_CHANNELS];
            config[config_id].rate = pcm_config_buf[AUD_PCM_ATTRIBUTE_RATE];
            config[config_id].period_size = pcm_config_buf[AUD_PCM_ATTRIBUTE_PERIOD_SIZE];
            config[config_id].period_count = pcm_config_buf[AUD_PCM_ATTRIBUTE_PERIOD_COUNT];
            config[config_id].format = (enum pcm_format )pcm_config_buf[AUD_PCM_ATTRIBUTE_FORMAT];
            config[config_id].start_threshold = pcm_config_buf[AUD_PCM_ATTRIBUTE_START_THRESHOLD];
            config[config_id].stop_threshold = pcm_config_buf[AUD_PCM_ATTRIBUTE_STOP_THRESHOLD];
            config[config_id].silence_threshold = pcm_config_buf[AUD_PCM_ATTRIBUTE_SILENCE_THRESHOLD];
            config[config_id].avail_min = pcm_config_buf[AUD_PCM_ATTRIBUTE_AVAIL_MIN];
            devices[config_id] = pcm_config_buf[AUD_PCM_ATTRIBUTE_DEVICES];

            LOG_V("pcm config[%d] channels:%d rate:%d period_size:%d period_count:%d format:%d start:%d stop:%d silence:%d avail_min:%d device:%d",
                  config_id,
                  config[config_id].channels,
                  config[config_id].rate,
                  config[config_id].period_size,
                  config[config_id].period_count,
                  config[config_id].format,
                  config[config_id].start_threshold,
                  config[config_id].stop_threshold,
                  config[config_id].silence_threshold,
                  config[config_id].avail_min,
                  devices[config_id]
                 );
        }

    }

    return 0;
}

static int parse_recordpcm_config(struct pcm_config *config, int *devices, TiXmlElement *priv)
{
    TiXmlElement *pcm_tmp = NULL;
    unsigned char *tmp = NULL;
    int pcm_config_buf[AUD_PCM_ATTRIBUTE_MAX] = {0};
    int i, j;

    int config_id = -1;
    LOG_D("parse_recordpcm_config\n");
    pcm_tmp = NULL;
    for(i = 0; i < AUD_RECORD_PCM_MAX; i++) {
        pcm_tmp = priv->FirstChildElement(recordpcm_config_name[i]);

        if(pcm_tmp == NULL) {
            LOG_E("parse_pcm_config find failed string:%s", recordpcm_config_name[i]);
            devices[i] = -1;
            continue;
        } else {
            for(j = 0; j < AUD_PCM_ATTRIBUTE_MAX; j++) {
                tmp = (unsigned char *)pcm_tmp->Attribute(pcm_config_attribute[j]);
                if(tmp != NULL) {
                    pcm_config_buf[j] = strtoul((const char *)tmp, NULL, 10);
                } else {
                    pcm_config_buf[j] = 0;
                }
            }

            config_id = i;
            config[config_id].channels = pcm_config_buf[AUD_PCM_ATTRIBUTE_CHANNELS];
            config[config_id].rate = pcm_config_buf[AUD_PCM_ATTRIBUTE_RATE];
            config[config_id].period_size = pcm_config_buf[AUD_PCM_ATTRIBUTE_PERIOD_SIZE];
            config[config_id].period_count = pcm_config_buf[AUD_PCM_ATTRIBUTE_PERIOD_COUNT];
            config[config_id].format = (enum pcm_format )pcm_config_buf[AUD_PCM_ATTRIBUTE_FORMAT];
            config[config_id].start_threshold = pcm_config_buf[AUD_PCM_ATTRIBUTE_START_THRESHOLD];
            config[config_id].stop_threshold = pcm_config_buf[AUD_PCM_ATTRIBUTE_STOP_THRESHOLD];
            config[config_id].silence_threshold = pcm_config_buf[AUD_PCM_ATTRIBUTE_SILENCE_THRESHOLD];
            config[config_id].avail_min = pcm_config_buf[AUD_PCM_ATTRIBUTE_AVAIL_MIN];
            devices[config_id] = pcm_config_buf[AUD_PCM_ATTRIBUTE_DEVICES];

            LOG_D("record pcm config[%d] channels:%d rate:%d period_size:%d period_count:%d format:%d start:%d stop:%d silence:%d avail_min:%d device:%d",
                  config_id,
                  config[config_id].channels,
                  config[config_id].rate,
                  config[config_id].period_size,
                  config[config_id].period_count,
                  config[config_id].format,
                  config[config_id].start_threshold,
                  config[config_id].stop_threshold,
                  config[config_id].silence_threshold,
                  config[config_id].avail_min,
                  devices[config_id]
                 );
        }

    }

    return 0;
}


int parse_audio_pcm_config(struct pcm_handle_t *pcm)
{
    TiXmlElement *root;
    TiXmlElement *pcm_tmp;

    LOG_D("parse_audio_pcm_config\n");
    memset(pcm, 0, sizeof(struct pcm_handle_t));
    TiXmlDocument *doc = new TiXmlDocument();
    if (!doc->LoadFile(AUDIO_PCM_FILE)) {
        LOG_E("failed to load the %s", AUDIO_PCM_FILE);
        delete doc;
        return -1;
    }

    root = doc->FirstChildElement();
    if(root == NULL) {
        LOG_E("find  root failed string:");
        return -1;
    }

    pcm_tmp = NULL;
    pcm_tmp = root->FirstChildElement("playback");
    if(pcm_tmp == NULL) {
        LOG_E("find  playback failed string:%s", root->GetText());
    } else {
        LOG_D("parse_audio_pcm_config :%s", pcm_tmp->Value());
        parse_pcm_config(pcm->play, pcm->playback_devices,pcm_tmp);
    }

    pcm_tmp = NULL;
    pcm_tmp = root->FirstChildElement("record");
    if(pcm_tmp == NULL) {
        LOG_E("find  record failed string:%s", root->GetText());
    } else {
        LOG_D("parse_audio_pcm_config :%s", pcm_tmp->Value());
        parse_recordpcm_config(pcm->record,pcm->record_devices, pcm_tmp);
    }

    pcm_tmp = NULL;
    pcm_tmp = root->FirstChildElement("compress");
    if(pcm_tmp == NULL) {
        LOG_E("find  record failed string:%s", root->GetText());
    } else {
        LOG_D("parse_compress_config :%s", pcm_tmp->Value());
        parse_compress_config(&(pcm->compress),pcm_tmp);
    }

    delete doc;
    return 0;
}

int parse_audio_route(struct audio_control *control)
{
    TiXmlElement *root;
    TiXmlElement *devices;
    TiXmlElement *dev;
    TiXmlElement *tmp;
    TiXmlDocument *doc = new TiXmlDocument();
    int ret=0;
    if (!doc->LoadFile(AUDIO_ROUTE_PATH)) {
        LOG_E("failed to load the route xml:%s",AUDIO_ROUTE_PATH);
        return -1;
    }

    LOG_D("parse_audio_route");

    devices=NULL;
    root = doc->FirstChildElement();
    devices = root->FirstChildElement("devices");
    if(NULL==devices){
        LOG_E("parse_audio_route devices not find");
        ret=-1;
        goto out;
    }else{
        LOG_D("parse_audio_route devices");
        dev = devices->FirstChildElement();
        parse_route_device(&(control->route.devices_route), control->mixer, dev);
    }

    devices = root->FirstChildElement("vbc_iis_mux");
    if(NULL==devices){
        LOG_E("parse_audio_route vbc_iis_mux not find");
        //ret=-1;
        //goto out;
    }else{
        LOG_D("parse_audio_route vbc_iis_mux");
        dev = devices->FirstChildElement();
        parse_vbc_route_device(&(control->route.vbc_iis_mux_route), control->mixer, dev);
    }

    devices = root->FirstChildElement("be_switch");
    if(NULL==devices){
        LOG_E("parse_audio_route be_switch not find");
        //ret=-1;
        //goto out;
    }else{
        LOG_D("parse_audio_route be_switch");
        dev = devices->FirstChildElement();
        parse_vbc_route_device(&(control->route.be_switch_route), control->mixer, dev);
    }

    tmp = root->FirstChildElement("dsploop");
    if(NULL!=tmp){
        parse_dsploop_control(&(control->route.dsploop_ctl), control->mixer,
                          tmp);
    }else{
        LOG_E("parse_audio_route dsploop not find");
        ret=-1;
        goto out;
    }

    LOG_I("parse_route_device vbc_iis");
    tmp = root->FirstChildElement("vbc_iis");
    if(NULL!=tmp){
        dev = tmp->FirstChildElement();
        if(NULL!=dev){
            parse_route_device(&(control->route.vbc_iis),control->mixer,
                          dev);
        }
    }else{
        LOG_E("parse_audio_route vbc_iis not find");
        ret=-1;
        goto out;
    }


    tmp = root->FirstChildElement("usecase_device");
    if(NULL!=tmp){
        dev = tmp->FirstChildElement();
        if(NULL!=dev){
            parse_route_device(&(control->route.usecase_ctl),control->mixer,
                          dev);
        }
    }else{
        LOG_W("parse_audio_route vbc_iis not find");
    }

    LOG_I("parse_route_device vbc_pcm_dump");
    tmp = root->FirstChildElement("vbc_pcm_dump");
    if(NULL!=tmp){
        dev = tmp->FirstChildElement();
        if(NULL!=dev){
            parse_route_device(&(control->route.vbc_pcm_dump),control->mixer,
                          dev);
        }
    }

    LOG_I("parse_route_device fbsmartamp");
    tmp = root->FirstChildElement("smartamp");
    if(NULL!=tmp){
        dev = tmp->FirstChildElement();
        if(NULL!=dev){
            parse_route_device(&(control->route.fb_smartamp),control->mixer,
                          dev);
        }
    }

out:
    delete doc;

    if(ret!=0){
        freee_audio_route(&control->route);
    }
    return ret;
}

static struct device_gain *alloc_device_gain(struct device_usecase_gain *gain)
{
    struct device_gain *new_gain;
    new_gain = (struct device_gain *) realloc(gain->dev_gain,
               (gain->gain_size + 1) * sizeof(struct device_gain));
    if (!new_gain) {
        LOG_E("alloc device gain failed");
        return NULL;
    } else {
        gain->dev_gain = new_gain;
        LOG_D("alloc_device_gain:%p", new_gain);
    }
    gain->dev_gain[gain->gain_size].name = NULL;
    gain->dev_gain[gain->gain_size].ctl_size = 0;
    gain->dev_gain[gain->gain_size].ctl = NULL;
    return & gain->dev_gain[gain->gain_size++];
}

static struct gain_mixer_control *alloc_gain_mixer_control(
    struct device_gain *gain)
{
    struct gain_mixer_control *new_gain_mixer;
    new_gain_mixer = (struct gain_mixer_control *) realloc(gain->ctl,
                     (gain->ctl_size + 1) * sizeof(struct gain_mixer_control));
    if (!new_gain_mixer) {
        return NULL;
    } else {
        gain->ctl = new_gain_mixer;
    }
    gain->ctl[gain->ctl_size].name = NULL;
    gain->ctl[gain->ctl_size].volume_size = 0;
    gain->ctl[gain->ctl_size].ctl = NULL;
    gain->ctl[gain->ctl_size].volume_value = NULL;
    return &gain->ctl[gain->ctl_size++];
}

static int *alloc_gain_volume(struct gain_mixer_control *gain_mixer)
{
    int *new_volume;
    new_volume = (int *) realloc(gain_mixer->volume_value,
                                 (gain_mixer->volume_size + 1) * sizeof(int));
    if (!new_volume) {
        LOG_E("alloc gain volume failed");
        return NULL;
    } else {
        gain_mixer->volume_value = new_volume;
    }
    return &gain_mixer->volume_value[gain_mixer->volume_size++];
}

static int parse_gain_volume(struct gain_mixer_control *gain_mixer,
                             TiXmlElement *mixer_element)
{
    int *volume = alloc_gain_volume(gain_mixer);
    *volume = strtoul(mixer_element->Attribute(VALUE),NULL,16);
    return 0;
}

static int parse_gain_mixer(struct mixer *mixer, struct device_gain *gain,
                            TiXmlElement *mixer_element)
{
    const char *svalue = NULL;
    const char *ctl_name = NULL;
    struct gain_mixer_control *gain_mixer = alloc_gain_mixer_control(gain);
    if (gain_mixer == NULL) {
        LOG_E("gain_mixer_control is NULL");
        return -1;
    }
    ctl_name = mixer_element->Attribute("ctl");
    if (ctl_name == NULL) {
        LOG_E("can not find the clt_name");
        return -1;
    }
    gain_mixer->name = strdup(ctl_name);
    gain_mixer->ctl = mixer_get_ctl_by_name(mixer, ctl_name);
    if ((svalue = mixer_element->Attribute(VALUE)) != NULL) {
        int *volume = alloc_gain_volume(gain_mixer);
        volume[0] = strtoul(svalue,NULL,16);
    } else {
        TiXmlElement *volume;
        volume = mixer_element->FirstChildElement();
        for (; volume != NULL; volume = volume->NextSiblingElement()) {
            parse_gain_volume(gain_mixer, volume);
        }
    }

    return 0;
}


int free_device_gain(struct device_usecase_gain *use_gain)
{
    struct gain_mixer_control *ctl;
    int i=0;
    int j=0;

    if((use_gain->gain_size>0)
        &&(use_gain->dev_gain!=NULL)){

        for(i=0;i<use_gain->gain_size;i++){
            if(use_gain->dev_gain[i].name!=NULL){
                free(use_gain->dev_gain[i].name);
                use_gain->dev_gain[i].name=NULL;
            }
            for(j=0;j<use_gain->dev_gain[i].ctl_size;j++){
                ctl=&use_gain->dev_gain[i].ctl[j];
                if(ctl->name!=NULL){
                    free(ctl->name);
                    ctl->name=NULL;
                }
        
                if(ctl->volume_value!=NULL){
                    free(ctl->volume_value);
                    ctl->volume_value=NULL;
                }
            }
            free(use_gain->dev_gain[i].ctl);
        }
        free(use_gain->dev_gain);
        use_gain->dev_gain=NULL;
        use_gain->gain_size=0;
    }
    return 0;
}

int parse_device_gain(struct device_usecase_gain *use_gain,
                             struct mixer *mixer, TiXmlElement *device)
{
    struct device_gain *gain = NULL;
    if(NULL==use_gain->dev_gain){
        LOG_I("parse_device_gain dev_gain is null");
    }
    gain = alloc_device_gain(use_gain);
    gain->name = strdup(device->Value());
    gain->id=use_gain->gain_size-1;

    TiXmlElement *_device_gain = device->FirstChildElement();
    for (; _device_gain != NULL; _device_gain = _device_gain->NextSiblingElement()) {
        parse_gain_mixer(mixer, gain, _device_gain);
    }

    LOG_D("parse_device_gain %p %d %s gain_size:%d %p %p",
          gain, gain->ctl_size,    gain->name, use_gain->gain_size, use_gain->dev_gain,
          &(use_gain->dev_gain[0]));
    return 0;
}

static void free_device_control(struct device_control *ctl)
{
    unsigned int i=0;
    if(NULL== ctl)
        return ;

    if (ctl->name) {
        free(ctl->name);
        ctl->name = NULL;
    }

    if((ctl->ctl_size>0)&&(ctl->ctl!=NULL)){
        for(i = 0; i < ctl->ctl_size; i++) {
            free_mixer_control(&(ctl->ctl[i]));
        }
        free(ctl->ctl);
        ctl->ctl=NULL;
        ctl->ctl_size=0;
    }
}

void free_device_route(struct device_route_handler *reoute_handler)
{
    unsigned int i=0;
    struct device_route *route = NULL;

    if((reoute_handler->route==NULL)||(reoute_handler->size==0)){
        return;
    }

    for (i = 0; i < reoute_handler->size; i++) {
        route = &(reoute_handler->route[i]);
        if (route->name) {
            free(route->name);
            route->name = NULL;
        }
        free_device_control(&(route->ctl_on));
        free_device_control(&(route->ctl_off));
    }

    free(reoute_handler->route);
    reoute_handler->route=NULL;
    reoute_handler->size=0;
}

static void free_vbc_device_route(struct vbc_device_route_handler *reoute_handler)
{
    unsigned int i=0;
    struct vbc_device_route *route = NULL;

    if((reoute_handler->route==NULL)||(reoute_handler->size==0)){
        return;
    }

    for (i = 0; i < reoute_handler->size; i++) {
        route = &(reoute_handler->route[i]);
        if (route->name) {
            free(route->name);
            route->name = NULL;
        }
        free_device_control(&(route->ctl_on));
        free_device_control(&(route->ctl_off));
    }

    free(reoute_handler->route);
    reoute_handler->route=NULL;
    reoute_handler->size=0;
    reoute_handler->pre_in_ctl=NULL;
    reoute_handler->pre_out_ctl=0;
}

void freee_audio_route(struct audio_route *route){
    free_device_route(&route->devices_route);
    free_vbc_device_route(&route->vbc_iis_mux_route);
    free_vbc_device_route(&route->be_switch_route);
    free_dsploop_control(&route->dsploop_ctl);

    free_device_route(&route->vbc_iis);
    free_device_route(&route->usecase_ctl);
    free_device_route(&route->vbc_pcm_dump);
    free_device_route(&route->devices_route);
    free_device_route(&route->fb_smartamp);
}

int init_sprd_codec_mixer(struct sprd_codec_mixer_t *codec,struct mixer *mixer){
#ifdef NORMAL_AUDIO_PLATFORM
    return 0;
#else
    codec->adcl_capture_volume=mixer_get_ctl_by_name(mixer, "ADCL Gain ADCL Capture Volume");
    if(NULL==codec->adcl_capture_volume){
        LOG_E("Open [ADCL Gain ADCL Capture Volume] Failed");
    }

    codec->adcr_capture_volume=mixer_get_ctl_by_name(mixer, "ADCR Gain ADCR Capture Volume");
    if(NULL==codec->adcr_capture_volume){
        LOG_E("Open [ADCR Gain ADCR Capture Volume] Failed");
    }

    codec->spkl_playback_volume=mixer_get_ctl_by_name(mixer, "SPKL Gain SPKL Playback Volume");
    if(NULL==codec->spkl_playback_volume){
        LOG_E("Open [SPKL Gain SPKL Playback Volume] Failed");
    }

    codec->hpl_playback_volume=mixer_get_ctl_by_name(mixer, "HPL Gain HPL Playback Volume");
    if(NULL==codec->hpl_playback_volume){
        LOG_E("Open [HPL Gain HPL Playback Volume] Failed");
    }

    codec->hpr_playback_volume=mixer_get_ctl_by_name(mixer, "HPR Gain HPR Playback Volume");
    if(NULL==codec->hpr_playback_volume){
        LOG_E("Open [HPR Gain HPR Playback Volume] Failed");
    }

    codec->ear_playback_volume=mixer_get_ctl_by_name(mixer, "EAR Gain EAR Playback Volume");
    if(NULL==codec->ear_playback_volume){
        LOG_E("Open [EAR Gain EAR Playback Volume] Failed");
    }

    codec->inner_pa=mixer_get_ctl_by_name(mixer, "Inter PA Config");
    if(NULL==codec->inner_pa){
        LOG_E("Open [Inter PA Config] Failed");
    }

    codec->hp_inner_pa=mixer_get_ctl_by_name(mixer, "Inter HP PA Config");
    if(NULL==codec->hp_inner_pa){
        LOG_E("Open [Inter HP PA Config] Failed");
    }

    codec->dac_playback_volume=mixer_get_ctl_by_name(mixer, "DAC Gain DAC Playback Volume");
    if(NULL==codec->dac_playback_volume){
        LOG_E("Open [DAC Gain DAC Playback Volume] Failed");
    }

#endif
    return 0;
 }
//TODO we may add some folders for audio codec param
#ifdef __cplusplus
}
#endif
