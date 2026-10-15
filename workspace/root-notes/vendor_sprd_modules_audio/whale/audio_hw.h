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

#ifndef _AUDIO_HW_H_
#define _AUDIO_HW_H_
#include <errno.h>
#include <pthread.h>
#include <stdint.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/mman.h>
#include <semaphore.h>
#include <sys/sysinfo.h>


#ifdef AUDIOHAL_V4
#include <log/log.h>
#else
#include <cutils/log.h>
#endif
#include <cutils/str_parms.h>
#include <cutils/properties.h>
#include <hardware/hardware.h>
#include <system/audio.h>
#include <hardware/audio.h>
#include <tinyalsa/asoundlib.h>
#include <audio_utils/resampler.h>
#include <tinycompress/tinycompress.h>
#include "audio_offload.h"
#include "audio_debug.h"
#include "ring_buffer.h"
#include "dsp_control.h"
#include "audio_param.h"
#include "record_nr_api.h"
#include <stdio.h>
#include <alsa_device_profile.h>
#include <alsa_device_proxy.h>
#include <sys/types.h>
#include <errno.h>
#include "endpoint_test.h"
#include "DspPreProcessing.h"
#include "compress_util.h"

#ifdef AWINIC_EFFECT_SUPPORT
#include "AwinicAPI.h"
#include<dlfcn.h>
#include<string.h>
#include<cutils/properties.h>
#define AWINIC_LIB_PATH    "/vendor/lib/hw/awinic.audio.effect.so"
#define AWINIC_PARAMS_PATH "/vendor/firmware/awinic_params.bin"
#endif


/* ALSA cards for sprd */
#define CARD_SPRDPHONE "sprdphone4adnc"

typedef enum {
    DAI_ID_NORMAL_OUTDSP_PLAYBACK=0,
    DAI_ID_NORMAL_OUTDSP_CAPTURE,
    DAI_ID_NORMAL_WITHDSP,
    DAI_ID_FAST_P,
    DAI_ID_OFFLOAD,
    DAI_ID_VOICE,
    DAI_ID_VOIP,
    DAI_ID_FM,
    DAI_ID_FM_C_WITHDSP,
    DAI_ID_VOICE_CAPTURE,
    DAI_ID_LOOP ,
    DAI_ID_BT_CAPTURE ,
} VBC_DAI_ID_T;

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
    AUDIO_HW_APP_VOICE_TX,
} AUDIO_HW_APP_T;

enum RECORD_NR_MASK {
    RECORD_48000_NR =0,
    RECORD_44100_NR =1,
    RECORD_16000_NR =2,
    RECORD_8000_NR =3,
    RECORD_UNSUPPORT_RATE_NR =4,
};

typedef enum {
    SWITCH_DEVICE,    /*select device*/
    SET_FM_VOLUME,
} SWITCH_DEVICE_CMD_T;

struct switch_device_cmd {
    struct listnode node;
    SWITCH_DEVICE_CMD_T cmd;
    AUDIO_HW_APP_T audio_app_type;
    int device;
    bool is_in;
    bool update_param;
    bool is_sync;
    bool is_force;
    int param1;
    sem_t       sync_sem;
};

typedef enum {
    SET_OUTPUT_DEVICE,
} AUDIO_SET_CMD_T;

struct audio_event_cmd {
    struct listnode node;
    AUDIO_SET_CMD_T cmd;
    int device;
};

/* ALSA ports for sprd */

typedef enum {
    AUD_NET_GSM_MODE,
    AUD_NET_TDMA_MODE,
    AUD_NET_WCDMA_MODE,
    AUD_NET_WCDMA_NB_MODE,
    AUD_NET_WCDMA_WB_MODE,
    AUD_NET_VOLTE_MODE,
    AUD_NET_VOLTE_NB_MODE,
    AUD_NET_VOLTE_WB_MODE,
    AUD_NET_MAX,
} AUDIO_NET_M;


