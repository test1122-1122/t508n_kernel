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

#ifndef AUDIO_A2DP_OFFLOAD_H
#define AUDIO_A2DP_OFFLOAD_H

#include <errno.h>
#include <pthread.h>
#include <stdint.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/mman.h>
#include <semaphore.h>

#include <log/log.h>
#include <cutils/str_parms.h>
#include <cutils/properties.h>
#include <hardware/hardware.h>
#include <system/audio.h>
#include <hardware/audio.h>
#include <tinyalsa/asoundlib.h>
#include <tinycompress/tinycompress.h>
#include "cutils/list.h"
#include "compress_util.h"
#define UNUSED_ATTR __attribute__((unused))

#define SOCKET_BUFFER_SIZE     128
#define A2DP_EXT_CONTROL_PIPE_MAX_BUFFER_SIZE   128

typedef struct{
    int SBCENC_Mode;
    int SBCENC_Blocks;
    int SBCENC_SubBands;
    int SBCENC_SamplingFreq;
    int SBCENC_AllocMethod;
    int SBCENC_Min_Bitpool;
    int SBCENC_Max_Bitpool;
}SBCENC_PARAM_T;

struct mixer_control {
    char *name;
    struct mixer_ctl *ctl;
    int value;
    int val_count;
    char *strval; 
};

struct device_control {
    char *name;
    unsigned int ctl_size;
    struct mixer_control *ctl;
};

struct device_route {
    char *name;
    int devices;
    struct device_control ctl_on;
    struct device_control ctl_off;
};

struct a2dp_route {
    struct device_route vbc_iis_mux_route;
    struct device_route be_switch_route;
    struct device_route vbc_iis;
};

typedef enum {
    AUDIO_HW_APP_INVALID = -1,
    AUDIO_HW_APP_PRIMARY=0,
    AUDIO_HW_APP_OFFLOAD,
    AUDIO_HW_APP_CALL,
    AUDIO_HW_APP_FM,
    AUDIO_HW_APP_VOIP,
    AUDIO_HW_APP_FAST,
    AUDIO_HW_APP_DEEP_BUFFER,
    AUDIO_HW_APP_NORMAL_RECORD,
    AUDIO_HW_APP_DSP_LOOP,
    AUDIO_HW_APP_VOIP_BT,
    AUDIO_HW_APP_FM_RECORD,
    AUDIO_HW_APP_VOIP_RECORD,
    AUDIO_HW_APP_CALL_RECORD,
    AUDIO_HW_APP_BT_RECORD,
    AUDIO_HW_APP_A2DP_OFFLOAD,
    AUDIO_HW_APP_A2DP_OFFLOAD_MIXER,
    AUDIO_HW_APP_MMAP,
    AUDIO_HW_APP_MMAP_RECORD,
    AUDIO_HW_APP_RECOGNITION,
    AUDIO_HW_APP_HIDL_OUTPUT,
} AUDIO_HW_APP_T;

typedef enum {
    UC_CALL     = 0x1,
    UC_VOIP     = 0x2,
    UC_FM       = 0x4,
    UC_NORMAL_PLAYBACK    = 0x8,
    UC_LOOP     = 0x10,
    UC_MM_RECORD = 0x20,
    UC_AGDSP_ASSERT = 0x40,
    UC_FM_RECORD = 0x80,
    UC_FAST_PLAYBACK = 0x100,
    UC_DEEP_BUFFER_PLAYBACK = 0x200,
    UC_OFFLOAD_PLAYBACK =0x400,
    UC_VOICE_RECORD = 0x800,
    UC_VOIP_RECORD = 0x1000,
    UC_AUDIO_TEST = 0x2000,
    UC_BT_RECORD = 0x4000,
    UC_VBC_PCM_DUMP     = 0x8000,
    UC_BT_OFFLOAD_MIXER = 0x10000,
    UC_BT_OFFLOAD = 0x20000,
    UC_MMAP_PLAYBACK = UC_BT_OFFLOAD<<1,
    UC_MMAP_RECORD = UC_MMAP_PLAYBACK<<1,
    UC_RECOGNITION = UC_MMAP_RECORD<<1,
    UC_SYSTEM_UNSLEEP = UC_RECOGNITION<<1,
    UC_UNKNOWN = 0x00000000,
} USECASE;

typedef int  (*AUDIO_OUTPUT_STANDBY_FUN)(void *dev,void *out,AUDIO_HW_APP_T audio_app_type);

typedef enum {
    AUDIO_OUTPUT_DESC_NONE = 0,
    AUDIO_OUTPUT_DESC_PRIMARY = 0x1,
    AUDIO_OUTPUT_DESC_OFFLOAD = 0x10
} AUDIO_OUTPUT_DESC_T;

typedef enum {
    AUDIO_OFFLOAD_CMD_EXIT,               /* exit compress offload thread loop*/
    AUDIO_OFFLOAD_CMD_DRAIN,              /* send a full drain request to driver */
    AUDIO_OFFLOAD_CMD_PARTIAL_DRAIN,      /* send a partial drain request to driver */
    AUDIO_OFFLOAD_CMD_WAIT_FOR_BUFFER    /* wait for buffer released by driver */
} AUDIO_OFFLOAD_CMD_T;

