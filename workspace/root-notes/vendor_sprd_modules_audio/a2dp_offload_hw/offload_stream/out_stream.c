#define LOG_TAG "audio_hw_a2dp_stream"
#include "offload_stream.h"
#include <sound/asound.h>
#include "sound/compress_params.h"
#define DEFAULT_FRAGMENT_SIZE    0x8000
#define DEFAULT_FRAGMENT_COUNTt    0x20
#define A2DP_PCM_FILE  "/vendor/etc/audio_pcm.xml"
#define A2DP_ROUTE_PATH "/vendor/etc/audio_route.xml"

#define A2DP_SBC_MODE_STR    "ch_mode"
#define A2DP_SBC_BLOCKS_STR    "blocks"
#define A2DP_SBC_SUBBANDS_STR    "SubBands"
#define A2DP_SBC_SAMPLINGFREQ_STR    "SamplingFreq"
#define A2DP_SBC_ALLOCMETHOD_STR    "AllocMethod"
#define A2DP_SBC_MINBITPOOL_STR    "Min_Bitpool"
#define A2DP_SBC_MAXBITPOOL_STR    "Max_Bitpool"

/* for Codec Specific Information Element */
#define A2DP_SBC_IE_SAMP_FREQ_MSK 0xF0 /* b7-b4 sampling frequency */
#define A2DP_SBC_IE_SAMP_FREQ_16 0x80  /* b7:16  kHz */
#define A2DP_SBC_IE_SAMP_FREQ_32 0x40  /* b6:32  kHz */
#define A2DP_SBC_IE_SAMP_FREQ_44 0x20  /* b5:44.1kHz */
#define A2DP_SBC_IE_SAMP_FREQ_48 0x10  /* b4:48  kHz */

struct pcm_config pcm_config_offload_ctl = {
    .channels = 2,
    .rate = 48000,
    .period_size = 320,
    .period_count = 8,
    .format = PCM_FORMAT_S16_LE,
};

struct pcm_config pcm_config_offload_mixer_stream = {
    .channels = 2,
    .rate = 48000,
    .period_size = 1280,
    .period_count = 2,
    .format = PCM_FORMAT_S16_LE,
};

static int do_normal_output_standby(UNUSED_ATTR void *dev, void *out_p,UNUSED_ATTR AUDIO_HW_APP_T type) {
    struct sprd_out_stream *normal_out = (struct sprd_out_stream *)out_p;
    struct sprdout_ctl *stream_ctl = normal_out->stream_ctl;

    ALOGI("do_normal_output_standby enter:%p standby:%d", normal_out->pcm, normal_out->standby);
    if(normal_out->standby) {
        return -1;
    }
     if (NULL!=normal_out->pcm) {
        ALOGI("do_normal_output_standby pcm_close:%p", normal_out->pcm);
        pcm_close(normal_out->pcm);
        ALOGI("do_normal_output_standby pcm_close:%p success", normal_out->pcm);
        normal_out->pcm = NULL;
    }
    normal_out->standby = true;

    if (normal_out->audio_app_type == AUDIO_HW_APP_A2DP_OFFLOAD_MIXER){
        set_usecase(stream_ctl , UC_BT_OFFLOAD_MIXER, false);
    }

    ALOGI("do_normal_output_standby :%p %d exit", normal_out, normal_out->audio_app_type);
    return 0;
}

