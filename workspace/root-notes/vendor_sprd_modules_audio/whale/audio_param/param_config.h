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

#include <stdbool.h>
#include <aud_common.h>

#ifndef _AUDIO_PARAM_CONFIG_H_
#define _AUDIO_PARAM_CONFIG_H_
#include <stdbool.h>
#include <aud_common.h>

#ifdef __cplusplus
extern "C" {
#endif

enum {
    AUD_FM_DIGITAL_TYPE = 0,
    AUD_FM_ANALOG_TYPE,
    AUD_FM_ANALOGVBC_TYPE,
    AUD_TYPE_MAX,
};

static const char  *fm_type_name[AUD_TYPE_MAX] = {
    "Digital",
    "Analog",
    "AnalogVbc",
};

typedef enum {
    AUDIOTESTER_USECASE_GSM=0,
    AUDIOTESTER_USECASE_TDMA,
    AUDIOTESTER_USECASE_WCDMA_NB,
    AUDIOTESTER_USECASE_VOLTE_NB,
    AUDIOTESTER_USECASE_VOWIFI_NB,

    AUDIOTESTER_USECASE_CDMA2000,

    AUDIOTESTER_USECASE_WCDMA_WB,
    AUDIOTESTER_USECASE_VOLTE_WB,
    AUDIOTESTER_USECASE_VOWIFI_WB,

    AUDIOTESTER_USECASE_VOLTE_SWB,
    AUDIOTESTER_USECASE_VOLTE_FB,
    AUDIOTESTER_USECASE_VOIP,

    AUDIOTESTER_USECASE_Playback,
    AUDIOTESTER_USECASE_Record,
    AUDIOTESTER_USECASE_UnprocessRecord,
    AUDIOTESTER_USECASE_VideoRecord,
    AUDIOTESTER_USECASE_Fm,
    AUDIOTESTER_USECASE_Loop,
    AUDIOTESTER_USECASE_RecognitionRecord,
    AUDIOTESTER_USECASE_Max,
}audiotester_usecase;

struct audiotester_param_config_handle {
    int usecase;
    int indevice;
    int outdevice;
    int paramid;
    int hwOutDevice;
    const char *modename;
    const char *usecasename;
};

struct audiotester_shareparam_config_handle {
    uint8_t paramid;
    uint8_t shareparamid;
    int type;
};

struct audiotester_config_handle {
    const char *ChipName;
    int FmType;
    int SocketBufferSize;
    int DiagBufferSize;
    int SmartAmpSupportMode;
    int SmartAmpUsecase;
    struct audiotester_param_config_handle * param_config;
    int param_config_num;
    struct audiotester_shareparam_config_handle * shareparam_config;
    int shareparam_config_num;
    bool audiotester_inited;
};

#ifdef FLAG_VENDOR_ETC
#define AUDIO_PARAM_CONFIG_PATH "/vendor/etc/audio_params/sprd/audioparam_config.xml"
#else
#define AUDIO_PARAM_CONFIG_PATH "/system/etc/audio_params/sprd/audioparam_config.xml"
#endif
#define AUDIO_PARAM_DATA_CONFIG_PATH "/data/vendor/local/media/audio_params/audioparam_config.xml"
int parse_audioparam_config(void* config,bool is_tunning);
void free_audioparam_config(struct audiotester_config_handle *config);
int get_hwdevice_mode(struct audiotester_config_handle *config,int param_id);
int save_1301_audioparam_config(int switch_1301);
int get_smartamp_config_mode(struct audiotester_config_handle *config);
int get_smartamp_config_usecase(struct audiotester_config_handle *config);
#ifdef __cplusplus
}
#endif
#endif
