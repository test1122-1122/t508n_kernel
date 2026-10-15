#define LOG_TAG  "audio_hw_npi"
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
#ifdef AUDIOHAL_V4
#include <log/log.h>
#else
#include <utils/Log.h>
#endif

extern int at_tok_nextint(char **p_cur, int *p_out) ;
extern int at_tok_equel_start(char **p_cur) ;
extern int SendAudioTestCmd(const char *cmd, int bytes) ;
extern unsigned char *hex_to_string(unsigned char *data, int size);
extern void dump_data(char *tag, char *buf, int len);
extern int at_tok_nextstr(char **p_cur, char **p_out);
static int audio_cploop (struct eng_audio_ctl *ctl, char *buf, char *rsp);
extern struct eng_audio_ctl engaudio;

static int record_read(uchar *data, int size)
{
    int ret = -1;
    int to_read = size;
    int num_read = 0;
    uchar *buf = (uchar *)data;
    int count = 20;
    memset(data, 0, size);

    if( engaudio.record_pcm_pipe < 0 )
    {
        ALOGE("%s open:%s failed error:%s",  __func__, AUDIO_TEST_FILE, strerror(errno));
        ret = -1;
        return ret;
    }

    while(to_read)
    {
        num_read = read(engaudio.record_pcm_pipe, (unsigned char *)buf, to_read);
        if(num_read <= 0)
        {
            ALOGE("%s read:%s failed error:%s",  __func__, AUDIO_TEST_FILE, strerror(errno));
            usleep(10000);
            count--;
            if(count <= 0)
            {
                break;
            }
            else
            {
                continue;
            }
        }

        if(num_read < to_read)
        {
            usleep(10000);
        }
        to_read -= num_read;
        buf += num_read;
    }
    return size - to_read;
}