/* 48000 pcm data */
static const unsigned char s_pcmdata_mono[] = {
    0x00,0x00,0x26,0x40,0x60,0x08,0x9a,0x3f,0x9b,0x10,0xf7,0x3d,0x8c,0x18,0x44,0x3b,
    0x13,0x20,0x8f,0x37,0x0e,0x27,0xe5,0x32,0x5c,0x2d,0x5d,0x2d,0xe5,0x32,0x0d,0x27,
    0x8e,0x37,0x13,0x20,0x44,0x3b,0x8d,0x18,0xf6,0x3d,0x9a,0x10,0x9a,0x3f,0x5f,0x08,
    0x26,0x40,0x00,0x00,0x9a,0x3f,0xa0,0xf7,0xf7,0x3d,0x66,0xef,0x44,0x3b,0x73,0xe7,
    0x8e,0x37,0xed,0xdf,0xe5,0x32,0xf3,0xd8,0x5c,0x2d,0xa4,0xd2,0x0d,0x27,0x1b,0xcd,
    0x13,0x20,0x72,0xc8,0x8d,0x18,0xbc,0xc4,0x9a,0x10,0x0a,0xc2,0x5f,0x08,0x66,0xc0,
    0x00,0x00,0xda,0xbf,0xa1,0xf7,0x66,0xc0,0x65,0xef,0x09,0xc2,0x73,0xe7,0xbc,0xc4,
    0xed,0xdf,0x72,0xc8,0xf2,0xd8,0x1b,0xcd,0xa4,0xd2,0xa3,0xd2,0x1b,0xcd,0xf4,0xd8,
    0x72,0xc8,0xed,0xdf,0xbc,0xc4,0x73,0xe7,0x09,0xc2,0x66,0xef,0x66,0xc0,0xa1,0xf7,
    0xda,0xbf,0x00,0x00,0x67,0xc0,0x5f,0x08,0x09,0xc2,0x9a,0x10,0xbc,0xc4,0x8d,0x18,
    0x71,0xc8,0x12,0x20,0x1b,0xcd,0x0d,0x27,0xa3,0xd2,0x5d,0x2d,0xf3,0xd8,0xe4,0x32,
    0xed,0xdf,0x8e,0x37,0x74,0xe7,0x45,0x3b,0x65,0xef,0xf7,0x3d,0xa0,0xf7,0x99,0x3f,
};

/* 48000 pcm data */
static const unsigned char s_pcmdata_left[] = {
    0x01,0x00,0x00,0x00,0x5f,0x08,0x00,0x00,0x9a,0x10,0x00,0x00,0x8c,0x18,0x00,0x00,
    0x14,0x20,0x00,0x00,0x0d,0x27,0x00,0x00,0x5d,0x2d,0x00,0x00,0xe5,0x32,0x00,0x00,
    0x8f,0x37,0x00,0x00,0x44,0x3b,0x00,0x00,0xf7,0x3d,0x00,0x00,0x99,0x3f,0x00,0x00,
    0x27,0x40,0x00,0x00,0x9a,0x3f,0x00,0x00,0xf7,0x3d,0x00,0x00,0x44,0x3b,0x00,0x00,
    0x8e,0x37,0x00,0x00,0xe5,0x32,0x00,0x00,0x5c,0x2d,0x00,0x00,0x0d,0x27,0x00,0x00,
    0x13,0x20,0x00,0x00,0x8d,0x18,0x00,0x00,0x9a,0x10,0x00,0x00,0x60,0x08,0x00,0x00,
    0x01,0x00,0x00,0x00,0xa0,0xf7,0x00,0x00,0x66,0xef,0x00,0x00,0x73,0xe7,0x00,0x00,
    0xed,0xdf,0x00,0x00,0xf2,0xd8,0x00,0x00,0xa4,0xd2,0x00,0x00,0x1b,0xcd,0x00,0x00,
    0x71,0xc8,0x00,0x00,0xbb,0xc4,0x00,0x00,0x09,0xc2,0x00,0x00,0x66,0xc0,0x00,0x00,
    0xd9,0xbf,0x00,0x00,0x66,0xc0,0x00,0x00,0x08,0xc2,0x00,0x00,0xbc,0xc4,0x00,0x00,
    0x72,0xc8,0x00,0x00,0x1c,0xcd,0x00,0x00,0xa4,0xd2,0x00,0x00,0xf2,0xd8,0x00,0x00,
    0xed,0xdf,0x00,0x00,0x74,0xe7,0x00,0x00,0x65,0xef,0x00,0x00,0xa1,0xf7,0x00,0x00,
};

