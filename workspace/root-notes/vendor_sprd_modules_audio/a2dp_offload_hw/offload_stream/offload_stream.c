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
#define LOG_TAG "audio_hw_offload_stream"

#include <errno.h>
#include <pthread.h>
#include <stdint.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/mman.h>
#include <log/log.h>
#include <cutils/str_parms.h>
//#include <cutils/properties.h>
#include "tinycompress/tinycompress.h"
#include "offload_stream.h"
#include <system/thread_defs.h>
#include <cutils/sched_policy.h>
#include <sys/prctl.h>
#include "sound/compress_params.h"
#include <sys/resource.h>

extern int offload_write(struct compress *compress, const void *buf, unsigned int size);

struct pcm_config pcm_config_offloa_ctl = {
    .channels = 2,
    .rate = 48000,
    .period_size = 0x21c0/2,
    .period_count = 10,
    .format = PCM_FORMAT_S16_LE,
    .start_threshold = 0x21c0/2,
    .avail_min = 0x21c0 / 2,
};

void audio_stop_compress_output(struct sprd_out_stream *out);

bool audio_is_offload_support_format(audio_format_t format)
{
    if ((format == AUDIO_FORMAT_MP3 ||
            format == AUDIO_FORMAT_AAC)) {
        return true;
    }
    return false;
}

int audio_get_offload_codec_id(audio_format_t format)
{
    int id = 0;

    switch (format) {
    case AUDIO_FORMAT_MP3:
        id = SND_AUDIOCODEC_MP3;
        break;
    case AUDIO_FORMAT_AAC:
        id = SND_AUDIOCODEC_AAC;
        break;
    default:
        ALOGE("%s: audio format (%d) is not supported ", __func__, format);
    }

    return id;
}

struct compress {
    int fd;
    unsigned int flags;
    char error[128];
    struct compr_config *config;
    int running;
    int max_poll_wait_ms;
    int nonblocking;
    unsigned int gapless_metadata;
    unsigned int next_track;
};

static int dsp_sleep_ctrl(struct sprdout_ctl *stream_ctl ,bool on_off){
    int ret=0;

    if(NULL ==stream_ctl->dsp_sleep_ctl){
        ALOGE("dsp_sleep_ctrl  failed mixer is null");
        ret= -1;
        goto exit;
    }

    if(on_off != stream_ctl->agdsp_sleep_status){
        ret = mixer_ctl_set_value(stream_ctl->dsp_sleep_ctl, 0, on_off);
        if (ret != 0) {
            ALOGE("dsp_sleep_ctrl Failed %d\n", on_off);
        }else{
            ALOGI("dsp_sleep_ctrl:%d",on_off);
            stream_ctl->agdsp_sleep_status=on_off;
        }
    }else{
        ALOGD("dsp_sleep_ctrl  the same values:%d",on_off);
    }

exit:
    return ret;
}

static int offload_apply_mixer_control(struct device_control *dev_ctl, const char *info)
{
    int ret=-1;
    unsigned int i=0;
    ALOGD("apply_mixer_control %s %s ",dev_ctl->name,info);
    for (i=0; i < dev_ctl->ctl_size; i++) {
        struct mixer_control *mixer_ctl = &dev_ctl->ctl[i];
        if(mixer_ctl->strval!=NULL){
            ret = mixer_ctl_set_enum_by_string(mixer_ctl->ctl, mixer_ctl->strval);
            if (ret != 0) {
                ALOGE("Failed to set '%s' to '%s'\n",mixer_ctl->name, mixer_ctl->strval);
            } else {
                ALOGI("Set '%s' to '%s'\n",mixer_ctl->name, mixer_ctl->strval);
            }
        }else{
            if(1!=mixer_ctl->val_count){
                int val[2]={0};
                val[0]=mixer_ctl->value;
                val[1]=mixer_ctl->value;
                ret=mixer_ctl_set_array(mixer_ctl->ctl,val,2);
            }else{
            ret=mixer_ctl_set_value(mixer_ctl->ctl, 0, mixer_ctl->value);
            }
            if (ret != 0) {
                ALOGE("Failed to set '%s'to %d\n",
                        mixer_ctl->name, mixer_ctl->value);
            } else {
                ALOGI("Set '%s' to %d\n",
                        mixer_ctl->name, mixer_ctl->value);
            }
        }
    }
    return ret;
}

