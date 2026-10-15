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

#ifndef _AUDIO_PARAM_
#define _AUDIO_PARAM_
#include "stdint.h"
#include "aud_proc.h"
#include "audio_xml_utils.h"
#include <semaphore.h>
#define MAX_LINE_LEN 512
#define SPLIT "\n"
#define BACKSLASH "\\"
#define MAX_SOCKT_LEN 65535
#define MIN_SOCKET_LEN 1024

#define CODEC_INFOR_CTL   "Aud Codec Info"
#define VBC_UL_MUTE    "VBC_UL_MUTE"
#define VBC_DL_MUTE    "VBC_DL_MUTE"
#define VBC_BT_DL_SRC   "VBC_SRC_BT_DAC"
#define VBC_BT_UL_SRC   "VBC_SRC_BT_ADC"

#define VOICE_UL_CAPTURE   "VBC_DSP_VOICE_CAPTURE_TYPE"

#define VOICE_UL_CAPTURE_DOWNLINK   0
#define VOICE_UL_CAPTURE_UPLINK   1
#define VOICE_UL_CAPTURE_UPLINK_DOWNLINK   2

#define VOICE_DL_PLAYBACK   "VBC_DSP_VOICE_PCM_PLAY_MODE"

#define VOICE_DL_PCM_PLAY_UPLINK_MIX   0
#define VOICE_DL_PCM_PLAY_UPLINK_ONLY   1

#define VBC_DSP_PROFILE_UPDATE   "DSP VBC Profile Update"
#define VBC_EQ_PROFILE_UPDATE    "Audio Structure Profile Update"
#define VBC_CVS_PROFILE_UPDATE   "CVS Profile Update"
#define VBC_CODEC_PROFILE_UPDATE   "CODEC Profile Update"
#define VBC_DSP_PROFILE_SELECT   "DSP VBC Profile Select"
#define VBC_EQ_PROFILE_SELECT    "Audio Structure Profile Select"
#define VBC_CVSPROFILE_SELECT   "NXP Profile Select"
#define VBC_CODEC_PROFILE_SELECT   "CODEC Profile Select"

#define DSP_SMARTAMP_PROFILE_SELECT   "DSP FFSMARTAMP Select"
#define DSP_SMARTAMP_PROFILE_UPDATE   "DSP SMARTAMP Update"

#define ENG_AUDIO_FFSMART_OPS ENG_AUDIO_PROCESS_OPS<<1

#define REALTEK_EXTEND_PARAM_UPDATE   "Realtek Extend Codec Param Update"
#define MODE_NAME_MAX_LEN   64
#define MODE_DEPTH_MAX_LEN   8

#define AUDIO_CODEC_GAIN_PARAM "/etc/audio_gain.xml"
#define AUDIO_CODEC_GAIN_TUNNING_PARAM "/data/local/media/audio_gain.xml"
#define AUDIO_CODEC_FIRST_NAME "audio_codec_param"

#define ENG_SET_RAM_OPS 0x01
#define ENG_SET_FLASH_OPS ENG_SET_RAM_OPS<<1
#define ENG_SET_REG_OPS ENG_SET_FLASH_OPS<<1
#define ENG_AUDIO_CODEC_OPS ENG_SET_REG_OPS<<1
#define ENG_AUDIO_VBCEQ_OPS ENG_AUDIO_CODEC_OPS<<1
#define ENG_DSP_VBC_OPS ENG_AUDIO_VBCEQ_OPS<<1
#define ENG_AUDIO_STRECTURE_OPS ENG_DSP_VBC_OPS<<1
#define ENG_CVS_OPS ENG_AUDIO_STRECTURE_OPS<<1
#define ENG_CODEC_OPS ENG_CVS_OPS<<1
#define ENG_PGA_OPS ENG_CODEC_OPS<<1
#define ENG_AUDIO_PROCESS_OPS ENG_PGA_OPS<<1