/* 48000 pcm data */
static const unsigned char s_pcmdata_right[] = {
    0x00,0x00,0x26,0x40,0x00,0x00,0x9a,0x3f,0x00,0x00,0xf6,0x3d,0x00,0x00,0x44,0x3b,
    0x00,0x00,0x8e,0x37,0x00,0x00,0xe5,0x32,0x00,0x00,0x5d,0x2d,0x00,0x00,0x0d,0x27,
    0x00,0x00,0x14,0x20,0x00,0x00,0x8d,0x18,0x00,0x00,0x9a,0x10,0x00,0x00,0x60,0x08,
    0x00,0x00,0x00,0x00,0x00,0x00,0xa1,0xf7,0x00,0x00,0x66,0xef,0x00,0x00,0x73,0xe7,
    0x00,0x00,0xec,0xdf,0x00,0x00,0xf3,0xd8,0x00,0x00,0xa4,0xd2,0x00,0x00,0x1b,0xcd,
    0x00,0x00,0x72,0xc8,0x00,0x00,0xbc,0xc4,0x00,0x00,0x09,0xc2,0x00,0x00,0x66,0xc0,
    0x00,0x00,0xdb,0xbf,0x00,0x00,0x66,0xc0,0x00,0x00,0x09,0xc2,0x00,0x00,0xbc,0xc4,
    0x00,0x00,0x71,0xc8,0x00,0x00,0x1b,0xcd,0x00,0x00,0xa4,0xd2,0x00,0x00,0xf3,0xd8,
    0x00,0x00,0xee,0xdf,0x00,0x00,0x73,0xe7,0x00,0x00,0x65,0xef,0x00,0x00,0xa0,0xf7,
    0x00,0x00,0x00,0x00,0x00,0x00,0x60,0x08,0x00,0x00,0x9a,0x10,0x00,0x00,0x8c,0x18,
    0x00,0x00,0x13,0x20,0x00,0x00,0x0d,0x27,0x00,0x00,0x5d,0x2d,0x00,0x00,0xe5,0x32,
    0x00,0x00,0x8e,0x37,0x00,0x00,0x44,0x3b,0x00,0x00,0xf7,0x3d,0x00,0x00,0x9a,0x3f,
};
/**
    container_of - cast a member of a structure out to the containing structure
    @ptr:    the pointer to the member.
    @type:   the type of the container struct this is embedded in.
    @member: the name of the member within the struct.

*/
#define container_of(ptr, type, member) ({      \
        const typeof( ((type *)0)->member ) *__mptr = (ptr);    \
        (type *)( (char *)__mptr - offsetof(type,member) );})


#define CHECK_OUT_DEVICE_IS_INVALID(out_device) \
        (((out_device & AUDIO_DEVICE_OUT_ALL) & ~AUDIO_DEVICE_OUT_ALL) || (AUDIO_DEVICE_NONE == out_device))

/* constraint imposed by VBC: all period sizes must be multiples of 160 */
#define VBC_BASE_FRAME_COUNT 160
/* number of base blocks in a short period (low latency) */
#define SHORT_PERIOD_MULTIPLIER 8 /* 29 ms */
/* number of frames per short period (low latency) */
#define SHORT_PERIOD_SIZE (VBC_BASE_FRAME_COUNT * SHORT_PERIOD_MULTIPLIER)
/* minimum sleep time in out_write() when write threshold is not reached */
#define MIN_WRITE_SLEEP_US 5000
#define RESAMPLER_BUFFER_FRAMES (SHORT_PERIOD_SIZE * 2)
#define RESAMPLER_BUFFER_SIZE (4 * RESAMPLER_BUFFER_FRAMES)

#define DEFAULT_OUT_SAMPLING_RATE 48000
#define DEFAULT_INPUT_SAMPLING_RATE 16000

#define MM_FULL_POWER_SAMPLING_RATE 48000
#define RECORD_POP_MIN_TIME    500   // ms

#define DUMP_AUDIO_PROPERTY     "persist.vendor.media.audio.dump"
#define DUMP_BUFFER_MAX_SIZE    256

#define VBC_REG_FILE "/proc/asound/sprdphonesc2730/vbc"
#define SPRD_CODEC_REG_FILE "/proc/asound/sprdphonesc2730/sprd-codec"
#define AUDIO_DMA_REG_FILE "/proc/asound/sprdphonesc2730/sprd-dmaengine"

#define REG_SPLIT "\n"


//#define AUDIO_DUMP_WRITE_STR(fd,buffer)     ((int)fd> 0) ? write(fd, buffer, strlen(buffer)): ALOGI("%s\n",buffer)

