#define LOG_TAG  "audio_diag"
#include "sprd_fts_type.h"
#include "sprd_fts_log.h"
#include "sprd_fts_cb.h"

#include <stdlib.h>
#include <malloc.h>
#include <string.h>
#include"unistd.h"
#include"sys/types.h"
#include"fcntl.h"
#include"stdio.h"
#include <errno.h>
#include <system/audio.h>
#include "audio_diag.h"
#include "audio_npi.h"
#ifdef AUDIO_WHALE
#include "audiotester.h"

typedef enum {
    GET_INFO,
    GET_PARAM_FROM_RAM,
    SET_PARAM_FROM_RAM,
    GET_PARAM_FROM_FLASH,
    SET_PARAM_FROM_FLASH,
    GET_REGISTER,
    SET_REGISTER,
    SET_PARAM_VOLUME=14,
    SET_VOLUME_APP_TYPE=15,
    IMPORT_XML_AUDIO_PARAM=16,
    GET_CURRENT_AUDIO_PARAM=17,
    AUDIO_PCM_DUMP=18,
    UPATE_AUDIOPARAM_TIME=19,
    CLONE_XML_AUDIO_PARAM,
    CONNECT_AUDIOTESTER,
    DIS_CONNECT_AUDIOTESTER,
    HARDWARE_TEST_CMD=0xf0,
    AUDIO_EXT_TEST_CMD=0xf1,
    SOCKET_TEST_CMD=0xff,
} AUDIO_CMD;

#endif
#ifdef AUDIOHAL_V4
#include <log/log.h>
#else
#include <utils/Log.h>
#endif

struct eng_audio_ctl engaudio;

long getCurrentTimeMs()
{
   struct timeval tv;
   gettimeofday(&tv,NULL);
   return tv.tv_sec* 1000+ tv.tv_usec/1000;
}

int audio_diag_write(struct eng_audio_ctl *ctl,char *rsp, int len){
    if(ctl->eng_diagcb!=NULL){
        return ctl->eng_diagcb(rsp,len);
    }
    return -1;
}

int register_diagcbfun(DYMIC_WRITETOPC_FUNC * write_interface_ptr)
{
    engaudio.eng_diagcb=write_interface_ptr[WRITE_TO_HOST_DIAG];
    return 0;
}

