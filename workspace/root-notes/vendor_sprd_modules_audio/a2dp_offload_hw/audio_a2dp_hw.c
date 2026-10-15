/******************************************************************************
 *
 *  Copyright (C) 2009-2012 Broadcom Corporation
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at:
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 ******************************************************************************/

/*****************************************************************************
 *
 *  Filename:      audio_a2dp_hw.c
 *
 *  Description:   Implements hal for bluedroid a2dp audio device
 *
 *****************************************************************************/
#define LOG_TAG "audio_hw_a2dp"
#include <errno.h>
#include <inttypes.h>
#include <pthread.h>
#include <stdint.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/poll.h>
#include <sys/errno.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <cutils/str_parms.h>
#include <cutils/sockets.h>
#include <log/log.h>

#include <system/audio.h>
#include <hardware/audio.h>

#include <hardware/hardware.h>
#include "audio_a2dp_hw.h"

#include <cutils/str_parms.h>
#include <cutils/properties.h>
#include <system/audio-base.h>

#include <cutils/list.h>
#include <dlfcn.h>
#include "dsp_monitor.h"

#define UNUSED_ATTR __attribute__((unused))

struct sprd_a2dp_stream_out {
  struct audio_stream_out stream;
  struct audio_stream_out* default_output;
  struct audio_stream_out* data_output;
  void *sprd_adev;
  bool standby;
  bool is_offload;
  pthread_mutex_t lock;
};

struct sprda2dp_audio_device {
    pthread_mutex_t lock;
    bool is_support_offload;

    void *stream_ctl;
    audio_mode_t mode;
    struct audio_hw_device default_a2dp;
    struct sprd_a2dp_stream_out *active_offload_stream;
    struct sprd_a2dp_stream_out *active_pcm_stream;
    bool a2dp_suspended;
    bool bt_sco_enable;
    bool voip_start;
    bool a2dp_disconnected;
    bool bypass;
    bool fm_status;
    bool force_standby;
    void *default_a2dp_hw_module;
    void * dsp_monitor;
};

extern void  stream_ctl_deinit(void *dev);
extern void *stream_ctl_init(void);
extern int sprd_open_output_stream(void *ctl,
                                audio_devices_t devices,
                                audio_output_flags_t flags,
                                struct audio_config *config,
                                struct audio_stream_out **stream_out);
extern int sprd_close_output_stream(struct audio_stream_out *stream);
bool check_sbc_paramter(void *ctl,char *kvpairs);

static void default_a2dp_module_exit(struct hw_module_t *hmi);

static struct sprda2dp_audio_device sprd_a2dp_dev;

UNUSED_ATTR static bool isOffloadSupported(void)
{
    char propValue[PROPERTY_VALUE_MAX];
    if (property_get("audio.offload.disable", propValue, "0")) {
        if (atoi(propValue) != 0) {
            ALOGV("offload disabled by audio.offload.disable=%s", propValue );
            return false;
        }
    }
    return true;
}

static uint32_t out_get_sample_rate(const struct audio_stream *stream)
{
    struct sprd_a2dp_stream_out *out=(struct sprd_a2dp_stream_out *)stream;

    if((NULL!=out->data_output)&&(NULL!=out->data_output->common.get_sample_rate)){
        return out->data_output->common.get_sample_rate(&out->data_output->common);
    }
    return 48000;
}

static ssize_t out_write(struct audio_stream_out *stream, const void* buffer,
                         size_t bytes)
{

    struct sprd_a2dp_stream_out *out=(struct sprd_a2dp_stream_out *)stream;
    struct sprda2dp_audio_device *sprd_adev=(struct sprda2dp_audio_device *)out->sprd_adev;
    int ret=0;
    bool bypass = false;

    pthread_mutex_lock(&sprd_adev->lock);
    pthread_mutex_lock(&out->lock);

    if(out->is_offload ) {
        if(sprd_adev->active_offload_stream && (sprd_adev->active_offload_stream != out)) {
            bypass = true;
        }
    } else if(sprd_adev->active_pcm_stream && (sprd_adev->active_pcm_stream != out)){
        bypass = true;
    }

    if((true==sprd_adev->bypass) || bypass){
        ALOGI("out_write bypass:%p a2dp_suspended:%d bt_sco_enable:%d,is_offload:%d",
            out,sprd_adev->a2dp_suspended,sprd_adev->bt_sco_enable,out->is_offload);
        if(!out->is_offload){
            pthread_mutex_unlock(&out->lock);
            pthread_mutex_unlock(&sprd_adev->lock);
            usleep((int64_t) bytes * 1000000 / audio_stream_out_frame_size(stream) / out_get_sample_rate(&stream->common) / 2);
            return bytes;
        }else{
            pthread_mutex_unlock(&out->lock);
            pthread_mutex_unlock(&sprd_adev->lock);
            usleep(100*1000);
            return bytes;
        }
    }
    if(true==out->standby){
        out->standby=false;
        ALOGI("out_write Start:%zd bytes out:%p",bytes,out);
    }

    if(out->is_offload) {
        sprd_adev->active_offload_stream = out;
    }
    else {
        sprd_adev->active_pcm_stream = out;
    }

    pthread_mutex_unlock(&out->lock);
    pthread_mutex_unlock(&sprd_adev->lock);

    pthread_mutex_lock(&out->lock);
    if((NULL!=out->default_output)&&(NULL!=out->default_output->write)){
        ret=out->default_output->write(out->default_output,buffer,bytes);
        if(ret<0){
            ALOGI("out_write default a2dp write failed");
            pthread_mutex_unlock(&out->lock);
            return ret;
        }
    }

    if((NULL!=out->data_output)&&(NULL!=out->data_output->write)){
        ret=out->data_output->write(out->data_output,buffer,bytes);
        if(ret<0){
            ALOGI("out_write sprd data write failed");
            pthread_mutex_unlock(&out->lock);
            return ret;
        }
    }
    pthread_mutex_unlock(&out->lock);
    return ret;
}