static  int start_output_stream(void *stream) {
    struct sprd_out_stream *out = (struct sprd_out_stream *)stream;
    struct sprdout_ctl *stream_ctl = out->stream_ctl;
    struct stream_param *stream_para = &stream_ctl->stream_para;
    int ret = 0;
    ALOGI("start_output_stream");

    if(NULL != out->pcm) {
        ALOGE("start_output_stream pcm err:%s",pcm_get_error(out->pcm));
        pcm_close(out->pcm);
        out->pcm = NULL;
    }

   if (out->audio_app_type == AUDIO_HW_APP_A2DP_OFFLOAD_MIXER){
       ret = set_usecase(stream_ctl , UC_BT_OFFLOAD_MIXER, true);
   }else {
       ret=-1;
       ALOGE("%s line:%d failed",__func__,__LINE__);
    }

    if(ret < 0) {
        goto error;
    }

    ALOGI("start_output_stream pcm_open start config:%p, open  device:%d"
          , out->config, stream_para->mixer.device);
    out->pcm = pcm_open(stream_para->mixer.card,
                        stream_para->mixer.device ,
                        PCM_OUT | PCM_MMAP | PCM_NOIRQ | PCM_MONOTONIC,
                        out->config);
    ALOGI("start_output_stream pcm_open end format %d", out->config->format);

    if (!pcm_is_ready(out->pcm)) {
        ALOGE("%s:cannot open pcm : %s", __func__,
              pcm_get_error(out->pcm));
        goto error;
    }

    ALOGI("start_output_stream pcm_open pcm:%p rate:%d", out->pcm
          , out->config->rate);

    out->standby_fun = do_normal_output_standby;
    return 0;

error:

    if(out->pcm) {
        pcm_close(out->pcm);
        out->pcm = NULL;
    }
    if (out->audio_app_type == AUDIO_HW_APP_A2DP_OFFLOAD_MIXER){
        ret = set_usecase(stream_ctl , UC_BT_OFFLOAD_MIXER, false);
    }
    return -1;
}

int out_offload_set_volume( struct sprdout_ctl *stream_ctl, float left, float right) {
    int mdg_arr[2] = {0};
    int max = 0;
    int ret = 0;

    if(NULL == stream_ctl->offload_dg) {
        stream_ctl->offload_dg = mixer_get_ctl_by_name(stream_ctl->mixer, "OFFLOAD DG Set");
    }

    if(stream_ctl->offload_dg) {
        max = mixer_ctl_get_range_max(stream_ctl->offload_dg);
        mdg_arr[0] = max * left;
        mdg_arr[1] = max * right;
        ALOGI("out_offload_set_volume left=%f,right=%f, max=%d", left, right, max);
        ret = mixer_ctl_set_array(stream_ctl->offload_dg, (void *)mdg_arr, 2);
    } else {
        ALOGE("out_offload_set_volume cannot get offload_dg ctrl");
    }

    return ret;
}

static int out_set_parameters(struct audio_stream *stream, const char *kvpairs) {
    struct sprd_out_stream *out = (struct sprd_out_stream *)stream;
    struct str_parms *parms;
    char value[32];
    int ret, val = 0;
    parms = str_parms_create_str(kvpairs);

    ALOGI("out_set_parameters type:%d:%s", out->audio_app_type, kvpairs);
    ret =
        str_parms_get_str(parms, AUDIO_PARAMETER_STREAM_ROUTING, value,
                          sizeof(value));
    if (ret >= 0) {
        val = atoi(value);
    }

    if (out->audio_app_type == AUDIO_HW_APP_A2DP_OFFLOAD) {
        audio_get_compress_metadata(&out->stream, parms);
    }
    str_parms_destroy(parms);
    return ret;
}

static uint32_t out_get_latency(const struct audio_stream_out *stream) {
    struct sprd_out_stream *out = (struct sprd_out_stream *)stream;
    if(out->audio_app_type == AUDIO_HW_APP_A2DP_OFFLOAD) {
        return 100;
    } else {
        uint32_t temp_latency = 0;

        temp_latency = (out->config->period_size * out->config->period_count * 1000) /
                       out->config->rate;
        ALOGV("out_get_latency a2dp mixer  latency=%d period_size=%d,period_count=%d,rate=%d",
              temp_latency, out->config->period_size, out->config->period_count, out->config->rate);

        return temp_latency;

    }
    return 0;
}

static int out_set_volume(struct audio_stream_out *stream, float left,
                          float right) {
    struct sprd_out_stream *out = (struct sprd_out_stream *)stream;
    struct sprdout_ctl *stream_ctl = out->stream_ctl;
    int ret = -ENOSYS;

    if (out->audio_app_type == AUDIO_HW_APP_A2DP_OFFLOAD) {
        pthread_mutex_lock(&stream_ctl->lock);
        pthread_mutex_lock(&out->lock);
        ret = out_offload_set_volume(stream_ctl, left, right);
        pthread_mutex_unlock(&out->lock);
        stream_ctl->offload_volume_index = left;
        pthread_mutex_unlock(&stream_ctl->lock);
    }
    return ret;
}