static int switch_a2dp_be_switch(struct sprdout_ctl *stream_ctl,bool on)
{
    if(true==on){
        dsp_sleep_ctrl(stream_ctl,true);
        offload_apply_mixer_control(&stream_ctl->route.be_switch_route.ctl_on, "a2dp be_switch switch on");
    }else{
        offload_apply_mixer_control(&stream_ctl->route.be_switch_route.ctl_off, "a2dp be_switch switch off");
    }
    return 0;
}

static int switch_a2dp_iis_mux_switch(struct sprdout_ctl *stream_ctl,bool on)
{
    if(true==on){
        dsp_sleep_ctrl(stream_ctl,true);
        offload_apply_mixer_control(&stream_ctl->route.vbc_iis_mux_route.ctl_on, "a2dp iis mux switch on");
    }else{
        offload_apply_mixer_control(&stream_ctl->route.vbc_iis_mux_route.ctl_off, "a2dp iis mux switch off");
    }
    return 0;
}

static void set_sbc_paramter(void *ctl){
    struct sprdout_ctl *stream_ctl = (struct sprdout_ctl *)ctl;
    mixer_ctl_set_array(stream_ctl->bt_sbc_ctl,&stream_ctl->sbc_param,sizeof(SBCENC_PARAM_T));
}

int set_usecase(struct sprdout_ctl *stream_ctl, int usecase, bool on)
{
    int ret = 0;
    int pre_usecase=0;
    ALOGI("set_usecase cur :0x%x usecase=0x%x  %s",stream_ctl->usecase, usecase, on ? "on" : "off");

    pthread_mutex_lock(&stream_ctl->kcontrol_lock);

    pre_usecase = stream_ctl->usecase;

    if(true==on){
        stream_ctl->usecase |= usecase;
        dsp_sleep_ctrl(stream_ctl,true);
        if(pre_usecase==0){
            offload_apply_mixer_control(&stream_ctl->route.vbc_iis.ctl_on, "a2dp switch on");
            set_sbc_paramter(stream_ctl);
            switch_a2dp_be_switch(stream_ctl,true);
            switch_a2dp_iis_mux_switch(stream_ctl,true);
        }
    }else{
        stream_ctl->usecase &= ~usecase;
        if(stream_ctl->usecase==0){
            offload_apply_mixer_control(&stream_ctl->route.vbc_iis.ctl_off, "a2dp switch off");
            switch_a2dp_be_switch(stream_ctl,false);
            switch_a2dp_iis_mux_switch(stream_ctl,false);
        }

        if(stream_ctl->usecase==0){
            dsp_sleep_ctrl(stream_ctl,false);
        }
    }

    pthread_mutex_unlock(&stream_ctl->kcontrol_lock);

    return ret;
}

static int do_offload_output_standby(void *dev,
    void *out, UNUSED_ATTR AUDIO_HW_APP_T type) {
    struct sprdout_ctl *stream_ctl=(struct sprdout_ctl *)dev;
    struct sprd_out_stream *offload_out = (struct sprd_out_stream *)out;

    if(offload_out->standby == true) {
        ALOGW("do_offload_output_standby exit");
        return 0;
    }
    offload_out->standby = true;

    audio_stop_compress_output(offload_out);
    offload_out->gapless_mdata.encoder_delay = 0;
    offload_out->gapless_mdata.encoder_padding = 0;
    if (offload_out->compress != NULL) {
        ALOGI("do_offload_output_standby audio_offload compress_close");
        compress_close(offload_out->compress);
        offload_out->compress = NULL;
    }

    if (offload_out->audio_app_type == AUDIO_HW_APP_A2DP_OFFLOAD){
        set_usecase(stream_ctl , UC_BT_OFFLOAD, false);
    }
    ALOGI("do_offload_output_standby exit");
    return 0;
}