static int out_set_sample_rate(UNUSED_ATTR struct audio_stream *stream, UNUSED_ATTR uint32_t rate)
{
    return -ENOSYS;
}

static size_t out_get_buffer_size(const struct audio_stream *stream)
{
    struct sprd_a2dp_stream_out *out=(struct sprd_a2dp_stream_out *)stream;

    if((NULL!=out->data_output)&&(NULL!=out->data_output->common.get_buffer_size)){
        return out->data_output->common.get_buffer_size(&out->data_output->common);
    }
    return -ENOSYS;
}

static uint32_t out_get_channels(const struct audio_stream *stream)
{
    struct sprd_a2dp_stream_out *out=(struct sprd_a2dp_stream_out *)stream;

    if((NULL!=out->data_output)&&(NULL!=out->data_output->common.get_channels)){
        return out->data_output->common.get_channels(&out->data_output->common);
    }
    return 2;
}

static audio_format_t out_get_format(const struct audio_stream *stream)
{
    struct sprd_a2dp_stream_out *out=(struct sprd_a2dp_stream_out *)stream;

    if((NULL!=out->data_output)&&(NULL!=out->data_output->common.get_format)){
        return out->data_output->common.get_format(&out->data_output->common);
    }
    return -ENOSYS;
}

static uint32_t out_get_latency(const struct audio_stream_out *stream)
{
    struct sprd_a2dp_stream_out *out=(struct sprd_a2dp_stream_out *)stream;

    if((NULL!=out->data_output)&&(NULL!=out->data_output->get_latency)){
        return out->data_output->get_latency(out->data_output);
    }
    return -ENOSYS;
}

static int out_set_volume(struct audio_stream_out *stream, float left,
                          float right)
{
    struct sprd_a2dp_stream_out *out=(struct sprd_a2dp_stream_out *)stream;

    if((NULL!=out->data_output)&&(NULL!=out->data_output->set_volume)){
        return out->data_output->set_volume(out->data_output,left,right);
    }
    return -ENOSYS;
}

static int out_get_render_position(const struct audio_stream_out *stream,
                                   uint32_t *dsp_frames)
{
    struct sprd_a2dp_stream_out *out=(struct sprd_a2dp_stream_out *)stream;

    if((NULL!=out->data_output)&&(NULL!=out->data_output->get_render_position)){
        return out->data_output->get_render_position(out->data_output,dsp_frames);
    }
    return -ENOSYS;
}

static int out_get_presentation_position(const struct audio_stream_out *stream,
        uint64_t *frames, struct timespec *timestamp)
{
    struct sprd_a2dp_stream_out *out=(struct sprd_a2dp_stream_out *)stream;

    if((NULL!=out->data_output)&&(NULL!=out->data_output->get_render_position)){
        return out->data_output->get_presentation_position(out->data_output,frames,timestamp);
    }
    return -ENOSYS;
}

static int out_set_format(UNUSED_ATTR struct audio_stream *stream, UNUSED_ATTR audio_format_t format)
{
    return -ENOSYS;
}

static int out_add_audio_effect(UNUSED_ATTR const struct audio_stream *stream, UNUSED_ATTR effect_handle_t effect)
{
    return -ENOSYS;
}

static int out_remove_audio_effect(UNUSED_ATTR const struct audio_stream *stream, UNUSED_ATTR effect_handle_t effect)
{
    return -ENOSYS;
}

static int a2dp_out_standby(void *stream)
{
    int ret=0;
    ALOGI("a2dp_out_standby");
    struct sprd_a2dp_stream_out *out=(struct sprd_a2dp_stream_out *)stream;

    if(false==out->standby){
        if((NULL!=out->data_output)&&(NULL!=out->data_output->common.standby)){
            ret= out->data_output->common.standby(&out->data_output->common);
        }

        if((NULL!=out->default_output)&&(NULL!=out->default_output->common.standby)){
            ret= out->default_output->common.standby(&out->default_output->common);
        }
    }
    out->standby=true;
    return ret;
}