#ifdef AUDIOHAL_V4
#define AUDIO_DEVICE_OUT_FM_SPEAKER   0x20000000
#define AUDIO_DEVICE_OUT_FM_HEADSET   0x10000000
#endif
#define ALL_USB_OUTPUT_DEVICES   (AUDIO_DEVICE_OUT_USB_HEADSET|AUDIO_DEVICE_OUT_USB_DEVICE)
#define ALL_USB_INPUT_DEVICES   (AUDIO_DEVICE_IN_USB_HEADSET|AUDIO_DEVICE_IN_USB_DEVICE)

extern void register_offload_pcmdump(struct compress *compress,void *fun);

typedef enum {
    ADEV_DUMP_DEFAULT,
    ADEV_DUMP_TINYMIX,
    ADEV_DUMP_VBC_REG,
    ADEV_DUMP_CODEC_REG,
    ADEV_DUMP_DMA_REG,
    ADEV_DUMP_PCM_ERROR_CODEC_REG,
    ADEV_DUMP_PCM_ERROR_VBC_REG,
    ADEV_DUMP_PCM_ERROR_DMA_REG,
}audio_dump_switch_e;

enum Result{
    OK,
    NOT_INITIALIZED,
    INVALID_ARGUMENTS,
    INVALID_STATE,
    /**
     * Methods marked as "Optional method" must return this result value
     * if the operation is not supported by HAL.
     */
    NOT_SUPPORTED
};

typedef int  (*AUDIO_OUTPUT_STANDBY_FUN)(void *dev,void *out,AUDIO_HW_APP_T audio_app_type);

struct stream_routing_manager {
    pthread_t   routing_switch_thread;
    bool        is_exit;
    sem_t       device_switch_sem;
};

typedef struct {
    timer_t timer_id;
    bool created;
} audio_timer_t;

struct bt_sco_thread_manager {
    bool             thread_is_exit;
    pthread_t        dup_thread;
    pthread_mutex_t  dup_mutex;
    pthread_mutex_t  cond_mutex;
    pthread_cond_t   cond;
    sem_t            dup_sem;
    volatile bool    dup_count;
    volatile bool    dup_need_start;
};

enum aud_dev_test_m {
    TEST_AUDIO_IDLE =0,
    TEST_AUDIO_OUT_DEVICES,
    TEST_AUDIO_IN_DEVICES,
    TEST_AUDIO_IN_OUT_LOOP,
    TEST_AUDIO_OUT_IN_LOOP,
};
struct stream_test_t{
    pthread_t thread;
    bool is_exit;
    sem_t   sem;
    int fd;
    void *stream;
    struct ring_buffer *ring_buf;
};

struct  dev_test_config{
    int channel;
    int sample_rate;
    int fd;
    int delay;
    int in_devices;
    int out_devices;
};

struct dev_test_t {
    struct stream_test_t in_test;
    struct stream_test_t  out_test;
    struct ring_buffer *ring_buf;
    struct dev_laohua_test_info_t *dev_laohua_in_test_info;
    struct dev_laohua_test_info_t  *dev_laohua_out_test_info;
    bool is_in_test ;
};

struct loop_ctl_t
{
    pthread_t thread;
    sem_t   sem;
    bool is_exit;
    pthread_mutex_t lock;
    void *dev;
    bool state;
    struct pcm *pcm;
    struct ring_buffer *ring_buf;
};

struct dsp_loop_t {
    struct loop_ctl_t in;
    struct loop_ctl_t out;
    bool state;
};

typedef enum {
    VOICE_INVALID_STATUS   = 0,
    VOICE_PRE_START_STATUS  =  1,
    VOICE_START_STATUS  =  2,
    VOICE_PRE_STOP_STATUS  =  3,
    VOICE_STOP_STATUS  =  4,
} voice_status_t;

typedef enum {
    USB_CHANNEL_INITED   = 0,
    USB_CHANNEL_OPENED  = 1,
    USB_CHANNEL_STARTED  =  2,
    USB_CHANNEL_STOPED  =  3,
    USB_CHANNEL_CLOSED  =  4,
} usb_status_t;

struct usbaudio_ctl {
    alsa_device_profile out_profile;
    alsa_device_profile in_profile;
    alsa_device_proxy in_proxy;
    alsa_device_proxy out_proxy;
    usb_status_t output_status;
    usb_status_t input_status;
    bool support_offload;
    bool support_record;
    pthread_mutex_t lock;
};