int audio_start_compress_output(struct audio_stream_out *out_p) {
    int ret = 0;
    struct sprd_out_stream *out = (struct sprd_out_stream *) out_p;
    struct sprdout_ctl *stream_ctl = out->stream_ctl;
    struct stream_param *stream_para = &(stream_ctl->stream_para);

    ALOG_ASSERT((out->audio_app_type == AUDIO_HW_APP_A2DP_OFFLOAD),
            "audio_start_compress_output  bad app type %d", out->audio_app_type);

    if (out->audio_app_type == AUDIO_HW_APP_A2DP_OFFLOAD){
        ret = set_usecase(stream_ctl,UC_BT_OFFLOAD, true);
    }else {
         ret=-1;
         ALOGE("%s line:%d failed audio_app_type:%d"
            ,__func__,__LINE__,out->audio_app_type);
    }

    if(ret < 0) {
        goto Err;
    }

    out->pcm=NULL;
    out->compress = compress_open(stream_para->sprd_out_stream.card,
                                  stream_para->sprd_out_stream.device,
                                  COMPRESS_IN, &out->compress_config);

    if(NULL == out->compress) {
        ALOGE("%s: compress_open ERR", __func__);
        goto Err;
    }

    if (out->compress && !is_compress_ready(out->compress)) {
        struct compress *compress_tmp = out->compress;
        ALOGE("%s: err:%s fd:%d return ", __func__, compress_get_error(out->compress), compress_tmp->fd);
        compress_close(out->compress);
        out->compress = NULL;
        goto Err;
    }

    ALOGI("%s: compress_open out compress:%p app_type:%d", __func__, out->compress, out->audio_app_type);

    if (out->audio_offload_callback) {
        compress_nonblock(out->compress, out->is_offload_nonblocking);
    }

    out->standby_fun = do_offload_output_standby;
    out->standby = false;
    out->audio_app_type = AUDIO_HW_APP_A2DP_OFFLOAD;
    return 0;
Err:
    ALOGE("audio_start_compress_output failed");

    if(out->compress != NULL) {
        compress_close(out->compress);
        out->compress = NULL;
    }
    return -1;
}

void audio_stop_compress_output(struct sprd_out_stream *out) {
    ALOGI("%s in, audio_app_type:%d, audio_offload_state:%d ",
          __func__, out->audio_app_type, out->audio_offload_state);

    out->audio_offload_state = AUDIO_OFFLOAD_STATE_STOPED;
    out->is_offload_compress_started = false;
    out->is_offload_need_set_metadata =
        true;  /* need to set metadata to driver next time */
    if (out->compress != NULL) {
        compress_stop(out->compress);
        /* wait for finishing processing the command */
        while (out->is_audio_offload_thread_blocked) {
            ALOGI("audio_stop_compress_output wait");
            pthread_cond_wait(&out->audio_offload_cond, &out->lock);
        }
    }
}

int audio_get_compress_metadata(struct audio_stream_out *out_p,
                                struct str_parms *parms) {
    int ret = 0;
    char value[32];
    int param_update = false;
    struct sprd_out_stream *out = (struct sprd_out_stream *) out_p;

    if (!out || !parms) {
        return -1;
    }
    /* get meta data from audio framework */
    ret = str_parms_get_str(parms, AUDIO_OFFLOAD_CODEC_DELAY_SAMPLES, value,
                            sizeof(value));
    if (ret >= 0) {
        out->gapless_mdata.encoder_delay = atoi(value);
        param_update = true;
    }

    ret = str_parms_get_str(parms, AUDIO_OFFLOAD_CODEC_PADDING_SAMPLES, value,
                            sizeof(value));
    if (ret >= 0) {
        out->gapless_mdata.encoder_padding = atoi(value);
        param_update = true;
    }

    ret = str_parms_get_str(parms, AUDIO_OFFLOAD_CODEC_SAMPLE_RATE, value,
                           sizeof(value));
    if (ret >= 0) {
       out->compr_mdata.samplerate = atoi(value);
       out->compress_config.codec->sample_rate = out->compr_mdata.samplerate;
       param_update = true;
    }

    ret = str_parms_get_str(parms, AUDIO_OFFLOAD_CODEC_NUM_CHANNEL, value,
                           sizeof(value));
    if (ret >= 0) {
       out->compr_mdata.channel= atoi(value);
       param_update = true;
    }

    ret = str_parms_get_str(parms, AUDIO_OFFLOAD_CODEC_AVG_BIT_RATE, value,
                           sizeof(value));
    if (ret >= 0) {
       out->compr_mdata.bitrate= atoi(value);
       param_update = true;
    }

    if(param_update) {
        out->is_offload_need_set_metadata = true;
        ALOGI("%s successfully, new encoder_delay: %u, encoder_padding: %u ",
              __func__, out->gapless_mdata.encoder_delay, out->gapless_mdata.encoder_padding);
        ALOGI("%s successfully, new samplerate:%u, out->mdata_channel: %u, bitrate: %u ",
              __func__, out->compr_mdata.samplerate, out->compr_mdata.channel, out->compr_mdata.bitrate);
    }
    return 0;
}