static int out_standby(struct audio_stream *stream)
{
    int ret=0;
    struct sprd_a2dp_stream_out *out=(struct sprd_a2dp_stream_out *)stream;
    struct sprda2dp_audio_device *sprd_adev=(struct sprda2dp_audio_device *)out->sprd_adev;
    ALOGI("out_standby:%p",out);
    pthread_mutex_lock(&out->lock);
    a2dp_out_standby(out);
    out->standby=true;
    pthread_mutex_unlock(&out->lock);
    pthread_mutex_lock(&sprd_adev->lock);
    if(out == sprd_adev->active_offload_stream) {
        sprd_adev->active_offload_stream = NULL;
    }
    else if(out == sprd_adev->active_pcm_stream){
        sprd_adev->active_pcm_stream = NULL;
    }
    pthread_mutex_unlock(&sprd_adev->lock);
    return ret;
}

static int out_dump(UNUSED_ATTR const struct audio_stream *stream,UNUSED_ATTR int fd)
{
    ALOGI("Line:%d",__LINE__);
    return 0;
}

static int out_set_parameters(struct audio_stream *stream, const char *kvpairs)
{
    struct sprd_a2dp_stream_out *out=(struct sprd_a2dp_stream_out *)stream;
    struct sprda2dp_audio_device *sprd_adev=(struct sprda2dp_audio_device *)out->sprd_adev;

    int ret=0;
    ALOGI("out_set_parameters:%s",kvpairs);

    struct str_parms *parms;
    char keyval[16];
    int retval;

    parms = str_parms_create_str(kvpairs);

    retval = str_parms_get_str(parms, "closing", keyval, sizeof(keyval));

    if (retval >= 0){
        if (strcmp(keyval, "true") == 0)
        {
            pthread_mutex_lock(&sprd_adev->lock);
            pthread_mutex_lock(&out->lock);
            a2dp_out_standby(out);
            pthread_mutex_unlock(&out->lock);
            pthread_mutex_unlock(&sprd_adev->lock);
        }
    }

    retval = str_parms_get_str(parms, "A2dpSuspended", keyval, sizeof(keyval));
    if (retval >= 0){
        if (strcmp(keyval, "true") == 0) {
            pthread_mutex_lock(&sprd_adev->lock);
            pthread_mutex_lock(&out->lock);
            a2dp_out_standby(out);
            pthread_mutex_unlock(&out->lock);
            pthread_mutex_unlock(&sprd_adev->lock);
        }
    }

    retval = str_parms_get_str(parms, "disconnect", keyval, sizeof(keyval));
    if (retval >= 0) {
        int devices=strtoul(keyval,NULL,0);
        if(AUDIO_DEVICE_OUT_BLUETOOTH_A2DP&devices){
            pthread_mutex_lock(&sprd_adev->lock);
            pthread_mutex_lock(&out->lock);
            a2dp_out_standby(out);
            pthread_mutex_unlock(&out->lock);
            pthread_mutex_unlock(&sprd_adev->lock);
        }
    }

    if((NULL!=out->default_output)
        &&(NULL!=out->default_output->common.set_parameters)){
        ret= out->default_output->common.set_parameters(&out->default_output->common,
            kvpairs);
    }

    if((NULL!=out->data_output)
        &&(NULL!=out->data_output->common.set_parameters)){
        ret= out->data_output->common.set_parameters(&out->data_output->common,
            kvpairs);
    }

    str_parms_destroy(parms);
    return 0;
}

static char * out_get_parameters(const struct audio_stream *stream, UNUSED_ATTR const char *keys)
{
    struct sprd_a2dp_stream_out *out=(struct sprd_a2dp_stream_out *)stream;

    ALOGI("out_set_parameters:%s",keys);

    if((NULL!=out->data_output)
        &&(NULL!=out->data_output->common.get_parameters)){
        return out->data_output->common.get_parameters(&out->data_output->common,keys);
    }
    return strdup("");
}

static int out_offload_set_callback(struct audio_stream_out *stream,
                             stream_callback_t callback, void *cookie)
{
    struct sprd_a2dp_stream_out *out = (struct sprd_a2dp_stream_out *)stream;

    if((NULL!=out->data_output)&&(NULL!=out->data_output->set_callback)){
        return out->data_output->set_callback(out->data_output,callback,cookie);
    }
    return -ENOSYS;
}

static int out_offload_pause(struct audio_stream_out *stream)
{
    struct sprd_a2dp_stream_out *out = (struct sprd_a2dp_stream_out *)stream;

    if((NULL!=out->data_output)&&(NULL!=out->data_output->pause)){
        return out->data_output->pause(out->data_output);
    }
    return -ENOSYS;
}