int testBbatAudioIn(char *buf, int buf_len, char *rsp, int rsplen)
{
    MSG_HEAD_T *msg_head_ptr = NULL;

    char cmd_buf[256] = {0};
    int input_devices = 0;
    int output_devices = 0;
    char *data = (char *)(buf + 1 + sizeof(MSG_HEAD_T));
    int data_len = buf_len - 2 - sizeof(MSG_HEAD_T);
    int rsp_buffer_size = 0;
    char *rsp_buffer = NULL;
    int ret = NPI_CMD_SUCCESS;
    uchar *rsp_data = NULL;
    dump_data("testBbatAudioIn Input", buf, buf_len);
    ALOGI("testBbatAudioIn  data_len:%d", data_len);

    memcpy(rsp, buf, 1 + sizeof(MSG_HEAD_T));
    msg_head_ptr = (MSG_HEAD_T *)(rsp + 1);
    msg_head_ptr->len = sizeof(MSG_HEAD_T);
    rsp_buffer = (char *)(rsp + 1 + sizeof(MSG_HEAD_T));

    if(3 == data_len)
    {
        uchar loop_act = data[0];
        uchar loop_mic = data[1];
        uchar loop_out = data[2];

        switch(loop_mic)
        {
        case AUD_INDEV_BUILTIN_MIC:
            input_devices = AUDIO_DEVICE_IN_BUILTIN_MIC;
            break;
        case AUD_INDEV_HEADSET_MIC:
            input_devices = AUDIO_DEVICE_IN_WIRED_HEADSET;
            break;
        case AUD_INDEV_BACK_MIC:
            input_devices = AUDIO_DEVICE_IN_BACK_MIC;
            break;
        default:
            ALOGE("%s line:%d error", __func__, __LINE__);
            break;
        }

        switch(loop_out)
        {
        case AUD_OUTDEV_SPEAKER:
            output_devices = AUDIO_DEVICE_OUT_SPEAKER;
            break;
        case AUD_OUTDEV_EARPIECE:
            output_devices = AUDIO_DEVICE_OUT_EARPIECE;
            break;
        case AUD_OUTDEV_HEADSET:
            output_devices = AUDIO_DEVICE_OUT_WIRED_HEADPHONE;
            break;
        default:
            ALOGE("%s line:%d error", __func__, __LINE__);
            break;
        }

        ALOGI("%s:len:%d loop_act: %02x %02x %02x \n", __func__,
              data_len, loop_act, loop_mic, loop_out);
        if(loop_act == 0x20)   //loop test
        {
            snprintf(cmd_buf,
                     sizeof(cmd_buf) - 1, "audio_loop_test=1;test_in_stream_route=0x%x;test_stream_route=%d;samplerate=%d;channels=%d",
                     input_devices, output_devices, 48000, 2);

#ifdef SPRD_AUDIO_HIDL_CLIENT
            audioclient_setParameters(cmd_buf);
#else
            SendAudioTestCmd((const char *)cmd_buf, strlen(cmd_buf));
#endif
            usleep(400 * 1000);
#ifdef SPRD_AUDIO_HIDL_CLIENT
            audioclient_setParameters(cmd_buf);
#else
            SendAudioTestCmd((const char *)cmd_buf, strlen(cmd_buf));
#endif

            ret = NPI_CMD_SUCCESS;
            goto out;

        }
        else if(loop_act == 0x10)
        {
            snprintf(cmd_buf, sizeof(cmd_buf) - 1, "test_stream_route=%d;test_in_stream_route=0x%x;endpoint_test=3;endpoint_teststep=1", output_devices, input_devices);
#ifdef SPRD_AUDIO_HIDL_CLIENT
            audioclient_setParameters(cmd_buf);
#else
            SendAudioTestCmd((const char *)cmd_buf, strlen(cmd_buf));
#endif
            ret = NPI_CMD_SUCCESS;
            goto out;
        }
    }

    if(data_len == 1)
    {
        uchar loop_act = data[0];

        if(loop_act == 0x11)
        {
            snprintf(cmd_buf, sizeof(cmd_buf) - 1, "endpoint_test=3;endpoint_teststep=0");
#ifdef SPRD_AUDIO_HIDL_CLIENT
            audioclient_setParameters(cmd_buf);
#else
            SendAudioTestCmd((const char *)cmd_buf, strlen(cmd_buf));
#endif
            ret = NPI_CMD_SUCCESS;
            goto out;
        }
    }
    else
    {
        uchar mic = data[0];
        uchar act = data[1];

        switch(mic)
        {
        case AUD_INDEV_BUILTIN_MIC:
            input_devices = AUDIO_DEVICE_IN_BUILTIN_MIC;
            break;
        case AUD_INDEV_HEADSET_MIC:
            input_devices = AUDIO_DEVICE_IN_WIRED_HEADSET;
            break;
        case AUD_INDEV_BACK_MIC:
            input_devices = AUDIO_DEVICE_IN_BACK_MIC;
            break;
        default:
            ALOGE("%s line:%d error", __func__, __LINE__);
            break;
        }

        switch( act )
        {
        case 0x01:    //start record
        {
#ifdef SPRD_AUDIO_HIDL_CLIENT
            openStreamIn(input_devices,16000,1);
            snprintf(cmd_buf, sizeof(cmd_buf) - 1, "endpoint_test=1;endpoint_teststep=1;test_in_stream_route=0x%x", input_devices);
            audioclient_setParameters(cmd_buf);
#else
            snprintf(cmd_buf, sizeof(cmd_buf) - 1, "endpoint_test=1;endpoint_teststep=1;test_in_stream_route=0x%x;pcm_rate=16000;pcm_channels=1", input_devices);
            SendAudioTestCmd((const char *)cmd_buf, strlen(cmd_buf));
#endif
            usleep(300 * 1000);
        }
        break;
        case 0x02:   //check record status

#ifndef SPRD_AUDIO_HIDL_CLIENT
            if(engaudio.record_pcm_pipe < 0)
            {
                engaudio.record_pcm_pipe = open(AUDIO_TEST_FILE, O_RDONLY | O_NONBLOCK);
            }
            if( engaudio.record_pcm_pipe < 0 )
            {
                ret = NPI_CMD_FAILED;
                ALOGE("%s open:%s failed error",  __func__, AUDIO_TEST_FILE, strerror(errno));
                break;
            }
#endif
            break;
        case 0x03:     //record record data
        {
            uchar idx = data[2];
            rsp_buffer_size = DIAG_MAX_DATA_SIZE;
            if( 0 == idx )
            {
#ifdef SPRD_AUDIO_HIDL_CLIENT
                if(engaudio.recordBuf==NULL){
                    engaudio.recordBuf=(char *)malloc(RECORD_MAX_SIZE);
                    memset(engaudio.recordBuf, 0, RECORD_MAX_SIZE);
                }
                snprintf(cmd_buf, sizeof(cmd_buf) - 1, "SprdAudioNpiReadDataSize=%d", RECORD_MAX_SIZE);
                StreamInsetParameters(cmd_buf);
                StreamInread(engaudio.recordBuf, RECORD_MAX_SIZE);
                ALOGI("StreamInread End");
                StreamInsetParameters("SprdAudioNpiReadDataSize=0");
#else
                snprintf(cmd_buf, sizeof(cmd_buf) - 1, "endpoint_test=1;endpoint_teststep=2;pcm_datasize=%d", RECORD_MAX_SIZE);
                SendAudioTestCmd((const char *)cmd_buf, strlen(cmd_buf));

                if(engaudio.recordBuf==NULL){
                    engaudio.recordBuf=(char *)malloc(RECORD_MAX_SIZE);
                }

                if(engaudio.recordBuf==NULL){
                    snprintf(cmd_buf, sizeof(cmd_buf) - 1, "endpoint_test=1;endpoint_teststep=0;");
                    ALOGD("line:%d malloc buffer failed:%s",__LINE__, cmd_buf);
                    SendAudioTestCmd((const char *)cmd_buf, strlen(cmd_buf));
                }else{
                    memset(engaudio.recordBuf, 0, RECORD_MAX_SIZE);
                    record_read(engaudio.recordBuf, RECORD_MAX_SIZE);
                }
#endif
            }
            if( RECORD_MAX_SIZE - (idx * DIAG_MAX_DATA_SIZE) < 0 )
            {
                ret = NPI_CMD_FAILED;
                break;
            }
            memcpy(rsp_buffer,
                   engaudio.recordBuf + idx * DIAG_MAX_DATA_SIZE, rsp_buffer_size);
        }
        break;
        case 0x04:   //stop record
            snprintf(cmd_buf, sizeof(cmd_buf) - 1, "endpoint_test=1;endpoint_teststep=0;");
            ALOGD("write:%s", cmd_buf);
#ifdef SPRD_AUDIO_HIDL_CLIENT
            StreamInClose();
            audioclient_setParameters(cmd_buf);
#else
            if(engaudio.record_pcm_pipe > 0)
            {
                close(engaudio.record_pcm_pipe);
                engaudio.record_pcm_pipe  = -1;
            }
            usleep(100 * 1000);
#endif
            break;
        default:
            ret = NPI_CMD_FAILED;
            ALOGE("%s line:%d error", __func__, __LINE__);
            break;
        }
    }

out:
    msg_head_ptr->subtype = ret;
    msg_head_ptr->len += rsp_buffer_size;
    rsp[msg_head_ptr->len + 2 - 1] = 0x7E;
    dump_data("testBbatAudioIn Output", rsp, msg_head_ptr->len + 2);
    return msg_head_ptr->len + 2;
}


