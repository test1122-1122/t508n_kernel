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

#ifndef _AUDIO_CONTROL_H_
#define _AUDIO_CONTROL_H_
#include <tinyalsa/asoundlib.h>
#include "audio_xml_utils.h"
#include "audio_hw.h"
#include "tinycompress/tinycompress.h"
#include "audio_param.h"
#include "smartamp.h"

typedef enum {
    AUDIO_NET_UNKNOWN=0x0,
    AUDIO_NET_TDMA=0x1<<0,
    AUDIO_NET_GSM=0x1<<1,
    AUDIO_NET_WCDMA=0x1<<2,
    AUDIO_NET_VOLTE=0x1<<4,
    AUDIO_NET_VOWIFI=0x1<<5,
    AUDIO_NET_CDMA2000=0x1<<6,
    AUDIO_NET_LOOP=0x1<<7,
}aud_net_m;

typedef enum {
    AUDIO_NET_NB=0,
    AUDIO_NET_WB=1,
    AUDIO_NET_SWB=2,
    AUDIO_NET_FB=3,
}aud_netrate;

#ifdef FLAG_VENDOR_ETC
#define AUDIO_ROUTE_PATH "/vendor/etc/audio_route.xml"
#define AUDIO_PCM_FILE  "/vendor/etc/audio_pcm.xml"
#else
#define AUDIO_ROUTE_PATH "/system/etc/audio_route.xml"
#define AUDIO_PCM_FILE  "/system/etc/audio_pcm.xml"
#endif

#define CODEC_SPK_MUTE (1<<0)
#define CODEC_EXT_SPK_MUTE (1<<1)
#define CODEC_HANDSET_MUTE (1<<2)
#define CODEC_HEADPHONE_MUTE (1<<3)

#define DSP_DA0_MDG_MUTE (1<<4)
#define DSP_DA1_MDG_MUTE (1<<5)
#define AUDIO_MDG_MUTE (1<<6)
#define AUDIO_MDG23_MUTE (1<<7)

#define CODEC_MIC_MUTE (1<<8)
#define DSP_VOICE_UL_MUTE (1<<16)
#define DSP_VOICE_DL_MUTE (1<<17)

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
    UC_VOICE_TX = UC_SYSTEM_UNSLEEP << 1,
    UC_UNKNOWN = 0x00000000,
} USECASE;

struct device_use_case_t {
    USECASE use;
    const char *case_name;
};

enum {
    PARAM_OUTDEVICES_CHANGE,
    PARAM_INDEVICES_CHANGE,
    PARAM_BT_NREC_CHANGE,
    PARAM_NET_CHANGE,
    PARAM_USECASE_CHANGE,
    PARAM_USECASE_DEVICES_CHANGE,
    PARAM_VOICE_VOLUME_CHANGE,
    PARAM_FM_VOLUME_CHANGE,
    PARAM_AUDIOTESTER_CHANGE,
};

enum {
    AUD_DEVICE_NONE,
    AUD_OUT_SPEAKER,
    AUD_OUT_HEADPHONE,
    AUD_OUT_HEADPHONE_SPEAKER,
    AUD_OUT_EARPIECE,
    AUD_OUT_SCO,
    AUD_IN_BULITION_MIC,
    AUD_IN_BACK_MIC,
    AUD_IN_HEADSET_MIC,
    AUD_IN_DUAL_MIC,
    AUD_IN_SCO,
    AUD_DEVICE_MAX,
};

struct device_map {
    int sprd_device;
    const char *device_name;
};


struct dev_condition_t {
    unsigned int check_size;
    unsigned int *condition;
};

struct  dev_description_t {
    char *device_name;
    struct dev_condition_t dev;
};

enum {
    AUD_SPRD_2731S_CODEC_TYPE = 0,
    AUD_REALTEK_CODEC_TYPE,
    AUD_CODEC_TYPE_MAX,
};

static const char  *audio_codec_chip_name[AUD_CODEC_TYPE_MAX] = {
    "2731S",
    "Realtek",
};