static int out_offload_resume(struct audio_stream_out *stream)
{
    struct sprd_a2dp_stream_out *out = (struct sprd_a2dp_stream_out *)stream;

    if((NULL!=out->data_output)&&(NULL!=out->data_output->pause)){
        return out->data_output->resume(out->data_output);
    }
    return -ENOSYS;
}

static int out_offload_drain(struct audio_stream_out *stream, audio_drain_type_t type )
{
    struct sprd_a2dp_stream_out *out = (struct sprd_a2dp_stream_out *)stream;

    if((NULL!=out->data_output)&&(NULL!=out->data_output->pause)){
        return out->data_output->drain(out->data_output,type);
    }
    return -ENOSYS;
}

static int out_offload_flush(struct audio_stream_out *stream)
{
    struct sprd_a2dp_stream_out *out = (struct sprd_a2dp_stream_out *)stream;

    if((NULL!=out->data_output)&&(NULL!=out->data_output->pause)){
        return out->data_output->flush(out->data_output);
    }
    return -ENOSYS;
}

static int adev_open_output_stream(struct audio_hw_device *dev,
                                   audio_io_handle_t handle,
                                   audio_devices_t devices,
                                   audio_output_flags_t flags,
                                   struct audio_config *config,
                                   struct audio_stream_out **stream_out,
                                   const char *address)

{
    struct sprda2dp_audio_device *sprd_adev = (struct sprda2dp_audio_device *)&sprd_a2dp_dev;
    struct audio_hw_device *a2dp_dev=(struct audio_hw_device *)&sprd_a2dp_dev.default_a2dp;
    struct sprd_a2dp_stream_out* out=NULL;
    struct audio_config config_tmp;
    char *sbc_str=NULL;

    int ret = 0;
    ALOGI("adev_open_output_stream flag:%d is_support_offload:%d",flags,sprd_adev->is_support_offload);
    memcpy(&config_tmp,config,sizeof(struct audio_config));
    if(false==sprd_adev->is_support_offload){
        ret=a2dp_dev->open_output_stream(dev,
            handle,
            devices,
            flags,
            &config_tmp,
            stream_out,
            address);

        if((ret)||(NULL==*stream_out)){
            ALOGE("adev_open_output_stream open default a2dp stream failed");
        }
        return ret;
    }else{
        out = (struct sprd_a2dp_stream_out*)calloc(1, sizeof(struct sprd_a2dp_stream_out));

        if (!out) return -ENOMEM;
#ifdef HAPS_TEST
        out->default_output =NULL;
#else
        ret=a2dp_dev->open_output_stream(dev,
            handle,
            devices,
            flags,
            &config_tmp,
            &out->default_output,
            address);

        if((ret)||(NULL==out->default_output)){
            ALOGE("adev_open_output_stream failed");
            goto open_failed;
        }
#endif
        ret=sprd_open_output_stream(sprd_adev->stream_ctl,
                    devices,flags,config,&out->data_output);
        if((ret)||(NULL==out->data_output)){
            ALOGE("adev_open_output_stream sprd_open_output_stream failed");
            goto open_failed;
        }
    }
    out->stream.common.get_sample_rate = out_get_sample_rate;
    out->stream.common.set_sample_rate = out_set_sample_rate;
    out->stream.common.get_buffer_size = out_get_buffer_size;
    out->stream.common.get_channels = out_get_channels;
    out->stream.common.get_format = out_get_format;
    out->stream.common.set_format = out_set_format;
    out->stream.common.standby = out_standby;
    out->stream.common.dump = out_dump;
    out->stream.common.set_parameters = out_set_parameters;
    out->stream.common.get_parameters = out_get_parameters;
    out->stream.common.add_audio_effect = out_add_audio_effect;
    out->stream.common.remove_audio_effect = out_remove_audio_effect;
    out->stream.get_latency = out_get_latency;
    out->stream.set_volume = out_set_volume;
    out->stream.write = out_write;
    out->stream.get_render_position = out_get_render_position;
    out->stream.get_presentation_position = out_get_presentation_position;

    out->sprd_adev=sprd_adev;

    pthread_mutex_init(&out->lock, NULL);

    *stream_out = &out->stream;

    if (flags & AUDIO_OUTPUT_FLAG_COMPRESS_OFFLOAD){
        out->stream.set_callback = out_offload_set_callback;
        out->stream.pause = out_offload_pause;
        out->stream.resume = out_offload_resume;
        out->stream.drain = out_offload_drain;
        out->stream.flush = out_offload_flush;
        out->is_offload = true;
    }else{
        out->is_offload = false;
    }

    sbc_str=out->default_output->common.get_parameters(&out->default_output->common,"read_codec_config");

    pthread_mutex_lock(&sprd_adev->lock);
    if(NULL!=sbc_str){
        if(false==check_sbc_paramter(sprd_adev->stream_ctl,sbc_str)){
            ALOGE("%s invalid sbc paramter",__func__);
        }
        free(sbc_str);
    }else{
        ALOGE("%s get sbc paramter failed",__func__);
    }
    pthread_mutex_unlock(&sprd_adev->lock);

    ALOGI("adev_open_output_stream success");
    return 0;

open_failed:

    if(out!=NULL){
        if(NULL!=out->default_output){
            a2dp_dev->close_output_stream(dev,out->default_output);
            out->default_output=NULL;
        }

        if(NULL!=out->data_output){
            sprd_close_output_stream(out->data_output);
            out->data_output=NULL;
        }

        free(out);
    }
    return ret;
}

