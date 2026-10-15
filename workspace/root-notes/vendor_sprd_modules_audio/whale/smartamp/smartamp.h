#ifndef __SMART_AMP_H__
#define __SMART_AMP_H__
#include <stdbool.h>
#include <semaphore.h>
#include <tinyalsa/asoundlib.h>
#include "audio_xml_utils.h"
#include "audio_record_nr.h"// include typedef short int16;

#define AMP_SAMPLERATE      (0x4-4)    //samplerate
#define V_FULLSCALE         (0x10-4)    //V_Fs
#define I_FULLSCALE         (0x12-4)    //I_Fs
#define RE_T_CALI           (0x34)    //Re_T_cali
#define PILOT_TONE_AMP_CALI (0x80-4)    //pilot_tone_amp
#define POSSTFILTER_FLAG    (0x538)    //M_PostFilterOn
#define AUDIO_SMARTAMP_CALI_PARAM_FILE "/mnt/vendor/audio/AudioSmartApm"

#define DEFAULT_SMARTAMP_CALI_Q10_MAX_OFFSET  256
#define DEFAULT_SMARTAMP_CALI_Q10_NORMAL_OFFSET  184
#define SMARTAMP_Q10_MAX_OFFSET_STR   "Q10_Max"
#define SMARTAMP_Q10_NORMAL_OFFSET_STR   "Q10_Normal"

typedef enum {
    SND_AUDIO_SMARTAMP_SPEAKER_MODE = 1<<0,
    SND_AUDIO_SMARTAMP_FB_MODE = 1<<1,
    SND_AUDIO_SMARTAMP_RECORD_MODE =1<<2,
} SND_AUDIO_SMARTAMP_MODE_E;

typedef enum {
    SND_AUDIO_UNSUPPORT_SMARTAMP_MODE = 0,
    SND_AUDIO_FF_SMARTAMP_MODE,
    SND_AUDIO_FB_SMARTAMP_MODE,
}SND_AUDIO_SMARTAMP_SUPPORT_MODE;

struct smart_amp_cali_param {
    int16 Re_T_cali_out;
    int16 postfilter_flag;
};

struct smart_amp_ctl {
    uint32_t R0_dc;
    int16 Re_T_cali;
    int16 R0_max_offset;
    int16 R0_normal_offset;
    int smartamp_usecase;
    int smartamp_support_mode;
    bool smart_cali_failed;
    bool iv_enable;
    bool calibrating;
    bool smartamp_func_enable;
    pthread_mutex_t lock;
    pthread_cond_t cond;
};

#ifdef __cplusplus
extern "C" {
#endif
int read_smartamp_cali_param(struct smart_amp_cali_param *cali_values);
int get_smartapm_cali_values(struct smart_amp_ctl *smartamp_ctl,struct smart_amp_cali_param *cali_values);
int set_smartamp_cali_param(void *data,int size);
int read_smart_firmware(void **param);
void set_smartamp_support_usecase(struct smart_amp_ctl *smartamp_ctl,int usecase,int mode);
void set_smartamp_cali_offset(struct smart_amp_ctl *smartamp_ctl,int q10_max, int q10_normal);
bool is_support_smartamp(struct smart_amp_ctl *smartamp_ctl);
void dump_smartamp(int fd,char* buffer,int buffer_size,void *ctl);
int get_smartamp_support_usecase(struct smart_amp_ctl *smartamp_ctl);
void free_smart_firmware_buffer(void *param);
int write_smartamp_cali_param(struct smart_amp_cali_param *cali_out);
int init_smartamp(struct smart_amp_ctl *smartamp_ctl);
void set_smartamp_calibrating(struct smart_amp_ctl *smartamp_ctl,bool on);
bool is_support_smartamp_calibrate(struct smart_amp_ctl *smartamp_ctl);
bool smartamp_cali_process(struct smart_amp_ctl *smartamp_ctl,struct smart_amp_cali_param *cali_out);
void set_smartamp_cali_values(struct smart_amp_ctl *smartamp_ctl,uint32_t value1,uint32_t value2);
bool get_smartamp_cali_iv(void *dsp_ctl,struct smart_amp_ctl *smartamp_ctl,int timeout_ms);
bool is_support_fbsmartamp(struct smart_amp_ctl *smartamp_ctl);
void enable_smartamp_func(void *dsp_ctl,bool on);
void enable_smartamp_pt(void *dsp_ctl,bool on);
bool is_smartamp_func_enable(struct smart_amp_ctl *smartamp_ctl);
#define NBLOCK_IV                   240
#define SMARTAMP_PREREAD_SIZE      NBLOCK_IV*2*2
#define SMARTAMP_CALI_TRY_COUNT    100;
#ifdef __cplusplus
}
#endif
#endif