enum {
    UCP_1301_UNKNOW = 0,
    UCP_1301_TYPE,
    UCP_1300A_TYPE,
    UCP_1300B_TYPE,
    UCP_1301_MAX_TYPE
};

static const char  *ucp_1301_chip_name[UCP_1301_MAX_TYPE] = {
    "unknow",
    "ucp_1301",
    "ucp_1300a",
    "ucp_1300b",
};

enum {
    AUD_PCM_MM_NORMAL = 0,
    AUD_PCM_MMAP_NOIRQ,
    AUD_PCM_VOIP,
    AUD_PCM_DIGITAL_FM,
    AUD_PCM_DIGITAL_FM_BYDSP,
    AUD_PCM_DEEP_BUFFER,
    AUD_PCM_MODEM_DL,
    AUD_PCM_DSP_LOOP,
    AUD_PCM_FAST,
    AUD_PCM_VOICE_TX,

    AUD_PCM_MAX,
};

static const char  *pcm_config_name[AUD_PCM_MAX] = {
    "mm_normal",
    "mmap_noirq",
    "voip",
    "fm",
    "fm_bydsp",
    "deep_buffer",
    "modem_dl",
    "dsp_loop",
    "fast",
    "voice_tx",
};

enum {
    AUD_RECORD_PCM_RECOGNITION=0,
    AUD_RECORD_PCM_NORMAL,
    AUD_RECORD_PCM_MMAP,
    AUD_RECORD_PCM_VOICE_UL,
    AUD_RECORD_PCM_VOIP,
    AUD_RECORD_PCM_FM,
    AUD_RECORD_PCM_VOICE_RECORD,
    AUD_RECORD_PCM_DSP_LOOP,
    AUD_RECORD_PCM_BT_RECORD,
    AUD_RECORD_PCM_DUMP,
    AUD_RECORD_PCM_MAX,
};

static const char  *recordpcm_config_name[AUD_RECORD_PCM_MAX] = {
    "recognition",
    "mm_normal",
    "mmap_noirq",
    "modem_ul",
    "voip",
    "fm",
    "voice_c",
    "dsp_loop",
    "btsco_record",
    "vbcdump",
};

enum {
    AUD_PCM_ATTRIBUTE_CHANNELS = 0,
    AUD_PCM_ATTRIBUTE_RATE,
    AUD_PCM_ATTRIBUTE_PERIOD_SIZE,
    AUD_PCM_ATTRIBUTE_PERIOD_COUNT,
    AUD_PCM_ATTRIBUTE_FORMAT,
    AUD_PCM_ATTRIBUTE_START_THRESHOLD,
    AUD_PCM_ATTRIBUTE_STOP_THRESHOLD,
    AUD_PCM_ATTRIBUTE_SILENCE_THRESHOLD,
    AUD_PCM_ATTRIBUTE_AVAIL_MIN,
    AUD_PCM_ATTRIBUTE_DEVICES,
    AUD_PCM_ATTRIBUTE_MAX,
};


#define AUDIO_CONTROL_MAX_SIZE  128

static  const char *pcm_config_attribute[AUD_PCM_ATTRIBUTE_MAX] = {
    "channels",
    "rate",
    "period_size",
    "period_count",
    "format",
    "start_threshold",
    "stop_threshold",
    "silence_threshold",
    "avail_min",
    "device",
};

static const  char *device_none = "none";

#define  VBC_DAC0_DG "VBC DAC0 DG Set"
#define  VBC_DAC1_DG "VBC DAC1 DG Set"

#define  VBC_DAC0_MDG "VBC DAC0 DSP MDG Set"
#define  VBC_DAC1_MDG "VBC DAC1 DSP MDG Set"

#define  VBC_DAC0_SMTHDG "VBC DAC0 SMTHDG Set"
#define  VBC_DAC1_SMTHDG "VBC DAC1 SMTHDG Set"

#define BIT(x,y)  (x>>y)
#define MASK(x,y)  (x&(1<<(y+1)-1))