/* Flags used to indicate current offload playback state */
typedef enum {
    AUDIO_OFFLOAD_STATE_STOPED,
    AUDIO_OFFLOAD_STATE_PLAYING,
    AUDIO_OFFLOAD_STATE_PAUSED
} AUDIO_OFFLOAD_STATE_T;

typedef enum {
    AUDIO_OFFLOAD_MIXER_INVALID = -1,
    AUDIO_OFFLOAD_MIXER_CARD0 = 0,      //ap main
    AUDIO_OFFLOAD_MIXER_CARD1,          //ap compress
    AUDIO_OFFLOAD_MIXER_CARD2          //cp compress-mixer
} AUDIO_OFFLOAD_MIXER_CARD_T;

struct audio_offload_cmd {
    struct listnode node;
    AUDIO_OFFLOAD_CMD_T cmd;
    int data[];
};

struct offload_pcm_param{
    int card;
    int device;
    struct pcm_config config;
};

struct a2dp_compress_param{
    int card;
    int device;
    int offload_fragement_size;
    int offload_fragements;
};

struct stream_param{
    struct offload_pcm_param mixer;
    struct a2dp_compress_param  sprd_out_stream;
};

struct a2dp_primary_msg_handle {
    bool is_exit;
    pthread_t   msgthread;
    sem_t       sem;
    sem_t       sync_sem;

    pthread_mutex_t lock;

    pthread_mutex_t msg_lock;
    char *msg;
};

struct sprd_out_stream {
    struct audio_stream_out stream;

    pthread_mutex_t lock;       /* see note below on mutex acquisition order */
    struct pcm_config *config;

    struct listnode node;
    AUDIO_OUTPUT_STANDBY_FUN standby_fun;

    bool standby;
    audio_output_flags_t flags;
    struct sprdout_ctl *stream_ctl;
    void *adev;
    int written;
    int write_threshold;
    struct compr_config compress_config;
    struct compr_gapless_mdata gapless_mdata;
    struct compr_mdata compr_mdata;
    stream_callback_t audio_offload_callback;
    void *audio_offload_cookie;
    AUDIO_HW_APP_T audio_app_type;         /* 0:primary; 1:offload */
    AUDIO_OFFLOAD_STATE_T
    audio_offload_state;    /* AUDIO_OFFLOAD_STOPED; PLAYING; PAUSED */
    unsigned int offload_format;
    unsigned int offload_samplerate;
    unsigned int offload_channel_mask;
    bool is_offload_compress_started;      /* flag indicates compress_start state */
    bool is_offload_need_set_metadata;     /* flag indicates to set the metadata to driver */
    bool is_audio_offload_thread_blocked;  /* flag indicates processing the command */
    int is_offload_nonblocking;            /* flag indicates to use non-blocking write */
    pthread_cond_t audio_offload_cond;
    pthread_t audio_offload_thread;
    struct listnode audio_offload_cmd_list;

    struct pcm *pcm;
    struct compress *compress;
    int a2dp_sbc_format;
    unsigned long dsp_frames;
};


struct sprdout_ctl {
    pthread_mutex_t lock;       /* see note below on mutex acquisition order */
    struct mixer *mixer;

    struct mixer_ctl *offload_dg;

    float offload_volume_index;

    struct stream_param stream_para;

    pthread_mutex_t kcontrol_lock;
    struct a2dp_route route;
    struct mixer_ctl *dsp_sleep_ctl;
    struct mixer_ctl *bt_sbc_ctl;
    bool agdsp_sleep_status;
    int usecase;
    SBCENC_PARAM_T sbc_param;
};

void init_sprd_offload_stream(void  *stream);
int set_usecase(struct sprdout_ctl *offload_ctl , int usecase, bool on);
int sprd_close_output_stream(struct audio_stream_out *stream);
#define DEBUG_LINE   ALOGI("%s %d",__func__,__LINE__);
#ifdef __cplusplus
extern "C" {
#endif
int  parse_offload_config(struct stream_param *stream_para,const char *config_file);
int parse_a2dp_route(struct a2dp_route *route,
    struct mixer *mixer,const char *config_file);
void free_a2dp_route(struct a2dp_route *route);

int audio_start_compress_output(struct audio_stream_out *out_p);
int audio_get_compress_metadata(struct audio_stream_out *out_p,
                                struct str_parms *parms);
int audio_offload_create_thread(struct audio_stream_out  *out_p) ;

int audio_offload_destroy_thread(struct audio_stream_out  *out_p) ;
ssize_t out_write_compress(struct audio_stream_out *out_p, const void *buffer,
                           size_t bytes) ;
bool audio_is_offload_support_format(audio_format_t format);
int audio_get_offload_codec_id(audio_format_t format);

#ifdef __cplusplus
}
#endif
#endif
