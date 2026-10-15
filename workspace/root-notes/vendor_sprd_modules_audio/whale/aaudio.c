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


#define LOG_TAG "audio_hw_aaudio"
#define LOG_NDEBUG 0

#include "audio_hw.h"
#include "audio_control.h"
#include "aaudio.h"
#include "audio_utils/clock.h"

#define MMAP_PERIOD_SIZE (DEFAULT_OUT_SAMPLING_RATE/1000)
#define MMAP_PERIOD_COUNT_MIN 32
#define MMAP_PERIOD_COUNT_MAX 512

#if 0
struct pcm_config pcm_config_mmap_playback = {
    .channels = DEFAULT_CHANNEL_COUNT,
    .rate = 48000,//DEFAULT_OUT_SAMPLING_RATE,
    .period_size = MMAP_PERIOD_SIZE,
    .period_count = MMAP_PERIOD_COUNT_DEFAULT,
    .format = PCM_FORMAT_S16_LE,
    .start_threshold = MMAP_PERIOD_SIZE*8,
    .stop_threshold = INT32_MAX,
    .silence_threshold = 0,
    .silence_size = 0,
    .avail_min = MMAP_PERIOD_SIZE, //1 ms
};

struct pcm_config pcm_config_mmap_capture = {
    .channels = DEFAULT_CHANNEL_COUNT,
    .rate = 48000,//DEFAULT_OUTPUT_SAMPLING_RATE,
    .period_size = MMAP_PERIOD_SIZE,
    .period_count = MMAP_PERIOD_COUNT_DEFAULT,
    .format = PCM_FORMAT_S16_LE,
    .start_threshold = 0,
    .stop_threshold = INT_MAX,
    .silence_threshold = 0,
    .silence_size = 0,
    .avail_min = MMAP_PERIOD_SIZE, //1 ms
};
#define MMAP_PERIOD_COUNT_DEFAULT (MMAP_PERIOD_COUNT_MAX)
#define MIN_CHANNEL_COUNT                1
#define DEFAULT_CHANNEL_COUNT            2
#endif

static int do_mmap_output_standby(void *dev,void * out,UNUSED_ATTR AUDIO_HW_APP_T type)
{
    int usecase = UC_UNKNOWN;
    struct tiny_audio_device *adev=(struct tiny_audio_device *)dev;
    struct tiny_stream_out *mmap_output=(struct tiny_stream_out *)out;
    LOG_I("do_normal_output_standby enter:%p standby:%d",mmap_output->pcm,mmap_output->standby);
    usecase = stream_type_to_usecase(mmap_output->audio_app_type);

    if (false == mmap_output->standby){
        set_mdg_mute(adev->dev_ctl,usecase,true);
    }

    if (NULL != mmap_output->pcm) {
        LOG_I("do_normal_output_standby pcm_close:%p",mmap_output->pcm);
        pcm_close(mmap_output->pcm);
        LOG_I("do_normal_output_standby pcm_close:%p success",mmap_output->pcm);
        mmap_output->pcm = NULL;
    }

    mmap_output->standby=true;
    mmap_output->playback_started = false;

    set_usecase(adev->dev_ctl, usecase, false);
    set_audioparam(adev->dev_ctl,PARAM_USECASE_CHANGE,NULL,false);
    LOG_I("do_normal_output_standby :%p %d exit",out,mmap_output->audio_app_type);
    return 0;
}

int start_mmap_output_stream(struct tiny_stream_out *out)
{
    struct tiny_audio_device *adev = out->dev;
    int ret = 0;
    int usecase=UC_UNKNOWN;

    usecase = stream_type_to_usecase(out->audio_app_type);
    LOG_I("start_output_stream usecase:%x",usecase);
    ret = set_usecase(adev->dev_ctl, usecase, true);
    if(ret < 0) {
        goto error;
    }

    set_mdg_mute(adev->dev_ctl, usecase, false);
    out->standby_fun = do_mmap_output_standby;
    switch_vbc_route(adev->dev_ctl,out->devices);
    select_devices_new(adev->dev_ctl, out->audio_app_type, out->devices, false, false, false, true);

    if (usecase == UC_MMAP_PLAYBACK) {
        if (out->pcm == NULL || !pcm_is_ready(out->pcm)) {
            LOG_E("%s: pcm stream not ready", __func__);
            goto error;
        }
        ret = pcm_start(out->pcm);
        if (ret < 0) {
            LOG_E("%s: MMAP pcm_start failed ret %d", __func__, ret);
            goto error;
        }
    } else {
        LOG_E("the usecase is error for mmap playback!!! usecase: %d", usecase);
        goto error;
    }

    set_audioparam(adev->dev_ctl, PARAM_USECASE_DEVICES_CHANGE, NULL, true);
    return 0;

error:
    set_usecase(adev->dev_ctl, usecase, false);
    set_audioparam(adev->dev_ctl, PARAM_USECASE_CHANGE, NULL, false);
    return -1;
}