static void adev_close_output_stream(struct audio_hw_device *dev,
                                     struct audio_stream_out *stream)
{
    struct sprda2dp_audio_device *sprd_adev = (struct sprda2dp_audio_device *)&sprd_a2dp_dev;
    struct audio_hw_device *a2dp_dev=(struct audio_hw_device *)&sprd_a2dp_dev.default_a2dp;

    ALOGI("adev_close_output_stream %p is_support_offload:%d"
        ,stream,sprd_adev->is_support_offload);

    pthread_mutex_lock(&sprd_adev->lock);
    if(sprd_adev->is_support_offload==false){
        a2dp_dev->close_output_stream(dev,stream);
    }else{
        struct sprd_a2dp_stream_out* out=(struct sprd_a2dp_stream_out*)stream;
        if(NULL!=out->data_output){
            sprd_close_output_stream(out->data_output);
            out->data_output=NULL;
        }

        if(NULL!=out->default_output){
            a2dp_dev->close_output_stream(dev,out->default_output);
            out->default_output=NULL;
        }
        if (sprd_adev->active_offload_stream == out){
            sprd_adev->active_offload_stream=NULL;
        } else if(sprd_adev->active_pcm_stream == out){
            sprd_adev->active_pcm_stream=NULL;
        }
        free(out);
    }
    pthread_mutex_unlock(&sprd_adev->lock);
    ALOGI("adev_close_output_stream exit");
}

static void check_a2dp_suspend(void *dev){
    struct sprda2dp_audio_device *sprd_adev=dev;
    struct sprd_a2dp_stream_out *offload_stream=NULL;
    struct sprd_a2dp_stream_out *pcm_stream=NULL;

    pthread_mutex_lock(&sprd_adev->lock);
    offload_stream=(struct sprd_a2dp_stream_out *)sprd_adev->active_offload_stream;
    pcm_stream=(struct sprd_a2dp_stream_out *)sprd_adev->active_pcm_stream;

    if((true==sprd_adev->bt_sco_enable)||(true==sprd_adev->a2dp_suspended)||(true==sprd_adev->voip_start)
        ||(AUDIO_MODE_IN_CALL==sprd_adev->mode)||(true==sprd_adev->a2dp_disconnected)||(true==sprd_adev->fm_status)
        || sprd_adev->force_standby){
        sprd_adev->bypass = true;
        if(NULL!=pcm_stream){
            ALOGI("check_a2dp_suspend pcm_stream bypass");
            pthread_mutex_lock(&pcm_stream->lock);
            a2dp_out_standby(pcm_stream);
            pthread_mutex_unlock(&pcm_stream->lock);
        }else{
            ALOGI("check_a2dp_suspend pcm_stream is null");
        }
        if(NULL!=offload_stream){
            ALOGI("check_a2dp_suspend offload_stream bypass");
            pthread_mutex_lock(&offload_stream->lock);
            a2dp_out_standby(offload_stream);
            pthread_mutex_unlock(&offload_stream->lock);
        }else{
            ALOGI("check_a2dp_suspend offload_stream is null");
        }
    }else{
        sprd_adev->bypass = false;
        ALOGI("check_a2dp_suspend Clear bypass");
    }
    pthread_mutex_unlock(&sprd_adev->lock);
}

void adev_a2dp_force_stanby(bool forcestandby) {
     struct sprda2dp_audio_device *sprd_adev=&sprd_a2dp_dev;
     pthread_mutex_lock(&sprd_adev->lock);
     sprd_adev->force_standby= forcestandby;
     pthread_mutex_unlock(&sprd_adev->lock);
     check_a2dp_suspend(sprd_adev);

}

static int adev_set_mode(UNUSED_ATTR struct audio_hw_device *dev, audio_mode_t mode)
{
    struct sprda2dp_audio_device *sprd_adev=&sprd_a2dp_dev;

    ALOGI("adev_set_mode:%d",mode);
    pthread_mutex_lock(&sprd_adev->lock);
    sprd_adev->mode=mode;
    pthread_mutex_unlock(&sprd_adev->lock);
    check_a2dp_suspend(sprd_adev);
    return 0;
}