int testBbatAudioOut(char *buf, int buf_len, char *rsp, int rsplen)
{
    int ret = NPI_CMD_SUCCESS;
    char cmd_buf[1024] = {0};
    int devices = 0;
    int output_devices = 0;
    MSG_HEAD_T *msg_head_ptr = NULL;
    char *data = (char *)(buf + 1 + sizeof(MSG_HEAD_T));
    int data_len = buf_len - 2 - sizeof(MSG_HEAD_T);
    uchar out = data[0];
    uchar act = data[1];
    int rsp_buffer_size = 0;

    memcpy(rsp, buf, 1 + sizeof(MSG_HEAD_T));
    msg_head_ptr = (MSG_HEAD_T *)(rsp + 1);
    msg_head_ptr->len = sizeof(MSG_HEAD_T);

    dump_data("testBbatAudioOut Input", buf, buf_len);
    ALOGI("testBbatAudioOut  size:%d", buf_len);
    if(data[0] == 0x20)   //skd test
    {
        out = data[1];
        output_devices = 0;

        switch(out)
        {
        case AUD_OUTDEV_SPEAKER:
            output_devices = AUDIO_DEVICE_OUT_SPEAKER;
            break;
        case AUD_OUTDEV_EARPIECE:
            output_devices = AUDIO_DEVICE_OUT_EARPIECE;
            break;
        case AUD_OUTDEV_HEADSET:
            output_devices = AUDIO_DEVICE_OUT_WIRED_HEADPHONE;
            break;
        default:
            ALOGE("%s line:%d error", __func__, __LINE__);
            break;
        }

#ifdef AUDIO_WHALE
        snprintf(cmd_buf, sizeof(cmd_buf) - 1,
                 "out_devices_test=1;test_stream_route=%d;samplerate=%d;channels=%d",
                 output_devices, 48000, 2);
#else
        snprintf(cmd_buf, sizeof(cmd_buf) - 1,
                 "out_devices_test=1;test_stream_route=%d;samplerate=%d;channels=%d",
                 output_devices, 44100, 2);
#endif

#ifdef SPRD_AUDIO_HIDL_CLIENT
        audioclient_setParameters(cmd_buf);
#else
        SendAudioTestCmd((const char *)cmd_buf, strlen(cmd_buf));
#endif

        snprintf(cmd_buf, sizeof(cmd_buf) - 1, "out_devices_test=0");
#ifdef SPRD_AUDIO_HIDL_CLIENT
        audioclient_setParameters(cmd_buf);
#else
        SendAudioTestCmd((const char *)cmd_buf, strlen(cmd_buf));
#endif
        ret = NPI_CMD_SUCCESS;
        goto out;
    }
    else
    {
        uchar out = data[0];
        uchar act = data[1];
        uchar chl = 0;
        if(data[2])
        {
            chl = 2;  //stereo
        }
        else
        {
            chl = 1;  //mono
        }

        switch(out)
        {
        case AUD_OUTDEV_SPEAKER:
            output_devices = AUDIO_DEVICE_OUT_SPEAKER;
            break;
        case AUD_OUTDEV_EARPIECE:
            output_devices = AUDIO_DEVICE_OUT_EARPIECE;
            break;
        case AUD_OUTDEV_HEADSET:
            output_devices = AUDIO_DEVICE_OUT_WIRED_HEADPHONE;
            break;
        default:
            ALOGE("%s line:%d error", __func__, __LINE__);
            break;
        }

        switch( act )
        {
        case 0x01:
        {
#ifdef AUDIO_WHALE
            snprintf(cmd_buf, sizeof(cmd_buf) - 1,
                     "endpoint_test=2;endpoint_teststep=1;test_stream_route=%d;pcm_rate=48000;pcm_channels=%d;",
                     output_devices, chl);
#else
            snprintf(cmd_buf, sizeof(cmd_buf) - 1,
                     "endpoint_test=2;endpoint_teststep=1;test_stream_route=%d;pcm_rate=44100;pcm_channels=%d;",
                     output_devices, chl);
#endif

#ifdef SPRD_AUDIO_HIDL_CLIENT
            audioclient_setParameters(cmd_buf);
#else
            SendAudioTestCmd((const char *)cmd_buf, strlen(cmd_buf));
            if(output_devices &( AUDIO_DEVICE_IN_WIRED_HEADSET | AUDIO_DEVICE_IN_WIRED_HEADSET)){
                usleep(200 * 1000);
             }
#endif
            break;
        }
        case 0x02:
        {
#ifdef SPRD_AUDIO_HIDL_CLIENT
#ifdef AUDIO_WHALE
            openStreamOut(output_devices,48000,chl);
#else
            openStreamOut(output_devices,44100,chl);
#endif
            snprintf(cmd_buf, sizeof(cmd_buf) - 1, "SprdAudioNpiWriteDataSize=%d", data_len - 3);
            StreamOutsetParameters(cmd_buf);

            snprintf(cmd_buf, sizeof(cmd_buf) - 1,
                     "test_stream_route=%d;",
                     output_devices);
            audioclient_setParameters(cmd_buf);
            memset(cmd_buf,0,sizeof(cmd_buf));

            StreamOutwrite(data + 3,data_len - 3);
            audioclient_setParameters("endpoint_test=2;endpoint_teststep=1;");
            StreamOutClose();
#else
            unsigned char *str = NULL;
            str = (unsigned char * )hex_to_string((uchar *)(data + 3), data_len - 3);
            if(NULL == str)
            {
                ret = NPI_CMD_FAILED;
            }
            else
            {
                snprintf(cmd_buf, sizeof(cmd_buf) - 1,
                         "endpoint_test=2;endpoint_teststep=1;test_stream_route=%d;pcm_rate=44100;pcm_channels=%d;pcm_datasize=%d;pcm_data=%s",
                         output_devices, chl, data_len - 3, str);
                SendAudioTestCmd((const char *)cmd_buf, strlen(cmd_buf));
                if(output_devices &( AUDIO_DEVICE_IN_WIRED_HEADSET | AUDIO_DEVICE_IN_WIRED_HEADSET)){
                    usleep(200 * 1000);
                 }
                free(str);
            }
#endif
            break;
        }
        case 0x03:
            snprintf(cmd_buf, sizeof(cmd_buf) - 1, "endpoint_test=2;endpoint_teststep=0;");
#ifdef SPRD_AUDIO_HIDL_CLIENT
            audioclient_setParameters(cmd_buf);
#else
            SendAudioTestCmd((const char *)cmd_buf, strlen(cmd_buf));
#endif
            break;
        default:
            ALOGE("%s line:%d error", __func__, __LINE__);
            ret = NPI_CMD_FAILED;
            break;
        }
    }
    usleep(200 * 1000);

out:
    msg_head_ptr->subtype = ret;
    msg_head_ptr->len += rsp_buffer_size;
    rsp[msg_head_ptr->len + 2 - 1] = 0x7E;
    dump_data("testBbatAudioOut Output", rsp, msg_head_ptr->len + 2);
    return msg_head_ptr->len + 2;
}

