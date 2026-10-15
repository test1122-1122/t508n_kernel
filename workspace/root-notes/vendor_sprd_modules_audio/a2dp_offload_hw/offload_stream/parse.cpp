#define LOG_TAG "a2dp_hw_parse"
#define LOG_NDEBUG 0
//#include "audio_xml_utils.h"
#include <tinyxml.h>
#include <log/log.h>
#include<stdlib.h>
#include "offload_stream.h"

#ifdef __cplusplus
extern "C" {
#endif
#define DEFAULT_CARD_NAME "sprdphone-sc2730"
extern int get_snd_card_number(const char *card_name);

static int parse_pcm_config(struct offload_pcm_param  *config,TiXmlElement *Ele){
    const char *attr = NULL;
    memset(config, 0, sizeof(struct offload_pcm_param));

    if(Ele == NULL) {
        return -1;
    } else {
        attr = Ele->Attribute("channels");
        if(attr != NULL) {
            config->config.channels = strtoul(attr, NULL, 10);
        }

        attr = Ele->Attribute("rate");
        if(attr != NULL) {
            config->config.rate = strtoul(attr, NULL, 10);
        }

        attr = Ele->Attribute("period_size");
        if(attr != NULL) {
            config->config.period_size = strtoul(attr, NULL, 10);
        }

        attr = Ele->Attribute("period_count");
        if(attr != NULL) {
            config->config.period_count = strtoul(attr, NULL, 10);
        }

        attr = Ele->Attribute("format");
        if(attr != NULL) {
            config->config.format = (enum pcm_format)strtoul(attr, NULL, 10);
        }

        attr = Ele->Attribute("start_threshold");
        if(attr != NULL) {
            config->config.start_threshold = strtoul(attr, NULL, 10);
        }

        attr = Ele->Attribute("channels");
        if(attr != NULL) {
            config->config.channels = strtoul(attr, NULL, 10);
        }

        attr = Ele->Attribute("stop_threshold");
        if(attr != NULL) {
            config->config.stop_threshold = strtoul(attr, NULL, 10);
        }

        attr = Ele->Attribute("silence_threshold");
        if(attr != NULL) {
            config->config.silence_threshold = strtoul(attr, NULL, 10);
        }

        attr = Ele->Attribute("avail_min");
        if(attr != NULL) {
            config->config.avail_min = strtoul(attr, NULL, 10);
        }

        attr = Ele->Attribute("card");
        if(attr != NULL) {
            config->card = get_snd_card_number(attr);
            if(config->card<0){
                config->card=get_snd_card_number(DEFAULT_CARD_NAME);
                ALOGI("parse_pcm_config use defualt card:%d",config->card);
            }
        }

        attr = Ele->Attribute("device");
        if(attr != NULL) {
            config->device = strtoul(attr, NULL, 10);
        }
    }
    return 0;
}

static int parse_offload_compress_config(struct a2dp_compress_param  *config,TiXmlElement *Ele){
    const char *attr = NULL;
    memset(config, 0, sizeof(struct a2dp_compress_param));

    if(Ele == NULL) {
        return -1;
    } else {
        attr = Ele->Attribute("fragment_size");
        if(attr != NULL) {
            config->offload_fragement_size = strtoul(attr, NULL, 0);
        }

        attr = Ele->Attribute("fragments");
        if(attr != NULL) {
            config->offload_fragements = strtoul(attr, NULL, 0);
        }

        attr = Ele->Attribute("card");
        if(attr != NULL) {
            config->card = get_snd_card_number(attr);
            if(config->card<0){
                config->card=get_snd_card_number(DEFAULT_CARD_NAME);
                ALOGI("parse_pcm_config use defualt card:%d",config->card);
            }
        }

        attr = Ele->Attribute("device");
        if(attr != NULL) {
            config->device = strtoul(attr, NULL, 0);
        }
    }
    return 0;
}

int  parse_offload_config(struct stream_param *stream_para,
    const char *config_file){
    TiXmlElement *root;
    TiXmlElement *pcm_tmp;
    TiXmlElement *tmp;
    TiXmlDocument *doc = new TiXmlDocument();

    memset(stream_para, 0, sizeof(struct stream_param));

    if (!doc->LoadFile(config_file)) {
        ALOGE("failed to load the %s", config_file);
        delete doc;
        return -1;
    }
    root = doc->FirstChildElement();
    if(root == NULL) {
        ALOGE("find  root failed");
        return -2;
    }

    pcm_tmp = NULL;
    pcm_tmp = root->FirstChildElement("offload");
    if(pcm_tmp == NULL) {
        ALOGE("find  offload failed string:%s", root->GetText());
    } else {

        pcm_tmp = pcm_tmp->FirstChildElement("a2dp");

        if(pcm_tmp == NULL) {
            ALOGE("find  failed");
        } else {
            tmp = pcm_tmp->FirstChildElement("mixer");
            if(tmp!=NULL){
                parse_pcm_config(&stream_para->mixer, tmp);
            }

            tmp = pcm_tmp->FirstChildElement("compress");
            if(tmp!=NULL){
                parse_offload_compress_config(&stream_para->sprd_out_stream, tmp);
            }
       }
    }
    return 0;
}

static struct mixer_control *alloc_mixer_control(struct device_control *control)
{
    struct mixer_control *new_ctl;
    new_ctl = (struct mixer_control *) realloc(control->ctl,
              (control->ctl_size + 1) * sizeof(struct mixer_control));
    if (!new_ctl) {
        ALOGE("failed to realloc mixer control");
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
        ALOGE("the mixer ctl :%s ctl_name is NULL",mixer_element->Value());
        return -1;
    }

    str=mixer_element->Attribute("val");
    if (str == NULL) {
        ALOGE("the mixer ctl :%s val is NULL",mixer_element->Value());
        return -1;
    }

    mixer_ctl = alloc_mixer_control(control);
    if (mixer_ctl == NULL) {
        ALOGE("%s alloc_mixer_control failed",mixer_element->Value());
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
    ALOGD("parse mixer control, Mixer=%s, Val=%d",ctl_name, value);
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

static int parse_route_device(struct device_route *route,
                              struct mixer *mixer, TiXmlElement *device)
{
    if(NULL==mixer){
        return -1;
    }

    route->name = strdup(device->Value());
    route->devices=-1;

    ALOGV("parse_route_device, Device=%s", route->name);
    TiXmlElement *ctl_on = device->FirstChildElement("on");
    if (ctl_on != NULL) {
        parse_route_control(mixer, &route->ctl_on, ctl_on);
    } else {
        ALOGE("ctl on is NULL, device=%s", device->Value());
    }
    TiXmlElement *ctl_off = device->FirstChildElement("off");
    if (ctl_off != NULL) {
        parse_route_control(mixer, &route->ctl_off, ctl_off);
    } else {
        ALOGI("ctl off is NULL, device=%s", device->Value());
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
    free(tmp);
    ALOGV("parse_vbc_devices_config devices:0x%x str(%s)",devices,device_str);
    return devices;
}

static int parse_a2dp_route_device(struct device_route *route,
                              struct mixer *mixer, TiXmlElement *device)
{
    const char *tmp=NULL;
    int device_val=0;

    for (; device != NULL; device = device->NextSiblingElement()){
        tmp=device->Attribute("device00");
        if(tmp==NULL){
            ALOGV("parse_a2dp_route_device, Device=%s ERR", route->name);
            continue;
        }else{
            device_val = parse_vbc_devices_config(tmp);
            if(device_val&AUDIO_DEVICE_BIT_IN){
                continue;
            }
            if(0==(AUDIO_DEVICE_OUT_ALL_A2DP&device_val)){
                continue;
            }
        }

        route->name = strdup(device->Value());
        route->devices = device_val;

        ALOGV("parse_a2dp_route_device, Device=%s", route->name);
        TiXmlElement *ctl_on = device->FirstChildElement("on");
        if (ctl_on != NULL) {
            parse_route_control(mixer, &route->ctl_on, ctl_on);
        } else {
            ALOGE("parse_a2dp_route_device ctl on is NULL, device=%s", device->Value());
        }
        TiXmlElement *ctl_off = device->FirstChildElement("off");
        if (ctl_off != NULL) {
            parse_route_control(mixer, &route->ctl_off, ctl_off);
        } else {
            ALOGE("parse_a2dp_route_device ctl off is NULL, device=%s", device->Value());
        }
        return 0;
    }
    return -1;
}

int parse_a2dp_route(struct a2dp_route *route, struct mixer *mixer,
    const char *config_file)
{
    TiXmlElement *root;
    TiXmlElement *private_control;
    TiXmlElement *dev;
    TiXmlDocument *doc = new TiXmlDocument();
    if (!doc->LoadFile(config_file)) {
        ALOGE("failed to load the route xml %s",config_file);
        return -1;
    }
    root = doc->FirstChildElement();

    ALOGI("parse_a2dp_route vbc_iis");
    private_control = root->FirstChildElement("vbc_iis");
    if(NULL!=private_control){
        dev = private_control->FirstChildElement("bt_offload");
        if(NULL!=dev){
            parse_route_device(&route->vbc_iis,mixer,
                          dev);
        }
    }

    private_control = root->FirstChildElement("vbc_iis_mux");
    if(NULL==private_control){
        ALOGE("parse_a2dp_route vbc_iis_mux not find");
    }else{
        ALOGD("parse_a2dp_route vbc_iis_mux");
        dev = private_control->FirstChildElement();
        parse_a2dp_route_device(&route->vbc_iis_mux_route,mixer,dev);
    }

    private_control = root->FirstChildElement("be_switch");
    if(NULL==private_control){
        ALOGE("parse_a2dp_route be_switch not find");
    }else{
        ALOGD("parse_a2dp_route be_switch");
        dev = private_control->FirstChildElement();
        parse_a2dp_route_device(&route->be_switch_route,mixer,dev);
    }

    delete doc;
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

static void free_device_route(struct device_route *route)
{
    free_device_control(&route->ctl_on);
    free_device_control(&route->ctl_off);

    if(route->name!=NULL){
        free(route->name);
        route->name=NULL;
    }
}

void free_a2dp_route(struct a2dp_route *route)
{
    free_device_route(&route->vbc_iis_mux_route);
    free_device_route(&route->be_switch_route);
    free_device_route(&route->vbc_iis);
}

#ifdef __cplusplus
}
#endif