static uint32_t out_get_sample_rate(const struct audio_stream *stream) {
    struct sprd_out_stream *out = (struct sprd_out_stream *)stream;

    if(out->audio_app_type == AUDIO_HW_APP_A2DP_OFFLOAD) {
        return out->offload_samplerate;
    } else {
        return out->config->rate;
    }
}

static size_t out_get_buffer_size(const struct audio_stream *stream) {
    struct sprd_out_stream *out = (struct sprd_out_stream *)stream;
    if(out->audio_app_type == AUDIO_HW_APP_A2DP_OFFLOAD) {
        return out->compress_config.fragment_size;
    } else {
        size_t size = out->config->period_size ;
        size = ((size + 15) / 16) * 16;
        return size * audio_stream_out_frame_size(&out->stream);
    }
}

static audio_channel_mask_t out_get_channels(const struct audio_stream *stream) {
    struct sprd_out_stream *out = (struct sprd_out_stream *)stream;

    if(out->audio_app_type == AUDIO_HW_APP_A2DP_OFFLOAD) {
        ALOGV("out_get_channels offload:%d",out->offload_channel_mask);
        return out->offload_channel_mask;
    } else {
        ALOGV("out_get_channels channels:%d",out->config->channels);
        if(1 == out->config->channels) {
            return AUDIO_CHANNEL_OUT_MONO;
        } else if(2 == out->config->channels) {
            return AUDIO_CHANNEL_OUT_STEREO;
        } else {
            return AUDIO_CHANNEL_NONE;
        }
    }
}

static audio_format_t out_get_format(const struct audio_stream *stream) {
    struct sprd_out_stream *out = (struct sprd_out_stream *)stream;

    if(out->audio_app_type == AUDIO_HW_APP_A2DP_OFFLOAD) {
        return out->offload_format;
    } else {
        return AUDIO_FORMAT_PCM_16_BIT;
    }
}

bool is_sprd_output_active(struct audio_stream_out *stream){
    struct sprd_out_stream *out = (struct sprd_out_stream *)stream;
    if(NULL==out){
        return false;
    }
    return !out->standby;
}
static ssize_t out_write(struct audio_stream_out *stream, const void *buffer,
                         size_t bytes) {
    int ret = 0;
    struct sprd_out_stream *out = (struct sprd_out_stream *)stream;
    struct sprdout_ctl *stream_ctl = (struct sprdout_ctl *)out->stream_ctl;
    int frame_size = 0;

    pthread_mutex_lock(&stream_ctl->lock);
    pthread_mutex_lock(&out->lock);
    frame_size = audio_stream_out_frame_size(&out->stream);
    out->written += bytes / frame_size;
    if (true == out->standby) {
        ALOGI("sprd_out_write start audio_app_type:%d",out->audio_app_type);
        if(out->audio_app_type == AUDIO_HW_APP_A2DP_OFFLOAD){
            ret = audio_start_compress_output(&out->stream);
        } else {
            ret = start_output_stream(&out->stream);
        }

        if (ret != 0) {
            pthread_mutex_unlock(&stream_ctl->lock);
            goto exit;
        }
        out_offload_set_volume(stream_ctl, stream_ctl->offload_volume_index, stream_ctl->offload_volume_index);;
        out->standby = false;
        ALOGI("sprd_out_write start apptype:%d success",out->audio_app_type);
    }
    pthread_mutex_unlock(&stream_ctl->lock);

    if(out->audio_app_type == AUDIO_HW_APP_A2DP_OFFLOAD) {
        ret = out_write_compress(stream, buffer, bytes);
    } else {
        ret = pcm_mmap_write(out->pcm, buffer, bytes);
    }

exit:
    if((ret < 0) && (NULL != out->standby_fun)) {
        ALOGW("sprd_out_write failed %p app type:%d", out, out->audio_app_type);
        out->standby_fun(out->stream_ctl, out, out->audio_app_type);
    }

    pthread_mutex_unlock(&out->lock);
    if(ret < 0 ) {
        ALOGE("sprd_out_write error");
        if (out->pcm) {
            ALOGE("sprd_out_write warning:%d, (%s)", ret,
                  pcm_get_error(out->pcm));
        }
        usleep(20 * 1000);
    }

    if(out->audio_app_type == AUDIO_HW_APP_A2DP_OFFLOAD){
        return ret;
    }else{
        return bytes;
    }
}