//AT+AUDIOPLAY=1,2,44100,2
static int audio_playback (struct eng_audio_ctl *ctl, char *buf, char *rsp)
{
    char *at_ptr = NULL;
    char *ptr = NULL;
    int cmd = 0;
    int output_devices = 0;
    char cmd_buf[256] = {0};
    int channel = 0;
    int rate = 0;
    int ret = -1;
    char *file_ptr = NULL;
    bool is_diag_cmd = false;

    if(NULL == rsp){
        ALOGE("%s Line:%d,null pointer", __FUNCTION__,__LINE__);
        return 0;
    }

    if (NULL == buf)
    {
        ALOGE("%s,null pointer", __FUNCTION__);
        sprintf(rsp, "\r\nERROR\r\n");
        return strlen(rsp);
    }

    memset(cmd_buf, 0, sizeof(cmd_buf));

    if(buf[0] == 0x7e)
    {
        ptr = buf + 1 + sizeof(MSG_HEAD_T);
        is_diag_cmd = true;
    }
    else
    {
        ptr = strdup(buf) ;
        at_ptr =ptr;
    }
    ALOGI("audio_playback:%s", ptr);

    ret=at_tok_equel_start(&ptr);
    ret|=at_tok_nextint(&ptr, &cmd);
    if(ret!=0){
        ALOGI("audio_playback line :%d error at_cmd:%s",__LINE__,ptr);
        cmd=-1;
    }

    if(1 == cmd)
    {
        ret =at_tok_nextint(&ptr, &output_devices);
        ret |=at_tok_nextint(&ptr, &rate);
        ret |=at_tok_nextint(&ptr, &channel);
        ret |=at_tok_nextstr(&ptr, &file_ptr);
        if((NULL != file_ptr)&&(0==ret))
        {
            ALOGI("audio_playback file_ptr:%s", file_ptr);
        }
        else
        {
            ALOGI("audio_playback line :%d error at_cmd:%s",__LINE__,ptr);
            file_ptr = default_pcm_file;
            ret=0;
        }
        snprintf(cmd_buf, sizeof(cmd_buf) - 1,
                 "endpoint_test=2;endpoint_teststep=1;test_stream_route=%d;pcm_rate=%d;pcm_channels=%d;pcm_file=%s;",
                 output_devices, rate, channel, file_ptr);
#ifdef SPRD_AUDIO_HIDL_CLIENT
        audioclient_setParameters(cmd_buf);
#else
        SendAudioTestCmd((const char *)cmd_buf, strlen(cmd_buf));
#endif
        if(output_devices &( AUDIO_DEVICE_IN_WIRED_HEADSET | AUDIO_DEVICE_IN_WIRED_HEADSET)){
            usleep(200 * 1000);
        }
        ret = 0;
        sprintf(rsp, "\r\nOK\r\n");
    }
    else if(0 == cmd)
    {
        snprintf(cmd_buf, sizeof(cmd_buf) - 1, "endpoint_test=2;endpoint_teststep=0;");
#ifdef SPRD_AUDIO_HIDL_CLIENT
        audioclient_setParameters(cmd_buf);
#else
        SendAudioTestCmd((const char *)cmd_buf, strlen(cmd_buf));
#endif
        ret = 0;
        sprintf(rsp, "\r\nOK\r\n");
    }
    else
    {
        sprintf(rsp, "\r\nERROR\r\n");
        ret = -1;
    }

    if((false == is_diag_cmd) && (NULL != at_ptr))
    {
        free(at_ptr);
    }

    if(ret != 0)
    {
        sprintf(rsp, "\r\nERROR\r\n");
    }

    usleep(200 * 1000);

    return strlen(rsp);
}