static int adev_set_parameters(struct audio_hw_device *dev, const char *kvpairs)
{
    struct audio_hw_device *a2dp_dev=(struct audio_hw_device *)&sprd_a2dp_dev.default_a2dp;
    struct str_parms *parms;
    char keyval[16];
    int retval;
    struct sprda2dp_audio_device *sprd_adev=&sprd_a2dp_dev;
    bool check_a2dp_status=false;

    int ret=0;
    ALOGI("adev_set_parameters :%s is_support_offload %d", kvpairs, sprd_adev->is_support_offload);

    parms = str_parms_create_str(kvpairs);

    retval = str_parms_get_str(parms, "A2dpSuspended", keyval, sizeof(keyval));
    if (retval >= 0){
        if (strcmp(keyval, "true") == 0) {
            pthread_mutex_lock(&sprd_adev->lock);
            sprd_adev->a2dp_suspended=true;
            check_a2dp_status=true;
            pthread_mutex_unlock(&sprd_adev->lock);
        }

        if (strcmp(keyval, "false") == 0) {
            pthread_mutex_lock(&sprd_adev->lock);
            sprd_adev->a2dp_suspended=false;
            check_a2dp_status=true;
            pthread_mutex_unlock(&sprd_adev->lock);
        }
    }

    retval = str_parms_get_str(parms, "BT_SCO", keyval, sizeof(keyval));
    if (retval >= 0){
        if (strcmp(keyval, "on") == 0) {
            pthread_mutex_lock(&sprd_adev->lock);
            sprd_adev->bt_sco_enable=true;
            check_a2dp_status=true;
            pthread_mutex_unlock(&sprd_adev->lock);
        }

        if (strcmp(keyval, "off") == 0) {
            pthread_mutex_lock(&sprd_adev->lock);
            sprd_adev->bt_sco_enable=false;
            check_a2dp_status=true;
            pthread_mutex_unlock(&sprd_adev->lock);
        }
    }

    retval = str_parms_get_str(parms, "disconnect", keyval, sizeof(keyval));
    if (retval >= 0) {
        int devices=strtoul(keyval,NULL,0);
        if(AUDIO_DEVICE_OUT_BLUETOOTH_A2DP&devices){
            pthread_mutex_lock(&sprd_adev->lock);
            sprd_adev->bt_sco_enable=false;
            sprd_adev->a2dp_suspended=false;
            sprd_adev->a2dp_disconnected=true;
            check_a2dp_status=true;
            ALOGI("A2dp disconnect clear bypass time");
            pthread_mutex_unlock(&sprd_adev->lock);
        }
    }

    retval = str_parms_get_str(parms, "connect", keyval, sizeof(keyval));
    if (retval >= 0) {
        int devices=strtoul(keyval,NULL,0);
        if(AUDIO_DEVICE_OUT_BLUETOOTH_A2DP&devices){
            pthread_mutex_lock(&sprd_adev->lock);
            sprd_adev->a2dp_disconnected=false;
            check_a2dp_status=true;
            ALOGI("A2dp connect clear bypass time");
            pthread_mutex_unlock(&sprd_adev->lock);
        }
    }
    retval = str_parms_get_str(parms,"handleFm",keyval,sizeof(keyval));
    if (retval >= 0) {
        int devices=strtoul(keyval,NULL,0);
        if (devices) {
            pthread_mutex_lock(&sprd_adev->lock);
            sprd_adev->fm_status = true;
            check_a2dp_status=true;
            pthread_mutex_unlock(&sprd_adev->lock);
        }
        if (!devices) {
            pthread_mutex_lock(&sprd_adev->lock);
            sprd_adev->fm_status = false;
            check_a2dp_status=true;
            pthread_mutex_unlock(&sprd_adev->lock);
        }
    }

    retval = str_parms_get_str(parms, "a2dp_voip_start", keyval, sizeof(keyval));
    if (retval >= 0){
        if (strcmp(keyval, "true") == 0) {
            pthread_mutex_lock(&sprd_adev->lock);
            sprd_adev->voip_start=true;
            if(true==sprd_adev->is_support_offload){
                check_a2dp_status=true;
            }
            pthread_mutex_unlock(&sprd_adev->lock);
        }

        if (strcmp(keyval, "false") == 0) {
            pthread_mutex_lock(&sprd_adev->lock);
            sprd_adev->voip_start=false;
            if(true==sprd_adev->is_support_offload){
                check_a2dp_status=true;
            }
            pthread_mutex_unlock(&sprd_adev->lock);
        }
    }

    retval = str_parms_get_str(parms, "setMode", keyval, sizeof(keyval));
    if (retval >= 0) {
        int mode=strtoul(keyval,NULL,0);
        if((AUDIO_MODE_IN_CALL==mode)||(AUDIO_MODE_NORMAL==mode)){
            adev_set_mode(dev,(audio_mode_t)mode);
        }
    }

    if((true==check_a2dp_status)&&(true==sprd_adev->is_support_offload)){
        check_a2dp_suspend(sprd_adev);
    }

    if(NULL!=a2dp_dev->set_parameters){
        ret= a2dp_dev->set_parameters(dev,kvpairs);
    }

    str_parms_destroy(parms);
    ALOGI("adev_set_parameters :%s exit", kvpairs);
    return ret;
}