static int do_mmap_inputput_standby(void *dev,void * in,UNUSED_ATTR AUDIO_HW_APP_T type)
{
    int usecase = UC_UNKNOWN;
    struct tiny_audio_device *adev=(struct tiny_audio_device *)dev;
    struct tiny_stream_in *mmap_in = (struct tiny_stream_in *)in;
    LOG_I("do_normal_inputput_standby:%p", mmap_in->pcm);
    if (NULL != mmap_in->pcm) {
        LOG_I("do_normal_inputput_standby pcm_close:%p", mmap_in->pcm);
        pcm_close(mmap_in->pcm);
        mmap_in->pcm = NULL;
    }

    mmap_in->standby = true;
    mmap_in->capture_started = false;
    usecase = stream_type_to_usecase(mmap_in->audio_app_type);
    set_usecase(adev->dev_ctl, usecase, false);
    set_audioparam(adev->dev_ctl, PARAM_USECASE_CHANGE, NULL, false);
    LOG_I("do_mmap_inputput_standby exit");
    return 0;
}

int start_mmap_input_stream(struct tiny_stream_in *in)
{
    struct tiny_audio_device *adev = in->dev;
    int ret = 0;
    int usecase = UC_UNKNOWN;
    struct audio_control *control = NULL;

    control = adev->dev_ctl;

    LOG_I("start_input_stream usecase:%x  type:%d",control->usecase,in->audio_app_type);

    usecase = stream_type_to_usecase(in->audio_app_type);
    ret = set_usecase(control, usecase, true);
    if(ret < 0) {
        return -2;
    }

#if 1
    //@TODO: mmap need set source ???
    if (is_usecase_unlock(control, UC_MMAP_RECORD)){
        set_record_source(control, in->source);
    }
#endif

    in->standby_fun = do_mmap_inputput_standby;
    switch_vbc_route(adev->dev_ctl,in->devices);
    select_devices_new(adev->dev_ctl, in->audio_app_type, in->devices, true, false, false, true);

    if (usecase == UC_MMAP_RECORD) {
        if (in->pcm == NULL || !pcm_is_ready(in->pcm)) {
            LOG_E("%s: pcm stream not ready", __func__);
            goto error;
        }
        ret = pcm_start(in->pcm);
        if (ret < 0) {
            LOG_E("%s: MMAP pcm_start failed ret %d", __func__, ret);
            goto error;
        }
    } else {
        LOG_E("the usecase is error for mmap capture !!! usecase: %d", usecase);
        goto error;
    }

    set_audioparam(adev->dev_ctl, PARAM_USECASE_DEVICES_CHANGE, NULL, false);

    return 0;

error:
    set_usecase(control,  usecase, false);
    set_audioparam(adev->dev_ctl, PARAM_USECASE_CHANGE, NULL, false);
    return -1;
}

int platform_get_mmap_data_fd(void *platform __unused, int fe_dev __unused, int dir __unused,
                              int *fd __unused, uint32_t *size __unused)
{
#if defined (SUPPORT_MMAP_IS_EXCLUSIVE_MODE)
//@TODO: codec should be has exclusive device, we not support now
#else
    return -1;
#endif
}

int out_start(const struct audio_stream_out* stream)
{
    struct tiny_stream_out *out = (struct tiny_stream_out *)stream;
    struct tiny_audio_device *adev = out->dev;
    int ret = -ENOSYS;

    LOG_I("%s", __func__);
    pthread_mutex_lock(&adev->lock);
    if (out->audio_app_type == AUDIO_HW_APP_MMAP && !out->standby &&
            !out->playback_started && out->pcm != NULL) {
        ret = start_mmap_output_stream(out);
        if (ret == 0) {
            out->playback_started = true;
        }
    }
    pthread_mutex_unlock(&adev->lock);
    return ret;
}