int AUDIO_PLAY_AT (char *req, char *rsp)
{
    return  audio_playback(&engaudio, req, rsp);
}

static int audio_headset_check (void)
{
    char buf[12] = {'\0'};
    int type = 0;
    const char *pHeadsetPath1 = "/sys/class/switch/h2w/state";
    const char *pHeadsetPath2 = "/sys/kernel/headset/state";
    const char *headsetStatePath = NULL;

    if (0 == access(pHeadsetPath2, R_OK))
    {
        headsetStatePath = pHeadsetPath2;
    }
    else if (0 == access(pHeadsetPath1, R_OK))
    {
        headsetStatePath = pHeadsetPath1;
    }

    if (headsetStatePath != NULL)
    {
        int fd = open(headsetStatePath, O_RDONLY);
        ALOGI("%s fd:%d boot before Path:%s", __FUNCTION__, fd, headsetStatePath);
        if (fd >= 0)
        {
            ssize_t readSize = read(fd, (char *)buf, 12);
            close(fd);
            if (readSize > 0)
            {
                int value = atoi((char *)buf);
                if (value == 1 || value == 2)
                {
                    int value = atoi((char *)buf);
                    if (value == AUDIO_WIRED_HEADSET_CONNECTED || value == AUDIO_WIRED_HEADPHONE_CONNECTED)
                    {
                        type = value;
                    }
                    else
                    {
                        value = 0;
                        ALOGI("%s headset is disconnect ! value:%d", __FUNCTION__, value);
                    }
                }
                else
                {
                    ALOGE("%s headset state is error! value:%d", __FUNCTION__, value);
                }
            }
            else
            {
                ALOGE("%s read %s is fail! readSize :%d", __FUNCTION__, headsetStatePath, readSize);
            }
        }
        else
        {
            ALOGE("%s open %s is fail!", __FUNCTION__, headsetStatePath);
        }
    }
    else
    {
        type = 0;
        ALOGE("%s headset state dev node is not access", __FUNCTION__);
    }
    return type;
}

int testheadsetplunge(char *buf, int buf_len, char *rsp, int rsplen)
{
    int type=0;
    MSG_HEAD_T *msg_head_ptr = NULL;

    char *data = (char *)(buf + 1 + sizeof(MSG_HEAD_T));
    char *rsp_buffer = NULL;

    memcpy(rsp, buf, 1 + sizeof(MSG_HEAD_T));
    msg_head_ptr = (MSG_HEAD_T *)(rsp + 1);
    msg_head_ptr->len = sizeof(MSG_HEAD_T);
    rsp_buffer = (char *)(rsp + 1 + sizeof(MSG_HEAD_T));

    type = audio_headset_check();
    msg_head_ptr->subtype=0;
    rsp_buffer[0]=type;
    msg_head_ptr->len++;
    rsp[msg_head_ptr->len + 2 - 1] = 0x7E;
    dump_data("testheadsetplunge", rsp, msg_head_ptr->len + 2);

    return msg_head_ptr->len + 2;
}

int AUDIO_HEADSET_CHECK_AT(char *req, char *rsp)
{
    int type = audio_headset_check();
    if(NULL!=rsp){
        sprintf(rsp, "%d\n", type);
        return strlen(rsp);
    }else{
        return 0;
    }
}

int AUDIO_HEADSET_TEST_AT(char *req, char *rsp)
{
    char *ptr = NULL;
    char *at_ptr=NULL;
    int ret = 0;
    int cmd = -1;
    int volume = 8;
    bool is_diag_cmd = false;
    char cmd_buf[256] = {0};
    int type = audio_headset_check();

    if(NULL == rsp){
        ALOGE("%s Line:%d,null pointer", __FUNCTION__,__LINE__);
        return 0;
    }
    if (NULL == req)
    {
        ALOGE("%s,null pointer", __FUNCTION__);
        sprintf(rsp, "\r\nERROR\r\n");
        return strlen(rsp);
    }

    if(req[0] == 0x7e)
    {
        ptr = req + 1 + sizeof(MSG_HEAD_T);
        is_diag_cmd = true;
    }
    else
    {
        ptr = strdup(req) ;
        at_ptr =ptr;
    }

    ret =at_tok_equel_start(&ptr);
    ret=at_tok_nextint(&ptr, &cmd);
    if(ret!=0){
        ALOGI("line :%d default at_cmd:%s",__LINE__,ptr);
    }else{
        if(1==cmd){
            if(AUDIO_WIRED_HEADSET_CONNECTED==type){
                audio_cploop(&engaudio,"AT+AUDIONPILOOP=1,4,4",rsp);
            }else if(AUDIO_WIRED_HEADPHONE_CONNECTED==type){
                audio_cploop(&engaudio,"AT+AUDIONPILOOP=1,1,4",rsp);
            }
        }else if(0==cmd){
            if(AUDIO_WIRED_HEADSET_CONNECTED==type){
                audio_cploop(&engaudio,"AT+AUDIONPILOOP=0,4,4",rsp);
            }else if(AUDIO_WIRED_HEADPHONE_CONNECTED==type){
                audio_cploop(&engaudio,"AT+AUDIONPILOOP=0,1,4",rsp);
            }else{
                audio_cploop(&engaudio,"AT+AUDIONPILOOP=0,1,4",rsp);
            }
        }
    }
    usleep(200 * 1000);
    if((false == is_diag_cmd) && (NULL != at_ptr))
    {
        free(at_ptr);
    }
    sprintf(rsp, "%d\n", type);
    return strlen(rsp);
}