#ifdef AUDIOHAL_V4
#define AUDIO_PARAM_TUNNING_FILE "/data/vendor/local/media/audio_param_tunning"
#define AUDIO_PARAM_INFOR_TUNNING_PATH "/data/vendor/local/media/audio_params/audio_param_infor"
#else
#define AUDIO_PARAM_TUNNING_FILE "/data/local/media/audio_param_tunning"
#define AUDIO_PARAM_INFOR_TUNNING_PATH "/data/local/media/audio_param_infor"
#endif
#ifdef FLAG_VENDOR_ETC
#define AUDIO_PARAM_INFOR_PATH "/vendor/etc/audio_params/sprd/audio_param_infor"
#else
#define AUDIO_PARAM_INFOR_PATH "/system/etc/audio_param_infor"
#endif

#define AUDIO_PARAM_FIRMWARE_NAME "audio_profile"

#define NUM_MODE     "num_mode"
#define STRUCT_SIZE   "struct_size"
#define SUB_LINE     "line"
#define MODE_NO   "mode"

#define AUDIO_COMMAND_BUF_SIZE 65536
#define AUDIO_TUNNING_PORT 9997 //zzj  for test
#define MAX_NAME_LENTH 20
#define AUDIO_CMD_TYPE  0x99
#define AUDIO_CMD_ERR "ERROR"
#define AUDIO_CMD_OK "OK"

#define PARAM_MAX_DEPTH   16
#define VBC_EQ_PARAM_MAX_SIZE 1024*4

#define TIME_ATTR "time"
#define VALUE "val"
#define VISIBLE "visible"
#define ID "id"
#define TYPE "t"
#define NAME "name"
#define STR_LEN "l"

#define U16 "u16"
#define U32 "u32"
#define U8  "u8"
#define STR "str"

#define ID "id"
#define BITS "bits"
#define OFFSETS "offset"

#define TRUE_STRING "true"

#define AUDIO_PARAM_UPDATE_MAX_SIZE 256
#define AUDIO_PARAM_UPLOAD_CTL "upload"
#define AUDIO_PARAM_SET_CTL "set"

typedef enum {
    SND_AUDIO_PARAM_PROFILE_START = 0,
    SND_AUDIO_PARAM_DSP_VBC_PROFILE_DSP = SND_AUDIO_PARAM_PROFILE_START,
    SND_AUDIO_PARAM_AUDIO_STRUCTURE_PROFILE,
    SND_AUDIO_PARAM_CVS_PROFILE,
    SND_AUDIO_PARAM_PGA_PROFILE,
    SND_AUDIO_PARAM_CODEC_PROFILE,
    SND_AUDIO_PARAM_RECORD_PROCESS_PROFILE,
    SND_AUDIO_PARAM_SMARTAMP_PROFILE,
    SND_AUDIO_PARAM_PROFILE_MAX
} SND_AUDIO_PARAM_PROFILE_USE_E;

enum {
    AUDIO_PARAM_ETC_XML,
    AUDIO_PARAM_ETC_BIN,
    AUDIO_PARAM_DATA_XML,
    AUDIO_PARAM_DATA_BIN,
};

#define AUDIO_PARAM_INVALID_32BIT_OFFSET 0xffffffff
#define AUDIO_PARAM_INVALID_8BIT_OFFSET 0xff

#define AUDIO_PARAM_ALL_LOADED_STATUS   ((1<<SND_AUDIO_PARAM_DSP_VBC_PROFILE_DSP) \
    |(1<<SND_AUDIO_PARAM_AUDIO_STRUCTURE_PROFILE) \
    |(1<<SND_AUDIO_PARAM_CVS_PROFILE) \
    |(1<<SND_AUDIO_PARAM_PGA_PROFILE) \
    |(1<<SND_AUDIO_PARAM_CODEC_PROFILE) \
    |(1<<SND_AUDIO_PARAM_RECORD_PROCESS_PROFILE) \
    |(1<<SND_AUDIO_PARAM_SMARTAMP_PROFILE))