int out_stop(const struct audio_stream_out* stream)
{
    struct tiny_stream_out *out = (struct tiny_stream_out *)stream;
    struct tiny_audio_device *adev = out->dev;
    int ret = -ENOSYS;

    LOG_I("%s", __func__);
    pthread_mutex_lock(&adev->lock);
    if (out->audio_app_type == AUDIO_HW_APP_MMAP && !out->standby &&
            out->playback_started && out->pcm != NULL) {
        pcm_stop(out->pcm);
        out->playback_started = false;
        ret = 0;
    }
    pthread_mutex_unlock(&adev->lock);
    return ret;
}

static void adjust_mmap_period_count(struct pcm_config *config, int32_t min_size_frames)
{
    int periodCountRequested = (min_size_frames + config->period_size - 1)
                               / config->period_size;
    int periodCount = MMAP_PERIOD_COUNT_MIN;

    LOG_I("%s original config.period_size = %d config.period_count = %d",
          __func__, config->period_size, config->period_count);

    while (periodCount < periodCountRequested && (periodCount * 2) < MMAP_PERIOD_COUNT_MAX) {
        periodCount *= 2;
    }
    config->period_count = periodCount;

    LOG_I("%s requested config.period_count = %d", __func__, config->period_count);
}

int out_create_mmap_buffer(const struct audio_stream_out *stream,
                                  int32_t min_size_frames,
                                  struct audio_mmap_buffer_info *info)
{
    struct tiny_stream_out *out = (struct tiny_stream_out *)stream;
    struct tiny_audio_device *adev = out->dev;
    int ret = 0;
    unsigned int offset1;
    unsigned int frames1=0;
    const char *step = "";
    uint32_t mmap_size;
    uint32_t buffer_size;
    int snd_device = 0;

    LOG_I("%s", __func__);
    pthread_mutex_lock(&adev->lock);

    if (info == NULL || min_size_frames == 0) {
        LOG_E("%s: info = %p, min_size_frames = %d", __func__, info, min_size_frames);
        ret = -EINVAL;
        goto exit;
    }
    if (out->audio_app_type != AUDIO_HW_APP_MMAP || !out->standby) {
        LOG_E("%s: audio_app_type = %d, standby = %d", __func__, out->audio_app_type, out->standby);
        ret = -ENOSYS;
        goto exit;
    }
    dev_ctl_get_out_pcm_config(adev->dev_ctl, AUDIO_HW_APP_MMAP, &snd_device, out->config);

    if (snd_device < 0) {
        LOG_E("%s: Invalid PCM device id(%d) for the audio_app_type(%d)",
              __func__, snd_device, out->audio_app_type);
        ret = -EINVAL;
        goto exit;
    }

    adjust_mmap_period_count(out->config, min_size_frames);

    LOG_V("%s: Opening PCM device card_id(%d) device_id(%d), channels %d",
          __func__, adev->dev_ctl->cards.s_tinycard, snd_device, out->config->channels);
    out->pcm = pcm_open(adev->dev_ctl->cards.s_tinycard, snd_device,
                        (PCM_OUT | PCM_MMAP | PCM_NOIRQ | PCM_MONOTONIC), out->config);
    if (out->pcm == NULL || !pcm_is_ready(out->pcm)) {
        step = "open";
        ret = -ENODEV;
        goto exit;
    }
    ret = pcm_mmap_begin(out->pcm, &info->shared_memory_address, &offset1, &frames1);
    if (ret < 0)  {
        step = "begin";
        goto exit;
    }
    info->buffer_size_frames = pcm_get_buffer_size(out->pcm);
    buffer_size = pcm_frames_to_bytes(out->pcm, info->buffer_size_frames);
    info->burst_size_frames = out->config->period_size;
    ret = platform_get_mmap_data_fd((void *)adev,
                                    snd_device, 0 /*playback*/,
                                    &info->shared_memory_fd,
                                    &mmap_size);
    if (ret < 0) {
        // Fall back to non exclusive mode
        info->shared_memory_fd = pcm_get_poll_fd(out->pcm);
    } else {
        if (mmap_size < buffer_size) {
            step = "mmap";
            goto exit;
        }
        // FIXME: indicate exclusive mode support by returning a negative buffer size
        info->buffer_size_frames *= -1;
    }
    memset(info->shared_memory_address, 0, buffer_size);