static int out_standby(struct audio_stream *stream) {
    struct sprd_out_stream *out = (struct sprd_out_stream *)stream;
    struct sprdout_ctl *stream_ctl = (struct sprdout_ctl *)out->stream_ctl;
    ALOGI("out_standby %p %d",out->standby_fun,out->standby);
    //    pthread_mutex_lock(&stream_ctl->lock);
    pthread_mutex_lock(&out->lock);
    if(NULL != out->standby_fun) {
        if(out->standby == false) {
            out->standby_fun(stream_ctl, out, out->audio_app_type);
        }
        out->standby=true;
    }
    pthread_mutex_unlock(&out->lock);
    //    pthread_mutex_unlock(&stream_ctl->lock);
    return 0;
}

static int out_get_render_position(const struct audio_stream_out *stream,
                                   uint32_t *dsp_frames) {
    struct sprd_out_stream *out = (struct sprd_out_stream *)stream;

    if (dsp_frames == NULL) {
        return -EINVAL;
    }

    if (out->audio_app_type == AUDIO_HW_APP_A2DP_OFFLOAD) {
        pthread_mutex_lock(&out->lock);
        if (out->compress != NULL) {
            unsigned long frames = 0;
            compress_get_tstamp(out->compress, &frames,
                                &out->offload_samplerate);
            *dsp_frames = (uint32_t)frames;
        }
        pthread_mutex_unlock(&out->lock);
        return 0;
    }
    return -EINVAL;
}

static int out_get_presentation_position(const struct audio_stream_out *stream,
        uint64_t *frames, struct timespec *timestamp) {
    struct sprd_out_stream *out = (struct sprd_out_stream *)stream;
    struct pcm *pcm = NULL;
    int ret = -1;
    unsigned long dsp_frames;
    pthread_mutex_lock(&out->lock);
    pcm = out->pcm;

    if(out->audio_app_type == AUDIO_HW_APP_A2DP_OFFLOAD) {
        if (out->compress != NULL) {
            ret=compress_get_tstamp(out->compress, &dsp_frames,
                                &out->offload_samplerate);
            if(ret!=0){
                ALOGE("%s compress_get_tstamp failed:%s",
                    __func__,compress_get_error(out->compress));
            }
            *frames = dsp_frames;
            ret = 0;
            clock_gettime(CLOCK_MONOTONIC, timestamp);
            out->dsp_frames=dsp_frames;
        }else{
            *frames = out->dsp_frames;
            ret = 0;
            clock_gettime(CLOCK_MONOTONIC, timestamp);
            ALOGI("out_get_presentation_position %d %d use pre dsp_frames:%d",
                 (int)timestamp->tv_sec,(int)(timestamp->tv_nsec/1000),(int)out->dsp_frames);
        }
    } else {
        if (pcm) {
            unsigned int avail;
            if (pcm_get_htimestamp(pcm, &avail, timestamp) == 0) {
                size_t kernel_buffer_size = out->config->period_size *
                                            out->config->period_count;
                int64_t signed_frames = out->written - kernel_buffer_size + avail;
                if (signed_frames >= 0) {
                    *frames = signed_frames;
                    ret = 0;
                }
            }
        }
    }
    pthread_mutex_unlock(&out->lock);
    //ALOGE("out_get_presentation_position ret %d",ret);
    return ret;
}