static char * adev_get_parameters(const struct audio_hw_device *dev,
                                  const char *keys)
{
    struct sprda2dp_audio_device *sprd_adev=&sprd_a2dp_dev;
    struct audio_hw_device *a2dp_dev=(struct audio_hw_device *)&sprd_a2dp_dev.default_a2dp;
    struct str_parms *query = str_parms_create_str(keys);

    if (str_parms_has_key(query, "isA2dpoffloadSupported")) {
        if((a2dp_dev->get_parameters==NULL)
            ||(a2dp_dev->set_parameters==NULL)){
            pthread_mutex_lock(&sprd_adev->lock);
            sprd_adev->is_support_offload=false;
            pthread_mutex_unlock(&sprd_adev->lock);
            str_parms_destroy(query);
            return strdup("A2dpoffloadSupported=0");
        }

        str_parms_destroy(query);
        if(true==sprd_adev->is_support_offload){
            a2dp_dev->set_parameters((struct audio_hw_device *)dev,"a2dpswitchoffload=true");
            return strdup("A2dpoffloadSupported=1");
        }else{
            a2dp_dev->set_parameters((struct audio_hw_device *)dev,"a2dpswitchoffload=false");
            return strdup("A2dpoffloadSupported=0");
        }
    }
    str_parms_destroy(query);
    return strdup("");
}

static int adev_dump(UNUSED_ATTR const audio_hw_device_t *device, UNUSED_ATTR int fd)
{
    return 0;
}

static int adev_close(hw_device_t *device)
{
    struct sprda2dp_audio_device *sprd_adev=&sprd_a2dp_dev;
    struct audio_hw_device *a2dp_dev=(struct audio_hw_device *)&sprd_a2dp_dev.default_a2dp;
    a2dp_dev->common.close(device);
    stream_ctl_deinit(sprd_adev->stream_ctl);
    default_a2dp_module_exit((struct hw_module_t *)sprd_adev->default_a2dp_hw_module);
    sprd_adev->default_a2dp_hw_module=NULL;
    if(sprd_adev->dsp_monitor) {
        dsp_monitor_close(sprd_adev->dsp_monitor);
    }
    ALOGI("adev_close");
    return 0;
}

#if defined(__LP64__)
const char * default_a2dphallib="/system/lib64/hw/audio.a2dp.default.so";
const char * default_bluetoothhallib="/system/lib64/hw/audio.bluetooth.default.so";

const char * vendor_a2dphallib="/vendor/lib64/hw/audio.a2dp.default.so";
const char * vendor_bluetoothhallib="/vendor/lib64/hw/audio.bluetooth.default.so";

#else
const char * default_a2dphallib="/system/lib/hw/audio.a2dp.default.so";
const char * default_bluetoothhallib="/system/lib/hw/audio.bluetooth.default.so";

const char * vendor_a2dphallib="/vendor/lib/hw/audio.a2dp.default.so";
const char * vendor_bluetoothhallib="/vendor/lib/hw/audio.bluetooth.default.so";
#endif
static int load_default_a2dp_modules(struct audio_hw_device **dev,struct hw_module_t **pHmi)
{
    void *handler = NULL;
    struct hw_module_t *hmi = NULL;
    const char *a2dphal_lib=NULL;
    if (property_get_bool("persist.bluetooth.bluetooth_audio_hal.disabled", false)){
        if(access(vendor_a2dphallib, R_OK) == 0){
            a2dphal_lib=vendor_a2dphallib;
        }else{
            a2dphal_lib=default_a2dphallib;
        }
    }else{
        if(access(vendor_bluetoothhallib, R_OK) == 0){
            a2dphal_lib=vendor_bluetoothhallib;
        }else{
            a2dphal_lib=default_bluetoothhallib;
        }
    }

    int rc=-1;

    ALOGI("%s load [%s] start",__func__,a2dphal_lib);

    if (access(a2dphal_lib, R_OK) == 0) {
        handler = NULL;
        handler = dlopen(a2dphal_lib, RTLD_LAZY);
        if (handler == NULL) {
            ALOGE("dlopen :%s fail! %s \n",a2dphal_lib, dlerror());
        } else {
            const char *sym = HAL_MODULE_INFO_SYM_AS_STR;
            hmi = (struct hw_module_t *)dlsym(handler, sym);
            if (hmi == NULL) {
                ALOGE("load: couldn't find symbol %s", sym);
                rc= -1;
                goto err;
            }

            hmi->dso = handler;

            rc = audio_hw_device_open(hmi, dev);
            if (rc) {
                ALOGE("%s couldn't open audio hw device in (%s)", __func__,
                        strerror(-rc));
                rc= -1;
                goto err;
            }
            rc=0;
            ALOGI("%s load [%s] success",__func__,a2dphal_lib);
        }
    }
err:
    if (rc != 0) {
        hmi = NULL;
        if (handler != NULL) {
            dlclose(handler);
            handler = NULL;
        }
    }

    *pHmi = hmi;

    return rc;
}