typedef enum {
    PROFILE_AUDIO_MODE_START = 0,

    PROFILE_MODE_AUDIO_Handset_NB1=PROFILE_AUDIO_MODE_START,
    PROFILE_MODE_AUDIO_Handset_NB2,
    PROFILE_MODE_AUDIO_Handset_WB1,
    PROFILE_MODE_AUDIO_Handset_WB2,
    PROFILE_MODE_AUDIO_Handset_SWB1,
    PROFILE_MODE_AUDIO_Handset_FB1,
    PROFILE_MODE_AUDIO_Handset_VOIP1,

    PROFILE_MODE_AUDIO_Handsfree_NB1,
    PROFILE_MODE_AUDIO_Handsfree_NB2,
    PROFILE_MODE_AUDIO_Handsfree_WB1,
    PROFILE_MODE_AUDIO_Handsfree_WB2,
    PROFILE_MODE_AUDIO_Handsfree_SWB1,
    PROFILE_MODE_AUDIO_Handsfree_FB1,
    PROFILE_MODE_AUDIO_Handsfree_VOIP1,

    PROFILE_MODE_AUDIO_Headset4P_NB1,
    PROFILE_MODE_AUDIO_Headset4P_NB2,
    PROFILE_MODE_AUDIO_Headset4P_WB1,
    PROFILE_MODE_AUDIO_Headset4P_WB2,
    PROFILE_MODE_AUDIO_Headset4P_SWB1,
    PROFILE_MODE_AUDIO_Headset4P_FB1,
    PROFILE_MODE_AUDIO_Headset4P_VOIP1,

    PROFILE_MODE_AUDIO_Headset3P_NB1,
    PROFILE_MODE_AUDIO_Headset3P_NB2,
    PROFILE_MODE_AUDIO_Headset3P_WB1,
    PROFILE_MODE_AUDIO_Headset3P_WB2,
    PROFILE_MODE_AUDIO_Headset3P_SWB1,
    PROFILE_MODE_AUDIO_Headset3P_FB1,
    PROFILE_MODE_AUDIO_Headset3P_VOIP1,

    PROFILE_MODE_AUDIO_BTHS_NB1,
    PROFILE_MODE_AUDIO_BTHS_NB2,
    PROFILE_MODE_AUDIO_BTHS_WB1,
    PROFILE_MODE_AUDIO_BTHS_WB2,
    PROFILE_MODE_AUDIO_BTHS_SWB1,
    PROFILE_MODE_AUDIO_BTHS_FB1,
    PROFILE_MODE_AUDIO_BTHS_VOIP1,

    PROFILE_MODE_AUDIO_BTHSNREC_NB1,
    PROFILE_MODE_AUDIO_BTHSNREC_NB2,
    PROFILE_MODE_AUDIO_BTHSNREC_WB1,
    PROFILE_MODE_AUDIO_BTHSNREC_WB2,
    PROFILE_MODE_AUDIO_BTHSNREC_SWB1,
    PROFILE_MODE_AUDIO_BTHSNREC_FB1,
    PROFILE_MODE_AUDIO_BTHSNREC_VOIP1,

    PROFILE_MODE_AUDIO_TYPEC_NB1,
    PROFILE_MODE_AUDIO_TYPEC_NB2,
    PROFILE_MODE_AUDIO_TYPEC_WB1,
    PROFILE_MODE_AUDIO_TYPEC_WB2,
    PROFILE_MODE_AUDIO_TYPEC_SWB1,
    PROFILE_MODE_AUDIO_TYPEC_FB1,
    PROFILE_MODE_AUDIO_TYPEC_VOIP1,

    PROFILE_MODE_AUDIO_HAC_NB1,
    PROFILE_MODE_AUDIO_HAC_NB2,
    PROFILE_MODE_AUDIO_HAC_WB1,
    PROFILE_MODE_AUDIO_HAC_WB2,
    PROFILE_MODE_AUDIO_HAC_SWB1,
    PROFILE_MODE_AUDIO_HAC_FB1,
    PROFILE_MODE_AUDIO_HAC_VOIP1,

    PROFILE_MUSIC_MODE_START,
    PROFILE_MODE_MUSIC_Headset_Playback=PROFILE_MUSIC_MODE_START,
    PROFILE_MODE_MUSIC_Headset_Record,
    PROFILE_MODE_MUSIC_Headset_UnprocessRecord,
    PROFILE_MODE_MUSIC_Headset_Recognition,
    PROFILE_MODE_MUSIC_Headset_FM,

    PROFILE_MODE_MUSIC_Handsfree_Playback,
    PROFILE_MODE_MUSIC_Handsfree_Record,
    PROFILE_MODE_MUSIC_Handsfree_UnprocessRecord,
    PROFILE_MODE_MUSIC_Handsfree_VideoRecord,
    PROFILE_MODE_MUSIC_Handsfree_Recognition,
    PROFILE_MODE_MUSIC_Handsfree_FM,

    PROFILE_MODE_MUSIC_TYPEC_Playback,
    PROFILE_MODE_MUSIC_TYPEC_Record,
    PROFILE_MODE_MUSIC_TYPEC_UnprocessRecord,
    PROFILE_MODE_MUSIC_TYPEC_Recognition,
    PROFILE_MODE_MUSIC_TYPEC_FM,

    PROFILE_MODE_MUSIC_Headfree_Playback,
    PROFILE_MODE_MUSIC_Handset_Playback,
    PROFILE_MODE_MUSIC_Bluetooth_Record,

    PROFILE_LOOP_MODE_START,
    PROFILE_MODE_LOOP_Handset_Loop1=PROFILE_LOOP_MODE_START,
    PROFILE_MODE_LOOP_Handsfree_Loop1,
    PROFILE_MODE_LOOP_Headset4P_Loop1,
    PROFILE_MODE_LOOP_Headset3P_Loop1,

    PROFILE_MODE_MAX
}AUDIO_PARAM_PROFILE_MODE_E;