#define PA_AGC_EN_OFFSET  0
#define PA_MODE_OFFSET  1
#define PA_PGA_GAIN_OFFSET  4
#define PA_CLASS_D_F_OFFSET  11

#define PA_AGC_EN_MASK  1
#define PA_MODE_MASK  3
#define PA_PGA_GAIN_MASK  7
#define PA_CLASS_D_F_MASK  5

#define GET_PA_PARAM_VALUE(para,x,y)    ((para>>x)&((1<<y)-1))

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

struct device_route_handler {
    struct device_route *route;
    unsigned int size;
    struct device_route *pre_in_ctl;
    struct device_route *pre_out_ctl;
};

struct gain_mixer_control {
    char *name;
    struct mixer_ctl *ctl;
    int volume_size;
    int *volume_value;
};

struct private_control {
    unsigned int size;
    struct device_control *priv;
};

struct dsploop_control {
    int type;
    int rate;
    int mode;
    int ctl_size;
    struct mixer_control *ctl;
};

struct private_dsploop_control {
    int size;
    struct dsploop_control *dsp_ctl;
};

struct vbc_device_route {
    char *name;
    int devices[3];
    struct device_control ctl_on;
    struct device_control ctl_off;
};

struct vbc_device_route_handler {
    struct vbc_device_route *route;
    unsigned int size;
    struct vbc_device_route *pre_in_ctl;
    struct vbc_device_route *pre_out_ctl;
};

struct audio_route {
    struct vbc_device_route_handler vbc_iis_mux_route;//vbc iis mux
    struct vbc_device_route_handler be_switch_route;
    struct device_route_handler devices_route;
    struct private_dsploop_control dsploop_ctl;
    struct device_route_handler vbc_iis;
    struct device_route_handler usecase_ctl;
    struct device_route_handler vbc_pcm_dump;
    struct device_route_handler fb_smartamp;
};

struct device_gain {
    char *name;
    int ctl_size;
    int id;
    struct gain_mixer_control *ctl;
};

struct device_usecase_gain {
    int gain_size;
    struct device_gain *dev_gain;
    struct mixer *mixer;
};

struct cards {
    int s_tinycard;
    int s_vaudio;
    int s_vaudio_w;
    int s_vaudio_lte;
    int s_voip;
    int s_bt_sco;
};

struct voice_handle_t {
    bool call_start;
    bool call_connected;
    bool call_prestop;
    struct pcm *pcm_modem_dl;
    struct pcm *pcm_modem_ul;
};

struct btsco_i2s_t {
    int cpuindex;
    char *ctl_file;
    int is_switch;
    int i2s_index;
    int fd;
};

#define MAX_GAIN_DEVICE_NAME_LEN 60


struct _compr_config {
    __u32 fragment_size;
    __u32 fragments;
    int devices;
};

struct pcm_handle_t {
    int playback_devices[AUD_PCM_MAX];
    struct pcm_config play[AUD_PCM_MAX];
    int record_devices[AUD_RECORD_PCM_MAX];
    struct pcm_config record[AUD_RECORD_PCM_MAX];
    struct _compr_config compress;
};

struct sprd_codec_mixer_t {
    struct mixer_ctl *adcl_capture_volume;
    struct mixer_ctl *adcr_capture_volume;

    struct mixer_ctl *spkl_playback_volume;

    struct mixer_ctl *hpl_playback_volume;
    struct mixer_ctl *hpr_playback_volume;

    struct mixer_ctl *ear_playback_volume;

    struct mixer_ctl *inner_pa;

    struct mixer_ctl *hp_inner_pa;

    struct mixer_ctl *dac_playback_volume;
};

struct routing_manager {
    pthread_t   routing_switch_thread;
    bool        is_exit;
    sem_t       device_switch_sem;
};

struct dev_bluetooth_t {
    bool bluetooth_nrec;
    int samplerate;
};

struct voice_net_t {
    aud_net_m net_mode;
    aud_netrate rate_mode;
};