void register_this_module_ext(struct eng_callback *reg, int *num)
{
    int moudles_num = 0;
    engaudio.record_pcm_pipe = -1;

    ALOGI("register_this_module_ext");

    (reg + moudles_num)->type = 0x38;
    (reg + moudles_num)->subtype = DIAG_CMD_AUDIO_IN;
    (reg + moudles_num)->eng_diag_func = testBbatAudioIn;
    ALOGV("register_this_module_ext Diag cmd testBbatAudioIn:%p",
          testBbatAudioIn);
    moudles_num++;

    (reg + moudles_num)->type = 0x38;
    (reg + moudles_num)->subtype = DIAG_CMD_AUDIO_OUT;
    (reg + moudles_num)->eng_diag_func = testBbatAudioOut;
    ALOGV("register_this_module_ext Diag cmd testBbatAudioOut:%p",
          testBbatAudioOut);
    moudles_num++;

    (reg + moudles_num)->type = 0x38;
    (reg + moudles_num)->subtype = DIAG_CMD_AUDIO_HEADSET_CHECK;
    (reg + moudles_num)->eng_diag_func = testheadsetplunge;
    (reg + moudles_num)->diag_ap_cmd = 6;// for headset plunge in test;
    ALOGV("register_this_module_ext Diag cmd testheadsetplunge:%p",
          testheadsetplunge);
    moudles_num++;
#ifdef AUDIO_WHALE
    (reg + moudles_num)->type = 0x99;
    (reg + moudles_num)->subtype = GET_INFO;
    (reg + moudles_num)->eng_diag_func = audiotester_fun;
    (reg + moudles_num)->eng_set_writeinterface_func = register_diagcbfun;
    (reg + moudles_num)->diag_ap_cmd = 6;// for headset plunge in test;
    moudles_num++;

    (reg + moudles_num)->type = 0x99;
    (reg + moudles_num)->subtype = GET_PARAM_FROM_RAM;
    (reg + moudles_num)->eng_diag_func = audiotester_fun;
    (reg + moudles_num)->eng_set_writeinterface_func = register_diagcbfun;
    (reg + moudles_num)->diag_ap_cmd = 6;// for headset plunge in test;
    moudles_num++;

    (reg + moudles_num)->type = 0x99;
    (reg + moudles_num)->subtype = SET_PARAM_FROM_RAM;
    (reg + moudles_num)->eng_diag_func = audiotester_fun;
    (reg + moudles_num)->eng_set_writeinterface_func = register_diagcbfun;
    (reg + moudles_num)->diag_ap_cmd = 6;// for headset plunge in test;
    moudles_num++;

    (reg + moudles_num)->type = 0x99;
    (reg + moudles_num)->subtype = GET_PARAM_FROM_FLASH;
    (reg + moudles_num)->eng_diag_func = audiotester_fun;
    (reg + moudles_num)->eng_set_writeinterface_func = register_diagcbfun;
    (reg + moudles_num)->diag_ap_cmd = 6;// for headset plunge in test;
    moudles_num++;

    (reg + moudles_num)->type = 0x99;
    (reg + moudles_num)->subtype = SET_PARAM_FROM_FLASH;
    (reg + moudles_num)->eng_diag_func = audiotester_fun;
    (reg + moudles_num)->eng_set_writeinterface_func = register_diagcbfun;
    (reg + moudles_num)->diag_ap_cmd = 6;// for headset plunge in test;
    moudles_num++;

    (reg + moudles_num)->type = 0x99;
    (reg + moudles_num)->subtype = GET_REGISTER;
    (reg + moudles_num)->eng_diag_func = audiotester_fun;
    (reg + moudles_num)->eng_set_writeinterface_func = register_diagcbfun;
    (reg + moudles_num)->diag_ap_cmd = 6;// for headset plunge in test;
    moudles_num++;

    (reg + moudles_num)->type = 0x99;
    (reg + moudles_num)->subtype = SET_REGISTER;
    (reg + moudles_num)->eng_diag_func = audiotester_fun;
    (reg + moudles_num)->eng_set_writeinterface_func = register_diagcbfun;
    (reg + moudles_num)->diag_ap_cmd = 6;// for headset plunge in test;
    moudles_num++;

    (reg + moudles_num)->type = 0x99;
    (reg + moudles_num)->subtype = SET_PARAM_VOLUME;
    (reg + moudles_num)->eng_diag_func = audiotester_fun;
    (reg + moudles_num)->eng_set_writeinterface_func = register_diagcbfun;
    (reg + moudles_num)->diag_ap_cmd = 6;// for headset plunge in test;
    moudles_num++;

    (reg + moudles_num)->type = 0x99;
    (reg + moudles_num)->subtype = SET_VOLUME_APP_TYPE;
    (reg + moudles_num)->eng_diag_func = audiotester_fun;
    (reg + moudles_num)->eng_set_writeinterface_func = register_diagcbfun;
    (reg + moudles_num)->diag_ap_cmd = 6;// for headset plunge in test;
    moudles_num++;

    (reg + moudles_num)->type = 0x99;
    (reg + moudles_num)->subtype = IMPORT_XML_AUDIO_PARAM;
    (reg + moudles_num)->eng_diag_func = audiotester_fun;
    (reg + moudles_num)->eng_set_writeinterface_func = register_diagcbfun;
    (reg + moudles_num)->diag_ap_cmd = 6;// for headset plunge in test;
    moudles_num++;

    (reg + moudles_num)->type = 0x99;
    (reg + moudles_num)->subtype = GET_CURRENT_AUDIO_PARAM;
    (reg + moudles_num)->eng_diag_func = audiotester_fun;
    (reg + moudles_num)->eng_set_writeinterface_func = register_diagcbfun;
    (reg + moudles_num)->diag_ap_cmd = 6;// for headset plunge in test;
    moudles_num++;
           
    (reg + moudles_num)->type = 0x99;
    (reg + moudles_num)->subtype = AUDIO_PCM_DUMP;
    (reg + moudles_num)->eng_diag_func = audiotester_fun;
    (reg + moudles_num)->eng_set_writeinterface_func = register_diagcbfun;
    (reg + moudles_num)->diag_ap_cmd = 6;// for headset plunge in test;
    moudles_num++;

    (reg + moudles_num)->type = 0x99;
    (reg + moudles_num)->subtype = UPATE_AUDIOPARAM_TIME;
    (reg + moudles_num)->eng_diag_func = audiotester_fun;
    (reg + moudles_num)->eng_set_writeinterface_func = register_diagcbfun;
    (reg + moudles_num)->diag_ap_cmd = 6;// for headset plunge in test;
    moudles_num++;

    (reg + moudles_num)->type = 0x99;
    (reg + moudles_num)->subtype = CLONE_XML_AUDIO_PARAM;
    (reg + moudles_num)->eng_diag_func = audiotester_fun;
    (reg + moudles_num)->eng_set_writeinterface_func = register_diagcbfun;
    (reg + moudles_num)->diag_ap_cmd = 6;// for headset plunge in test;
    moudles_num++;

    (reg + moudles_num)->type = 0x99;
    (reg + moudles_num)->subtype = CONNECT_AUDIOTESTER;
    (reg + moudles_num)->eng_diag_func = audiotester_fun;
    (reg + moudles_num)->eng_set_writeinterface_func = register_diagcbfun;
    (reg + moudles_num)->diag_ap_cmd = 6;// for headset plunge in test;
    moudles_num++;

    (reg + moudles_num)->type = 0x99;
    (reg + moudles_num)->subtype = DIS_CONNECT_AUDIOTESTER;
    (reg + moudles_num)->eng_diag_func = audiotester_fun;
    (reg + moudles_num)->eng_set_writeinterface_func = register_diagcbfun;
    (reg + moudles_num)->diag_ap_cmd = 6;// for headset plunge in test;
    moudles_num++;

    (reg + moudles_num)->type = 0x99;
    (reg + moudles_num)->subtype = HARDWARE_TEST_CMD;
    (reg + moudles_num)->eng_diag_func = audiotester_fun;
    (reg + moudles_num)->eng_set_writeinterface_func = register_diagcbfun;
    (reg + moudles_num)->diag_ap_cmd = 6;// for headset plunge in test;
    moudles_num++;


#endif
    sprintf((reg + moudles_num)->at_cmd, "%s", at_play);
    (reg + moudles_num)->eng_linuxcmd_func = AUDIO_PLAY_AT;
    (reg + moudles_num)->subtype = 0x68;
    ALOGV("register_this_module_ext AUDIO_PLAY_AT:%p",
          AUDIO_PLAY_AT);
    moudles_num++;

    sprintf((reg + moudles_num)->at_cmd, "%s", at_headsettype);
    (reg + moudles_num)->eng_linuxcmd_func = AUDIO_HEADSET_TEST_AT;
    (reg + moudles_num)->subtype = 0x68;
    moudles_num++;
    ALOGV("register_this_module_ext AUDIO_HEADSET_TEST_AT:%p",
          AUDIO_HEADSET_TEST_AT);

    sprintf((reg + moudles_num)->at_cmd, "%s", at_headsetcheck);
    (reg + moudles_num)->eng_linuxcmd_func = AUDIO_HEADSET_CHECK_AT;
    (reg + moudles_num)->subtype = 0x68;
    moudles_num++;
    ALOGV("register_this_module_ext AUDIO_HEADSET_CHECK_AT:%p",
          AUDIO_HEADSET_TEST_AT);

    sprintf((reg + moudles_num)->at_cmd, "%s", at_audiofm);
    (reg + moudles_num)->eng_linuxcmd_func = AUDIO_FM_AT;
    (reg + moudles_num)->subtype = 0x68;
    moudles_num++;
    ALOGV("register_this_module_ext AUDIO_FM_AT:%p",
          AUDIO_FM_AT);

    sprintf((reg + moudles_num)->at_cmd, "%s", at_audiocploop);
    (reg + moudles_num)->eng_linuxcmd_func = AUDIO_CPLOOP_AT;
    (reg + moudles_num)->subtype = 0x68;
    moudles_num++;
    ALOGV("register_this_module_ext AUDIO_CPLOOP_AT:%p",
          AUDIO_CPLOOP_AT);

    sprintf((reg + moudles_num)->at_cmd, "%s", at_audiopipe);
    (reg + moudles_num)->eng_linuxcmd_func = AUDIO_PIPE_AT;
    (reg + moudles_num)->subtype = 0x68;
    moudles_num++;
    ALOGV("register_this_module_ext AUDIO_PIPE_AT:%p",
          AUDIO_PIPE_AT);

#if defined(SPRD_AUDIO_HIDL_CLIENT) && defined(AUDIO_WHALE)
    sprintf((reg + moudles_num)->at_cmd, "%s", at_calibration);
    (reg + moudles_num)->eng_linuxcmd_func = AUDIO_CALIBRATION_AT;
    (reg + moudles_num)->subtype = 0x68;
    moudles_num++;
    ALOGV("register_this_module_ext AUDIO_CALIBRATION_AT:%p",
          AUDIO_CALIBRATION_AT);
#endif

    *num = moudles_num;
    ALOGI("register_this_module_ext:%d", *num);
#ifdef AUDIO_WHALE
    audiotester_init();
#endif
}

void register_fw_function(struct fw_callback *reg)
{
    engaudio.eng_cb.ptfQueryInterface = reg->ptfQueryInterface;

    if (engaudio.eng_cb.ptfQueryInterface("send at command", &engaudio.atc_fun) != 0)
    {
        ALOGE("query interface(send at command) fail!");
    }
}