int sprd_open_output_stream(void *ctl,
                                audio_devices_t devices,
                                audio_output_flags_t flags,
                                struct audio_config *config,
                                struct audio_stream_out **stream_out) {
    struct sprdout_ctl *stream_ctl = (struct sprdout_ctl *)ctl;
    struct sprd_out_stream *out = NULL;
    struct stream_param *stream_para = &stream_ctl->stream_para;

    int ret=0;

    ALOGI("%s, devices = %d flags:0x%x rate:%d", __func__, devices, flags, config->sample_rate);

    out =
        (struct sprd_out_stream *)calloc(1, sizeof(struct sprd_out_stream));
    if (!out) {
        ALOGE("sprd_open_output_stream calloc fail");
        return -ENOMEM;
    }
    ALOGI("sprd_open_output_stream out:%p", out);

    memset(out, 0, sizeof(struct sprd_out_stream));
    out->config = (struct pcm_config *) malloc(sizeof(struct pcm_config));
    if(NULL==out->config) {
        ALOGE(" sprd_open_output_stream malloc failed");
        goto err_open;
    }
    pthread_mutex_init(&out->lock, NULL);

    if (flags & AUDIO_OUTPUT_FLAG_COMPRESS_OFFLOAD) {
        /* check offload supported information */
        if (config->offload_info.version != 1 ||
                config->offload_info.size != AUDIO_INFO_INITIALIZER.size) {
            ALOGE("%s: offload information is not supported ", __func__);
            ret = -EINVAL;
            goto err_open;
        }
        if (!audio_is_offload_support_format(config->offload_info.format)) {
            ALOGE("%s: offload audio format(%d) is not supported ",
                  __func__, config->offload_info.format);
            ret = -EINVAL;
            goto err_open;
        }
        /*  codec type and parameters requested */
        out->compress_config.codec = (struct snd_codec *)calloc(1,
                                     sizeof(struct snd_codec));
        out->audio_app_type = AUDIO_HW_APP_A2DP_OFFLOAD;

        out->offload_format = config->format;
        out->offload_samplerate = config->sample_rate;
        out->offload_channel_mask = config->channel_mask;
        init_sprd_offload_stream(out);

        out->compress_config.codec->id =
            audio_get_offload_codec_id(config->offload_info.format);
        out->compress_config.fragment_size = stream_para->sprd_out_stream.offload_fragement_size;
        out->compress_config.fragments = stream_para->sprd_out_stream.offload_fragements;
        out->compress_config.codec->sample_rate =
            compress_get_alsa_rate(config->offload_info.sample_rate);
        out->compress_config.codec->bit_rate =
            config->offload_info.bit_rate;
        out->compress_config.codec->ch_in =
            popcount(config->channel_mask);
        out->compress_config.codec->ch_out = out->compress_config.codec->ch_in;
        ALOGI("compress_config fragment_size:0x%x fragments:0x%x",
              out->compress_config.fragment_size, out->compress_config.fragments);

        if (flags & AUDIO_OUTPUT_FLAG_NON_BLOCKING) {
            out->is_offload_nonblocking = 1;
        }
        out->is_offload_need_set_metadata = 1;
        audio_offload_create_thread(&out->stream);

    } else {
        ALOGI("pcm config:%d %d  %d", stream_para->mixer.config.rate, stream_para->mixer.card, stream_para->mixer.device);
        memcpy(out->config, &stream_para->mixer.config, sizeof(struct pcm_config));
        out->audio_app_type = AUDIO_HW_APP_A2DP_OFFLOAD_MIXER;
    }
    out->stream.common.get_sample_rate = out_get_sample_rate;
    out->stream.common.get_buffer_size = out_get_buffer_size;
    out->stream.common.get_channels = out_get_channels;
    out->stream.common.get_format = out_get_format;
    out->stream.common.standby = out_standby;
    out->stream.common.set_parameters = out_set_parameters;
    out->stream.get_latency = out_get_latency;
    out->stream.set_volume = out_set_volume;
    out->stream.write = out_write;
    out->stream.get_render_position = out_get_render_position;
    out->stream.get_presentation_position = out_get_presentation_position;

    out->stream_ctl = stream_ctl;
    out->standby = true;
    out->flags = flags;
    out->dsp_frames=0;
    *stream_out = &out->stream;

    ALOGI("sprd_open_output_stream Successful audio_app_type:%d out:%p",
        out->audio_app_type, out);
    return 0;

err_open:
    ALOGE("Error sprd_open_output_stream");
    if(out->config) {
        free(out->config);
        out->config = NULL;
    }
    free(out);
    return ret;
}