static void default_a2dp_module_exit(struct hw_module_t *hmi)
{
    if(NULL!=hmi){
        void *handler = hmi->dso;
        if (handler != NULL) {
            dlclose(handler);
            handler = NULL;
        }
    }
}

static int adev_open(const hw_module_t* module,UNUSED_ATTR const char* name,
                     hw_device_t** device)
{
    struct sprda2dp_audio_device *sprd_adev=&sprd_a2dp_dev;
    struct audio_hw_device *a2dp_dev=NULL;
    void *stream_ctl=NULL;
    int ret=0;

    ALOGI("adev_open in A2dp_hw module");

    if (property_get_bool("ro.bluetooth.a2dp_offload.supported", false)) {
        if (property_get_bool("persist.bluetooth.bluetooth_audio_hal.disabled", false) &&
            property_get_bool("persist.bluetooth.a2dp_offload.disabled", false)) {
            // Both BluetoothAudio@2.0 and BluetoothA2dp@1.0 (Offlaod) are disabled, and uses
            // the legacy hardware module for A2DP and hearing aid.
            ALOGI("%s line:%d disable a2dp offload",__func__,__LINE__);
            return load_default_a2dp_modules((struct audio_hw_device **)device,&module);
        } else if (property_get_bool("persist.bluetooth.a2dp_offload.disabled", false)) {
            // A2DP offload supported but disabled: try to use special XML file
            ALOGI("%s line:%d disable a2dp offload",__func__,__LINE__);
            return load_default_a2dp_modules((struct audio_hw_device **)device,&module);
        }
    }

    stream_ctl=stream_ctl_init();
    if(stream_ctl==NULL){
        ALOGE("adev_open stream_ctl_init Failed");
        return -1;
    }
#ifdef HAPS_TEST
    * device=calloc(1, sizeof(struct audio_hw_device));
#else
    ret= load_default_a2dp_modules((struct audio_hw_device **)device,(struct hw_module_t **)&sprd_adev->default_a2dp_hw_module);
    if((ret!=0)||(*device==NULL)){
        ALOGE("adev_open load_default_a2dp_modules Failed");
        if(NULL!=stream_ctl){
            free(stream_ctl);
            stream_ctl=NULL;
        }
        return ret;
    }
#endif
    a2dp_dev=(struct audio_hw_device *)*device;

    a2dp_dev->common.module = (struct hw_module_t*)module;

    sprd_adev->default_a2dp.common.close=a2dp_dev->common.close;
    sprd_adev->default_a2dp.set_mode=a2dp_dev->set_mode;
    sprd_adev->default_a2dp.set_parameters=a2dp_dev->set_parameters;
    sprd_adev->default_a2dp.get_parameters=a2dp_dev->get_parameters;
    sprd_adev->default_a2dp.open_output_stream=a2dp_dev->open_output_stream;
    sprd_adev->default_a2dp.close_output_stream=a2dp_dev->close_output_stream;
    sprd_adev->default_a2dp.dump=a2dp_dev->dump;

    a2dp_dev->common.close = adev_close;
    a2dp_dev->set_mode = adev_set_mode;
    a2dp_dev->set_parameters = adev_set_parameters;
    a2dp_dev->get_parameters = adev_get_parameters;
    a2dp_dev->open_output_stream = adev_open_output_stream;
    a2dp_dev->close_output_stream = adev_close_output_stream;
    a2dp_dev->dump = adev_dump;

    sprd_adev->stream_ctl=stream_ctl;
    sprd_adev->is_support_offload=true;
    sprd_adev->a2dp_suspended=false;
    sprd_adev->bt_sco_enable=false;
    sprd_adev->voip_start=false;
    sprd_adev->a2dp_disconnected=false;
    sprd_adev->mode=AUDIO_MODE_NORMAL;
    pthread_mutex_init(&sprd_adev->lock, NULL);
    sprd_adev->dsp_monitor = dsp_monitor_open();
    ALOGI("adev_open A2dp_hw module success");
    return 0;
}

static struct hw_module_methods_t hal_module_methods = {
    .open = adev_open,
};

struct audio_module HAL_MODULE_INFO_SYM = {
    .common = {
        .tag = HARDWARE_MODULE_TAG,
        .version_major = 1,
        .version_minor = 0,
        .id = AUDIO_HARDWARE_MODULE_ID,
        .name = "A2DP Audio HW HAL",
        .author = "The Android Open Source Project",
        .methods = &hal_module_methods,
    },
};