    if (out->config->period_size != MMAP_PERIOD_SIZE) {
        LOG_E("out pcm period_size is invaild! period_size :%d", out->config->period_size);
        out->config->period_size = MMAP_PERIOD_SIZE;
    }
    ret = pcm_mmap_commit(out->pcm, 0, MMAP_PERIOD_SIZE);
    if (ret < 0) {
        step = "commit";
        goto exit;
    }

    out->standby = false;
    ret = 0;

    LOG_I("%s: got mmap buffer address %p info->buffer_size_frames %d",
          __func__, info->shared_memory_address, info->buffer_size_frames);

exit:
    if (ret != 0) {
        if (out->pcm == NULL) {
            LOG_E("%s: %s - %d", __func__, step, ret);
        } else {
            LOG_E("%s: %s %s", __func__, step, pcm_get_error(out->pcm));
            pcm_close(out->pcm);
            out->pcm = NULL;
        }
    }
    pthread_mutex_unlock(&adev->lock);
    return ret;
}

int out_get_mmap_position(const struct audio_stream_out *stream,
                                  struct audio_mmap_position *position)
{
    struct tiny_stream_out *out = (struct tiny_stream_out *)stream;
    LOG_I("%s", __func__);
    if (position == NULL) {
        return -EINVAL;
    }
    if (out->audio_app_type != AUDIO_HW_APP_MMAP) {
        return -ENOSYS;
    }
    if (out->pcm == NULL) {
        return -ENOSYS;
    }

    struct timespec ts = { 0, 0 };
    int ret = pcm_mmap_get_hw_ptr(out->pcm, (unsigned int *)&position->position_frames, &ts);
    if (ret < 0) {
        LOG_E("%s: %s", __func__, pcm_get_error(out->pcm));
        return ret;
    }
    position->time_nanoseconds = audio_utils_ns_from_timespec(&ts);
    return 0;
}

int in_start(const struct audio_stream_in* stream)
{
    struct tiny_stream_in *in = (struct tiny_stream_in *)stream;
    struct tiny_audio_device *adev = in->dev;
    int ret = -ENOSYS;

    LOG_I("%s in %p", __func__, in);
    pthread_mutex_lock(&adev->lock);
    if (in->audio_app_type == AUDIO_HW_APP_MMAP_RECORD && !in->standby &&
            !in->capture_started && in->pcm != NULL) {
        if (!in->capture_started) {
            ret = start_mmap_input_stream(in);
            if (ret == 0) {
                in->capture_started = true;
            }
        }
    }
    pthread_mutex_unlock(&adev->lock);
    return ret;
}

int in_stop(const struct audio_stream_in* stream)
{
    struct tiny_stream_in *in = (struct tiny_stream_in *)stream;
    struct tiny_audio_device *adev = in->dev;

    int ret = -ENOSYS;
    LOG_I("%s", __func__);
    pthread_mutex_lock(&adev->lock);
    if (in->audio_app_type == AUDIO_HW_APP_MMAP_RECORD && !in->standby &&
            in->capture_started && in->pcm != NULL) {
        pcm_stop(in->pcm);
        in->capture_started = false;
        ret = 0;
    }
    pthread_mutex_unlock(&adev->lock);
    return ret;
}

int in_create_mmap_buffer(const struct audio_stream_in *stream,
                                  int32_t min_size_frames,
                                  struct audio_mmap_buffer_info *info)
{
    struct tiny_stream_in *in = (struct tiny_stream_in *)stream;
    struct tiny_audio_device *adev = in->dev;
    int ret = 0;
    unsigned int offset1;
    unsigned int frames1=0;
    const char *step = "";
    uint32_t mmap_size=0;
    uint32_t buffer_size=0;
    int snd_device = 0;

    pthread_mutex_lock(&adev->lock);
    LOG_I("%s in %p", __func__, in);