struct modem_monitor_handler {
    pthread_t thread_id;
    void *dev;
    int pipe_fd[2];
    bool is_running;
    pthread_mutex_t lock;
};

#define DEVICE_CHANGE_REG_DUMP_COUNT  2
#define DEFAULT_REG_DUMP_COUNT  3
#define VOICE_REG_DUMP_COUNT  5

struct debug_dump_handler {
    pthread_t thread_id;
    bool is_close;
    bool is_start;
    int dumpcount;
    int timems;
    pthread_mutex_t lock;
    pthread_mutex_t cond_lock;
    pthread_cond_t cond;
};

struct tiny_audio_device {
    struct audio_hw_device hw_device;

    pthread_mutex_t lock;       /* see note below on mutex acquisition order */
    struct mixer *mixer;
    struct audio_control *dev_ctl;
    struct dev_test_t dev_test;
    audio_mode_t call_mode;
    voice_status_t call_status;
    bool low_power;
    bool mic_mute;
    bool dsp_mic_mute;
    bool bluetooth_nrec;
    int bluetooth_type;
    bool master_mute;
    int audio_outputs_state;
    int voice_volume;

    AUDIO_PARAM_T  audio_param;


    struct pcm *pcm_modem_dl;
    int offload_on;

    pthread_mutex_t output_list_lock;
    struct listnode active_out_list;

    pthread_mutex_t input_list_lock;
    struct listnode active_input_list;
    struct dsp_loop_t loop_ctl;
    bool loop_from_dsp;

    bool voip_start;
    bool sprd_aec_on;
    int sprd_aec_effect_valid;
    bool normal_record_start;
    bool voip_record_start;
    bool fm_record_start;
    pthread_mutex_t voip_start_lock;
    bool fm_record;
    bool fm_bydsp;
    bool fm_start;
    bool bt_wbs ;

#ifdef LOCAL_SOCKET_SERVER
    struct socket_handle local_socket;
#endif
    int dump_flag;

    bool call_start;
    int is_agdsp_asserted;
    int is_dsp_loop;
    bool pipe_process_exit;
    int sprdaudio_usbsupport;
    int inputs_open;
    int usbchannel_status;
    bool usboutput_init;
    int usboutput_refcount;
    unsigned int device_sample_rate;
    struct endpoint_control test_ctl;
#ifdef AUDIOHAL_V4
    audio_patch_handle_t patch_handle;
#endif
    audio_devices_t in_devices;
    audio_devices_t out_devices;
    struct usbaudio_ctl usb_ctl;
    struct modem_monitor_handler modem_monitor;
    bool boot_completed;
    audio_timer_t bootup_timer; //for bootup
    bool bt_sco_status;
#ifdef AUDIO_DEBUG
    struct debug_dump_handler debugdump;
#endif
    bool issupport_usb;
	#ifdef AWINIC_EFFECT_SUPPORT  
	aw_skt_t awinic_skt; 
	#endif
};

#define AUDIO_OUT_TIME_BUFFER_COUNT  4

#define USB_OFFLOAD_SAMPLERATE 48000
#define USB_OFFLOAD_FORMAT PCM_FORMAT_S16_LE

struct tiny_stream_out {
    struct audio_stream_out stream;

    pthread_mutex_t lock;       /* see note below on mutex acquisition order */
    struct pcm_config *config;
    struct audio_config request_config;
    unsigned int requested_rate;
    struct pcm *pcm;
    struct resampler_itfe *resampler;
    int resampler_buffer_size;
    char *buffer;

    int is_voip;
    int is_bt_sco;

    struct listnode node;
    AUDIO_OUTPUT_STANDBY_FUN standby_fun;

    bool standby;
    audio_devices_t devices;
    audio_devices_t apm_devices;//only modify by audiopolicymanager
    audio_output_flags_t flags;
    bool low_power;
    struct tiny_audio_device *dev;

    int written;
    int write_threshold;
    struct compr_config compress_config;
    struct compress *compress;
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

#ifdef SPRD_AUDIO_HIDL_CLIENT
    int diagsize;
#endif
    /*used for voice call*/
    struct pcm *pcm_modem_ul;
    struct pcm *pcm_modem_dl;

    int primary_latency;
    int primary_out_buffer_size;

    int playback_started;
    unsigned int time1[AUDIO_OUT_TIME_BUFFER_COUNT];//start out-write
    unsigned int time2[AUDIO_OUT_TIME_BUFFER_COUNT];//exit out_write
    int timeID;
    bool testMode;
    enum pcm_format format;
    void *format_buffer;
    int format_buffer_len;
    struct timespec last_timespec;
};