/*
AT+ SPVLOOP= <cmd>,<mode>< volume ><loopbacktype><voiceformat><delaytime><
outdevice><indevice>

AT+ SPVLOOP= <cmd>,<mode>< volume ><loopbacktype><voiceformat><delaytime><outdevice><indevice>
Return:OK
Parameter:<cmd>:
cmd   Description
0     DISABLE
1     EABLE
2     SETMODE
3     SETVOLUME
4     SETDEVICE
<mode>:Optional mode exists 0-7
    0:handhold
    1:handfree
    2:earphone

<volume>: the size of volume
<loopbacktype>:
0: AD->DA loop,
1: AD->ul_process->dl_process->DA loop,
2: AD->ul_process->encoder->decoder->dl_process->DA loop
<voiceformat>: (1-3)
1: EFS vocoder
2: HR Vocoder
3: AMR Vocoder
<delaytime>: (0-1000) ms
<outdevice>:
1 ear
2 spk
4 hp
<indevice>
1 mic_0
2 mic_1
4 mic_hp
*/

static int at_cmd_audio_loop(struct eng_audio_ctl *ctl, int enable, int mode,
                             int volume, int loopbacktype, int voiceformat, int delaytime,
                             int output_device, int input_device)
{
#ifdef AUDIO_WHALE
    char at_cmd[128]={0};
    int device_in=0;
    if(input_device&(1<<0)){
        device_in|=0x80000004;
    }
    if(input_device&(1<<1)){
        device_in|=0x80000080;
    }
    if(input_device&(1<<2)){
        device_in|=0x80000010;
    }
    ALOGV("at_cmd_audio_loop:%p enable:%d mode:%d",
        ctl,enable,mode);

    ALOGV("at_cmd_audio_loop volume:%d loopbacktype:%d voiceformat:%d delaytime:%d",
        volume,loopbacktype,voiceformat,delaytime);

    ALOGV("at_cmd_audio_loop output_device:%d input_device:%d mode:%d",
        output_device,input_device,mode);

    if(enable){
        snprintf(at_cmd, sizeof(at_cmd) - 1, "test_out_stream_route=%d;test_in_stream_route=0x%x;dsp_loop=1;",
            output_device, device_in);
        return SendAudioTestCmd(at_cmd, strlen(at_cmd));
    }else{
        return SendAudioTestCmd("dsp_loop=0;", strlen("dsp_loop=0;"));
    }
#else
    char at_cmd[128];
    char at_rsp[128];

    snprintf(at_cmd, sizeof(at_cmd) - 1, "AT+SPVLOOP=%d,%d,%d,%d,%d,%d",
             enable, mode, volume, loopbacktype, voiceformat, delaytime);
    ALOGI("Send AT to cp:%s", at_cmd);
    if(NULL != ctl->atc_fun)
    {
        ctl->atc_fun(0, at_cmd, at_rsp, sizeof(at_rsp), -1);
        if(enable)
        {
            memset(at_cmd, 0, sizeof(at_cmd));
            snprintf(at_cmd, sizeof(at_cmd) - 1, "AT+SPVLOOP=4,,,,,,%d,%d", output_device, input_device);
            ALOGI("Send AT to cp:%s", at_cmd);
            if(NULL != ctl->atc_fun)
            {
                ctl->atc_fun(0, at_cmd, at_rsp, sizeof(at_rsp), -1);
            }else{
                ALOGE("at_cmd_audio_loop atc_fun is null:%s",at_cmd);
            }
        }
    }else{
        ALOGE("at_cmd_audio_loop atc_fun is null:%s",at_cmd);
    }
#endif
    return  0;
}