    if (info == NULL || min_size_frames == 0) {
        LOG_E("%s invalid argument info %p min_size_frames %d", __func__, info, min_size_frames);
        ret = -EINVAL;
        goto exit;
    }
    if (in->audio_app_type != AUDIO_HW_APP_MMAP_RECORD || !in->standby) {
        LOG_E("%s: audio_app_type = %d, standby = %d", __func__, in->audio_app_type, in->standby);
        LOG_V("%s in %p", __func__, in);
        ret = -ENOSYS;
        goto exit;
    }

    dev_ctl_get_in_pcm_config(adev->dev_ctl, AUDIO_HW_APP_MMAP_RECORD, &snd_device, in->config);
    if (snd_device < 0) {
        LOG_E("%s: Invalid PCM device id(%d) for the audio_app_type(%d)",
              __func__, snd_device, in->audio_app_type);
        ret = -EINVAL;
        goto exit;
    }

    adjust_mmap_period_count(in->config, min_size_frames);

    LOG_I("%s: Opening PCM device card_id(%d) device_id(%d), channels %d",
          __func__, adev->dev_ctl->cards.s_tinycard, snd_device, in->config->channels);
    in->pcm = pcm_open(adev->dev_ctl->cards.s_tinycard, snd_device,
                        (PCM_IN | PCM_MMAP | PCM_NOIRQ | PCM_MONOTONIC), in->config);
    if (in->pcm == NULL || !pcm_is_ready(in->pcm)) {
        step = "open";
        ret = -ENODEV;
        goto exit;
    }

    frames1=pcm_get_buffer_size(in->pcm);

    ret = pcm_mmap_begin(in->pcm, &info->shared_memory_address, &offset1, &frames1);
    if (ret < 0)  {
        step = "begin";
        goto exit;
    }
    info->buffer_size_frames = pcm_get_buffer_size(in->pcm);
    buffer_size = pcm_frames_to_bytes(in->pcm, info->buffer_size_frames);
    info->burst_size_frames = in->config->period_size;
    #if 0
    ret = platform_get_mmap_data_fd(adev,
                                    in->pcm_device_id, 1 /*capture*/,
                                    &info->shared_memory_fd,
                                    &mmap_size);
    #endif
    if (ret < 0) {
        // Fall back to non exclusive mode
        info->shared_memory_fd = pcm_get_poll_fd(in->pcm);
    } else {
        if (mmap_size < buffer_size) {
            step = "mmap";
            goto exit;
        }
        // FIXME: indicate exclusive mode support by returning a negative buffer size
        info->buffer_size_frames *= -1;
    }

    memset(info->shared_memory_address, 0, buffer_size);

    if (in->config->period_size != MMAP_PERIOD_SIZE) {
        LOG_E("in pcm period_size is invaild! period_size :%d", in->config->period_size);
        in->config->period_size = MMAP_PERIOD_SIZE;
    }

    ret = pcm_mmap_commit(in->pcm, 0, MMAP_PERIOD_SIZE);
    if (ret < 0) {
        step = "commit";
        goto exit;
    }

    in->standby = false;
    ret = 0;

    LOG_I("%s: got mmap buffer address %p info->buffer_size_frames %d",
          __func__, info->shared_memory_address, info->buffer_size_frames);

exit:
    if (ret != 0) {
        if (in->pcm == NULL) {
            LOG_E("%s: %s - %d", __func__, step, ret);
        } else {
            LOG_E("%s: %s %s", __func__, step, pcm_get_error(in->pcm));
            pcm_close(in->pcm);
            in->pcm = NULL;
        }
    }
    pthread_mutex_unlock(&adev->lock);
    return ret;
}

int in_get_mmap_position(const struct audio_stream_in *stream,
                                  struct audio_mmap_position *position)
{
    struct tiny_stream_in *in = (struct tiny_stream_in *)stream;
    LOG_V("%s", __func__);
    if (position == NULL) {
        return -EINVAL;
    }
    if (in->audio_app_type != AUDIO_HW_APP_MMAP_RECORD) {
        return -ENOSYS;
    }
    if (in->pcm == NULL) {
        return -ENOSYS;
    }
    struct timespec ts = { 0, 0 };
    int ret = pcm_mmap_get_hw_ptr(in->pcm, (unsigned int *)&position->position_frames, &ts);
    if (ret < 0) {
        LOG_E("%s: %s", __func__, pcm_get_error(in->pcm));
        return ret;
    }
    position->time_nanoseconds = audio_utils_ns_from_timespec(&ts);
    return 0;
}
