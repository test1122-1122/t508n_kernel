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
#define LOG_TAG "audio_hw_param"

#define LOG_NDEBUG 0

#include "audio_hw.h"
#include "fcntl.h"
#include "audio_control.h"
#include "aud_proc.h"
#include "audio_xml_utils.h"
#include "audio_param.h"
#include "tinyalsa_util.h"
#include "tinyxml.h"
#include <tinyalsa/asoundlib.h>
#include "stdint.h"
#include "audio_debug.h"
#include "smartamp.h"
#ifdef __cplusplus
extern "C" {
#endif

extern void add_path(char *dst, const char *src);
typedef int (*load_sprd_audio_param) (void *,param_group_t ele, struct param_infor_t  * param_infor,char * path);
extern int parse_device_gain(struct device_usecase_gain *use_gain,
                             struct mixer *mixer, TiXmlElement *device);
static int load_sprd_audio_pga_param(void *use_gain,param_group_t ele, param_infor_t  * param_infor,char *path);
static int load_sprd_ap_audio_param(void *param,param_group_t ele,param_infor_t  * param_infor, char *path);
static int upload_realtek_extend_param(struct mixer *mixer);
static void init_ap_audio_param(void *audio_param);
static int read_audio_firmware(const char *firmware_file,char **data);
static int init_sprd_xml(void * load_param_func_res,
                   struct xml_handle *xml,
                   const char *tunning_param_path,
                   const char *param_path,
                   param_infor_t  * param_infor,
                   load_sprd_audio_param load_param_func,bool update_param);

static const char *default_etc_param_path="/vendor/etc/audio_params/sprd";
const char *data_parm_path="/data/vendor/local/media/audio_params";
static int load_smartamp_caliparam(AUDIOVBCEQ_PARAM_T *audio_param);

static const char * audio_prfile_name[SND_AUDIO_PARAM_PROFILE_MAX] = {
    "dsp_vbc",
    "audio_structure",
    "cvs",
    "audio_pga",
    "codec",
    "audio_process",
#ifdef SPRD_AUDIO_SMARTAMP
    "dsp_smartamp"
#endif
};

const char * get_audioparam_filename(AUDIO_PARAM_T *param,int profile, int type){
    if((profile<0)||(profile>=SND_AUDIO_PARAM_PROFILE_MAX)){
        LOG_E("%s line:%d invalid profile:%d",__func__,__LINE__,profile);
        return NULL;
    }

    switch(type){
        case AUDIO_PARAM_ETC_XML:
            return param->audio_param_file_table[profile].etc_xml;
        case AUDIO_PARAM_ETC_BIN:
            return param->audio_param_file_table[profile].etc_bin;
        case AUDIO_PARAM_DATA_XML:
            return param->audio_param_file_table[profile].data_xml;
        case AUDIO_PARAM_DATA_BIN:
            return param->audio_param_file_table[profile].data_bin;
        default:
            LOG_E("%s line:%d invalid type:%d profile:%d",__func__,__LINE__,type,profile);
            break;
    }

    return NULL;
}

void init_audioparam_filename(AUDIO_PARAM_T *param){
    int i=0;
    char file_path[128]={0};
    char *etc_path;
    struct audiotester_config_handle *audiotester_config=&param->dev_ctl->config.audiotester_config;

    if(param->dev_ctl->config.audioparampath!=NULL){
        etc_path=param->dev_ctl->config.audioparampath;
    }else{
        etc_path=(char *)default_etc_param_path;
        LOG_W("%s line:%d usedefault etc file:%s",__func__,__LINE__,etc_path);
    }

    for(i=0;i<SND_AUDIO_PARAM_PROFILE_MAX;i++){
        memset(file_path,0,sizeof(file_path));
        snprintf(file_path,sizeof(file_path)-1,"%s/%s.xml",
            etc_path,audio_prfile_name[i]);
        param->audio_param_file_table[i].etc_xml=strdup(file_path);

        memset(file_path,0,sizeof(file_path));
        snprintf(file_path,sizeof(file_path)-1,"%s/%s.xml",
            data_parm_path,audio_prfile_name[i]);
        param->audio_param_file_table[i].data_xml=strdup(file_path);

        if((i==SND_AUDIO_PARAM_PGA_PROFILE)
            ||(i==SND_AUDIO_PARAM_PGA_PROFILE)){
            param->audio_param_file_table[i].etc_bin=NULL;
            param->audio_param_file_table[i].data_bin=NULL;
        }else{
            memset(file_path,0,sizeof(file_path));
            snprintf(file_path,sizeof(file_path)-1,"%s/%s",etc_path,
                audio_prfile_name[i]);
            param->audio_param_file_table[i].etc_bin=strdup(file_path);

            memset(file_path,0,sizeof(file_path));
            snprintf(file_path,sizeof(file_path)-1,"%s/%s",
                data_parm_path,audio_prfile_name[i]);
            param->audio_param_file_table[i].data_bin=strdup(file_path);
        }

        LOG_I("profile:%d etc:%s:%s data:%s:%s",i,
           param->audio_param_file_table[i].etc_xml,
           param->audio_param_file_table[i].etc_bin,
           param->audio_param_file_table[i].data_xml,
           param->audio_param_file_table[i].data_bin);
    }

    memset(file_path,0,sizeof(file_path));
    snprintf(file_path,sizeof(file_path)-1,"%s/audio_param_infor",etc_path);
    param->etc_audioparam_infor=strdup(file_path);

    if(access(get_audioparam_filename(param,SND_AUDIO_PARAM_SMARTAMP_PROFILE,AUDIO_PARAM_ETC_XML), R_OK) ==0){
        set_smartamp_support_usecase(&param->dev_ctl->smartamp_ctl,
            get_smartamp_config_usecase(audiotester_config),
            get_smartamp_config_mode(audiotester_config));
    }else{
        set_smartamp_support_usecase(&param->dev_ctl->smartamp_ctl,0,SND_AUDIO_UNSUPPORT_SMARTAMP_MODE);
    }
    LOG_I("init_audioparam_filename audioparaminfor etc:%s",
        param->etc_audioparam_infor);
}

int load_xml_handle(struct xml_handle *xmlhandle, const char *xmlpath)
{
    LOG_D("load_xml_handle:%s",xmlpath);
    TiXmlElement *root=NULL;
    xmlhandle->param_doc = XML_open_param(xmlpath);
    if (xmlhandle->param_doc == NULL) {
        LOG_E("load xml handle failed (%s)", xmlpath);
        xmlhandle->param_root=NULL;
        return -1;
    }

    xmlhandle->param_root =  XML_get_root_param(xmlhandle->param_doc);
    root = (TiXmlElement *)xmlhandle->param_root;
    if (root != NULL) {
        root = (TiXmlElement *)xmlhandle->param_root;
        xmlhandle->first_name = strdup(root->Value());
    }else{
        LOG_E("load_xml_handle :%s failed",xmlpath);
        XML_release_param(xmlhandle->param_doc);
        xmlhandle->param_doc=NULL;
        return -1;
    }
    if(NULL!=xmlpath){
        LOG_I("load_xml_handle:%p %s",xmlhandle,xmlpath);
    }
    return 0;
}

void release_xml_handle(struct xml_handle *xmlhandle)
{
    LOG_I("release_xml_handle:%p",xmlhandle);
    if (xmlhandle->param_doc) {
        XML_release_param(xmlhandle->param_doc);
        xmlhandle->param_doc = NULL;
        xmlhandle->param_root = NULL;
    }
    if (xmlhandle->first_name) {
        free(xmlhandle->first_name);
        xmlhandle->first_name = NULL;
    }
}

const char * tinymix_get_enum(struct mixer_ctl *ctl)
{
    unsigned int num_enums;
    unsigned int i;
    const char *string;

    num_enums = mixer_ctl_get_num_enums(ctl);

    for (i = 0; i < num_enums; i++) {
        string = mixer_ctl_get_enum_string(ctl, i);
        if (mixer_ctl_get_value(ctl, 0) == (int)i){
            return string;
        }
    }

    return NULL;
}

static const struct audio_param_mode_t  audio_param_mode_table[] = {
    { PROFILE_MODE_AUDIO_Handset_NB1,           "Audio\\Handset\\NB1"          },
    { PROFILE_MODE_AUDIO_Handset_NB2,           "Audio\\Handset\\NB2"         },
    { PROFILE_MODE_AUDIO_Handset_WB1,           "Audio\\Handset\\WB1"     },
    { PROFILE_MODE_AUDIO_Handset_WB2,           "Audio\\Handset\\WB2"     },
    { PROFILE_MODE_AUDIO_Handset_SWB1,          "Audio\\Handset\\SWB1"     },
    { PROFILE_MODE_AUDIO_Handset_FB1,           "Audio\\Handset\\FB1"     },
    { PROFILE_MODE_AUDIO_Handset_VOIP1,         "Audio\\Handset\\VOIP1"         },

    { PROFILE_MODE_AUDIO_Handsfree_NB1,         "Audio\\Handsfree\\NB1"        },
    { PROFILE_MODE_AUDIO_Handsfree_NB2,         "Audio\\Handsfree\\NB2"       },
    { PROFILE_MODE_AUDIO_Handsfree_WB1,         "Audio\\Handsfree\\WB1"   },
    { PROFILE_MODE_AUDIO_Handsfree_WB2,         "Audio\\Handsfree\\WB2"   },
    { PROFILE_MODE_AUDIO_Handsfree_SWB1,        "Audio\\Handsfree\\SWB1"   },
    { PROFILE_MODE_AUDIO_Handsfree_FB1,         "Audio\\Handsfree\\FB1"   },
    { PROFILE_MODE_AUDIO_Handsfree_VOIP1,       "Audio\\Handsfree\\VOIP1"       },

    { PROFILE_MODE_AUDIO_Headset4P_NB1,         "Audio\\Headset4P\\NB1"        },
    { PROFILE_MODE_AUDIO_Headset4P_NB2,         "Audio\\Headset4P\\NB2"       },
    { PROFILE_MODE_AUDIO_Headset4P_WB1,         "Audio\\Headset4P\\WB1"   },
    { PROFILE_MODE_AUDIO_Headset4P_WB2,         "Audio\\Headset4P\\WB2"   },
    { PROFILE_MODE_AUDIO_Headset4P_SWB1,        "Audio\\Headset4P\\SWB1"   },
    { PROFILE_MODE_AUDIO_Headset4P_FB1,         "Audio\\Headset4P\\FB1"   },
    { PROFILE_MODE_AUDIO_Headset4P_VOIP1,       "Audio\\Headset4P\\VOIP1"       },

    { PROFILE_MODE_AUDIO_Headset3P_NB1,         "Audio\\Headset3P\\NB1"        },
    { PROFILE_MODE_AUDIO_Headset3P_NB2,         "Audio\\Headset3P\\NB2"       },
    { PROFILE_MODE_AUDIO_Headset3P_WB1,         "Audio\\Headset3P\\WB1"   },
    { PROFILE_MODE_AUDIO_Headset3P_WB2,         "Audio\\Headset3P\\WB2"   },
    { PROFILE_MODE_AUDIO_Headset3P_SWB1,        "Audio\\Headset3P\\SWB1"   },
    { PROFILE_MODE_AUDIO_Headset3P_FB1,         "Audio\\Headset3P\\FB1"   },
    { PROFILE_MODE_AUDIO_Headset3P_VOIP1,       "Audio\\Headset3P\\VOIP1"       },

    { PROFILE_MODE_AUDIO_BTHS_NB1,              "Audio\\BTHS\\NB1"             },
    { PROFILE_MODE_AUDIO_BTHS_NB2,              "Audio\\BTHS\\NB2"            },
    { PROFILE_MODE_AUDIO_BTHS_WB1,              "Audio\\BTHS\\WB1"        },
    { PROFILE_MODE_AUDIO_BTHS_WB2,              "Audio\\BTHS\\WB2"        },
    { PROFILE_MODE_AUDIO_BTHS_SWB1,             "Audio\\BTHS\\SWB1"        },
    { PROFILE_MODE_AUDIO_BTHS_FB1,              "Audio\\BTHS\\FB1"        },
    { PROFILE_MODE_AUDIO_BTHS_VOIP1,            "Audio\\BTHS\\VOIP1"            },

    { PROFILE_MODE_AUDIO_BTHSNREC_NB1,          "Audio\\BTHSNREC\\NB1"         },
    { PROFILE_MODE_AUDIO_BTHSNREC_NB2,          "Audio\\BTHSNREC\\NB2"        },
    { PROFILE_MODE_AUDIO_BTHSNREC_WB1,          "Audio\\BTHSNREC\\WB1"    },
    { PROFILE_MODE_AUDIO_BTHSNREC_WB2,          "Audio\\BTHSNREC\\WB2"    },
    { PROFILE_MODE_AUDIO_BTHSNREC_SWB1,         "Audio\\BTHSNREC\\SWB1"    },
    { PROFILE_MODE_AUDIO_BTHSNREC_FB1,          "Audio\\BTHSNREC\\FB1"    },
    { PROFILE_MODE_AUDIO_BTHSNREC_VOIP1,        "Audio\\BTHSNREC\\VOIP1"        },

    { PROFILE_MODE_AUDIO_TYPEC_NB1,             "Audio\\TypeC_Digital\\NB1"         },
    { PROFILE_MODE_AUDIO_TYPEC_NB2,             "Audio\\TypeC_Digital\\NB2"        },
    { PROFILE_MODE_AUDIO_TYPEC_WB1,             "Audio\\TypeC_Digital\\WB1"    },
    { PROFILE_MODE_AUDIO_TYPEC_WB2,             "Audio\\TypeC_Digital\\WB2"    },
    { PROFILE_MODE_AUDIO_TYPEC_SWB1,            "Audio\\TypeC_Digital\\SWB1"    },
    { PROFILE_MODE_AUDIO_TYPEC_FB1,             "Audio\\TypeC_Digital\\FB1"    },
    { PROFILE_MODE_AUDIO_TYPEC_VOIP1,           "Audio\\TypeC_Digital\\VOIP1"        },

    { PROFILE_MODE_AUDIO_HAC_NB1,               "Audio\\Hac\\NB1"         },
    { PROFILE_MODE_AUDIO_HAC_NB2,               "Audio\\Hac\\NB2"        },
    { PROFILE_MODE_AUDIO_HAC_WB1,               "Audio\\Hac\\WB1"    },
    { PROFILE_MODE_AUDIO_HAC_WB2,               "Audio\\Hac\\WB2"    },
    { PROFILE_MODE_AUDIO_HAC_SWB1,              "Audio\\Hac\\SWB1"    },
    { PROFILE_MODE_AUDIO_HAC_FB1,               "Audio\\Hac\\FB1"    },
    { PROFILE_MODE_AUDIO_HAC_VOIP1,             "Audio\\Hac\\VOIP1"        },


    { PROFILE_MODE_MUSIC_Headset_Playback,           "Music\\Headset\\Playback"     },
    { PROFILE_MODE_MUSIC_Headset_Record,             "Music\\Headset\\Record"       },
    { PROFILE_MODE_MUSIC_Headset_UnprocessRecord,     "Music\\Headset\\UnprocessRecord"},
    { PROFILE_MODE_MUSIC_Headset_Recognition,     "Music\\Headset\\VoiceRecognition"},
    { PROFILE_MODE_MUSIC_Headset_FM,                 "Music\\Headset\\FM"           },

    { PROFILE_MODE_MUSIC_Handsfree_Playback,         "Music\\Handsfree\\Playback"   },
    { PROFILE_MODE_MUSIC_Handsfree_Record,           "Music\\Handsfree\\Record"     },
    { PROFILE_MODE_MUSIC_Handsfree_UnprocessRecord,  "Music\\Handsfree\\UnprocessRecord"},
    { PROFILE_MODE_MUSIC_Handsfree_VideoRecord,  "Music\\Handsfree\\VideoRecord"},
    { PROFILE_MODE_MUSIC_Handsfree_Recognition,     "Music\\Handsfree\\VoiceRecognition"},
    { PROFILE_MODE_MUSIC_Handsfree_FM,               "Music\\Handsfree\\FM"         },

    { PROFILE_MODE_MUSIC_TYPEC_Playback,         "Music\\TypeC_Digital\\Playback"   },
    { PROFILE_MODE_MUSIC_TYPEC_Record,           "Music\\TypeC_Digital\\Record"     },
        { PROFILE_MODE_MUSIC_TYPEC_Recognition,     "Music\\TypeC_Digital\\VoiceRecognition"},
    { PROFILE_MODE_MUSIC_TYPEC_UnprocessRecord,   "Music\\TypeC_Digital\\UnprocessRecord"},
    { PROFILE_MODE_MUSIC_TYPEC_FM,               "Music\\TypeC_Digital\\FM"         },


    { PROFILE_MODE_MUSIC_Handset_Playback,           "Music\\Handset\\Playback"     },
    { PROFILE_MODE_MUSIC_Headfree_Playback,          "Music\\Headfree\\Playback"    },
    { PROFILE_MODE_MUSIC_Bluetooth_Record,           "Music\\Bluetooth\\Record"     },

    { PROFILE_MODE_LOOP_Handset_Loop1,             "Loopback\\Handset\\Loop1"       },
    { PROFILE_MODE_LOOP_Handsfree_Loop1,           "Loopback\\Handsfree\\Loop1"     },
    { PROFILE_MODE_LOOP_Headset4P_Loop1,           "Loopback\\Headset4P\\Loop1"     },
    { PROFILE_MODE_LOOP_Headset3P_Loop1,           "Loopback\\Headset3P\\Loop1"     },
};

const char * get_audio_param_name(uint8_t param_id){
    for(uint8_t i=0;i<PROFILE_MODE_MAX;i++){
        if(param_id==audio_param_mode_table[i].mode){
            return audio_param_mode_table[i].name;
        }
    }
    return NULL;
}

void dump_data(char *buf, int len)
{
    int i = len;
    int line = 0;
    int size = 0;
    int j = 0;
    char dump_buf[60] = {0};
    int dump_buf_len = 0;

    char *tmp = (char *) buf;
    line = i / 16 + 1;

    for(i = 0; i < line; i++) {
        dump_buf_len = 0;
        memset(dump_buf, 0, sizeof(dump_buf));

        if(i < line - 1) {
            size = 16;
        } else {
            size = len % 16;
        }
        tmp = (char *)buf + i * 16;

        sprintf(dump_buf + dump_buf_len, "%04x: ", i*16);
        dump_buf_len = 5;

        for(j = 0; j < size; j++) {
            sprintf(dump_buf + dump_buf_len, " %02x", tmp[j]);
            dump_buf_len += 3;
        }

        LOG_I("%s\n", dump_buf);

    }
}

int get_audio_param_mode_name(AUDIO_PARAM_T *audio_param,char *str){
    struct audiotester_config_handle *audiotester_config=
        &audio_param->dev_ctl->config.audiotester_config;
    struct audiotester_param_config_handle * param_config=NULL;
    int i=0;
    int max_mode=sizeof(audio_param_mode_table)/sizeof(struct audio_param_mode_t);
    if(max_mode>audiotester_config->param_config_num){
        max_mode=audiotester_config->param_config_num;
    }

    for(i=0;i<max_mode;i++){
        char mode_name[128]={0};
        param_config=&audiotester_config->param_config[i];

        snprintf(mode_name,sizeof(mode_name)-1,"modename\\%s(%s)=%d",
            get_audio_param_name(param_config->paramid)
            ,param_config->usecasename,i);

        strcat(str, mode_name);
        strcat(str, SPLIT);
        LOG_V("mode:%s %p",mode_name,param_config);

        snprintf(mode_name,sizeof(mode_name)-1,"Path\\%s(%s)=%d",
            get_audio_param_name(param_config->paramid)
            ,param_config->modename,i);

        strcat(str, mode_name);
        strcat(str, SPLIT);

        LOG_V("mode:%s %p",mode_name,param_config);
    }
    return 0;
}

int get_audio_param_id_frome_name(const char *name)
{
    int i = 0;
    LOG_V("get_audio_param_id_frome_name:%s\n", name);
    int max_mode = sizeof(audio_param_mode_table) / sizeof(struct
                   audio_param_mode_t);
    for(i = 0; i < max_mode; i++) {
        if(strcmp(name, audio_param_mode_table[i].name) == 0) {
            return audio_param_mode_table[i].mode;
        }
    }
    LOG_I("get_audio_param_id_frome_name failed:%s",name);
    return -1;
}

int save_audio_param_infor(struct param_infor *infor){
    int fd=-1;
    LOG_I("save_audio_param_infor");
    fd= open(AUDIO_PARAM_INFOR_TUNNING_PATH, O_RDWR | O_CREAT,
                  S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);

    if(fd<0){
        LOG_E("save_audio_param_infor open:%s failed",AUDIO_PARAM_INFOR_TUNNING_PATH);
        return -1;
    }
    write(fd, infor->data, sizeof(infor->data));

    close(fd);

    return 0;
}

int load_audio_param_infor(struct param_infor *infor, char *etc_file){
    int fd=-1;
    int size=0;

    if(access(AUDIO_PARAM_INFOR_TUNNING_PATH, R_OK) == 0){
        fd= open(AUDIO_PARAM_INFOR_TUNNING_PATH, O_RDONLY);
        LOG_I("load_audio_param_infor:%s fd:%d",AUDIO_PARAM_INFOR_TUNNING_PATH,fd);
    }else if((etc_file!=NULL)&&(access(etc_file, R_OK) == 0)){
        fd= open(etc_file, O_RDONLY);
        LOG_I("load_audio_param_infor:%s fd:%d",etc_file,fd);
    }else if(etc_file==NULL){
        fd= open(AUDIO_PARAM_INFOR_PATH, O_RDONLY);
        LOG_I("load_audio_param_infor:%s fd:%d",etc_file,fd);
    }

    if(fd<0){
        LOG_E("load_audio_param_infor open failed ;%s",etc_file);
        return -2;
    }

    size=read(fd, infor->data, sizeof(infor->data));
    LOG_I("load_audio_param_infor size:0x%x 0x%x :%s"
        ,size,sizeof(infor->data),etc_file);
    close(fd);

    if(size!=sizeof(infor->data)){
        LOG_E("load_audio_param_infor read failed 0x%x!=0x%x",
            size,sizeof(infor->data));

        return -1;
    }
    return 0;
}

int save_audio_param_to_bin(AUDIO_PARAM_T *param, int profile)
{
    int ret = 0;
    struct vbc_fw_header *fw_header;
    AUDIOVBCEQ_PARAM_T *audio_param;
    int size = 0;

    if((profile<0)||(profile>=SND_AUDIO_PARAM_PROFILE_MAX)){
        return -1;
    }

    audio_param = &(param->param[profile]);
    fw_header = &(param->header[profile]);

#ifndef NORMAL_AUDIO_PLATFORM
    if((NULL ==get_audioparam_filename(param,profile,AUDIO_PARAM_DATA_BIN))
       ||(NULL==param->select_mixer[profile])
       ||(NULL==param->update_mixer[profile])){
        LOG_D("save_audio_param_to_bin:No need to save");
        return 0;
    }
#endif

    if(param->fd_bin[profile]<=0){
        LOG_D("save_audio_param_to_bin open:%s",get_audioparam_filename(param,profile,AUDIO_PARAM_DATA_BIN))
        param->fd_bin[profile] = open(get_audioparam_filename(param,profile,AUDIO_PARAM_DATA_BIN), O_RDWR | O_CREAT,
                      S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);

        if(NULL==audio_param->xml.first_name){
            ALOGE("Line:%d",__LINE__);
        }else{
            LOG_I("save_audio_param_to_bin Line:%d %s",__LINE__, audio_param->xml.first_name);
        }

        strncpy(fw_header->magic, audio_param->xml.first_name, FIRMWARE_MAGIC_MAX_LEN);
    }

    if(param->fd_bin[profile] < 0) {
        LOG_E("open:%s failed Error:%s\n"
            ,get_audioparam_filename(param,profile,AUDIO_PARAM_DATA_BIN),
            strerror(errno));
        return -1;
    }

    strncpy(fw_header->magic, AUDIO_PARAM_FIRMWARE_NAME, FIRMWARE_MAGIC_MAX_LEN);
    fw_header->num_mode = audio_param->num_mode;
    fw_header->len = audio_param->param_struct_size;

    ret = write(param->fd_bin[profile], fw_header, sizeof(struct vbc_fw_header));

    if(profile==SND_AUDIO_PARAM_SMARTAMP_PROFILE){
        load_smartamp_caliparam(audio_param);
    }

    size = audio_param->param_struct_size * audio_param->num_mode;
    ret += write(param->fd_bin[profile], audio_param->data, size);
    {
        int i=0;
        char *ptr=NULL;
        for(i=0;i<audio_param->num_mode;i++){
            ptr=audio_param->data+audio_param->param_struct_size*i;
            LOG_D("save_audio_param_to_bin1 profile:%d %02x %02x %02x %02x"
            ,profile,ptr[0],ptr[1],ptr[2],ptr[3]);

            ptr+=audio_param->param_struct_size/2;
            LOG_D("save_audio_param_to_bin1 profile:%d offset:%d %02x %02x %02x %02x"
            ,profile,audio_param->param_struct_size/2,ptr[0],ptr[1],ptr[2],ptr[3]);

            ptr+=audio_param->param_struct_size-5;

            LOG_D("save_audio_param_to_bin1 profile:%d offset:%d %02x %02x %02x %02x"
            ,profile,audio_param->param_struct_size-5,ptr[0],ptr[1],ptr[2],ptr[3]);
        }
    }
    LOG_I("save_audio_param_to_bin[%d] point:%p size:0x%x ret:0x%x :0x%x 0x%x\n",profile,audio_param->data,
          size, ret, audio_param->param_struct_size, audio_param->num_mode);

    close(param->fd_bin[profile]);
    param->fd_bin[profile]=-1;
    return ret;
}

int _get_param_size_from_xml(TiXmlElement *param_group)
{
    const char *type;
    unsigned int size = 0;
    unsigned int reserver_size=0;

    TiXmlElement *group = param_group;
    while (group != NULL) {
        //     LOG_D("_get_param_size_from_xml %s \n",group->Value());
        type = group->Attribute(TYPE);
        if (type == NULL) {
            size += _get_param_size_from_xml(group->FirstChildElement());
            LOG_V("_get_param_size_from_xml %s size:0x%x\n", group->Value(), size);
        } else {
            if(strcmp(type, U32) == 0) {
                size += sizeof(int32_t);
            } else if(strcmp(type, U16) == 0) {
                size += sizeof(int16_t);
            } else if(strcmp(type, U8) == 0) {
                size += sizeof(uint8_t);
            } else{
                reserver_size=strtoul(type+1,NULL,10);
                if(reserver_size>8*sizeof(int32_t)){
                    LOG_I("_get_param_size_from_xml reserver bits:%d",
                        reserver_size);
                    size += (reserver_size/8);
                }
            }
        }
        group = group->NextSiblingElement();
    }

    //  LOG_D("_get_param_size_from_xml return:%d\n",size);
    return size;
}

int get_param_size_from_xml(TiXmlElement *Element)
{
    TiXmlElement *tmpElement = NULL;
    TiXmlElement *tmpElement01;
    TiXmlElement *tmpElement02;
    TiXmlElement *tmpElement03;
    int size = 0;


    tmpElement = (TiXmlElement *)Element;
    tmpElement01 = (TiXmlElement *)XML_get_first_sub_group(Element);
    if(tmpElement01 != NULL) {
        tmpElement02 = (TiXmlElement *)XML_get_first_sub_group(tmpElement01);
        if(tmpElement02 != NULL) {
            tmpElement03 = (TiXmlElement *)XML_get_first_sub_group(tmpElement02);
            if(NULL!=tmpElement03){
                LOG_V("get_param_size_from_xml:%s\n", tmpElement03->Value());
                size = _get_param_size_from_xml((TiXmlElement *)XML_get_first_sub_group(
                                                    tmpElement03));
              }
        }
    }
    LOG_D("get_param_size_from_xml size:0x%x\n", size);
    return size;
}

int get_param_count_from_xml(TiXmlElement *Element)
{
    TiXmlElement *tmpElement01;
    TiXmlElement *tmpElement02;
    TiXmlElement *tmpElement03;
    int size = 0;

    tmpElement01 = (TiXmlElement *)XML_get_first_sub_group(Element);
    while(tmpElement01 != NULL) {
        tmpElement02 = (TiXmlElement *)XML_get_first_sub_group(tmpElement01);
        while(tmpElement02 != NULL) {
            tmpElement03 = (TiXmlElement *)XML_get_first_sub_group(tmpElement02);
            while(tmpElement03 != NULL){
                tmpElement03->SetAttribute("mode", size);
                size++;
                tmpElement03 = (TiXmlElement *)XML_get_next_sibling_group(tmpElement03);
            }
            tmpElement02 = (TiXmlElement *)XML_get_next_sibling_group(tmpElement02);
        }
        tmpElement01 = (TiXmlElement *)XML_get_next_sibling_group(tmpElement01);
    }
    LOG_D("get_param_count_from_xml size:0x%x\n", size);
    return size;
}

int read_audio_param_for_element(char *data,param_group_t Element){
    TiXmlElement *group = (TiXmlElement *)Element;
    TiXmlElement *child=NULL;
    const char *tmp=NULL;
    const char *type=NULL;
    bool has_child=false;
    int addr_offset=0;

    int size=0;
    int values=0;

    int child_val=0;
    int bits=0;
    int offset=0;

    int i=0;
    int mask=0;

    if((data==NULL) || (NULL==group)){
        LOG_E("%s %d",__func__,__LINE__);
        return -1;
    }

    type =group->Attribute(TYPE);
    if(NULL==type){
        group = (TiXmlElement *)group->Parent();
        if(NULL == group){
            LOG_E("%s %d",__func__,__LINE__);
            return -2;
        }

        type =group->Attribute(TYPE);

        has_child=true;
    }

    if(NULL==type){
            LOG_E("%s %d",__func__,__LINE__);
            return -3;
    }

    if(strcmp(type, U32) == 0) {
        size = sizeof(int32_t);
    } else if(strcmp(type, U16) == 0) {
        size = sizeof(int16_t);
    } else if(strcmp(type, U8) == 0) {
        size = sizeof(uint8_t);
    }

    tmp=group->Attribute(ID);
    if(NULL==tmp){
            LOG_E("%s %d",__func__,__LINE__);
            return -4;
    }
    addr_offset=string_to_value(tmp);

    if(false==has_child){
        tmp=group->Attribute(BITS);
        if(NULL==tmp){
                LOG_E("%s %d %s",__func__,__LINE__,group->Value());
                return -6;
        }

        bits=string_to_value(tmp);

        tmp=group->Attribute(OFFSETS);
        if(NULL==tmp){
                LOG_E("%s %d %s",__func__,__LINE__,group->Value());
                return -7;
        }

        offset=string_to_value(tmp);

        tmp=group->Attribute(VALUE);
        if(NULL==tmp){
                LOG_E("%s %d %s",__func__,__LINE__,group->Value());
                return -7;
        }

        values=string_to_value(tmp);

        if(bits<32) {
            mask=0;
            for(i=0;i<bits;i++) {
                mask |=1<<i;
            }
            values &=mask;
        }

        LOG_I("read_audio_param_for_xml %s offset:0x%x mask:%x values:0x%x",
            group->Value(),offset,mask,values);

        values=strtoul(tmp,NULL,16);
    }else{
        child=group->FirstChildElement();
        while(NULL!=child){
            tmp=child->Attribute(BITS);
            if(NULL==tmp){
                    LOG_E("%s %d %s",__func__,__LINE__,child->Value());
                    return -6;
            }

            bits=string_to_value(tmp);

            tmp=child->Attribute(OFFSETS);
            if(NULL==tmp){
                    LOG_E("%s %d %s",__func__,__LINE__,child->Value());
                    return -7;
            }

            offset=string_to_value(tmp);

            tmp=child->Attribute(VALUE);
            if(NULL==tmp){
                    LOG_E("%s %d %s",__func__,__LINE__,child->Value());
                    return -7;
            }

            child_val=string_to_value(tmp);

            mask=0;
            for(i=0;i<bits;i++){
                mask |=1<<i;
            }

            child_val &=mask;

            values |= (child_val<<offset);

            LOG_I("read_audio_param_for_xml child:%s child_val:0x%x offset:0x%x mask:%x values:0x%x",
                child->Value(),child_val,offset,mask,values);

            child=child->NextSiblingElement();
        }
    }

    if(sizeof(int16_t) == size){
        values &=0xffff;
    }else if(sizeof(uint8_t) == size){
        values &=0xff;
    }

    memcpy(data+addr_offset,&values,size);

    return 0;

}

int get_ele_value(param_group_t Element){
    TiXmlElement *ele;
    TiXmlElement *tmpele = (TiXmlElement *)Element;
    const char *tmp = NULL;
    int values = 0;
    int bits = 0;
    int offset = 0;
    int val = 0;

    ele = (TiXmlElement *)XML_get_first_sub_group(tmpele);
    if(NULL == ele) {
        tmp = tmpele->Attribute(VALUE);
        if(NULL == tmp) {
            LOG_E("%s Not find :%s", tmpele->Value(), VALUE);
            return -1;
        }

        values = string_to_value(tmp);
        LOG_V("name:%s values:0x%x\n",
              tmpele->Value(), values);
    } else {
        LOG_V("get_ele_value 1 %s\n", tmpele->Value());
        while(ele != NULL) {

            tmp = ele->Attribute(BITS);
            if(tmp == NULL) {
                LOG_W("%s name:%s not find:%s\n", __func__, ele->Value(), BITS);
                return -2;
            }
            bits = string_to_value(tmp);

            if((bits < 0) || (bits > 32)) {
                LOG_W("%s name:%s :%s err:%d\n", __func__, ele->Value(), BITS, bits);
                return -3;
            }

            tmp = ele->Attribute(OFFSETS);
            if(tmp == NULL) {
                LOG_W("%s name:%s not find:%s\n", __func__, ele->Value(), BITS);
                return -2;
            }
            offset = string_to_value(tmp);
            if((offset < 0) || (offset > 32)) {
                LOG_W("%s name:%s :%s err:%d\n", __func__, ele->Value(), OFFSETS, bits);
                return -3;
            }

            tmp = ele->Attribute(VALUE);
            if(tmp == NULL) {
                LOG_W("%s name:%s not find:%s\n", __func__, ele->Value(), BITS);
                return -2;
            }
            val = string_to_value(tmp);

            if(bits<32) {
                val = val & ((1 << bits) - 1);
            }else {
                if(offset!=0) {
                    LOG_E("offset error:%d bits:%d ele:%s",offset,bits,ele->Value());
                    offset=0;
                }
            }
            values |= (val << offset);
            LOG_V("name:%s bits:%d offset:%d val:0x%x values:0x%x\n",
                  ele->Value(), bits, offset, val, values);
            ele = (TiXmlElement *) XML_get_next_sibling_group(ele);
        }
    }
    return values;
}


static int _init_sprd_audio_param_from_xml(param_group_t param,bool root,
                                  unsigned char *data,int *offset,unsigned int max_size,int depth,
                                  struct param_infor_t  * param_infor,
                                  char *prev_path)
{
    int ret=0;
    unsigned int size=0;
    char val_str[32]={0};

    char cur_path[MAX_LINE_LEN] = {0};
    if (prev_path != NULL) {
        strcat(cur_path, prev_path);
    }

    const char * tmp=NULL;

    TiXmlElement *group = (TiXmlElement *)param;
    while(group!=NULL){
        if(offset!=NULL){
            LOG_V("ele:%d: %s total_size:0x%x",depth,group->Value(),*offset);
        }
        tmp = group->Attribute(TYPE);
        if(tmp==NULL){
            char next_path[MAX_LINE_LEN] = {0};
            char *path=NULL;

            if(param_infor!=NULL){
                if((depth>=1)&&(depth<3)){
                    strcat(next_path, cur_path);
                    add_path(next_path, group->Value());
                    path=next_path;
                }else if (depth==3){
                    int id=0xff;
                    strcat(next_path, cur_path);
                    strcat(next_path, group->Value());
                    id=get_audio_param_id_frome_name(next_path);
                    if((id>=PROFILE_AUDIO_MODE_START)&&(id<PROFILE_MODE_MAX)){
                        param_infor->offset[id]=*offset;
                        LOG_D("next_path:%s name:%s depth:%d id:%d id:0x%08x max:%x offset:%x",
                            next_path,group->Value(),depth,id,param_infor->offset[id],max_size,*offset);
                    }else{
                        LOG_W("invalid param next_path:%s name:%s depth:%d id:%d max:%x offset:%x",
                            next_path,group->Value(),depth,id,max_size,*offset);
                    }
                    path=NULL;
                }else{
                    path=NULL;
                }
            }else{
                path=NULL;
            }
            _init_sprd_audio_param_from_xml(group->FirstChildElement(),false,data,offset,max_size,depth+1,param_infor,path);
        }else{
            if(strcmp(tmp, U32) == 0){
                size = sizeof(int32_t);
            } else if(strcmp(tmp, U16) == 0){
                size = sizeof(int16_t);
            } else if(strcmp(tmp, U8) == 0){
                size = sizeof(uint8_t);
            } else{
                tmp = group->Attribute(BITS);
                if(tmp != NULL) {
                    size = string_to_value(tmp);
                    size/=8;
                }
            };

            sprintf(val_str,"0x%x",*offset);
            group->SetAttribute(ID, val_str);

            if(((unsigned int)(*offset)+size)>max_size){
                LOG_E("[Fatal error]inavlie audio param xml max:%x offset:%x size:%d",
                    max_size,*offset,size);
                break;
            }

            if(size>sizeof(int32_t)){
                memset(data+(*offset),0,size);
                *offset+=size;
                LOG_V("%s id:0x%x reserve size:%dbytes",group->Value(),*offset,size);
            }else{
                ret=get_ele_value(group);
                if(NULL!=data){
                    memcpy(data+(*offset),&ret,size);
                    *offset+=size;
                }
                LOG_V("%s id:0x%x val:0x%x",group->Value(),*offset,ret);
            }
        }

        if(root){
            break;
        }

        if(depth==2){
           LOG_V("ele NextSiblingElement1:%d: %s total_size:0x%x",depth,group->Value(),*offset);
        }
        group = group->NextSiblingElement();
        if((depth==2) &&(group!=NULL)) {
           LOG_V("ele NextSiblingElement2:%d: %s total_size:0x%x",depth,group->Value(),*offset);
        }
    }
    return *offset;
}

static int open_audio_param_file(AUDIO_PARAM_T *param,int profile){
    int ret=0;
    bool tunning=false;
    AUDIOVBCEQ_PARAM_T *audio_param = &(param->param[profile]);

    if(NULL!=audio_param->xml.param_root){
        return ret;
    }

    if((access(get_audioparam_filename(param,profile,AUDIO_PARAM_DATA_XML), R_OK) == 0) ){
        tunning=true;
    } else {
        if(access(get_audioparam_filename(param,profile,AUDIO_PARAM_ETC_XML), R_OK)!= 0){
            LOG_E("open_audio_param_file %s not exit",get_audioparam_filename(param,profile,AUDIO_PARAM_ETC_XML));
            return -1;
        }
        tunning=false;
    }

    if(true==tunning){
        LOG_I("open_audio_param_file:%s",get_audioparam_filename(param,profile,AUDIO_PARAM_DATA_XML));
        load_xml_handle(&(audio_param->xml), get_audioparam_filename(param,profile,AUDIO_PARAM_DATA_XML));
    }else{
        LOG_I("open_audio_param_file:%s",get_audioparam_filename(param,profile,AUDIO_PARAM_ETC_XML));
        load_xml_handle(&(audio_param->xml), get_audioparam_filename(param,profile,AUDIO_PARAM_ETC_XML));
    }

    if(NULL==audio_param->xml.param_root) {
        LOG_E("open_audio_param_file can not get root\n");
        if(true==tunning){
            char cmd[256]={0};
            snprintf(cmd,sizeof(cmd),"rm -rf %s",get_audioparam_filename(param,profile,AUDIO_PARAM_DATA_XML));
            system(cmd);
            LOG_E("open_audio_param_file system:%s",cmd);
            snprintf(cmd,sizeof(cmd),"rm -rf %s",get_audioparam_filename(param,profile,AUDIO_PARAM_DATA_BIN));
            system(cmd);
            LOG_E("open_audio_param_file system:%s",cmd);
            load_xml_handle(&(audio_param->xml), get_audioparam_filename(param,profile,AUDIO_PARAM_ETC_XML));
            if(NULL==audio_param->xml.param_root) {
                LOG_E("open_audio_param_file src file can not get root\n");
                ret=-1;
            }
        }else{
            ret=-1;
        }
    }
    return ret;
}

static int malloc_sprd_audio_param(AUDIO_PARAM_T *param,int profile){
    int ret=0;
    AUDIOVBCEQ_PARAM_T *audio_param = &(param->param[profile]);
    param_group_t root_param;
    TiXmlElement *tmpElement;
    const char *tmp = NULL;
    char val_str[32]={0};


    ret=open_audio_param_file(param,profile);
    if(ret!=0){
        LOG_E("open_audio_param_file :%d failed",profile);
        return ret;
    }
    root_param=audio_param->xml.param_root;
    if(NULL==root_param){
        audio_param->data=NULL;
        LOG_E("malloc_sprd_audio_param param_root is null");
        return -1;
    }

    LOG_D("%s %d %p",__func__,__LINE__,root_param);
    tmpElement = (TiXmlElement *)root_param;
    tmp = tmpElement->Attribute(STRUCT_SIZE);
    if(tmp != NULL) {
        audio_param->param_struct_size = string_to_value(tmp);
    }

    tmp = tmpElement->Attribute(NUM_MODE);
    if(tmp != NULL) {
        audio_param->num_mode = string_to_value(tmp);
    }

    if(SND_AUDIO_PARAM_SMARTAMP_PROFILE==profile){
        int q10_max=DEFAULT_SMARTAMP_CALI_Q10_MAX_OFFSET;
        int q10_normal=DEFAULT_SMARTAMP_CALI_Q10_NORMAL_OFFSET;
        tmp = tmpElement->Attribute(SMARTAMP_Q10_MAX_OFFSET_STR);
        if(tmp != NULL) {
            q10_max = string_to_value(tmp);
        }
        tmp = tmpElement->Attribute(SMARTAMP_Q10_NORMAL_OFFSET_STR);
        if(tmp != NULL) {
            q10_normal = string_to_value(tmp);
        }
        set_smartamp_cali_offset(&param->dev_ctl->smartamp_ctl,q10_max,q10_normal);
    }

    LOG_D("malloc_sprd_audio_param<%s> mode:%x size:0x%x\n",
          tmpElement->Value(), audio_param->num_mode, audio_param->param_struct_size);

    if((audio_param->num_mode <= 0) || (audio_param->param_struct_size <= 0)) {
        audio_param->num_mode = 0;
        audio_param->param_struct_size = 0;
        tmpElement = (TiXmlElement *)root_param;
        LOG_D("malloc_sprd_audio_param name:%s\n", tmpElement->Value());
        audio_param->param_struct_size = get_param_size_from_xml(tmpElement);
        audio_param->num_mode = get_param_count_from_xml(tmpElement);
    }

    ret=audio_param->num_mode *audio_param->param_struct_size;

    if(audio_param->data == NULL){
        audio_param->data = (char *)malloc(ret);
        LOG_D("malloc_sprd_audio_param profile %x malloc :%x %p", profile, audio_param->num_mode *
              audio_param->param_struct_size, audio_param->data);
        if(audio_param->data == NULL) {
            LOG_E("malloc_sprd_audio_param malloc audio_param err\n");
            ret = 0;
            goto err;
        }
    }

    tmpElement = (TiXmlElement *)root_param;
    sprintf(val_str,"0x%x",audio_param->num_mode);
    tmpElement->SetAttribute(NUM_MODE, val_str);
    memset(val_str,0,sizeof(val_str));
    sprintf(val_str,"0x%x",audio_param->param_struct_size);
    tmpElement->SetAttribute(STRUCT_SIZE, val_str);
    return ret;
err:
    LOG_E("malloc_sprd_audio_param err");
    if(NULL != audio_param->data){
        free(audio_param->data);
        audio_param->data=NULL;
    }
    return ret;
}

int init_sprd_audio_param_from_xml(AUDIO_PARAM_T *param,int profile,bool is_tunnng)
{
    AUDIOVBCEQ_PARAM_T *audio_param = &(param->param[profile]);
    TiXmlDocument *doc;
    int ret = 0;
    int offset=0;
    struct param_infor_t *infor=NULL;
    if(param->infor==NULL){
        infor=NULL;
    }else{
        infor=&(param->infor->data[profile]);
    }

    LOG_D("%s %d",__func__,__LINE__);
    ret=malloc_sprd_audio_param(param,profile);
    if(ret>0){

        if(infor!=NULL){
            infor->param_struct_size=audio_param->param_struct_size;
        }

        ret=_init_sprd_audio_param_from_xml(audio_param->xml.param_root,true,(unsigned char*)
                audio_param->data,&offset,(unsigned int)(audio_param->param_struct_size*audio_param->num_mode),0,
                infor,
                NULL);
        LOG_I("init_sprd_audio_param_from_xml profile:%d end 0x%x offset:0x%x",
            profile,ret,offset);

        if(offset!=audio_param->param_struct_size*audio_param->num_mode){
            LOG_E("[Fatal error]init_sprd_audio_param_from_xml profile:%d failed offset:0x%x pre_size:0x%x mode:0x%x",
                profile,offset,audio_param->param_struct_size,audio_param->num_mode);
            ret=-1;
            goto out;
        }

        doc = (TiXmlDocument *)audio_param->xml.param_doc;

        if(infor!=NULL){
            int i=0;
            for(i=0;i<PROFILE_MODE_MAX;i++){
                if((infor->offset[i]>0)&&(infor->offset[i]%audio_param->param_struct_size)!=0){
                    LOG_E("[Fatal error]init_sprd_audio_param_from_xml Check:%d failed mode:%d 0x%x 0x%x",
                    profile,i,infor->offset[i],audio_param->param_struct_size);
                    ret=-2;
                    goto out;
                }
            }
        }

        if(profile!=SND_AUDIO_PARAM_CODEC_PROFILE){

            char *buffer=NULL;
            int buffer_size=0;
            unsigned int param_size=audio_param->param_struct_size * audio_param->num_mode;
            buffer_size=read_audio_firmware(
               get_audioparam_filename(param,
               profile,AUDIO_PARAM_DATA_BIN),&buffer);
            LOG_I("check audio param profile:%d buffer_size:%d param size:%d header size:%d",profile,
               buffer_size,param_size,sizeof(struct vbc_fw_header));

            if((profile==SND_AUDIO_PARAM_SMARTAMP_PROFILE)
                &&(access(AUDIO_SMARTAMP_CALI_PARAM_FILE, R_OK) == 0)){
                param->header[profile].num_mode = audio_param->num_mode;
                param->header[profile].len = audio_param->param_struct_size;
                load_smartamp_caliparam(audio_param);
            }

            if(buffer_size<=0){
                LOG_I("OTA update audio param profile:%d size:%d",profile,buffer_size);
                save_audio_param_to_bin(param, profile);
            }else{
                if((buffer_size-sizeof(struct vbc_fw_header))!=param_size){
                    LOG_I("OTA update audio param profile:%d size(0x%x:0x%x)",profile,
                        buffer_size-sizeof(struct vbc_fw_header),param_size);
                    save_audio_param_to_bin(param, profile);
                }else if(0!=memcmp((char *)(buffer+sizeof(struct vbc_fw_header)),(char *)audio_param->data,param_size)){
                    LOG_I("OTA update audio param profile:%d",profile);
                    ret=save_audio_param_to_bin(param, profile);
                    if(ret!=buffer_size){
                        LOG_E("save_audio_param_to_bin failed %d %d",ret,buffer_size);
                    }
                }
            }
            if(NULL!=buffer){
               free(buffer);
               buffer=NULL;
               buffer_size=0;
            }
        }

        if(true==is_tunnng){
            LOG_I("init_sprd_audio_param_from_xml save:%s",get_audioparam_filename(param,profile,AUDIO_PARAM_DATA_XML));
            doc->SaveFile(get_audioparam_filename(param,profile,AUDIO_PARAM_DATA_XML));
        }else{
            release_xml_handle(&audio_param->xml);
        }

        if(is_tunnng==true){
            return ret;
        }
    }

out:

    if(profile==SND_AUDIO_PARAM_CODEC_PROFILE){
        return ret;
    }

    if(NULL != audio_param->data){
        free(audio_param->data);
        audio_param->data=NULL;
    }
    return ret;
}

static int free_sprd_audio_pga_param(struct device_usecase_gain *use_gain){
    int i=0,j=0;
    struct device_gain *dev_gain;
    struct gain_mixer_control *ctl;

    if(NULL!=use_gain->dev_gain){
        for(i=0;i<use_gain->gain_size;i++){
            dev_gain=&use_gain->dev_gain[i];

            if(NULL!=dev_gain){
                LOG_D("free_sprd_audio_pga_param dev_gain:%p",dev_gain);
                for(j=0;j<dev_gain->ctl_size;j++){
                    ctl=&dev_gain->ctl[j];

                    if(NULL!=ctl){
                        if(ctl->volume_value!=NULL){
                            free(ctl->volume_value);
                        }

                        if(NULL!=ctl->name){
                            free(ctl->name);
                        }
                    }
                }

                LOG_D("free_sprd_audio_pga_param dev_gain:%p",dev_gain);
                if(NULL!=dev_gain->name){
                    free(dev_gain->name);
                }

                if(NULL!=dev_gain->ctl){
                    free(dev_gain->ctl);
                }
            }
        }
    }

    if(NULL!=use_gain->dev_gain){
        free(use_gain->dev_gain);
    }
    use_gain->dev_gain=NULL;
    use_gain->gain_size=0;

    return 0;
}

bool is_audio_param_ready(AUDIO_PARAM_T *audio_param,int profile){
    bool ret=false;
    pthread_mutex_lock(&audio_param->audio_param_lock);
    if(audio_param->load_status&(1<<profile)){
        ret =true;
    }else{
        LOG_W("%s param:%d is not ready",__func__,profile);
        ret =false;
    }
    pthread_mutex_unlock(&audio_param->audio_param_lock);
    return ret;
}

bool is_all_audio_param_ready(AUDIO_PARAM_T *audio_param){
    bool ret=false;
    pthread_mutex_lock(&audio_param->audio_param_lock);
    if((audio_param->load_status&AUDIO_PARAM_ALL_LOADED_STATUS)
        ==AUDIO_PARAM_ALL_LOADED_STATUS){
        ret =true;
    }else{
        ret =false;
    }
    pthread_mutex_unlock(&audio_param->audio_param_lock);
    return ret;
}

int reload_sprd_audio_process_param_withflash(AUDIO_PARAM_T *audio_param){
    LOG_I("reload_sprd_audio_process_param_withflash");
    init_ap_audio_param(&audio_param->param[SND_AUDIO_PARAM_RECORD_PROCESS_PROFILE]);
    init_sprd_xml(&audio_param->param[SND_AUDIO_PARAM_RECORD_PROCESS_PROFILE],&(audio_param->param[SND_AUDIO_PARAM_RECORD_PROCESS_PROFILE].xml),
        get_audioparam_filename(audio_param,SND_AUDIO_PARAM_RECORD_PROCESS_PROFILE,AUDIO_PARAM_DATA_XML),
        get_audioparam_filename(audio_param,SND_AUDIO_PARAM_RECORD_PROCESS_PROFILE,AUDIO_PARAM_ETC_XML),
      &(audio_param->infor->data[SND_AUDIO_PARAM_RECORD_PROCESS_PROFILE]),
        load_sprd_ap_audio_param,true);
    return 0;
}

int reload_sprd_audio_pga_param_withflash(AUDIO_PARAM_T *param){
    int profile=SND_AUDIO_PARAM_PGA_PROFILE;

    param->param[SND_AUDIO_PARAM_PGA_PROFILE].data=NULL;
    LOG_I("reload_sprd_audio_pga_param_withflash");
    free_sprd_audio_pga_param(&param->dev_ctl->dg_gain);
    init_sprd_xml(&param->dev_ctl->dg_gain,&(param->param[profile].xml),get_audioparam_filename(param,profile,AUDIO_PARAM_DATA_XML),
        get_audioparam_filename(param,profile,AUDIO_PARAM_ETC_XML),NULL,load_sprd_audio_pga_param,true);
    return 0;
}

int reload_sprd_audio_pga_param_withram(AUDIO_PARAM_T *param){
    struct xml_handle *xmlhandle=&(param->param[SND_AUDIO_PARAM_PGA_PROFILE].xml);
    TiXmlElement * ele_1=NULL;
    TiXmlElement * ele_2=NULL;
    TiXmlElement * ele_3=NULL;
    TiXmlElement * group=NULL;
    TiXmlElement * root=NULL;
    char *ele_1_name=NULL;
    char *ele_2_name=NULL;
    char *ele_3_name=NULL;
    char path[MAX_LINE_LEN] = {0};

    LOG_I("reload_sprd_audio_pga_param_withram");
    if(NULL==xmlhandle->param_root){
        LOG_E("reload_sprd_audio_pga_param_withram param_root is null");
        return -1;
    }
    free_sprd_audio_pga_param(&param->dev_ctl->dg_gain);

    root=(TiXmlElement * )xmlhandle->param_root;

    ele_1 = root->FirstChildElement();
    while(ele_1!=NULL){
        ele_1_name=strdup(ele_1->Value());
        ele_2 = ele_1->FirstChildElement();
        while(ele_2!=NULL){
            ele_2_name=strdup(ele_2->Value());
            ele_3 = ele_2->FirstChildElement();
            while(NULL!=ele_3){
                ele_3_name=strdup(ele_3->Value());

                strcat(path, ele_1_name);
                strcat(path, BACKSLASH);
                strcat(path, ele_2_name);
                strcat(path, BACKSLASH);
                strcat(path, ele_3_name);
                group=ele_3;
                load_sprd_audio_pga_param(&param->dev_ctl->dg_gain,group,NULL,path);
                memset(path,0,sizeof(path));

                free(ele_3_name);
                ele_3_name=NULL;
                ele_3 = ele_3->NextSiblingElement();
            }
            ele_2 = ele_2->NextSiblingElement();

            free(ele_2_name);
            ele_2_name=NULL;
        }
        ele_1 = ele_1->NextSiblingElement();

        free(ele_1_name);
        ele_1_name=NULL;
    }
    return 0;
}

uint8_t get_audio_param_mode(AUDIO_PARAM_T  *audio_param,int param_type,uint8_t param_id){
    AUDIOVBCEQ_PARAM_T *param=&audio_param->param[param_type];
    struct param_infor_t  *param_infor=&audio_param->infor->data[param_type];
    uint8_t default_mode=param_id;
    uint8_t param_mode=default_mode;

    if(param_id>=PROFILE_MODE_MAX){
        return AUDIO_PARAM_INVALID_8BIT_OFFSET;
    }
    if(audio_param->infor==NULL){
        param_mode=default_mode;
        LOG_W("get_audio_param_mode infor is null return");
        goto out;
    }

    if(NULL!=param->data){
        if(param_infor->param_struct_size!=param->param_struct_size){
            LOG_W("get_audio_param_mode type:%d param_id:%d failed param_struct_size:%x %x",
                param_type,param_id,param->param_struct_size,param_infor->param_struct_size);
            param_mode=default_mode;
            goto out;
        }
    }else{
        if(param_infor->param_struct_size<=0){
            param_mode=default_mode;
            LOG_W("get_audio_param_mode type:%d param_id:%d failed param_struct_size:%x",
                param_type,param_id,param_infor->param_struct_size);
            goto out;
        }
    }

    if(AUDIO_PARAM_INVALID_32BIT_OFFSET==(unsigned int)param_infor->offset[param_id]){
        LOG_I("get_audio_param_mode invalid offset, param_id:%d type:%d name:%s",param_id,param_type,get_audio_param_name(param_id));
        param_mode=AUDIO_PARAM_INVALID_8BIT_OFFSET;
        goto out;
    }

    param_mode=(uint8_t)(param_infor->offset[param_id]/param_infor->param_struct_size);

    if((param_infor->offset[param_id]%param_infor->param_struct_size)){
        LOG_E("get_audio_param_mode error param_id:%d type:%d name:%s offset:0x%x",
        param_id,param_type,get_audio_param_name(param_id),param_infor->offset[param_id]);
    }

    if(param_mode>=PROFILE_MODE_MAX){
        LOG_I("get_audio_param_mode not support param_id:%d type:%d name:%s",param_id,param_type,get_audio_param_name(param_id));
        param_mode=AUDIO_PARAM_INVALID_8BIT_OFFSET;
        goto out;
    }

    if(param_mode!=default_mode){
        LOG_I("get_audio_param_mode type:%d param_id:%d param_mode:%x default_mode:%x"
            ,param_type,param_id,param_mode,default_mode);
    }

out:
    return param_mode;
}

static int read_audio_firmware(const char *firmware_file,char **data){
    struct vbc_fw_header header;
    FILE *file=NULL;
    int ret=0;
    char *buffer=NULL;
    int bytesRead=0;

    if(NULL==firmware_file){
        return 0;
    }

    file = fopen(firmware_file, "r");
    if(NULL==file){
        LOG_W("read_audio_firmware open %s failed err:%s"
            ,firmware_file,strerror(errno));
        ret= -1;
        goto out;
    }

    bytesRead = fread(&header, 1, sizeof(header), file);
    if(bytesRead!=sizeof(header)){
        LOG_W("read_audio_firmware read vbc_fw_header failed bytesRead:%d",bytesRead);
        ret= -2;
        goto out;
    }

    buffer=(char *)malloc(sizeof(header)+header.len*header.num_mode);
    if(NULL==buffer){
        LOG_W("read_audio_firmware malloc buffer failed");
        ret= -3;
        goto out;

    }
    memset(buffer,0,sizeof(header)+header.len*header.num_mode);
    memcpy(buffer,&header,sizeof(header));

    bytesRead = fread(buffer+sizeof(header), 1, header.len*header.num_mode, file);
    if(bytesRead!=header.len*header.num_mode){
        LOG_W("read_audio_firmware read firmware file failed bytesRead:%d",bytesRead);
        ret= -4;
        goto out;
    }

    ret=bytesRead+sizeof(header);

out:
    if(NULL!=file){
        fclose(file);
        file=NULL;
    }

    if((ret<0)&&(NULL!=buffer)){
        free(buffer);
        buffer=NULL;
    }

    *data=buffer;
    return ret;
}

static int load_smartamp_caliparam(AUDIOVBCEQ_PARAM_T *audio_param){

    struct smart_amp_cali_param cali_values;
    int ret=read_smartamp_cali_param(&cali_values);
    if(ret==sizeof(smart_amp_cali_param)){
        int i=0;
        char *data_point=(char *)audio_param->data;

        for(i=0;i<audio_param->num_mode;i++){
            data_point=(char *)audio_param->data+i*audio_param->param_struct_size;

            if((*(int16 *)(data_point+RE_T_CALI)!=cali_values.Re_T_cali_out)
                ||(*(int16 *)(data_point+POSSTFILTER_FLAG)!=cali_values.postfilter_flag)){
                LOG_I("load_smartamp_caliparam RE_T_CALI [%x:%x] POSSTFILTER_FLAG [%x:%x]",
                    *(int16 *)(data_point+RE_T_CALI),cali_values.Re_T_cali_out,*(int16 *)(data_point+POSSTFILTER_FLAG),cali_values.postfilter_flag);
            }
            *(int16 *)(data_point+RE_T_CALI)=cali_values.Re_T_cali_out;
            *(int16 *)(data_point+POSSTFILTER_FLAG)=cali_values.postfilter_flag;
        }
    }
    return ret;
}

UNUSED_ATTR static bool check_audiofimware(AUDIO_PARAM_T *param,int profile){
    char *buffer1=NULL;
    char *buffer2=NULL;
    int buffer1_size;
    int buffer2_size;
    bool ret=true;
    AUDIOVBCEQ_PARAM_T audio_param;
    struct vbc_fw_header  *header=NULL;

    memset(&audio_param,0,sizeof(audio_param));

    if(profile==SND_AUDIO_PARAM_SMARTAMP_PROFILE){
        if(access(get_audioparam_filename(param,SND_AUDIO_PARAM_SMARTAMP_PROFILE,AUDIO_PARAM_ETC_XML), R_OK) !=0){
            LOG_I("check_audiofimware do not check smartamp param");
            return true;
        }
    }

    buffer1_size=read_audio_firmware(
        get_audioparam_filename(param,
        profile,AUDIO_PARAM_ETC_BIN),&buffer1);
    buffer2_size=read_audio_firmware(
        get_audioparam_filename(param,
        profile,AUDIO_PARAM_DATA_BIN),&buffer2);

    if(profile==SND_AUDIO_PARAM_SMARTAMP_PROFILE){
        if((buffer1!=NULL)&&(buffer1_size>0)){
            audio_param.data=buffer1+sizeof(struct vbc_fw_header);
            header=(struct vbc_fw_header *)buffer1;
            audio_param.param_struct_size=header->len;
            audio_param.num_mode=header->num_mode;
            audio_param.data=buffer1+sizeof(struct vbc_fw_header);
            load_smartamp_caliparam(&audio_param);
        }
        if((buffer2!=NULL)&&(buffer2_size>0)){
            header=(struct vbc_fw_header *)buffer2;
            audio_param.param_struct_size=header->len;
            audio_param.num_mode=header->num_mode;
            audio_param.data=buffer2+sizeof(struct vbc_fw_header);
            load_smartamp_caliparam(&audio_param);
        }
    }

    if((buffer1_size<=0)
        ||(buffer1==NULL)){
        if(SND_AUDIO_PARAM_SMARTAMP_PROFILE==profile){
            ret=true;
        }else{
            ret=false;
            LOG_W("check_audiofimware load etc bin failed profile:%d",profile);
        }
        goto free_buffer;
    }

    if((buffer1_size==buffer2_size)&&(buffer2!=NULL)){
        int i=0;
        int *data1=NULL;
        int *data2=NULL;
        int param_size=0;

        param_size=buffer1_size/sizeof(int);
        data1=(int *)buffer1;
        data2=(int *)buffer2;
        for(i=0;i<param_size;i++){
            if(*(data1+i)!=*(data2+i)){
                ret=false;
                break;
            }
        }
    }else{
        ret=false;
    }

    if(ret==false){
        int dst_fd=-1;
        char *param_data=NULL;
        int writebytes=0;

        dst_fd= open(get_audioparam_filename(param,
            profile,AUDIO_PARAM_DATA_BIN), O_RDWR | O_CREAT,
                      S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
        if(dst_fd<0){
            LOG_E("check_audiofimware: create %s failed",
                get_audioparam_filename(param,profile,AUDIO_PARAM_DATA_BIN));
            goto free_buffer;
        }else{
            param_data=buffer1;
            writebytes=0;
            do {
                writebytes = write(dst_fd, param_data, buffer1_size);
                if (writebytes > 0) {
                    if (writebytes <= buffer1_size) {
                        buffer1_size -= writebytes;
                        param_data+=writebytes;
                    }
                } else if ((!((errno == EAGAIN) || (errno == EINTR))) || (0 == writebytes)) {
                    LOG_E("check_audiofimware:write error %d", errno);
                    break;
                } else {
                    LOG_E("check_audiofimware:write_warning: %d, writebytes is %d",
                        errno, writebytes);
                }
            } while (buffer1_size);
        }

        if(dst_fd>=0){
            close(dst_fd);
            dst_fd=-1;
        }
        ret=true;
    }

free_buffer:

    if(buffer1!=NULL){
        free(buffer1);
    }

    if(buffer2!=NULL){
        free(buffer2);
    }
    LOG_I("check_audiofimware:%d ret:%d",profile,ret);
    return ret;
}

void dump_audio_param(int fd,char * buffer,UNUSED_ATTR int buffer_size,AUDIO_PARAM_T  *audio_param){
    snprintf(buffer,(DUMP_BUFFER_MAX_SIZE-1),
        "audio param dump: "
        "mode:%d "
        "current_param:%d "
        "input_source:%d "
        "agdsp_ctl:%p "
        "dev_ctl:%p "
        "tunning sn:0x%x "
        "backup sn:0x%x\n",
        audio_param->mode,
        audio_param->current_param,
        audio_param->input_source,
        audio_param->agdsp_ctl,
        audio_param->dev_ctl,
        audio_param->tunning_param_sn,
        audio_param->tunning_param_sn);
    AUDIO_DUMP_WRITE_STR(fd,buffer);
    memset(buffer,0,sizeof(buffer));
}

int load_audio_param(AUDIO_PARAM_T  *audio_param,bool is_tunning,int profile)
{
    int ret = 0;
    struct param_infor_t *infor=NULL;

    if(audio_param== NULL) {
        LOG_D("load_audio_param error");
        return -1;
    }

    LOG_I("load_audio_param profile:%d is_tunning:%d",profile,is_tunning);
    if(SND_AUDIO_PARAM_PGA_PROFILE ==profile){
        infor=NULL;

        if(is_tunning){
            free_sprd_audio_pga_param(&audio_param->dev_ctl->dg_gain);
            audio_param->param[profile].data=NULL;
            init_sprd_xml(&audio_param->dev_ctl->dg_gain,&(audio_param->param[profile].xml),get_audioparam_filename(audio_param,profile,AUDIO_PARAM_DATA_XML),
                get_audioparam_filename(audio_param,profile,AUDIO_PARAM_ETC_XML),infor,load_sprd_audio_pga_param,is_tunning);
        }else{
            init_sprd_xml(&audio_param->dev_ctl->dg_gain,NULL,get_audioparam_filename(audio_param,profile,AUDIO_PARAM_DATA_XML),
                get_audioparam_filename(audio_param,profile,AUDIO_PARAM_ETC_XML),infor,load_sprd_audio_pga_param,is_tunning);
        }
    }else if(SND_AUDIO_PARAM_RECORD_PROCESS_PROFILE ==profile){
        LOG_I("load_audio_param audio_process");
        if(audio_param->infor==NULL){
            infor=NULL;
        }else{
            infor=&(audio_param->infor->data[profile]);
        }

        init_ap_audio_param(&audio_param->param[profile]);
        if(is_tunning){
            init_sprd_xml(&audio_param->param[profile],&(audio_param->param[profile].xml),
                get_audioparam_filename(audio_param,profile,AUDIO_PARAM_DATA_XML),
                get_audioparam_filename(audio_param,profile,AUDIO_PARAM_ETC_XML),
                infor
                ,load_sprd_ap_audio_param,is_tunning);
        }else{
            init_sprd_xml(&audio_param->param[profile],NULL,
                get_audioparam_filename(audio_param,profile,AUDIO_PARAM_DATA_XML),
                get_audioparam_filename(audio_param,profile,AUDIO_PARAM_ETC_XML),
                infor,
                load_sprd_ap_audio_param,is_tunning);
        }
    } else if(SND_AUDIO_PARAM_CODEC_PROFILE ==profile){
        ret = init_sprd_audio_param_from_xml(audio_param,profile,is_tunning);
    }else{
        ret = init_sprd_audio_param_from_xml(audio_param,profile,is_tunning);
        upload_audio_profile_param_firmware(audio_param,profile);
    }

    if(false==is_tunning){
        AUDIOVBCEQ_PARAM_T *param = &(audio_param->param[profile]);
        LOG_I("load_audio_param check audio param load_status:0x%x",audio_param->load_status);
        if((SND_AUDIO_PARAM_PGA_PROFILE ==profile)
            ||(SND_AUDIO_PARAM_CODEC_PROFILE ==profile)
            ||(SND_AUDIO_PARAM_RECORD_PROCESS_PROFILE ==profile)){
            if(SND_AUDIO_PARAM_PGA_PROFILE ==profile){
                if(audio_param->dev_ctl->dg_gain.dev_gain==NULL){
                    LOG_E("load_audio_param failed profile %d data is NULL",profile);
                }
            }else{
                if(param->data==NULL){
                    LOG_E("load_audio_param failed profile %d data is NULL",profile);
                }
            }
        }else{
            if(param->data!=NULL){
                LOG_E("load_audio_param profile %d data is not free",profile);
                free(param->data);
                param->data=NULL;
            }
        }

        if(param->xml.param_doc!=NULL){
            LOG_E("load_audio_param profile %d xml is not free",profile);
            release_xml_handle(&param->xml);
        }
    }

    pthread_mutex_lock(&audio_param->audio_param_lock);
    audio_param->load_status|=1<<profile;
    pthread_mutex_unlock(&audio_param->audio_param_lock);

    return ret;
}

int init_sprd_audio_param(AUDIO_PARAM_T  *audio_param,bool is_tunning)
{
    int profile=0;

    if(audio_param== NULL) {
        LOG_D("init_sprd_audio_param error");
        return -1;
    }

    ALOG_ASSERT(audio_param->infor!=NULL);

    LOG_I("init_sprd_audio_param is_tunning:%d",is_tunning);

    for(profile=0;profile<SND_AUDIO_PARAM_PROFILE_MAX;profile++){
        LOG_I("init_sprd_audio_param profile:%d",profile);
        load_audio_param(audio_param,is_tunning,profile);
    }

    return 0;
}

void audio_param_clear_load_status(AUDIO_PARAM_T  *audio_param){
    LOG_I("audio_param_clear_load_status:%x",audio_param->load_status);
    pthread_mutex_lock(&audio_param->audio_param_lock);
    audio_param->load_status=0;
    pthread_mutex_unlock(&audio_param->audio_param_lock);
}

int clear_audio_param(AUDIO_PARAM_T  *audio_param){
    LOG_D("clear_audio_param:%x",audio_param->current_param);
    pthread_mutex_lock(&audio_param->audio_param_lock);
    audio_param->current_param=0;
    pthread_mutex_unlock(&audio_param->audio_param_lock);
    return 0;
}

static int load_sprd_audio_pga_param(void *gain,param_group_t ele,
    UNUSED_ATTR param_infor_t * param_infor,char *path){
    int id=0;
    struct device_usecase_gain *use_gain=(struct device_usecase_gain *)gain;
    id=get_audio_param_id_frome_name(path);
    if(id<0){
        return id;
    }

    parse_device_gain(use_gain,use_gain->mixer,(TiXmlElement *)ele);
    (use_gain->dev_gain+use_gain->gain_size-1)->id=id;
    LOG_D("\nload_sprd_audio_pga_param ID:%d :%p",id,ele);

    for(int j = 0; j < use_gain->gain_size; j++) {
        struct device_gain *dump = NULL;
        dump = &(use_gain->dev_gain[j]);
        LOG_D("load_sprd_audio_pga_param dump audio gain[%d]:%s size:%d", j, dump->name,
              dump->ctl_size);
        for(int z = 0; z < dump->ctl_size; z++) {
            struct gain_mixer_control *dump_mixer = NULL;
            dump_mixer = &(dump->ctl[z]);
            LOG_D("[\t%s %d  %d]\n", dump_mixer->name, dump_mixer->volume_size,
                  dump_mixer->volume_value[0]);
        }
    }

    return 0;
}

bool is_voice_param(int param_id){
    if((param_id>=PROFILE_MODE_AUDIO_Handset_NB1)
        &&(param_id<=PROFILE_MODE_AUDIO_HAC_VOIP1)){
        return true;
    }
    return false;
}

bool is_record_param(int param_id){
    int ret=false;
    switch(param_id){
        case PROFILE_MODE_MUSIC_Headset_Record:
        case PROFILE_MODE_MUSIC_Headset_UnprocessRecord:
        case PROFILE_MODE_MUSIC_Headset_Recognition:
        case PROFILE_MODE_MUSIC_Handsfree_Record:
        case PROFILE_MODE_MUSIC_Handsfree_UnprocessRecord:
        case PROFILE_MODE_MUSIC_Handsfree_VideoRecord:
        case PROFILE_MODE_MUSIC_Handsfree_Recognition:
        case PROFILE_MODE_MUSIC_TYPEC_Record:
        case PROFILE_MODE_MUSIC_TYPEC_UnprocessRecord:
        case PROFILE_MODE_MUSIC_TYPEC_Recognition:
        case PROFILE_MODE_MUSIC_Bluetooth_Record:
            ret= true;
            break;
        default:
            break;

    }
    return ret;
}

bool is_loop_param(int param_id){
    if((param_id>=PROFILE_MODE_LOOP_Handset_Loop1)
        &&(param_id<=PROFILE_MODE_LOOP_Headset3P_Loop1)){
        return true;
    }
    return false;
}

static void * select_apparam_with_paramid(struct audio_ap_param *ap_param,int param_id){
    int i=0;

    if(is_voice_param(param_id)){
        for(i=0;i<VOICE_AP_PARAM_COUNT;i++){
            if(false==is_voice_param(ap_param->voice_parmid[i])){
                ap_param->voice_parmid[i]=param_id;
                return (void *)&ap_param->voice[i];
            }

            if(ap_param->voice_parmid[i]==param_id){
                LOG_E("select_apparam_with_paramid voice param:%d aready used:%d",
                ap_param->voice_parmid[i],i);
                return NULL;
            }

        }
    }

    if(is_record_param(param_id)){
        for(i=0;i<RECORD_AP_PARAM_COUNT;i++){
            if(false==is_record_param(ap_param->record_parmid[i])){
                ap_param->record_parmid[i]=param_id;
                return (void *)&ap_param->record[i];
            }

            if(ap_param->record_parmid[i]==param_id){
                LOG_E("select_apparam_with_paramid record param:%d aready used:%d",
                ap_param->record_parmid[i],i);
                return NULL;
            }
        }
    }

    if(is_loop_param(param_id)){
        for(i=0;i<LOOP_AP_PARAM_COUNT;i++){
            if(false==is_loop_param(ap_param->loop_parmid[i])){
                ap_param->loop_parmid[i]=param_id;
                return (void *)&ap_param->loop[i];
            }

            if(ap_param->loop_parmid[i]==param_id){
                LOG_E("select_apparam_with_paramid loop param:%d aready used:%d",
                ap_param->loop_parmid[i],i);
                return NULL;
            }
        }
    }
    LOG_E("select_apparam_with_paramid Failed with:%d",param_id);
    return NULL;
}

static void init_ap_audio_param(void *audio_param){
    AUDIOVBCEQ_PARAM_T *param =(AUDIOVBCEQ_PARAM_T *)audio_param;
    struct audio_ap_param *ap_param=NULL;
    int i=0;

    if(NULL==audio_param){
        LOG_E("init_ap_audio_param failed");
        return;
    }

    param->param_struct_size=sizeof(struct audio_ap_param);
    param->num_mode=1;

    ap_param=(struct audio_ap_param *)param->data;
    if(NULL==ap_param){
        LOG_I("init_ap_audio_param data is null failed");
        return;
    }

    for(i=0;i<VOICE_AP_PARAM_COUNT;i++){
        ap_param->voice_parmid[i]=PROFILE_MODE_MAX;
    }
    for(i=0;i<RECORD_AP_PARAM_COUNT;i++){
        ap_param->record_parmid[i]=PROFILE_MODE_MAX;
    }
    for(i=0;i<LOOP_AP_PARAM_COUNT;i++){
        ap_param->loop_parmid[i]=PROFILE_MODE_MAX;
    }
}
static int load_sprd_ap_audio_param(void *audio_param,param_group_t ele, param_infor_t * param_infor,char *path){
    char * process_param=NULL;
    AUDIOVBCEQ_PARAM_T *param=(AUDIOVBCEQ_PARAM_T *)audio_param;
    TiXmlElement *param_group=(TiXmlElement *)ele;
    int id=0;
    const char  *tmp=NULL;
    tmp=param_group->Value();
    int ret=-1;
    struct audio_ap_param *ap_param=NULL;
    int offset=0;

    if(NULL==tmp){
        LOG_E("load_sprd_ap_audio_param err");
        return -1;
    }

    id=get_audio_param_id_frome_name(path);
    if((id<0)||(id>PROFILE_MODE_MAX)){
        LOG_E("load_sprd_ap_audio_param:%s failed",path);
        return id;
    }

    if(NULL==param->data){
        ap_param=(struct audio_ap_param *)malloc(sizeof(struct audio_ap_param));
        if(NULL==ap_param){
               LOG_E("load_sprd_ap_audio_param malloc failed");
               param->data=NULL;
               return -1;
        }
        param->param_struct_size=sizeof(struct audio_ap_param);
        param->num_mode=1;
        param->data=(char *)ap_param;
        init_ap_audio_param(param);
    }else{
        ap_param=(struct audio_ap_param *)(param->data);
    }

    process_param=(char *)select_apparam_with_paramid(ap_param,id);

    if(NULL==process_param){
        LOG_E("load_sprd_ap_audio_param %s not support",tmp);
        return -1;
    }

    offset=(char *)process_param-(char *)ap_param;
    if(NULL!=param_infor){
        param_infor->offset[id]=offset;
        LOG_D("load_sprd_ap_audio_param return:%d process_param:%p offset:%d id:%d %s",ret,process_param,param_infor->offset[id],id,path);
    }
    ret = _init_sprd_audio_param_from_xml(param_group,true,(unsigned char*)
    param->data,&offset,(unsigned int )(param->param_struct_size*param->num_mode),3,
    NULL,
    NULL);

    return 0;
}

static int init_sprd_xml(void * load_param_func_res,
                   struct xml_handle *xml,
                   const char *tunning_param_path,
                   const char *param_path,
                   struct param_infor_t  * param_infor,
                   load_sprd_audio_param load_param_func,bool is_tunning){
    int ret = 0;
    const char *parampath;
    TiXmlDocument *param_doc = NULL;

    TiXmlElement * ele_1=NULL;
    TiXmlElement * ele_2=NULL;
    TiXmlElement * ele_3=NULL;
    TiXmlElement * group=NULL;
    TiXmlElement * root=NULL;
    struct xml_handle *xmlhandle;
    struct xml_handle xml_tmp;
    char *ele_1_name=NULL;
    char *ele_2_name=NULL;
    char *ele_3_name=NULL;
    char path[MAX_LINE_LEN] = {0};

    LOG_I("init_sprd_xml enter");
    memset(&xml_tmp,0,sizeof(struct xml_handle));
    if(xml==NULL){
        xmlhandle=&xml_tmp;
    }else{
        xmlhandle=xml;
        if(NULL!=xmlhandle->param_root){
            release_xml_handle(xmlhandle);
        }
    }

    if(NULL==xmlhandle->param_root){
        xmlhandle->first_name=NULL;
        xmlhandle->param_root=NULL;
        xmlhandle->param_doc=NULL;

        if((tunning_param_path != NULL)&&(access(tunning_param_path, R_OK) ==0)){
            parampath=tunning_param_path;
        } else {
            parampath = param_path;
        }

        LOG_I("init_sprd_xml:%s",parampath);

        ret = load_xml_handle(xmlhandle, parampath);
        if (ret != 0) {
            LOG_E("load xml handle failed (%s)", parampath);
            if(tunning_param_path == parampath){
                parampath=param_path;
                ret = load_xml_handle(xmlhandle, parampath);
                if (ret != 0) {
                    LOG_E("load xml handle failed (%s)", param_path);
                    return ret;
                }
            }
        }
    }

    if(NULL==xmlhandle->param_root){
        LOG_E("init_sprd_xml param_root is null");
        return -1;
    }

    root=(TiXmlElement * )xmlhandle->param_root;

    ele_1 = root->FirstChildElement();
    while(ele_1!=NULL){
        ele_1_name=strdup(ele_1->Value());
        LOG_V("init_sprd_xml ele_1 start:%s",ele_1_name);
        ele_2 = ele_1->FirstChildElement();
        while(ele_2!=NULL){
            ele_2_name=strdup(ele_2->Value());
            LOG_V("init_sprd_xml ele_2 start:%s :%p",ele_2_name,ele_2);
            ele_3 = ele_2->FirstChildElement();
            while(NULL!=ele_3){
                ele_3_name=strdup(ele_3->Value());
                LOG_V("init_sprd_xml ele_3 start:%s :%p",ele_3_name,ele_3);

                if(NULL!=load_param_func){
                    strcat(path, ele_1_name);
                    strcat(path, BACKSLASH);
                    strcat(path, ele_2_name);
                    strcat(path, BACKSLASH);
                    strcat(path, ele_3_name);
                    group=ele_3;
                    load_param_func(load_param_func_res,group,param_infor,path);
                    memset(path,0,sizeof(path));
                }
                LOG_V("init_sprd_xml ele_3 end:%s :%p",ele_3_name,ele_3);
                ele_3 = ele_3->NextSiblingElement();

                free(ele_3_name);
                ele_3_name=NULL;
            }

            LOG_V("init_sprd_xml ele_2 end:%s :%p",ele_2_name,ele_2);
            ele_2 = ele_2->NextSiblingElement();

            free(ele_2_name);
            ele_2_name=NULL;
        }
        LOG_V("init_sprd_xml ele_1 end:%s",ele_1_name);
        ele_1 = ele_1->NextSiblingElement();

        free(ele_1_name);
        ele_1_name=NULL;
    }

    if(xml==NULL){
        release_xml_handle(xmlhandle);
    }else{
        param_doc = (TiXmlDocument *)xmlhandle->param_doc;

        if((true==is_tunning)&&(tunning_param_path!=NULL)){
            LOG_I("init_sprd_xml update_param save:%s",tunning_param_path);
            param_doc->SaveFile(tunning_param_path);
        }

        if (false==is_tunning) {
            LOG_I("init_sprd_xml exit release_xml_handle");
            release_xml_handle(xmlhandle);
        }
    }
    LOG_I("init_sprd_xml exit");
    return ret;
}



int upload_audio_profile_param_firmware(AUDIO_PARAM_T * audio_param,int profile){
    struct mixer_ctl *mixer;
    int ret=-1;

    mixer = audio_param->update_mixer[profile];
    if (!mixer) {
        LOG_E("upload_audio_profile_param_firmware Failed to open mixer, profile: %d", profile);
        return -1;
    }

    ret = mixer_ctl_set_value(mixer, 0, 1);
    if (ret != 0) {
        LOG_E("upload_audio_profile_param_firmware Failed profile:%d\n",profile);
    }
    LOG_I("upload_audio_profile_param_firmware:%d, ret(%d)\n", profile,ret);
    return 0;
}

int upload_audio_param_firmware(AUDIO_PARAM_T * audio_param){
    int ret=-1;
    LOG_I("upload_audio_param_firmware");
#ifdef NORMAL_AUDIO_PLATFORM
    return 0;
#else
    ret |=upload_audio_profile_param_firmware(audio_param,SND_AUDIO_PARAM_DSP_VBC_PROFILE_DSP);
    ret |=upload_audio_profile_param_firmware(audio_param,SND_AUDIO_PARAM_AUDIO_STRUCTURE_PROFILE);
    ret |=upload_audio_profile_param_firmware(audio_param,SND_AUDIO_PARAM_CVS_PROFILE);
#ifdef SPRD_AUDIO_SMARTAMP
    if(true==is_support_smartamp(&audio_param->dev_ctl->smartamp_ctl)){
        ret |=upload_audio_profile_param_firmware(audio_param,SND_AUDIO_PARAM_SMARTAMP_PROFILE);
    }
#endif
    if(AUD_REALTEK_CODEC_TYPE == audio_param->dev_ctl->codec_type){
        ret |=upload_audio_profile_param_firmware(audio_param,SND_AUDIO_PARAM_CODEC_PROFILE);
        ret |=upload_realtek_extend_param(audio_param->dev_ctl->mixer);
    }
#endif
    return ret;
}

static int upload_realtek_extend_param(struct mixer *mixer){
    struct mixer_ctl * upload_ctl=NULL;
    int ret=-1;

    upload_ctl=mixer_get_ctl_by_name(mixer, REALTEK_EXTEND_PARAM_UPDATE);
    if (!upload_ctl) {
        LOG_E("upload_realtek_extend_param Failed to open mixer:%s", REALTEK_EXTEND_PARAM_UPDATE);
        return -1;
    }

    ret = mixer_ctl_set_value(upload_ctl, 0, 1);
    if (ret != 0) {
        LOG_E("upload_realtek_extend_param Failed");
    }
    return ret;
}
#ifdef __cplusplus
}
#endif