//AT+AUDIONPILOOP
/*
param0:0 loopback stop
    param1:input devices
    param2:output devices

param0:1 nb loopback test
    param1:input devices
    param2:output devices

param0:2
    param1: 0:nb 1:wb 2:swb 3:fb
    param2:input devices
    param3:output devices
    param4:volume
    param5:loopbacktype
    param6:voiceformat
    param7:delaytime
*/
static int audio_cploop (struct eng_audio_ctl *ctl, char *buf, char *rsp)
{
    char *ptr = NULL;
    char *at_ptr = NULL;
    int ret = 0;
    int net = 0;
    int output_devices = -1;
    int input_devices = -1;
    int cmd_type = 0;
    int at_mode = 1;
    int volume = 8;
    int loopbacktype = 2;
    int voiceformat = 3;
    int delaytime = 0;
    bool is_diag_cmd = false;
    int test_mode = 0;
    if(NULL == rsp){
        ALOGE("%s Line:%d,null pointer", __FUNCTION__,__LINE__);
        return 0;
    }
    if (NULL == buf)
    {
        ALOGE("%s,null pointer", __FUNCTION__);
        sprintf(rsp, "\r\nERROR\r\n");
        return strlen(rsp);
    }

    if(buf[0] == 0x7e)
    {
        ptr = buf + 1 + sizeof(MSG_HEAD_T);
        is_diag_cmd = true;
    }
    else
    {
        ptr = strdup(buf) ;
        at_ptr =ptr;
    }

    ALOGI("audio_cploop:%s", ptr);
    ret =at_tok_equel_start(&ptr);
    ret |=at_tok_nextint(&ptr, &cmd_type);
    if(ret!=0){
        ALOGI("audio_cploop line :%d error at_cmd:%s",__LINE__,ptr);
    }

    switch(cmd_type)
    {
    case 1:
        ret=at_tok_nextint(&ptr, &output_devices);
        ret|=at_tok_nextint(&ptr, &input_devices);
        if(ret!=0){
            ALOGI("audio_cploop line :%d error at_cmd:%s",__LINE__,ptr);
        }else{
            if((output_devices == 4) && (input_devices == 4))
            {
                at_mode = 2;
            }
            else if(output_devices == 2)
            {
                at_mode = 1;
            }
            at_cmd_audio_loop(&engaudio, 1, at_mode, volume,
                              loopbacktype, voiceformat, delaytime, output_devices, input_devices);
        }
        break;
    case 2:
        ret=at_tok_nextint(&ptr, &net);
        ret|=at_tok_nextint(&ptr, &output_devices);
        ret|=at_tok_nextint(&ptr, &input_devices);
        ret|=at_tok_nextint(&ptr, &volume);
        ret|=at_tok_nextint(&ptr, &loopbacktype);
        ret|=at_tok_nextint(&ptr, &voiceformat);
        ret|=at_tok_nextint(&ptr, &delaytime);
        if(ret!=0){
            ALOGI("audio_cploop line :%d error at_cmd:%s",__LINE__,ptr);
        }else{
            at_cmd_audio_loop(&engaudio, 1, at_mode, volume,
                              loopbacktype, voiceformat, delaytime, output_devices, input_devices);
        }
        break;
    case 0:
        ret=at_tok_nextint(&ptr, &output_devices);
        ret|=at_tok_nextint(&ptr, &input_devices);
        if(ret!=0){
            ALOGI("audio_cploop line :%d error at_cmd:%s",__LINE__,ptr);
        }else{
            at_cmd_audio_loop(&engaudio, 0, at_mode, volume,
                              loopbacktype, voiceformat, delaytime, output_devices, input_devices);
        }

        break;
    default:
        ret = -1;
        break;
    }

    if((false == is_diag_cmd) && (NULL != at_ptr))
    {
        free(at_ptr);
    }

    if(ret != 0)
    {
        sprintf(rsp, "\r\nERROR\r\n");
    }
    else
    {
        sprintf(rsp, "\r\nOK\r\n");
    }
    return strlen(rsp);
}

int AUDIO_CPLOOP_AT (char *req, char *rsp)
{
    return  audio_cploop(&engaudio, req, rsp);
}


//AT+AUDIOFM
/*
param0:0 stop fm audio

param0:1 start fm audio
    param1:fm volume

param0:2 set fm volume
    param1:fm volume
*/
static int audio_fm_fun (struct eng_audio_ctl *ctl, char *buf, char *rsp)
{
    char *ptr = NULL;
    char *at_ptr =NULL;
    int ret = 0;
    int cmd = -1;
    int volume = 8;
    bool is_diag_cmd = false;
    char cmd_buf[256] = {0};

    if(NULL == rsp){
        ALOGE("%s Line:%d,null pointer", __FUNCTION__,__LINE__);
        return 0;
    }

    if (NULL == buf)
    {
        ALOGE("%s,null pointer", __FUNCTION__);
        sprintf(rsp, "\r\nERROR\r\n");
        return strlen(rsp);
    }

    if(buf[0] == 0x7e)
    {
        ptr = buf + 1 + sizeof(MSG_HEAD_T);
        is_diag_cmd = true;
    }
    else
    {
        ptr = strdup(buf) ;
        at_ptr=ptr;
    }

    ALOGI("audio_fm_fun:%s", ptr);
    ret =at_tok_equel_start(&ptr);
    ret |=at_tok_nextint(&ptr, &cmd);
    if(ret!=0){
        ALOGI("audio_fm_fun line :%d default at_cmd:%s",__LINE__,ptr);
    }

    if(1==cmd){
        ret=at_tok_nextint(&ptr, &volume);
        if(ret!=0){
            ALOGI("audio_fm_fun line :%d default at_cmd:%s",__LINE__,ptr);
        }else{
            snprintf(cmd_buf, sizeof(cmd_buf) - 1, "handleFm=1;FM_Volume=%d;",volume);
#ifdef SPRD_AUDIO_HIDL_CLIENT
            audioclient_setParameters(cmd_buf);
#else
            SendAudioTestCmd((const char *)cmd_buf, strlen(cmd_buf));
            usleep(800 * 1000);
#endif
        }
    }else if(2==cmd){
        ret=at_tok_nextint(&ptr, &volume);
        if(ret==0){
            snprintf(cmd_buf, sizeof(cmd_buf) - 1, "FM_Volume=%d;",volume);
#ifdef SPRD_AUDIO_HIDL_CLIENT
            audioclient_setParameters(cmd_buf);
#else
            SendAudioTestCmd((const char *)cmd_buf, strlen(cmd_buf));
            usleep(100 * 1000);
#endif
        }else{
            ALOGI("audio_fm_fun line :%d default at_cmd:%s",__LINE__,ptr);
        }
    }else if(0==cmd){
        snprintf(cmd_buf, sizeof(cmd_buf) - 1, "handleFm=0;");
#ifdef SPRD_AUDIO_HIDL_CLIENT
        audioclient_setParameters(cmd_buf);
#else
        SendAudioTestCmd((const char *)cmd_buf, strlen(cmd_buf));
        usleep(400 * 1000);
#endif
    }

    if((false == is_diag_cmd) && (NULL != at_ptr))
    {
        free(at_ptr);
    }

    if(ret != 0)
    {
        sprintf(rsp, "\r\nERROR\r\n");
    }
    else
    {
        sprintf(rsp, "\r\nOK\r\n");
    }
    return strlen(rsp);
}