struct tiny_stream_in {
    struct audio_stream_in stream;

    pthread_mutex_t lock;       /* see note below on mutex acquisition order */
    struct pcm_config *config;
    struct pcm *pcm;
    struct pcm *mux_pcm;
    bool standby;
    unsigned int requested_rate;
    unsigned int requested_channels;
    struct resampler_itfe *resampler;
    struct resampler_buffer_provider buf_provider;
    size_t frames_in;
    int16_t *buffer;
    int16_t *channel_buffer;
    int16_t *nr_buffer;
    audio_devices_t devices;
    bool pop_mute;
    int pop_mute_bytes;
    int16_t *proc_buf;
    size_t proc_buf_size;
    size_t proc_frames_in;
    int read_status;
    int capture_started;
    int64_t frames_read; /* total frames read, not cleared when entering standby */
    audio_source_t source;
    bool active_rec_proc;
    record_nr_handle rec_nr_handle;
    struct tiny_audio_device *dev;
    unsigned int readbytes;

    struct listnode node;

    AUDIO_OUTPUT_STANDBY_FUN standby_fun;
    AUDIO_HW_APP_T audio_app_type;         /* 0:primary; 1:offload */
    recordproc_handle recordproc_handle;
};

#ifdef __cplusplus
extern "C" {
#endif

bool init_rec_process(struct audio_stream_in *stream, int sample_rate);
int aud_rec_do_process(void *buffer, size_t bytes, int channels,void *tmp_buffer,
                              size_t tmp_buffer_bytes);
struct tiny_stream_out * force_out_standby(struct tiny_audio_device *adev,AUDIO_HW_APP_T audio_app_type);
void force_in_standby(struct tiny_audio_device *adev,AUDIO_HW_APP_T audio_app_type);
int GetAudio_InMode_number_from_device(int in_dev);
int Aud_Dsp_Boot_OK(void * adev);
int do_output_standby(struct tiny_stream_out *out);
struct tiny_stream_out * get_output_stream(struct tiny_audio_device *adev,AUDIO_HW_APP_T audio_app_type);
ssize_t normal_out_write(struct tiny_stream_out *out,void *buffer,
                                size_t bytes);
int start_output_stream(struct tiny_stream_out *out);
int do_input_standby(struct tiny_stream_in *in);
int is_call_active_unlock(struct tiny_audio_device *adev);
int adev_open_input_stream(struct audio_hw_device *dev,
                                  audio_io_handle_t handle,
                                  audio_devices_t devices,
                                  struct audio_config *config,
                                  struct audio_stream_in **stream_in,
                                  audio_input_flags_t flags,
                                  const char *address __unused,
                                  audio_source_t source );
int32_t  mono2stereo(int16_t *out, int16_t * in, uint32_t in_samples);
int audio_add_output(struct tiny_audio_device *adev,struct tiny_stream_out *out);
void force_all_standby(void *dev);
int aud_dsp_assert_set(void * dev, bool asserted);
int set_voice_status(struct audio_hw_device *dev, voice_status_t status);
int start_usb_channel(struct usbaudio_ctl *usb_ctl,bool is_output);
int stop_usb_channel(struct usbaudio_ctl *usb_ctl,bool is_output);
int open_usboutput_channel(void *dev);
int open_usbinput_channel(void *dev);
void disconnect_usb(struct usbaudio_ctl *usb_ctl,bool is_output);
void output_sync(struct tiny_audio_device *adev,AUDIO_HW_APP_T audio_app_type);
int adev_out_apm_devices_check(void *dev,audio_devices_t device);
void dump_all_audio_reg(int fd,int dump_flag);
#ifdef AUDIO_DEBUG
int debug_dump_open(struct debug_dump_handler *debugdump);
void debug_dump_close(struct debug_dump_handler *debugdump);
void debug_dump_start(struct debug_dump_handler *debugdump,int count);
void debug_dump_stop(struct debug_dump_handler *debugdump);
ssize_t out_write_test(struct audio_stream_out *stream, const void *buffer,
    size_t bytes);
void set_out_testmode(struct tiny_audio_device *adev,AUDIO_HW_APP_T audio_app_type,bool testMode);
#endif
#ifdef __cplusplus
}
#endif
#endif //_AUDIO_HW_H_