int sprd_close_output_stream(struct audio_stream_out *stream) {
    struct sprd_out_stream *out = (struct sprd_out_stream *)stream;
    ALOGI("sprd_close_output_stream enter");
    if((NULL != out) && (NULL != out->stream.common.standby)) {
        out->stream.common.standby(&out->stream.common);
    }

    pthread_mutex_lock(&out->lock);
    if(out->config != NULL) {
        free(out->config);
        out->config = NULL;
    }
    pthread_mutex_unlock(&out->lock);

    if (out->audio_app_type == AUDIO_HW_APP_A2DP_OFFLOAD){
        audio_offload_destroy_thread(&out->stream);
        if(out->pcm != NULL) {
            pcm_close(out->pcm);
            out->pcm = NULL;
        }

        if (out->compress_config.codec != NULL) {
            free(out->compress_config.codec);
        }
    }
    free(out);
    ALOGI("sprd_close_output_stream exit");
    return 0;
}

static int open_mixer(void *ctl) {
    struct sprdout_ctl *stream_ctl = (struct sprdout_ctl *)ctl ;
    struct stream_param *stream_para = &stream_ctl->stream_para;

    if(stream_para->sprd_out_stream.card >= 0) {
        ALOGI("open_mixer card:%d", stream_para->sprd_out_stream.card);
        stream_ctl->mixer = mixer_open(stream_para->sprd_out_stream.card);
        if (!stream_ctl->mixer) {
            ALOGE("open_mixer Unable to open the mixer, aborting. card:%d", stream_para->sprd_out_stream.card);
        }

        stream_ctl->agdsp_sleep_status= false;

        stream_ctl->dsp_sleep_ctl= mixer_get_ctl_by_name(stream_ctl->mixer, "agdsp_access_a2dp_en");
        stream_ctl->bt_sbc_ctl= mixer_get_ctl_by_name(stream_ctl->mixer, "SBC_PARAS");
        if(NULL==stream_ctl->bt_sbc_ctl){
            ALOGE("open_mixer open SBC_PARAS failed");
            return -1;
        }
        return 0;
    }
    return -1;
}

bool check_sbc_paramter(void *ctl,char *kvpairs){
    struct sprdout_ctl *stream_ctl = (struct sprdout_ctl *)ctl;
    struct str_parms *parms;
    char value[32];
    int ret=-1;

    parms = str_parms_create_str(kvpairs);

    ALOGI("check_sbc_paramter:%s",kvpairs);

    pthread_mutex_lock(&stream_ctl->lock);

    ret = str_parms_get_str(parms, A2DP_SBC_MODE_STR, value,sizeof(value));
    if (ret >= 0) {
        stream_ctl->sbc_param.SBCENC_Mode=strtoul(value,NULL,0);
    }

    ret = str_parms_get_str(parms, A2DP_SBC_BLOCKS_STR, value,sizeof(value));
    if (ret >= 0) {
        stream_ctl->sbc_param.SBCENC_Blocks=strtoul(value,NULL,0);
    }

    ret = str_parms_get_str(parms, A2DP_SBC_SUBBANDS_STR, value,sizeof(value));
    if (ret >= 0) {
        stream_ctl->sbc_param.SBCENC_SubBands=strtoul(value,NULL,0);
    }

    ret = str_parms_get_str(parms, A2DP_SBC_SAMPLINGFREQ_STR, value,sizeof(value));
    if (ret >= 0) {
        stream_ctl->sbc_param.SBCENC_SamplingFreq=strtoul(value,NULL,0);
    }

    ret = str_parms_get_str(parms, A2DP_SBC_ALLOCMETHOD_STR, value,sizeof(value));
    if (ret >= 0) {
        stream_ctl->sbc_param.SBCENC_AllocMethod=strtoul(value,NULL,0);
    }

    ret = str_parms_get_str(parms, A2DP_SBC_MINBITPOOL_STR, value,sizeof(value));
    if (ret >= 0) {
        stream_ctl->sbc_param.SBCENC_Min_Bitpool=strtoul(value,NULL,0);
    }

    ret = str_parms_get_str(parms, A2DP_SBC_MAXBITPOOL_STR, value,sizeof(value));
    if (ret >= 0) {
        stream_ctl->sbc_param.SBCENC_Max_Bitpool=strtoul(value,NULL,0);
    }
    pthread_mutex_unlock(&stream_ctl->lock);

    str_parms_destroy(parms);

    ALOGI("Sbc param Mode:%d Blocks:%d SubBands:%d SamplingFreq:%d AllocMethod:%d Min_Bitpool:%d Max_Bitpool:%d",
        stream_ctl->sbc_param.SBCENC_Mode,
        stream_ctl->sbc_param.SBCENC_Blocks,
        stream_ctl->sbc_param.SBCENC_SubBands,
        stream_ctl->sbc_param.SBCENC_SamplingFreq,
        stream_ctl->sbc_param.SBCENC_AllocMethod,
        stream_ctl->sbc_param.SBCENC_Min_Bitpool,
        stream_ctl->sbc_param.SBCENC_Max_Bitpool);
    if((stream_ctl->sbc_param.SBCENC_SamplingFreq&
        (A2DP_SBC_IE_SAMP_FREQ_44|A2DP_SBC_IE_SAMP_FREQ_48))){
        ALOGI("check_sbc_paramter true");
        return true;
    }else{
        ALOGI("check_sbc_paramter false");
        return false;
    }
}