int AUDIO_FM_AT (char *req, char *rsp)
{
    return  audio_fm_fun(&engaudio, req, rsp);
}

#if defined(SPRD_AUDIO_HIDL_CLIENT) && defined(AUDIO_WHALE)
static int audio_calibration_fun (struct eng_audio_ctl *ctl, char *buf, char *rsp)
{
    char *ptr = NULL;
    char *at_ptr=NULL;
    int ret = -1;
    bool is_diag_cmd = false;
    char *cmd_str=NULL;
    char cmd_buffer[1024]={0};

    if(NULL == rsp){
        ALOGE("%s Line:%d,null pointer", __FUNCTION__,__LINE__);
        return 0;
    }
    if (NULL == buf)
    {
        ALOGE("%s,null pointer", __FUNCTION__);
        sprintf(rsp, "\r\nERROR\r\n");
        return strlen(rsp);
    }

    if(buf[0] == 0x7e)
    {
        ptr = buf + 1 + sizeof(MSG_HEAD_T);
        is_diag_cmd = true;
    }
    else
    {
        ptr = strdup(buf) ;
        at_ptr =ptr;
    }

    ALOGI("audio_calibration_fun:%s", ptr);
    ret =at_tok_equel_start(&ptr);
    ret |=at_tok_nextstr(&ptr, &cmd_str);
    if(ret!=0){
        ALOGI("audio_calibration_fun line :%d default at_cmd:%s",__LINE__,ptr);
    }

    if(0 == strncmp(cmd_str,"?",strlen("?"))){
        ret=audioclient_setParameters("SmartAmpCalibration=0");
    }else if(0 == strncmp(cmd_str,"SmartAmp",strlen("SmartAmp"))){
        ret=audioclient_setParameters("SmartAmpCalibration=0");
    }

    if((false == is_diag_cmd) && (NULL != at_ptr))
    {
        free(at_ptr);
    }

    if(ret != 0)
    {
        sprintf(rsp, "\r\nERROR\r\n");
    }
    else
    {
        sprintf(rsp, "\r\nOK\r\n");
    }
    return strlen(rsp);
}

int AUDIO_CALIBRATION_AT (char *req, char *rsp)
{
    return  audio_calibration_fun(&engaudio, req, rsp);
}
#endif

static int audio_extpipe_fun (struct eng_audio_ctl *ctl, char *buf, char *rsp)
{
    char *ptr = NULL;
    char *at_ptr=NULL;
    int ret = 0;
    int cmd = -1;
    int volume = 8;
    bool is_diag_cmd = false;
    char cmd_buf[256] = {0};
    char *pipe_msg=NULL;

    if(NULL == rsp){
        ALOGE("%s Line:%d,null pointer", __FUNCTION__,__LINE__);
        return 0;
    }

    if (NULL == buf)
    {
        ALOGE("%s,null pointer", __FUNCTION__);
        sprintf(rsp, "\r\nERROR\r\n");
        return strlen(rsp);
    }

    if(buf[0] == 0x7e)
    {
        ptr = buf + 1 + sizeof(MSG_HEAD_T);
        is_diag_cmd = true;
    }
    else
    {
        ptr = strdup(buf) ;
        at_ptr= ptr;
    }

    ALOGI("audio_extpipe_fun:%s", ptr);
    ret =at_tok_equel_start(&ptr);
    ret |=at_tok_nextstr(&ptr, &pipe_msg);
    if((ret!=0)||(NULL==pipe_msg)){
        ALOGI("audio_extpipe_fun line :%d default at_cmd:%s",__LINE__,ptr);
        sprintf(rsp, "\r\nERROR\r\n");
    }

#ifdef SPRD_AUDIO_HIDL_CLIENT
    audioclient_setParameters(pipe_msg);
#else
    SendAudioTestCmd((const char *)pipe_msg, strlen(pipe_msg));
#endif
    if((false == is_diag_cmd) && (NULL != at_ptr))
    {
        free(at_ptr);
    }
    sprintf(rsp, "\r\nOK\r\n");
    return strlen(rsp);
}

int AUDIO_PIPE_AT (char *req, char *rsp)
{
    return  audio_extpipe_fun(&engaudio, req, rsp);
}
