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

#ifndef _AUDIO_DEBUG_H_
#define _AUDIO_DEBUG_H_

#include <stdbool.h>
#include <aud_common.h>
#include <audio_param/param_config.h>

#ifdef AUDIOHAL_V4
#include "audio_platform.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

struct param_tunning_debug_handle {
    bool txt_enable;
    int txt_fd;
    bool hex_enable;
    int hex_fd;
    bool err_enable;
    int err_fd;
};

struct tunning_debug {
    struct param_tunning_debug_handle rx_debug;
    struct param_tunning_debug_handle tx_debug;
};

struct mute_control_name {
    char * spk_mute;
    char * spk2_mute;
    char * handset_mute;
    char * headset_mute;
    char * linein_mute;

    char * dsp_da0_mdg_mute;
    char * dsp_da1_mdg_mute;
    char * audio_mdg_mute;
    char * audio_mdg23_mute;
};

struct audio_config_handle {
    int log_level;
    char *card_name;
    int mic_switch;
    bool support_24bits;
    char *audioparampath;
    struct audiotester_config_handle audiotester_config;
    struct mute_control_name mute;
    int nr_mask;
    bool enable_other_rate_nr;
#ifdef AUDIOHAL_V4
    struct audio_platform_data audio_data;
#endif
};

#define MAX_SELECT_FD   1023

#define UNUSED_ATTR __attribute__((unused))

extern int log_level;
#define LOG_V(...)  ALOGV_IF(log_level >= 5,__VA_ARGS__);
#define LOG_D(...)  ALOGD_IF(log_level >= 4,__VA_ARGS__);
#define LOG_I(...)  ALOGI_IF(log_level >= 3,__VA_ARGS__);
#define LOG_W(...)  ALOGW_IF(log_level >= 2,__VA_ARGS__);
#define LOG_E(...)  ALOGE_IF(log_level >= 1,__VA_ARGS__);

#define AUDIO_DUMP_WRITE_BUFFER(fd,buffer,size)    do   \
    {   \
        if(fd>0)  \
            write(fd, buffer, size);    \
        else    \
            wirte_buffer_to_log(buffer,size);   \
    } while(0)


#define AUDIO_DUMP_WRITE_STR(fd,buffer)    do   \
    {   \
        if(fd>0)  \
            write(fd, buffer, strlen(buffer));    \
        else    \
            ALOGI("%s\n",buffer);   \
    } while(0)

#ifdef AUDIOHAL_V4
#define  AUDIO_DEBUG_CONFIG_TUNNING_PATH "/data/vendor/local/media/audio_config.xml"
#else
#define AUDIO_DEBUG_CONFIG_TUNNING_PATH "/data/local/media/audio_config.xml"
#endif

#ifdef FLAG_VENDOR_ETC
#define AUDIO_DEBUG_CONFIG_PATH "/vendor/etc/audio_config.xml"
#else
#define AUDIO_DEBUG_CONFIG_PATH "/system/etc/audio_config.xml"
#endif

#define AUDIO_DEBUG_CONFIG_FIRSTNAME "audio_debug"
#define DEBUG_POINT  do{LOG_I("%s %d",__func__,__LINE__);}while(0)

int parse_audio_config(struct audio_config_handle *config);
#define AUDIO_EXT_CONTROL_PIPE "/dev/pipe/mmi.audio.ctrl"
#ifdef AUDIOHAL_V4
#define  AUDIO_EXT_DATA_CONTROL_PIPE "/data/vendor/local/media/mmi.audio.ctrl"
#define  MMI_DEFAULT_PCM_FILE "/data/vendor/local/media/aploopback.pcm"
#define AUDIO_DATA_PATH "/data/vendor/local/media"
#else
#define  AUDIO_EXT_DATA_CONTROL_PIPE "/data/local/media/mmi.audio.ctrl"
#define  MMI_DEFAULT_PCM_FILE "/data/local/media/aploopback.pcm"
#define AUDIO_DATA_PATH "/data/local/media"
#endif
#define AUDIO_EXT_CONTROL_PIPE_MAX_BUFFER_SIZE  1024
#define AUDIO_HARDWARE_NAME_PROPERTY  "persist.vendor.audiohardware"
#define AUDIO_DEFAULT_HARDWARE_NAME  "base"

void free_audio_config(struct audio_config_handle *config);
void audiohal_pcmdump(void *ctl,int flag,void *buffer,int size,int dumptype);
unsigned int getCurrentTimeMs(void);
long getCurrentTimeUs(void);
int string_to_hex(unsigned char * dst,const  char *str, int max_size);
void wirte_buffer_to_log(void *buffer,int size);
#ifdef __cplusplus
}
#endif
#endif