void *stream_ctl_init(void) {
    struct sprdout_ctl *stream_ctl = NULL;

    stream_ctl =
        (struct sprdout_ctl *)calloc(1, sizeof(struct sprdout_ctl));
    if (!stream_ctl) {
        ALOGE("stream_ctl_init calloc fail, size:%zd",
              sizeof(struct sprd_out_stream));
        return NULL;
    }
    ALOGI("stream_ctl_init dev:%p", stream_ctl);

    memset(stream_ctl, 0, sizeof(struct sprdout_ctl));
    pthread_mutex_init(&stream_ctl->lock, NULL);
    stream_ctl->mixer = NULL;
    stream_ctl->offload_dg = NULL;
    stream_ctl->offload_volume_index = 0.0;

    if(parse_offload_config(&stream_ctl->stream_para,A2DP_PCM_FILE)<0){
        goto error;
    }

    ALOGI("mixer %d %d %d %d %d %d",
          stream_ctl->stream_para.mixer.card,
          stream_ctl->stream_para.mixer.device,
          stream_ctl->stream_para.mixer.config.period_size,
          stream_ctl->stream_para.mixer.config.rate,
          stream_ctl->stream_para.mixer.config.channels,
          stream_ctl->stream_para.mixer.config.period_count);

    ALOGI("sprd_out_stream %d %d %d %d",
          stream_ctl->stream_para.sprd_out_stream.card,
          stream_ctl->stream_para.sprd_out_stream.device,
          stream_ctl->stream_para.sprd_out_stream.offload_fragement_size,
          stream_ctl->stream_para.sprd_out_stream.offload_fragements);

    open_mixer(stream_ctl);
    parse_a2dp_route(&stream_ctl->route,stream_ctl->mixer,A2DP_ROUTE_PATH);

    return stream_ctl;

error:
    if(stream_ctl != NULL) {
        free(stream_ctl);
        stream_ctl = NULL;
    }
    ALOGI("stream_ctl_init:%p", stream_ctl);

    return stream_ctl;
}

void  stream_ctl_deinit(void *dev) {
    struct sprdout_ctl *stream_ctl = (struct sprdout_ctl *)dev;

    if(NULL!=stream_ctl->mixer){
        mixer_close(stream_ctl->mixer);
        stream_ctl->mixer=NULL;
    }
    free_a2dp_route(&stream_ctl->route);
    free(stream_ctl);
    ALOGI("stream_ctl_deinit:%p", stream_ctl);
}