struct audio_param_res {
    int usecase;
    aud_net_m net_mode;
    aud_netrate rate_mode;
    audio_devices_t in_devices;
    audio_devices_t out_devices;
    uint8_t voice_volume;
    struct dev_bluetooth_t bt_infor;
    bool bt_nrec;
    uint8_t codec_type;

    /*fm*/
    uint8_t cur_fm_dg_id;
    uint8_t cur_fm_dg_volume;
    uint8_t fm_volume;

    /*record*/
    uint8_t cur_record_dg_id;

    /*play*/
    uint8_t cur_vbc_playback_id;
    uint8_t cur_playback_dg_id;

    /* voice */
    /*dg*/
    uint8_t cur_voice_dg_id;
    uint8_t cur_voice_dg_volume;

    /*dsp*/
    uint8_t cur_dsp_id;
#ifdef SPRD_AUDIO_SMARTAMP
    uint8_t  cur_dsp_smartamp_id;
#endif
    uint8_t cur_dsp_volume;

    /*vbc*/
    uint8_t cur_vbc_id;

    uint8_t cur_voice_volume;

    /* codec param id */
    uint8_t cur_codec_p_id;
    uint8_t cur_codec_p_volume;
    uint8_t cur_codec_c_id;
};

struct mixer_ctrl_t {
    int value;
    struct mixer_ctl * mixer;
};

struct mute_control {
    struct mixer_ctrl_t spk_mute;
    struct mixer_ctrl_t spk2_mute;
    struct mixer_ctrl_t handset_mute;
    struct mixer_ctrl_t headset_mute;
    struct mixer_ctrl_t linein_mute;

    struct mixer_ctrl_t dsp_da0_mdg_mute;
    struct mixer_ctrl_t dsp_da1_mdg_mute;
    struct mixer_ctrl_t audio_mdg_mute;
    struct mixer_ctrl_t audio_mdg23_mute;

    struct mixer_ctrl_t voice_ul_mute_ctl;
    struct mixer_ctrl_t voice_dl_mute_ctl;
};

typedef enum {
    VBC_INVALID_DMUP    = 0,
    DUMP_POS_DAC0_E     = (1<<0),
    DUMP_POS_DAC1_E     = (1<<1),
    DUMP_POS_A4         = (1<<2),
    DUMP_POS_A3         = (1<<3),
    DUMP_POS_A2         = (1<<4),

    DUMP_POS_A1         = (1<<5),
    DUMP_POS_V2         = (1<<6),
    DUMP_POS_V1         = (1<<7),

    VBC_DISABLE_DMUP    = (1<<24),
} PCM_DUMP_TYPE;

typedef enum {
    PCMDUMP_PRIMARY_PLAYBACK_VBC,
    PCMDUMP_PRIMARY_PLAYBACK_MUSIC,

    PCMDUMP_OFFLOAD_PLAYBACK_DSP,

    PCMDUMP_NORMAL_RECORD_VBC,
    PCMDUMP_NORMAL_RECORD_PROCESS,
    PCMDUMP_NORMAL_RECORD_NR,
    PCMDUMP_NORMAL_RECORD_HAL,

    PCMDUMP_VOIP_PLAYBACK_VBC,
    PCMDUMP_VOIP_RECORD_VBC,

    PCMDUMP_LOOP_PLAYBACK_DSP,
    PCMDUMP_LOOP_PLAYBACK_RECORD,

    PCMDUMP_VOICE_TX,
    PCMDUMP_VOICE_RX,

    PCMDUMP_MAX
} AUDIOHAL_PCM_DUMP_TYPE;

struct vbc_dump_ctl {
    bool is_exit;
    pthread_t   thread;
    struct pcm_config config;
    int dump_fd;
    int pcm_devices;
    const char * dump_name;
};

struct ucp1301_mixer_t {
    struct mixer_ctl *pa_agc_en;
    struct mixer_ctl *pa_mode;
    struct mixer_ctl *pa_agc_gain;
    struct mixer_ctl *pa_class_d_f;
    struct mixer_ctl *rl;
    struct mixer_ctl *power_limit_p2;
};

#define MAX_1301_NUMBER   3