int audio_send_offload_cmd(struct audio_stream_out *out_p,
                           AUDIO_OFFLOAD_CMD_T command) {
    struct sprd_out_stream *out = (struct sprd_out_stream *) out_p;
    struct audio_offload_cmd *cmd = (struct audio_offload_cmd *)calloc(1,
                                    sizeof(struct audio_offload_cmd));

    ALOGD("%s, cmd:%d, offload_state:%d ",
          __func__, command, out->audio_offload_state);
    /* add this command to list, then send signal to offload thread to process the command list */
    cmd->cmd = command;
    list_add_tail(&out->audio_offload_cmd_list, &cmd->node);
    pthread_cond_signal(&out->audio_offload_cond);
    return 0;
}

void *audio_offload_thread_loop(void *param) {
    struct sprd_out_stream *out = (struct sprd_out_stream *) param;
    struct listnode *item;


    /* init the offload state */
    out->audio_offload_state = AUDIO_OFFLOAD_STATE_STOPED;
    out->is_offload_compress_started = false;

    setpriority(PRIO_PROCESS, 0, ANDROID_PRIORITY_AUDIO);
    set_sched_policy(0, SP_FOREGROUND);
    prctl(PR_SET_NAME, (unsigned long)"Audio Offload Thread", 0, 0, 0);

    ALOGI("%s in", __func__);
    pthread_mutex_lock(&out->lock);
    for (;;) {
        bool need_send_callback = false;
        struct audio_offload_cmd *cmd = NULL;
        stream_callback_event_t event;

        ALOGD("%s, offload_cmd_list:%d, offload_state:%d",
              __func__, list_empty(&out->audio_offload_cmd_list), out->audio_offload_state);

        /*
            If the command list is not empty, don't need to wait for new, just process it.
            Otherwise, wait for new command.
        */
        if (list_empty(&out->audio_offload_cmd_list)) {
            ALOGI("audio_offload_thread_loop audio_offload_cmd_list is empty,wait");
            pthread_cond_wait(&out->audio_offload_cond, &out->lock);
            continue;
        }
        /* get the command from the list, then process the command */
        item = list_head(&out->audio_offload_cmd_list);
        cmd = node_to_item(item, struct audio_offload_cmd, node);
        list_remove(item);
        ALOGI("%s, offload_state:%d, offload_cmd:%d, out->compress:%p ",
              __func__, out->audio_offload_state, cmd->cmd, out->compress);

        if (cmd->cmd == AUDIO_OFFLOAD_CMD_EXIT) {
            ALOGI("audio_offload_thread_loop AUDIO_OFFLOAD_CMD_EXIT");
            free(cmd);
            break;
        }
        if (out->compress == NULL) {
            ALOGI("audio_offload_thread_loop:compress is null");
            pthread_cond_signal(&out->audio_offload_cond);
            continue;
        }
        out->is_audio_offload_thread_blocked = true;
        pthread_mutex_unlock(&out->lock);
        need_send_callback = false;
        switch(cmd->cmd) {
        case AUDIO_OFFLOAD_CMD_WAIT_FOR_BUFFER:
            compress_wait(out->compress, -1);
            need_send_callback = true;
            event = STREAM_CBK_EVENT_WRITE_READY;
            break;
        case AUDIO_OFFLOAD_CMD_PARTIAL_DRAIN:
            compress_next_track(out->compress);
            compress_partial_drain(out->compress);
            need_send_callback = true;
            event = STREAM_CBK_EVENT_DRAIN_READY;
            break;
        case AUDIO_OFFLOAD_CMD_DRAIN:
            compress_drain(out->compress);
            need_send_callback = true;
            event = STREAM_CBK_EVENT_DRAIN_READY;
            break;
        default:
            ALOGE("%s unknown command received: %d", __func__, cmd->cmd);
            break;
        }
        ALOGI("audio_offload_thread_loop try lock");
        pthread_mutex_lock(&out->lock);
        out->is_audio_offload_thread_blocked = false;
        /* send finish processing signal to awaken where is waiting for this information */
        pthread_cond_signal(&out->audio_offload_cond);
        if (need_send_callback && out->audio_offload_callback) {
            out->audio_offload_callback(event, NULL, out->audio_offload_cookie);
        }
        free(cmd);
    }

    ALOGI("audio_offload_thread_loop:start exit");
    pthread_cond_signal(&out->audio_offload_cond);
    /* offload thread loop exit, free the command list */
    while (!list_empty(&out->audio_offload_cmd_list)) {
        item = list_head(&out->audio_offload_cmd_list);
        list_remove(item);
        free(node_to_item(item, struct audio_offload_cmd, node));
    }
    pthread_mutex_unlock(&out->lock);
    ALOGI("audio_offload_thread_loop:exit");
    return NULL;
}