#define DEFAULT_MIN_BUFFER_SIZE    4096

#define FIRMWARE_MAGIC_MAX_LEN       (16)
#define DSP_VBC_FIRMWARE_MAGIC_ID        ("DSP_VBC")
#define DSP_VBC_PROFILE_VERSION          (0x00000002)
#define DSP_VBC_PROFILE_CNT_MAX          (50)
#define DSP_VBC_PROFILE_NAME_MAX         (32)

#define VBC_DA_EFFECT_PARAS_LEN         (20+72*2)
#define VBC_AD_EFFECT_PARAS_LEN         (2+ 43*2)
#define VBC_EFFECT_PROFILE_CNT          (4)

#define MAX_VOICE_VOLUME 7

#define CODEC_MAX_VOLUME (MAX_VOICE_VOLUME+1)
struct dsp_loop_param {
    int16_t type;
    int16_t mode;
    int16_t delay;//ms
};

#ifdef __cplusplus
extern "C" {
#endif

struct audio_param_file_t{
    int profile;
    const char *data_xml;
    const char *data_bin;
    const char *etc_xml;
    const char *etc_bin;
};

struct audio_record_proc_param {
    RECORDEQ_CONTROL_PARAM_T record_eq;
    DP_CONTROL_PARAM_T dp_control;
    NR_CONTROL_PARAM_T nr_control;
};

struct audio_ap_voice_param{
    int16_t mic_swicth;
};

#define VOICE_AP_PARAM_COUNT (PROFILE_MODE_AUDIO_HAC_VOIP1-PROFILE_AUDIO_MODE_START+1)


/* record param
PROFILE_MODE_MUSIC_Headset_Record,
PROFILE_MODE_MUSIC_Headset_UnprocessRecord,
PROFILE_MODE_MUSIC_Headset_Recognition,

PROFILE_MODE_MUSIC_Handsfree_Record,
PROFILE_MODE_MUSIC_Handsfree_UnprocessRecord,
PROFILE_MODE_MUSIC_Handsfree_VideoRecord,
PROFILE_MODE_MUSIC_Handsfree_Recognition,

PROFILE_MODE_MUSIC_TYPEC_Record,
PROFILE_MODE_MUSIC_TYPEC_UnprocessRecord,
PROFILE_MODE_MUSIC_TYPEC_Recognition,

PROFILE_MODE_MUSIC_Bluetooth_Record,
*/
#define RECORD_AP_PARAM_COUNT 11  //add VideoRecord

#define LOOP_AP_PARAM_COUNT (PROFILE_MODE_LOOP_Headset3P_Loop1-PROFILE_LOOP_MODE_START+1)

struct audio_ap_param{
    struct audio_ap_voice_param voice[VOICE_AP_PARAM_COUNT];
    int voice_parmid[VOICE_AP_PARAM_COUNT];

    struct audio_record_proc_param record[RECORD_AP_PARAM_COUNT];
    int record_parmid[RECORD_AP_PARAM_COUNT];

    struct dsp_loop_param loop[LOOP_AP_PARAM_COUNT];
    int loop_parmid[LOOP_AP_PARAM_COUNT];
};
struct xml_handle {
    param_doc_t param_doc;
    param_group_t param_root;
    char *first_name;
};

#define SET_AUDIO_PARAM_THREAD_NO 10
#define AUDIO_PARAM_THREAD_PROCESS_BUF 128

typedef int  (*AUDIO_SOCKET_PROCESS_FUN)(void *dev, uint8_t *received_buf,
                         int rev_len);

struct socket_handle {

    uint8_t * audio_received_buf;
    uint8_t * audio_cmd_buf;
    uint8_t * time_buf;
    int sockfd;
    int seq;
    int rx_packet_len;
    int rx_packet_total_len;
    bool wire_connected;
    int diag_seq;

    uint8_t * data_buf;
    uint8_t * send_buf;
    int max_len;
    int cur_len;
    int data_state;

    void *res;
    AUDIO_SOCKET_PROCESS_FUN process;
    bool running;
    bool param_sync[SND_AUDIO_PARAM_PROFILE_MAX];
    struct xml_handle audio_config_xml;
    int update_flag;

    pthread_mutex_t diag_lock;
    uint8_t * diag_send_buffer;
    unsigned int send_buffer_size;
    bool readthreadwait;
    sem_t sem_wakeup_readthread;
    bool diag_cmd_process;

    FILE *diag_tx_file;
    FILE *diag_rx_file;
    FILE *tx_file;

};


struct vbc_fw_header {
    char magic[FIRMWARE_MAGIC_MAX_LEN];
    /*total num of profile */
    int32_t num_mode;
    /* total mode num in each profile */
    int32_t len; /* size of each mode in profile */
};

struct param_infor_t {
    int32_t offset[PROFILE_MODE_MAX];
    int32_t param_struct_size;
};

struct param_infor {
    struct param_infor_t data[SND_AUDIO_PARAM_PROFILE_MAX];
    unsigned int param_sn;
    struct audiotester_config_handle *config;
};

typedef struct {
    int version;
    int32_t param_struct_size;
    char *data;
    int num_mode;
    struct xml_handle xml;
} AUDIOVBCEQ_PARAM_T;


typedef struct {
    int opt;
    void *res;
} audio_param_dsp_cmd_t;

typedef struct {
    int mode; //0:init first 1:normal 2:tunning
    struct vbc_fw_header header[SND_AUDIO_PARAM_PROFILE_MAX];
    int fd_bin[SND_AUDIO_PARAM_PROFILE_MAX];
    AUDIOVBCEQ_PARAM_T param[SND_AUDIO_PARAM_PROFILE_MAX];
    struct audio_param_file_t audio_param_file_table[SND_AUDIO_PARAM_PROFILE_MAX];
    char *etc_audioparam_infor;
    struct socket_handle tunning;
    int current_param;
    int input_source;
    pthread_mutex_t audio_param_lock;
    struct dsp_control_t *agdsp_ctl;//used send cmd to dsp thread
    struct audio_control *dev_ctl;//used control devices in audiotester

    struct mixer_ctl *update_mixer[SND_AUDIO_PARAM_PROFILE_MAX];
    struct mixer_ctl *select_mixer[SND_AUDIO_PARAM_PROFILE_MAX];
    struct param_infor *infor;
    unsigned int backup_param_sn;
    unsigned int tunning_param_sn;
    bool audio_param_update;
    int load_status;

    pthread_t thread[SND_AUDIO_PARAM_PROFILE_MAX];
    sem_t   sem;
} AUDIO_PARAM_T;

struct param_head_t {
    int param_type;
    int message;
    int param_num;
    int param_size;
};

struct param_thread_res {
    void *audio_param;
    int profile;
    bool is_tunning;
};

struct sprd_code_param_t {
    int mic_boost;

    int adcl_capture_volume;
    int adcr_capture_volume;

    int spkl_playback_volume[CODEC_MAX_VOLUME];
    int spkr_playback_volume[CODEC_MAX_VOLUME];

    int hpl_playback_volume[CODEC_MAX_VOLUME];
    int hpr_playback_volume[CODEC_MAX_VOLUME];

    int ear_playback_volume[CODEC_MAX_VOLUME];

    int inter_pa_config;
    int inter_hp_pa_config;

    int dac_playback_volume[CODEC_MAX_VOLUME];
    int dacl_playback_volume[CODEC_MAX_VOLUME];
    int dacr_playback_volume[CODEC_MAX_VOLUME];
};

struct ucp_1301_param_t {
    int pa_config;
    int rl;
    int power_limit_p2;
    int reserve[2];
};

struct audio_param_mode_t {
    AUDIO_PARAM_PROFILE_MODE_E mode;
    const char *name;
};
extern const char * data_parm_path;
int load_audio_param_infor(struct param_infor *infor, char *etc_file);
int load_audio_param(AUDIO_PARAM_T  *audio_param,bool is_tunning,int profile);
int load_xml_handle(struct xml_handle *xmlhandle, const char *xmlpath);
void release_xml_handle(struct xml_handle *xmlhandle);
void dump_data(char *buf, int len);
int save_audio_param_to_bin(AUDIO_PARAM_T *param, int profile);
int get_ele_value(param_group_t Element);
int read_audio_param_for_element(char *data,param_group_t Element);
int upload_audio_param_firmware(AUDIO_PARAM_T * audio_param);
int upload_audio_profile_param_firmware(AUDIO_PARAM_T * audio_param,int profile);
int get_audio_param_mode_name(AUDIO_PARAM_T *audio_param,char *str);
int clear_audio_param(AUDIO_PARAM_T  *audio_param);
int reload_sprd_audio_pga_param_withflash(AUDIO_PARAM_T *param);
int reload_sprd_audio_pga_param_withram(AUDIO_PARAM_T *param);
int init_sprd_audio_param(AUDIO_PARAM_T  *audio_param, bool force);
const char * tinymix_get_enum(struct mixer_ctl *ctl);
const char * get_audio_param_name(uint8_t param_id);
int reload_sprd_audio_process_param_withflash(AUDIO_PARAM_T *audio_param);
int init_sprd_audio_param_from_xml(AUDIO_PARAM_T *param,int profile,bool is_tunning);
int save_audio_param_infor(struct param_infor *infor);
void dump_audio_param(int fd,char * buffer,int buffer_size,AUDIO_PARAM_T *audio_param);
int get_audio_param_id_frome_name(const char *name);
void init_audioparam_filename(AUDIO_PARAM_T *param);
const char * get_audioparam_filename(AUDIO_PARAM_T *param,int profile, int type);
uint8_t get_audio_param_mode(AUDIO_PARAM_T  *audio_param,int param_type,uint8_t param_id);
bool is_audio_param_ready(AUDIO_PARAM_T *audio_param,int profile);
bool is_all_audio_param_ready(AUDIO_PARAM_T *audio_param);
void audio_param_clear_load_status(AUDIO_PARAM_T  *audio_param);
#ifdef __cplusplus
}
#endif
#endif //_AUDIO_PARAM_T