struct ucp_1301_handle_t {
    int ucp_1301_type[MAX_1301_NUMBER];
    int count;
    struct ucp1301_mixer_t ctl[MAX_1301_NUMBER];
};

struct bt_timer_t {
    timer_t timer_id;
    bool created;
};


struct audio_control {
    struct mixer *mixer;
    struct mixer_ctl *vbc_iis_loop;
    struct mixer_ctl *offload_dg;
    struct cards cards;
    struct audio_route route;
    struct device_usecase_gain dg_gain;
    struct sprd_codec_mixer_t codec;
    struct audio_param_res param_res;
    int usecase;
    int fm_volume;
    bool fm_mute;
    bool enable_fm_dspsleep;
    bool fm_policy_mute;
    int voice_volume;
    float music_volume;
    struct routing_manager routing_mgr;
    struct tiny_audio_device *adev;

    struct pcm_handle_t pcm_handle;

    struct mixer_ctl *digital_codec_ctl;

    //use for set dsp volume
    struct mixer_ctl *dsp_volume_ctl;
    struct mixer_ctl *bt_ul_src;
    struct mixer_ctl *bt_dl_src;
    struct mixer_ctl *voice_ul_capture;
    struct mixer_ctl *voice_dl_playback;
    int codec_type;
    pthread_mutex_t lock;
    pthread_mutex_t lock_route;
    struct dsp_control_t *agdsp_ctl;
    AUDIO_PARAM_T  *audio_param;

    audio_devices_t out_devices;
    audio_devices_t in_devices;

    audio_devices_t debug_out_devices;
    audio_devices_t debug_in_devices;

    struct audio_config_handle config;
    struct mute_control mute;

    struct listnode switch_device_cmd_list;
    pthread_mutex_t cmd_lock;

    struct listnode audio_event_list;
    pthread_mutex_t audio_event_lock;
    struct routing_manager audio_event_mgr;

    struct vbc_dump_ctl vbc_dump;
    int access_set;
    int pcmdumpfd[PCMDUMP_MAX];
    int pcmdumpflag;
    int available_outputdevices;
    int available_inputdevices;
    bool set_fm_speaker;
    bool fm_mute_l;
    bool fm_pre_stop;
    int fm_cur_param_id;
    struct smart_amp_ctl smartamp_ctl;
    struct ucp_1301_handle_t ucp_1301;

    struct bt_timer_t bt_ul_mute_timer; //for bt_ul_mute
};