int audio_offload_create_thread(struct audio_stream_out  *out_p) {
    struct sprd_out_stream *out = (struct sprd_out_stream *) out_p;
    pthread_cond_init(&out->audio_offload_cond, (const pthread_condattr_t *) NULL);
    list_init(&out->audio_offload_cmd_list);
    pthread_create(&out->audio_offload_thread, (const pthread_attr_t *) NULL,
                   audio_offload_thread_loop, out);
    ALOGI("%s, successful, id:%lu ", __func__, out->audio_offload_thread);
    return 0;
}

int audio_offload_destroy_thread(struct audio_stream_out  *out_p) {
    struct sprd_out_stream *out = (struct sprd_out_stream *) out_p;
    ALOGI("audio_offload_destroy_thread enter");
    pthread_mutex_lock(&out->lock);
    audio_stop_compress_output(out);
    /* send command to exit the thread_loop */
    audio_send_offload_cmd(out_p, AUDIO_OFFLOAD_CMD_EXIT);
    pthread_mutex_unlock(&out->lock);
    pthread_join(out->audio_offload_thread, (void **) NULL);
    pthread_cond_destroy(&out->audio_offload_cond);
    ALOGI("audio_offload_destroy_thread exit");
    return 0;
}

ssize_t out_write_compress(struct audio_stream_out *out_p, const void *buffer,
                           size_t bytes) {
    int ret = 0;
    struct sprd_out_stream *out = (struct sprd_out_stream * )out_p;
    ALOGI("%s: want to write buffer (%zd bytes) to compress device, offload_state:%d metadata:%d ",
          __func__, bytes, out->audio_offload_state, out->is_offload_need_set_metadata);

    if (out->is_offload_need_set_metadata) {
        ALOGW("%s: need to send new metadata to driver ", __func__);
        compress_set_gapless_metadata(out->compress, &out->gapless_mdata);
        compress_set_metadata(out->compress, &out->compr_mdata);
        out->is_offload_need_set_metadata = 0;
    }

    //dump_pcm(buffer, bytes);
#if 1
    ret = offload_write(out->compress, buffer, bytes);
#else
    if (NULL == out->compress || NULL == buffer) {
        ret = -1;
    } else {
        ret = compress_write(out->compress, buffer, bytes);
    }
#endif
    ALOGI("%s: finish writing buffer (%zd bytes) to compress device, and return %d %d",
          __func__, bytes, ret, out->compress->nonblocking);
    /* neet to wait for ring buffer to ready for next read or write */
    if (ret >= 0 && ret < (ssize_t)bytes) {
        audio_send_offload_cmd(&out->stream, AUDIO_OFFLOAD_CMD_WAIT_FOR_BUFFER);
    }

    if (!out->is_offload_compress_started) {
        compress_start(out->compress);
        out->is_offload_compress_started = true;
        out->audio_offload_state = AUDIO_OFFLOAD_STATE_PLAYING;
    }

    ALOGI("out_write_compress return:%d", ret);
    return ret;
}

static int offload_set_callback(struct audio_stream_out *stream,
                                stream_callback_t callback, void *cookie) {
    struct sprd_out_stream *out = (struct sprd_out_stream *)stream;

    ALOGD("%s in, audio_app_type:%d callback:%p cookie:%p", __func__, out->audio_app_type,
          callback, cookie);
    pthread_mutex_lock(&out->lock);
    out->audio_offload_callback = callback;
    out->audio_offload_cookie = cookie;
    pthread_mutex_unlock(&out->lock);
    return 0;
}

static int offload_pause(struct audio_stream_out *stream) {
    struct sprd_out_stream *out = (struct sprd_out_stream *)stream;

    int status = -ENOSYS;
    ALOGI("%s in, audio_app_type:%d, audio_offload_state:%d ",
          __func__, out->audio_app_type, out->audio_offload_state);

    if (out->audio_app_type == AUDIO_HW_APP_A2DP_OFFLOAD) {
        pthread_mutex_lock(&out->lock);
        if (out->compress != NULL
                && out->audio_offload_state == AUDIO_OFFLOAD_STATE_PLAYING) {
            status = compress_pause(out->compress);
            out->audio_offload_state = AUDIO_OFFLOAD_STATE_PAUSED;
        }
        status = 0;
        pthread_mutex_unlock(&out->lock);
    }
    return status;
}

static  int offload_resume(struct audio_stream_out *stream) {
    struct sprd_out_stream *out = (struct sprd_out_stream *)stream;

    int ret = -ENOSYS;
    ALOGI("%s in, audio_app_type:%d, audio_offload_state:%d ",
          __func__, out->audio_app_type, out->audio_offload_state);

    if (out->audio_app_type == AUDIO_HW_APP_A2DP_OFFLOAD) {
        ret = 0;
        pthread_mutex_lock(&out->lock);
        if (out->compress != NULL
                && out->audio_offload_state == AUDIO_OFFLOAD_STATE_PAUSED) {
            ret = compress_resume(out->compress);
            out->audio_offload_state = AUDIO_OFFLOAD_STATE_PLAYING;
        }
        pthread_mutex_unlock(&out->lock);
    }
    return ret;
}

static  int offload_drain(struct audio_stream_out *stream,
    audio_drain_type_t type ) {
    struct sprd_out_stream *out = (struct sprd_out_stream *)stream;

    int ret = -ENOSYS;
    ALOGI("%s in, audio_app_type:%d, audio_offload_state:%d, type:%d ",
          __func__, out->audio_app_type, out->audio_offload_state, type);

    if (out->audio_app_type ==  AUDIO_HW_APP_A2DP_OFFLOAD) {
        pthread_mutex_lock(&out->lock);
        if (type == AUDIO_DRAIN_EARLY_NOTIFY) {
            ret = audio_send_offload_cmd(stream, AUDIO_OFFLOAD_CMD_PARTIAL_DRAIN);
        } else {
            ret = audio_send_offload_cmd(stream, AUDIO_OFFLOAD_CMD_DRAIN);
        }
        pthread_mutex_unlock(&out->lock);
    }
    ALOGI("out_offload_drain exit");
    return ret;
}

static int offload_flush(struct audio_stream_out *stream) {
    struct sprd_out_stream *out = (struct sprd_out_stream *)stream;
    ALOGI("%s in, audio_app_type:%d, audio_offload_state:%d ",
          __func__, out->audio_app_type, out->audio_offload_state);

    if (out->audio_app_type == AUDIO_HW_APP_A2DP_OFFLOAD) {
        pthread_mutex_lock(&out->lock);
        if(out->standby == false) {
            audio_stop_compress_output(out);
        }
        pthread_mutex_unlock(&out->lock);
        ALOGI("out_offload_flush exit");
        return 0;
    }
    return -ENOSYS;
}
void init_sprd_offload_stream(void  *stream) {
    struct sprd_out_stream *out = (struct sprd_out_stream *)stream;

    out->stream.set_callback = offload_set_callback;
    out->stream.pause = offload_pause;
    out->stream.resume = offload_resume;
    out->stream.drain = offload_drain;
    out->stream.flush = offload_flush;
}