#ifdef __cplusplus
extern "C" {
#endif

int start_fm(struct audio_control *ctl);
int stop_fm(struct audio_control *ctl);
struct audio_control *init_audio_control(struct tiny_audio_device
    *adev);/* define in Audio_parse.cpp */
void free_audio_control(struct audio_control
    *control);/* define in Audio_parse.cpp */
int set_fm_volume(struct audio_control *ctl, int index);
int set_private_control(struct private_control *pri, const char *priv_name,
    int value);
int set_usecase(struct audio_control *actl, int usecase, bool on);
bool is_usecase(struct audio_control *actl, int usecase);
int stream_routing_manager_create(struct audio_control *actl);
void stream_routing_manager_close(struct audio_control *actl);
int vb_effect_profile_apply(struct audio_control *actl);
int close_all_control(struct audio_control *actl);
int close_in_control(struct audio_control *actl);
int close_in_control(struct audio_control *actl);
int set_vdg_gain(struct device_usecase_gain *dg_gain, int param_id, int volume);
int switch_vbc_iis_route(struct audio_control *ctl,USECASE uc,bool on);
void free_audio_control(struct audio_control *control);
struct audio_control *init_audio_control(struct tiny_audio_device *adev);
int init_audio_param( struct tiny_audio_device *adev);
int set_dsp_volume(struct audio_control *ctl,int volume);
int set_dsp_volume_update(struct audio_control *ctl);
int dev_ctl_iis_set(struct audio_control *ctl, int usecase,int on);
int set_offload_volume( struct audio_control *dev_ctl, float left,
    float right);
int set_voice_ul_mute(struct audio_control *actl, bool mute);
int set_voice_dl_mute(struct audio_control *actl, bool mute);
int set_vbc_bt_src(struct audio_control *actl, int rate);
int set_vbc_bt_nrec(struct audio_control *actl, bool nrec);
int set_codec_mute(struct audio_control *actl,bool on);
int set_mic_mute(struct audio_control *actl, bool on);
int set_mdg_mute(struct audio_control *actl,int usecase,bool on);
int set_codec_volume(struct audio_control *dev_ctl,int param_id,int vol_index);
struct pcm_config * dev_ctl_get_pcm_config(struct audio_control *dev_ctl,int app_type, int * dev);
int set_audioparam(struct audio_control *dev_ctl,int type, void *param_change,int force);
int set_audioparam_unlock(struct audio_control *dev_ctl,int type, void *param_change,int force);
uint8_t get_loopback_param(struct audiotester_config_handle *config,audio_devices_t out_devices,audio_devices_t in_devices);
void *get_ap_record_param(AUDIO_PARAM_T  *audio_param,audio_devices_t in_devices);
int set_vbc_dump_control(struct audio_control *ctl, const char * dump_name, bool on);
bool is_usecase_unlock(struct audio_control *actl, int usecase);
void dump_audio_control(int fd,char * buffer,int buffer_size,struct audio_control  *ctl);
void init_audioparam_filename(AUDIO_PARAM_T *param);
int stream_type_to_usecase(int audio_app_type);
int select_devices_new(struct audio_control *actl, int audio_app_type, audio_devices_t device,
                bool is_in, bool update_param, bool sync, bool force);
int enable_dspsleep_for_fm(struct audio_control *actl, bool on_off);
int disable_codec_dig_access(struct audio_control *actl, bool on_off);
int set_fm_mute(struct audio_control *dev_ctl, bool mute);
int free_audio_param(AUDIO_PARAM_T * param);
int dev_ctl_get_in_pcm_config(struct audio_control *dev_ctl, int app_type, int * dev, struct pcm_config * config);
void set_record_source(struct audio_control *control, audio_source_t source);
int dev_ctl_get_out_pcm_config(struct audio_control *dev_ctl,int app_type, int * dev, struct pcm_config *config);
int switch_vbc_mux_route(struct audio_control *ctl,int device);
uint8_t get_smartamp_playback_param_mode(struct audio_control *dev_ctl);
int switch_vbc_route(struct audio_control *ctl,int device);
bool check_smartamp_audioparam(AUDIO_PARAM_T *param);
int set_available_outputdevices(struct audio_control *dev_ctl,int devices,bool avaiable);
bool is_usbmic_connected(struct audio_control *dev_ctl);
bool is_usbdevice_connected(struct audio_control *dev_ctl);
int is_fm_active(int usecase);
int is_voice_active(int usecase);
int is_playback_active(int usecase);
int is_record_active(int usecase);
bool is_usbmic_offload_supported(struct audio_control *dev_ctl);
int set_fm_speaker(struct audio_control *actl,bool on);
int set_fm_prestop(struct audio_control *actl,bool on);
int set_fm_policy_mute(struct audio_control *dev_ctl,bool mute);
int clear_fm_param_state(struct audio_control *dev_ctl);
void noity_usbmic_connected_for_call(struct audio_control *dev_ctl);
void clear_audio_param_load_status(struct audio_control *dev_ctl);
uint8_t check_shareparam(struct audio_control *dev_ctl,int paramtype,uint8_t param_id);
int do_set_output_device(struct audio_control *actl,audio_devices_t device);
int adev_out_devices_check(void *dev,audio_devices_t device);
bool check_audioparam_exist(void *ctl,int profile,uint8_t param_id);
int audio_dump_reg(struct audio_control *actl);
void ucp1301_type_to_str(char *str,struct ucp_1301_handle_t *ucp_1301);
void clear_smartamp_param_state(struct audio_param_res  *param_res);
#ifdef __cplusplus
}
#endif

#endif //_AUDIO_CONTROL_H_
