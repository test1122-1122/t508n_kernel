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
#define LOG_TAG "audio_hw_primary"

#define LOG_NDEBUG 0
#include "aud_proc.h"

#include "audio_hw.h"
#include "audio_control.h"
#include "audio_param/audio_param.h"

#include <hardware/hardware.h>
#include <system/audio.h>
#include <hardware/audio.h>
#include <hardware/audio_alsaops.h>
#include <hardware_legacy/power.h>
#include <audio_utils/primitives.h>
#include "audio_offload.h"
#include "tinycompress/tinycompress.h"
#include "audio_debug.h"
#include "tinyalsa_util.h"
#ifdef LOCAL_SOCKET_SERVER
#include "audiotester/local_socket.h"
#endif
#include "aaudio.h"
#include "audio_control.h"
#include "endpoint_test.h"
#include "smartamp.h"

//#define MINI_AUDIO
//#define NORMAL_AUDIO_PLATFORM
#define RECORD_POP_MIN_TIME_CAM    800   // ms
#define RECORD_POP_MIN_TIME_MIC    200   // ms
#define RECORD_STANDBY_POP_MIN_TIME_MIC    50   // ms

#define USB_HEADSET_ADD_ZERO_DATA_TIME   20  //20ms

static pthread_mutex_t adev_init_lock = PTHREAD_MUTEX_INITIALIZER;

extern int ext_control_init(struct tiny_audio_device *adev);
extern void ext_control_close(struct tiny_audio_device *adev);

//int enable_device_gain(struct audio_control *ctl, int device, int usecase, int in_device);
int aud_rec_do_process(void *buffer, size_t bytes, int channels,void *tmp_buffer,
                              size_t tmp_buffer_bytes);

void start_audio_tunning_server(struct tiny_audio_device *adev);
void stop_audio_tunning_server(struct tiny_audio_device *adev);
extern int modem_monitor_open(void *dev,struct modem_monitor_handler *modem_monitor);
extern void modem_monitor_close(struct modem_monitor_handler *modem_monitor);
extern int ext_contrtol_process(struct tiny_audio_device *adev,const char *cmd_string);
static int out_deinit_resampler(struct tiny_stream_out *out);
static int out_init_resampler(struct tiny_stream_out *out);
int send_cmd_to_dsp_thread(struct dsp_control_t *agdsp_ctl,int cmd,void * parameter);
static ssize_t read_pcm_data(void *stream, void* buffer, size_t bytes);

extern int audio_dsp_loop_open(struct tiny_audio_device *adev);
extern int handle_received_data(void *param, uint8_t *received_buf,
                         int rev_len);
extern int audiotester_connected(AUDIO_PARAM_T *audio_param,int socket_fd);
extern int audiotester_disconnected(AUDIO_PARAM_T *audio_param);
extern void agdsp_boot(void);
extern int fm_open(void *dev);
extern int voice_open(void *dev);
extern bool is_bt_voice(void *dev);
extern int stop_voice_call(struct audio_control *ctl);
extern int is_startup(struct pcm *pcm);
#ifdef SPRD_AUDIO_HIDL_CLIENT
static ssize_t diag_read(struct audio_stream_in *stream, void* buffer,
        size_t bytes);
static ssize_t diag_write(struct audio_stream_out *stream, const void* buffer,
    size_t bytes);
static ssize_t hidl_out_write(struct audio_stream_out *stream, const void* buffer,
    size_t bytes);
static ssize_t hidl_in_read(struct audio_stream_in *stream, void* buffer,
        size_t bytes);
#endif
static char * usbout_get_parameters(void *dev, const char *keys);
static int usbout_set_parameters(void *dev, const char *kvpairs);

static struct tiny_audio_device * primary_dev=NULL;
static int primary_dev_count=0;

UNUSED_ATTR static int right2left(int16_t *buffer, uint32_t samples){
    int i = samples/2;
    while(i--){
        buffer[2*i+1]=buffer[2*i];
    }
    return 0;
}

UNUSED_ATTR static int left2right(int16_t *buffer, uint32_t samples)
{
    int i = samples/2;
    while(i--){
        buffer[2*i]=buffer[2*i+1];
    }
    return 0;
}

static inline size_t audio_stream_in_frame_size_l(const struct audio_stream_in *s)
{
    size_t chan_samp_sz;
    struct tiny_stream_in *in = (struct tiny_stream_in *) s;
    audio_format_t format = s->common.get_format(&s->common);

    if (audio_is_linear_pcm(format)) {
        chan_samp_sz = audio_bytes_per_sample(format);
        return in->config->channels* chan_samp_sz;
    }

    return sizeof(int8_t);
}

static ssize_t _pcm_read(void *stream, void* buffer,
        size_t bytes)
{
    int ret =0;
    struct tiny_stream_in *in = (struct tiny_stream_in *)stream;
    struct tiny_audio_device *adev = in->dev;

    ret = pcm_mmap_read(in->pcm, buffer,bytes);

    audiohal_pcmdump(adev->dev_ctl,in->source,buffer,bytes,PCMDUMP_NORMAL_RECORD_VBC);

    if(0==ret){
        if((in->config->channels==2)
            &&((in->devices==AUDIO_DEVICE_IN_BUILTIN_MIC)||(in->devices==AUDIO_DEVICE_IN_BACK_MIC))){
            right2left(buffer, bytes/2);
        }
    }

    return ret;
}

UNUSED_ATTR static record_nr_handle init_nr_process(struct audio_stream_in *stream)
{
    struct tiny_stream_in *in = (struct tiny_stream_in *)stream;
    struct tiny_audio_device *adev = in->dev;

    struct audio_record_proc_param *param_data = NULL;
    NR_CONTROL_PARAM_T *nr_control=NULL;

    LOG_I("init_nr_process");

    param_data=(struct audio_record_proc_param *)get_ap_record_param(adev->dev_ctl->audio_param,adev->dev_ctl->in_devices);

    if(NULL==param_data){
      LOG_I("audio record process param is null");
      return false;
    }

    nr_control = &param_data->nr_control;

    LOG_D("init_nr_process nr_switch:%x nr_dgain:%x ns_factor:%x",
        nr_control->nr_switch,nr_control->nr_dgain,nr_control->ns_factor);
    return AudioRecordNr_Init((int16 *)nr_control, _pcm_read, (void *)in,
        in->config->channels);
}

static int pcm_mixer(int16_t *buffer, uint32_t samples)
{
    unsigned int i = 0;
    int16_t *tmp_buf = buffer;
    for (i = 0; i < (samples / 2); i++) {
        tmp_buf[i] = (buffer[2 * i + 1] + buffer[2 * i]) / 2;
    }
    return 0;
}

static bool get_boot_status(void){
    char bootvalue[PROPERTY_VALUE_MAX];

    property_get("persist.vendor.audio.boot_completed", bootvalue, "");
    if (strncmp("1", bootvalue, 1) != 0) {
        LOG_I("get_boot_status false");
        return false;
    }else{
        LOG_I("get_boot_status true");
        return true;
    }
}

static int get_headset_device(void *dev) {
    char buf[12] = {'\0'};
    const char *pHeadsetPath1 = "/sys/class/switch/h2w/state";
    const char *pHeadsetPath2 = "/sys/kernel/headset/state";
    const char* headsetStatePath = NULL;
    struct tiny_audio_device *adev = (struct tiny_audio_device *)dev;
    int devices=0;
    if (0 == access(pHeadsetPath2, R_OK)) {
        headsetStatePath = pHeadsetPath2;
    } else if (0 == access(pHeadsetPath1, R_OK)) {
        headsetStatePath = pHeadsetPath1;
    }

    if (headsetStatePath != NULL) {
        int fd = open(headsetStatePath,O_RDONLY);
        if (fd >= 0) {
            ssize_t readSize = read(fd,(char*)buf,12);
            close(fd);
            if (readSize > 0) {
                int value = atoi((char*)buf);
                if (value == 1 || value == 2) {
                    devices =
                    (value == 1) ?  AUDIO_DEVICE_OUT_WIRED_HEADSET : AUDIO_DEVICE_OUT_WIRED_HEADPHONE;
                }
            }
        } else {
            LOG_E("%s open %s is fail!", __FUNCTION__, headsetStatePath);
        }
    } else {
       LOG_E("%s headset state dev node is not access", __FUNCTION__);
        if(NULL!=adev){
            adev->boot_completed=true;
       }
    }
    return devices;
}

static void check_headset_status(struct tiny_stream_out *out){
    struct tiny_audio_device *adev = out->dev;
    audio_devices_t devices=out->devices;
    audio_devices_t pre_devices=out->devices;
    devices=get_headset_device(adev);

    if(devices==0){
        out->devices=out->apm_devices;
    }else{
        out->devices=devices;
    }
    if(pre_devices!=out->devices){
        adev->out_devices=out->devices;
        select_devices_new(adev->dev_ctl,out->audio_app_type, adev->out_devices,false,true,false, true);
    }
}

static void do_bootcomplated(void *dev){
    struct tiny_audio_device *adev = (struct tiny_audio_device *)dev;
    struct tiny_stream_out *out=NULL;
    struct listnode *item;
    struct listnode *item2;

    pthread_mutex_lock(&adev->output_list_lock);
    if(!list_empty(&adev->active_out_list)){
        list_for_each_safe(item, item2,&adev->active_out_list){
           out = node_to_item(item, struct tiny_stream_out, node);
            if((out!=NULL)
                &&(out->devices !=out->apm_devices)
                &&(out->apm_devices!=0)){
                LOG_I("do_bootcomplated update output flag: 0x%x device:0x%x apm_devices:0x%x", out->flags, out->devices, out->apm_devices);
                pthread_mutex_lock(&out->lock);
                out->devices=out->apm_devices;
                adev->out_devices=out->devices;
                pthread_mutex_unlock(&out->lock);
                LOG_I("do_bootcomplated update normal output devices:0x%x",out->devices);
                select_devices_new(adev->dev_ctl,out->audio_app_type, adev->out_devices,false,true,false, true);
                break;
            }
        }
    }
    pthread_mutex_unlock(&adev->output_list_lock);
}

static void set_audio_boot_completed(void *dev){
    struct tiny_audio_device *adev = (struct tiny_audio_device *)dev;

    if(adev->boot_completed)
        return;

    adev->boot_completed=true;
    property_set("persist.vendor.audio.boot_completed","1");
    do_bootcomplated(adev);
}

static void check_boot_status(void *dev){
    struct tiny_audio_device *adev = (struct tiny_audio_device *)dev;

    if(false==adev->boot_completed){
        bool boot_status =get_boot_status();
        if(boot_status==true){
            //boot completed
            set_audio_boot_completed(dev);
        }
    }
}

static void bootup_timer_handler(union sigval arg){
   LOG_I("%s in",__func__);
   struct tiny_audio_device *adev = (struct tiny_audio_device *)arg.sival_ptr;
   pthread_mutex_lock(&adev->lock);
   timer_delete(adev->bootup_timer.timer_id);
   adev->bootup_timer.created = false;
   set_audio_boot_completed(adev);
   pthread_mutex_unlock(&adev->lock);
}

static void create_bootup_timer(struct tiny_audio_device * adev,int delay){
    LOG_I("%s ,in",__func__);
    int status;
    struct sigevent se;
    struct itimerspec ts;

    se.sigev_notify = SIGEV_THREAD;
    se.sigev_signo = SIGALRM;
    se.sigev_value.sival_ptr = adev;
    se.sigev_notify_function = bootup_timer_handler;
    se.sigev_notify_attributes = NULL;

    ts.it_value.tv_sec = delay;
    ts.it_value.tv_nsec = 0;
    ts.it_interval.tv_sec = 0;
    ts.it_interval.tv_nsec = 0;

    status = timer_create(CLOCK_MONOTONIC, &se,&(adev->bootup_timer.timer_id));
    if(status == 0){
        adev->bootup_timer.created = true;
        timer_settime(adev->bootup_timer.timer_id, 0, &ts, 0);
    }else{
        adev->bootup_timer.created = false;
        LOG_E("create timer err !");
    }
    LOG_I("%s ,out",__func__);
}

int adev_out_apm_devices_check(void *dev,audio_devices_t device){
    struct tiny_audio_device *adev =(struct tiny_audio_device *)dev;
    struct tiny_stream_out *out = NULL;
    struct listnode *item;
    struct listnode *item2;

    pthread_mutex_lock(&adev->output_list_lock);
    if(!list_empty(&adev->active_out_list)){
        list_for_each_safe(item, item2,&adev->active_out_list){
            out = node_to_item(item, struct tiny_stream_out, node);
            if(out!=NULL){
                pthread_mutex_lock(&out->lock);
                out->apm_devices=device;
                LOG_I("adev_out_apm_devices_check update apm_devices:0x%x",out->apm_devices);
                pthread_mutex_unlock(&out->lock);
            }
        }
    }
    pthread_mutex_unlock(&adev->output_list_lock);
    return 0;
}

void force_all_standby(void *dev)
{
    struct tiny_stream_in *in;
    struct tiny_stream_out *out;
    struct listnode *item;
    struct listnode *item2;

    struct tiny_audio_device *adev = (struct tiny_audio_device *)dev;
    LOG_I("force_all_standby");

    pthread_mutex_lock(&adev->output_list_lock);
    if(!list_empty(&adev->active_out_list)){
        list_for_each_safe(item, item2,&adev->active_out_list){
            out = node_to_item(item, struct tiny_stream_out, node);
            if(NULL!=out->standby_fun){
                pthread_mutex_lock(&out->lock);
                out->standby_fun(adev,out,out->audio_app_type);
                pthread_mutex_unlock(&out->lock);
            }
        }
    }
    pthread_mutex_unlock(&adev->output_list_lock);

    pthread_mutex_lock(&adev->input_list_lock);
    if(!list_empty(&adev->active_input_list)){
        list_for_each_safe(item, item2,&adev->active_input_list){
            in = node_to_item(item, struct tiny_stream_in, node);
            if(NULL!=in->standby_fun){
                pthread_mutex_lock(&in->lock);
                in->standby_fun(adev,in,in->audio_app_type);
                pthread_mutex_unlock(&in->lock);
            }
        }
    }
    pthread_mutex_unlock(&adev->input_list_lock);

    LOG_I("force_all_standby:exit");
}

int audio_agdsp_reset(void * dev,UNUSED_ATTR struct str_parms *parms,int is_reset,UNUSED_ATTR char * val){
    int ret=0;
    struct tiny_audio_device * adev=(struct tiny_audio_device *)dev;

    if(is_reset){
        force_all_standby(adev);
        pthread_mutex_lock(&adev->lock);
        adev->call_mode = AUDIO_MODE_NORMAL;
        agdsp_boot();
        pthread_mutex_unlock(&adev->lock);
    }
    return ret;
}

int audio_add_input(struct tiny_audio_device *adev,struct tiny_stream_in *in){
    LOG_I("audio_add_input type:%d in:%p ",in->audio_app_type,in);

    struct listnode *item;
    struct listnode *item2;

    struct tiny_stream_in *in_tmp;
    pthread_mutex_lock(&adev->input_list_lock);

    list_for_each_safe(item,item2, (&adev->active_input_list)) {
        in_tmp = node_to_item(item,struct tiny_stream_in, node);
        if (in_tmp == in) {
            LOG_W("audio_add_input:%p type:%d aready in list",in,in->audio_app_type);
            pthread_mutex_unlock(&adev->input_list_lock);
            return 0;
        }
    }
    list_add_tail(&adev->active_input_list, &in->node);
    pthread_mutex_unlock(&adev->input_list_lock);
    return 0;
}

int audio_del_input(struct tiny_audio_device *adev,struct tiny_stream_in *in)
{
    struct listnode *item;
    struct listnode *item2;
    struct tiny_stream_in *in_tmp;

    pthread_mutex_unlock(&adev->input_list_lock);
    LOG_I("audio_del_input type:%d out:%p",in->audio_app_type,in);
    if(!list_empty(&adev->active_input_list)){
        list_for_each_safe(item, item2,&adev->active_input_list) {
            in_tmp = node_to_item(item, struct tiny_stream_in, node);
            if (in_tmp == in) {
                LOG_I("audio_del_input:%p type:%d",in,in->audio_app_type);
                list_remove(item);
                break;
            }
        }
    }
    pthread_mutex_unlock(&adev->input_list_lock);
    return 0;
}

int audio_add_output(struct tiny_audio_device *adev,struct tiny_stream_out *out)
{
    struct listnode *item;
    struct listnode *item2;

    struct tiny_stream_out *out_tmp;

    LOG_I("audio_add_output type:%d out:%p",out->audio_app_type,out);

    pthread_mutex_lock(&adev->output_list_lock);
    list_for_each_safe(item, item2,(&adev->active_out_list)){
        out_tmp = node_to_item(item, struct tiny_stream_out, node);
        if (out_tmp == out) {
            LOG_W("audio_add_output:%p type:%d aready in list",out,out->audio_app_type);
            pthread_mutex_unlock(&adev->output_list_lock);
            return 0;
        }
    }

    list_add_tail(&adev->active_out_list, &out->node);
    pthread_mutex_unlock(&adev->output_list_lock);
    return 0;
}

int audio_del_output(struct tiny_audio_device *adev,struct tiny_stream_out *out)
{
    struct listnode *item=NULL;
    struct listnode *item2=NULL;
    struct tiny_stream_out * out_tmp = NULL;
    LOG_I("audio_del_output type:%d out:%p",out->audio_app_type,out);

    pthread_mutex_lock(&adev->output_list_lock);
    if(!list_empty(&adev->active_out_list)){
        LOG_I("audio_del_output out:%p %p %p %p",item,item2,&adev->active_out_list,&adev->active_out_list.next);
            list_for_each_safe(item, item2,&adev->active_out_list){
            out_tmp = node_to_item(item, struct tiny_stream_out, node);
            if(out == out_tmp){
                LOG_I("audio_del_output:%p type:%d",out,out->audio_app_type);
                list_remove(item);
                break;
             }
        }
    }
    pthread_mutex_unlock(&adev->output_list_lock);

    return 0;
    LOG_I("audio_del_output:exit");
}

static int do_normal_output_standby(void *dev,void * out,UNUSED_ATTR AUDIO_HW_APP_T type)
{
    int usecase = UC_UNKNOWN;
    struct tiny_audio_device *adev=(struct tiny_audio_device *)dev;
    struct tiny_stream_out *normal_out=(struct tiny_stream_out *)out;
    LOG_I("do_normal_output_standby enter:%p standby:%d",normal_out->pcm,normal_out->standby);
    usecase = stream_type_to_usecase(normal_out->audio_app_type);

    if(normal_out->standby) {
        return -1;
    }

    set_mdg_mute(adev->dev_ctl,usecase,true);

    if (NULL!=normal_out->pcm) {
        LOG_I("do_normal_output_standby pcm_close:%p",normal_out->pcm);
        pcm_close(normal_out->pcm);
        LOG_I("do_normal_output_standby pcm_close:%p success",normal_out->pcm);
        normal_out->pcm = NULL;
    }

    out_deinit_resampler(normal_out);
    normal_out->standby=true;

    set_usecase(adev->dev_ctl, usecase, false);
    set_audioparam(adev->dev_ctl,PARAM_USECASE_CHANGE,NULL,false);
    LOG_I("do_normal_output_standby :%p %d exit",out,normal_out->audio_app_type);
    if(normal_out->audio_app_type == AUDIO_HW_APP_VOIP) {
        normal_out->audio_app_type = AUDIO_HW_APP_PRIMARY;
    }
    return 0;
}

ssize_t normal_out_write(struct tiny_stream_out *out, void *buffer,
                                size_t bytes)
{
    void *buf;
    size_t frame_size = 0;
    size_t in_frames = 0;
    size_t out_frames = 0;
    bool low_power;
    int kernel_frames;
    static long time1 = 0, time2 = 0;
    int ret=-1;
    struct tiny_audio_device *adev = out->dev;

    frame_size = audio_stream_out_frame_size(&out->stream);
    in_frames = bytes / frame_size;
    out_frames = RESAMPLER_BUFFER_SIZE / frame_size;
    low_power = adev->low_power && !is_usecase_unlock(adev->dev_ctl, UC_MM_RECORD | UC_FM_RECORD | UC_VOICE_RECORD |
        UC_VOIP_RECORD | UC_BT_RECORD |UC_RECOGNITION);

    if(out->pcm){
        if(out->config->rate !=out->requested_rate){
            audio_format_t format = out->stream.common.get_format(&out->stream.common);
            if((out->config->format ==  PCM_FORMAT_S16_LE)
                && (format ==  AUDIO_FORMAT_PCM_8_24_BIT)){
                memcpy_to_i16_from_q8_23((int16_t *)out->buffer, buffer,
                    bytes/audio_bytes_per_sample(AUDIO_FORMAT_PCM_8_24_BIT));
                out->resampler->resample_from_input(out->resampler,
                    (int16_t *)out->buffer,&in_frames,
                    (int16_t *) buffer,&out_frames);
                frame_size  =4;
                buf = buffer;
            }
            else {
                out->resampler->resample_from_input(out->resampler,
                        (int16_t *)buffer,&in_frames,
                        (int16_t *) out->buffer,&out_frames);
                buf = out->buffer;
            }
        }
        else if((out->config->format == PCM_FORMAT_S16_LE)
                && (out->format == PCM_FORMAT_S24_LE)) {
            if(!out->format_buffer || (out->format_buffer_len < bytes)) {
                out->format_buffer_len = bytes;
                if(out->format_buffer) {
                    free(out->format_buffer);
                }
                out->format_buffer = malloc(bytes);
                if(!out->format_buffer) {
                    ALOGE("error:out->format_buffer malloc :%d falied",bytes);
                    return 0;
                }
            }
            memcpy_to_i16_from_q8_23((int16_t *)out->format_buffer, buffer, in_frames<<1);
            buf = out->format_buffer;
            out_frames = in_frames;
            frame_size = 4;
        } else {
            out_frames = in_frames;
            buf = (void *)buffer;
        }

        if((out->config->channels==1) &&(frame_size == 4)){
            frame_size=2;
            LOG_D("normal_out_write chanage channle");
            pcm_mixer(buf, out_frames*frame_size);
        }
        if(AUDIO_HW_APP_DEEP_BUFFER == out->audio_app_type){
            if(low_power != out->low_power){
                if(low_power){
                    out->write_threshold = out->config->period_size * out->config->period_count;
                    out->config->avail_min = ( out->write_threshold *3 ) /4;
                    LOG_I("low_power avail_min:%d, write_threshold:%d", out->config->avail_min, out->write_threshold);
                } else{
                    out->write_threshold = (out->config->period_size * out->config->period_count)/2;
                    out->config->avail_min = out->write_threshold;
                    LOG_I("avail_min:%d, write_threshold:%d", out->config->avail_min, out->write_threshold);
                }
                pcm_set_avail_min(out->pcm, out->config->avail_min);
                out->low_power = low_power;
            }

            do{
                struct timespec time_stamp;

                if (pcm_get_htimestamp(out->pcm, (unsigned int *)&kernel_frames, &time_stamp) < 0)
                    break;

                kernel_frames = pcm_get_buffer_size(out->pcm) - kernel_frames;
                if(kernel_frames > out->write_threshold){
                    unsigned long time = (unsigned long)(((int64_t)(kernel_frames - out->write_threshold) * 1000000) /DEFAULT_OUT_SAMPLING_RATE);
                    if (time < MIN_WRITE_SLEEP_US){
                        time = MIN_WRITE_SLEEP_US;
                    }
                    time1 = getCurrentTimeUs();
                    usleep(time);
                    time2 = getCurrentTimeUs();
                }
            }while(kernel_frames > out->write_threshold);
        }
        ret = pcm_mmap_write(out->pcm, (void *)buf,
                         out_frames * frame_size);
        if(0==ret){
            LOG_D("normal_out_write out frames  is %d bytes:%d", out_frames,bytes);
            audiohal_pcmdump(adev->dev_ctl,out->flags,buf,out_frames * frame_size,PCMDUMP_PRIMARY_PLAYBACK_VBC);
        }else{
            LOG_E("normal_out_write ret:0x%x pcm:0x%p", ret,out->pcm);
            if (ret < 0) {
                if (out->pcm) {
                    LOG_W("normal_out_write warning:%d, (%s)", ret,
                          pcm_get_error(out->pcm));
                }
            }
        }
    }
    return (ret == 0) ? bytes:ret;
}

int start_output_stream(struct tiny_stream_out *out)
{
    struct tiny_audio_device *adev = out->dev;
    int pcm_devices=-1;
    int ret = 0;
    int usecase=UC_UNKNOWN;
    struct audio_control *control = NULL;

    control = adev->dev_ctl;

    if(out->buffer!=NULL){
        memset(out->buffer,0,out->resampler_buffer_size);
    }

    if(NULL!=out->pcm){
        LOG_E("start_output_stream pcm is not null:%p",out->pcm);
        pcm_close(out->pcm);
        out->pcm=NULL;
    }

    if(out->devices == AUDIO_DEVICE_NONE){
        LOG_E("start_output_stream, devices is 0");
        return -1;
    }

    ret = dev_ctl_get_out_pcm_config(control, out->audio_app_type, &pcm_devices, out->config);
    if(ret != 0) {
        LOG_E("start_output_stream, out->config is NULL");
        return -1;
    }

    usecase = stream_type_to_usecase(out->audio_app_type);
    LOG_I("start_output_stream usecase:%x  device:0x%x",usecase,out->devices);
    ret=set_usecase(adev->dev_ctl, usecase, true);
    if(ret < 0) {
        ALOGE("start_output_stream set_usecase error:ret:%d,usecase:%x", ret,usecase);
        return -1;
    }
    switch_vbc_route(adev->dev_ctl,out->devices);

    LOG_I("start_output_stream pcm_open start pcm device:%d config rate:%d channels:%d format:%d %d %d",pcm_devices
        ,out->config->rate,out->config->channels,out->config->format,out->config->period_size,out->config->start_threshold);
    out->pcm =
        pcm_open(adev->dev_ctl->cards.s_tinycard, pcm_devices, PCM_OUT | PCM_MMAP | PCM_NOIRQ |PCM_MONOTONIC, out->config);
    if (!pcm_is_ready(out->pcm)) {
        LOG_E("%s:cannot open pcm:%s", __func__,
          pcm_get_error(out->pcm));
        goto error;
    }
    LOG_I("start_output_stream pcm_open end");

    out->low_power = false;

    if(AUDIO_HW_APP_VOICE_TX==out->audio_app_type){
        ret=mixer_ctl_set_value(control->voice_dl_playback, 0, VOICE_DL_PCM_PLAY_UPLINK_MIX);
        if(ret!=0){
            LOG_E("%s set voice playback mode failed:%d",__func__,ret);
            goto error;
        }else{
            ALOGI("start:voice_tx");
        }
    }

    if(AUDIO_HW_APP_DEEP_BUFFER == out->audio_app_type){
        out->write_threshold = (out->config->period_size * out->config->period_count)/2;
        out->config->avail_min = out->write_threshold;
        LOG_I("%s avail_min:%d, write_threshold:%d", __FUNCTION__, out->config->avail_min, out->write_threshold);
        pcm_set_avail_min(out->pcm, out->config->avail_min);
    }

    if((out->config->rate !=out->requested_rate)&&(NULL==out->resampler)){
        LOG_I("start_output_stream create_resampler");
        if(0!=out_init_resampler(out)){
            goto error;
        }
        out->resampler->reset(out->resampler);
    }

    if(AUDIO_HW_APP_PRIMARY==out->audio_app_type){
        out->primary_latency = (out->config->period_size * out->config->period_count * 1000) /
                       out->config->rate;
        out->primary_out_buffer_size = (out->config->period_size * DEFAULT_OUT_SAMPLING_RATE) /
                       out->config->rate;
    }

    LOG_I("start_output_stream pcm_open devices:%d pcm:%p %d %d",pcm_devices,out->pcm
        ,out->config->rate,out->requested_rate);

    set_mdg_mute(adev->dev_ctl,usecase,false);

    out->standby_fun=do_normal_output_standby;
    select_devices_new(adev->dev_ctl,out->audio_app_type, out->devices,false,false,true,true);

    if((out->devices & AUDIO_DEVICE_OUT_USB_HEADSET) && (out->audio_app_type == AUDIO_HW_APP_PRIMARY || out->audio_app_type == AUDIO_HW_APP_FAST)){
        int fill_len=0;
        char *fill_buff = NULL;
        /* 20ms */
        fill_len = audio_stream_out_frame_size(&out->stream) * out->request_config.sample_rate * USB_HEADSET_ADD_ZERO_DATA_TIME /1000 ;
        fill_buff = calloc(1, fill_len);
        ALOGI("fill_len=%d, fill_buff=%p",fill_len, fill_buff);
        if (fill_buff) {
            memset(fill_buff, 0, fill_len);
            normal_out_write(out, fill_buff, fill_len);
            free(fill_buff);
        }
    }

    if(out->audio_app_type == AUDIO_HW_APP_VOIP){
        set_dsp_volume(adev->dev_ctl,adev->voice_volume);
    }

    if(out->audio_app_type == AUDIO_HW_APP_VOIP){
        pthread_mutex_lock(&adev->voip_start_lock);
        adev->voip_start = true;
        pthread_mutex_unlock(&adev->voip_start_lock);
    }

    set_audioparam(adev->dev_ctl,PARAM_USECASE_DEVICES_CHANGE,NULL,true);

#ifdef AUDIO_DEBUG
    if(out->audio_app_type != AUDIO_HW_APP_FAST){
        debug_dump_start(&adev->debugdump,DEFAULT_REG_DUMP_COUNT);
    }
#endif
    return 0;

error:
    if(out->pcm) {
        pcm_close(out->pcm);
        out->pcm = NULL;
    }
    set_usecase(adev->dev_ctl, usecase, false);
    set_audioparam(adev->dev_ctl,PARAM_USECASE_CHANGE,NULL,false);
    return -1;
}

static int check_input_parameters(uint32_t sample_rate, int format,
                                  int channel_count)
{
    if (format != AUDIO_FORMAT_PCM_16_BIT)
    { return -EINVAL; }

    if ((channel_count < 1) || (channel_count > 2))
    { return -EINVAL; }

    switch (sample_rate) {
    case 8000:
    case 11025:
    case 16000:
    case 22050:
    case 24000:
    case 32000:
    case 44100:
    case 48000:
        break;
    default:
        return -EINVAL;
    }

    return 0;
}

static bool samplerate_dsp_support(uint32_t sample_rate)
{
    switch (sample_rate) {
    case 8000:
    case 16000:
    case 44100:
    case 48000:
        break;
    default:
        return false;
    }
    return true;
}

static size_t get_input_buffer_size(const struct tiny_audio_device *adev,
                                    uint32_t sample_rate, int format,
                                    int channel_count)
{
    size_t size;
    struct audio_control *control = adev->dev_ctl;
    if (check_input_parameters(sample_rate, format, channel_count) != 0) {
        return 0;
    }

    /*  take resampling into account and return the closest majoring
        multiple of 16 frames, as audioflinger expects audio buffers to
        be a multiple of 16 frames */
    size =
        (control->pcm_handle.play[AUD_PCM_MM_NORMAL].period_size *
         sample_rate) / control->pcm_handle.play[AUD_PCM_MM_NORMAL].rate;
    size = ((size + 15) / 16) * 16;
    return size * channel_count * sizeof(short);
}

static uint32_t out_get_sample_rate(const struct audio_stream *stream)
{
    struct tiny_stream_out *out = (struct tiny_stream_out *)stream;

    if(out->audio_app_type == AUDIO_HW_APP_OFFLOAD) {
        return out->offload_samplerate;
    }
    return out->request_config.sample_rate;
}

static int out_set_sample_rate(UNUSED_ATTR struct audio_stream *stream, UNUSED_ATTR uint32_t rate)
{
    return 0;
}

static size_t out_get_buffer_size(const struct audio_stream *stream)
{
    struct tiny_stream_out *out = (struct tiny_stream_out *)stream;
    if(out->audio_app_type == AUDIO_HW_APP_OFFLOAD)
    { return out->compress_config.fragment_size; }

    /*  take resampling into account and return the closest majoring
        multiple of 16 frames, as audioflinger expects audio buffers to
        be a multiple of 16 frames */
    size_t size =
        (out->config->period_size * DEFAULT_OUT_SAMPLING_RATE) /
        out->config->rate;

    if(AUDIO_HW_APP_VOIP == out->audio_app_type){
       if(0!=out->primary_out_buffer_size){
             size=out->primary_out_buffer_size;
       }
    }

    size = ((size + 15) / 16) * 16;
    LOG_D("%s size=%zd, frame_size=%zd audio_app_type:%d", __func__, size,
          audio_stream_out_frame_size(&out->stream),out->audio_app_type);
    return size * audio_stream_out_frame_size(&out->stream);
}

static audio_channel_mask_t out_get_channels(const struct audio_stream *stream)
{
    struct tiny_stream_out *out = (struct tiny_stream_out *)stream;

    if(out->audio_app_type == AUDIO_HW_APP_OFFLOAD) {
        return out->offload_channel_mask;
    }
    return out->request_config.channel_mask;
}

static audio_format_t out_get_format(const struct audio_stream *stream)
{
    struct tiny_stream_out *out = (struct tiny_stream_out *)stream;

    if(out->audio_app_type == AUDIO_HW_APP_OFFLOAD){
        return out->offload_format;
    }
    return out->request_config.format;
}

static int out_set_format(UNUSED_ATTR struct audio_stream *stream, UNUSED_ATTR audio_format_t format)
{
    return 0;
}

struct tiny_stream_out * get_output_stream_unlock(struct tiny_audio_device *adev,AUDIO_HW_APP_T audio_app_type){
    struct tiny_stream_out *out = NULL;
    struct tiny_stream_out * out_ret = NULL;
    struct listnode *item;
    struct listnode *item2;
    LOG_D("get_output_stream_unlock type:%d",audio_app_type);
    if(!list_empty(&adev->active_out_list)){
        list_for_each_safe(item, item2,&adev->active_out_list){
            out = node_to_item(item, struct tiny_stream_out, node);
            if(out->audio_app_type==audio_app_type){
                out_ret = out;
                break;
             }
        }
    }
    return out_ret;
}

struct tiny_stream_in * get_input_stream(struct tiny_audio_device *adev,AUDIO_HW_APP_T audio_app_type){
    struct tiny_stream_in *in = NULL;
    struct tiny_stream_in * in_ret = NULL;
    struct listnode *item;
    struct listnode *item2;
    pthread_mutex_lock(&adev->input_list_lock);
    LOG_D("get_input_stream_unlock type:%d",audio_app_type);
    if(!list_empty(&adev->active_input_list)){
        list_for_each_safe(item, item2,&adev->active_input_list){
            in = node_to_item(item, struct tiny_stream_in, node);
            if(in->audio_app_type==audio_app_type){
                in_ret = in;
                break;
             }
        }
    }
    pthread_mutex_unlock(&adev->input_list_lock);
    return in_ret;
}

struct tiny_stream_out * get_output_stream(struct tiny_audio_device *adev,AUDIO_HW_APP_T audio_app_type){
    struct tiny_stream_out *out_ret = NULL;
    pthread_mutex_lock(&adev->output_list_lock);
    out_ret=get_output_stream_unlock(adev,audio_app_type);
    pthread_mutex_unlock(&adev->output_list_lock);
    return out_ret;
}

struct tiny_stream_out * force_out_standby(struct tiny_audio_device *adev,AUDIO_HW_APP_T audio_app_type){
    struct tiny_stream_out *out = NULL;
    struct listnode *item;
    struct listnode *tmp;
    struct listnode *list = NULL;

    LOG_I("force_out_standby audio_app_type:0x%x",audio_app_type);
    pthread_mutex_lock(&adev->output_list_lock);
    list=&adev->active_out_list;
    list_for_each_safe(item, tmp, list){
        out = node_to_item(item, struct tiny_stream_out, node);
        if(out->audio_app_type==audio_app_type){
            if (NULL!=out->standby_fun) {
                pthread_mutex_lock(&out->lock);
                LOG_I("force_out_standby audio_app_type:0x%x in:%p node:%p list:%p,app_byte:%d",
                    audio_app_type,out,&(out->node),list,out->audio_app_type);
                out->standby_fun(adev,out,out->audio_app_type);
                pthread_mutex_unlock(&out->lock);
            }
         }
    }
    pthread_mutex_unlock(&adev->output_list_lock);
    LOG_I("force_out_standby:exit");
    return out;
}

void set_out_testmode(struct tiny_audio_device *adev,AUDIO_HW_APP_T audio_app_type,bool testMode){
    struct tiny_stream_out *out = NULL;
    struct listnode *item;
    struct listnode *tmp;
    struct listnode *list = NULL;

    LOG_I("set_out_testmode audio_app_type:0x%x %d",audio_app_type,testMode);
    pthread_mutex_lock(&adev->output_list_lock);
    list=&adev->active_out_list;
    list_for_each_safe(item, tmp, list){
        out = node_to_item(item, struct tiny_stream_out, node);
        if(out->audio_app_type==audio_app_type){
            pthread_mutex_lock(&out->lock);
            out->testMode = testMode;
            if ((NULL!=out->standby_fun) && (true==testMode)) {
                out->standby_fun(adev,out,out->audio_app_type);
            }
            pthread_mutex_unlock(&out->lock);
         }
    }
    pthread_mutex_unlock(&adev->output_list_lock);
    LOG_I("set_out_testmode:exit");
}

void output_sync(struct tiny_audio_device *adev,AUDIO_HW_APP_T audio_app_type){
    struct tiny_stream_out *out = NULL;
    LOG_I("output_sync audio_app_type:0x%x",audio_app_type);
    pthread_mutex_lock(&adev->output_list_lock);
    out=get_output_stream_unlock(adev,audio_app_type);

    if((NULL!=out) && (NULL!=out->standby_fun)){
        pthread_mutex_lock(&out->lock);
        LOG_I("output_sync");
        pthread_mutex_unlock(&out->lock);
    }
    pthread_mutex_unlock(&adev->output_list_lock);
    LOG_I("output_sync:exit");
}

int do_output_standby(struct tiny_stream_out *out)
{
    struct tiny_audio_device *adev = out->dev;
    LOG_I("do_output_standby %p audio_app_type:%d",out,out->audio_app_type);
    if(NULL!=out->standby_fun){
        pthread_mutex_lock(&out->lock);
        if(out->standby == false){
            out->standby_fun(adev,out,out->audio_app_type);
        }
        if(out->audio_app_type == AUDIO_HW_APP_VOIP){
            pthread_mutex_lock(&adev->voip_start_lock);
            adev->voip_start = false;
            pthread_mutex_unlock(&adev->voip_start_lock);
        }
        pthread_mutex_unlock(&out->lock);
    }
    return 0;
}

static int out_standby(struct audio_stream *stream)
{
    struct tiny_stream_out *out = (struct tiny_stream_out *)stream;
    int status=-1;
    LOG_I("out_standby %p",stream);
    status = do_output_standby(out);
    return status;
}

static int out_dump(const struct audio_stream *stream, int fd)
{
    struct tiny_stream_out *out = (struct tiny_stream_out *)stream;

    char buffer[DUMP_BUFFER_MAX_SIZE]={0};

    if(out->standby){
        snprintf(buffer,(DUMP_BUFFER_MAX_SIZE-1),
            "\noutput dump ---->\n"
            "flags:%x  "
            "audio_app_type:%d standby\n",
            out->flags,
            out->audio_app_type);
        AUDIO_DUMP_WRITE_STR(fd,buffer);
    }else{
        snprintf(buffer,(DUMP_BUFFER_MAX_SIZE-1),
            "\noutput dump ---->\n "
            "flags:%x  "
            "primary_latency:%d "
            "primary_out_buffer_size:%d "
            "audio_app_type:%d  \n"
            "pcm:%p "
            "resampler::%p "
            "buffer::%p\n",
            out->flags,
            out->primary_latency,
            out->primary_out_buffer_size,
            out->audio_app_type,
            out->pcm,
            out->resampler,
            out->buffer);
        AUDIO_DUMP_WRITE_STR(fd,buffer);
        memset(buffer,0,sizeof(buffer));

        snprintf(buffer,(DUMP_BUFFER_MAX_SIZE-1),
            "config:%d %d %d %d 0x%x 0x%x 0x%x\n",
            out->config->channels,
            out->config->rate,
            out->config->period_size,
            out->config->period_count,
            out->config->start_threshold,
            out->config->avail_min,
            out->config->stop_threshold);
        AUDIO_DUMP_WRITE_STR(fd,buffer);
        memset(buffer,0,sizeof(buffer));

        snprintf(buffer,(DUMP_BUFFER_MAX_SIZE-1),
            "low_power:%d "
            "write_threshold:%d "
            "devices:%d "
            "written:%d \n",
            out->low_power,
            out->write_threshold,
            out->devices,
            out->written);
        AUDIO_DUMP_WRITE_STR(fd,buffer);
        memset(buffer,0,sizeof(buffer));

        if(out->audio_app_type == AUDIO_HW_APP_OFFLOAD){
            snprintf(buffer,(DUMP_BUFFER_MAX_SIZE-1),
                "\noffload output dump\n "
                "flags:%x  "
                "pcm:%p "
                "compress::%p "
                "state::%d "
                "format::%d "
                "samplerate::%d "
                "channel_mask::%d\n",
                out->flags,
                out->pcm,
                out->compress,
                out->audio_offload_state,
                out->offload_format,
                out->offload_samplerate,
                out->offload_channel_mask);
            AUDIO_DUMP_WRITE_STR(fd,buffer);
            memset(buffer,0,sizeof(buffer));

          snprintf(buffer,(DUMP_BUFFER_MAX_SIZE-1),
              "\noffload output dump\n "
              "started:%d "
              "metadata:%d "
              "thread_blocked:%d "
              "nonblocking:0x%x "
              "offload_cookie:%p \n",
              out->is_offload_compress_started,
              out->is_offload_need_set_metadata,
              out->is_audio_offload_thread_blocked,
              out->is_offload_nonblocking,
              out->audio_offload_cookie);
          AUDIO_DUMP_WRITE_STR(fd,buffer);
          memset(buffer,0,sizeof(buffer));
        }
    }

    snprintf(buffer,(DUMP_BUFFER_MAX_SIZE-1),"<----output dump\n");
    AUDIO_DUMP_WRITE_STR(fd,buffer);
    LOG_I("out_dump exit");
	return 0;
}

static bool is_primary_output(struct tiny_stream_out *out){
    if((AUDIO_HW_APP_PRIMARY ==out->audio_app_type)
        ||(AUDIO_HW_APP_VOIP ==out->audio_app_type)){
        return true;
    }else{
        return false;
    }
}

int adev_out_devices_check(void *dev,audio_devices_t device){
    struct tiny_audio_device *adev =(struct tiny_audio_device *)dev;
    struct tiny_stream_out *out = NULL;
    struct listnode *item;
    struct listnode *item2;

    pthread_mutex_lock(&adev->output_list_lock);
    if(!list_empty(&adev->active_out_list)){
        list_for_each_safe(item, item2,&adev->active_out_list){
            out = node_to_item(item, struct tiny_stream_out, node);
            if(out!=NULL){
                pthread_mutex_lock(&out->lock);
                out->devices=device;
                pthread_mutex_unlock(&out->lock);
            }
        }
    }
    pthread_mutex_unlock(&adev->output_list_lock);
    return 0;
}

static int adev_in_devices_check(void *dev,audio_devices_t device){
    struct tiny_audio_device *adev =(struct tiny_audio_device *)dev;
    struct tiny_stream_in *in = NULL;
    struct listnode *item;
    struct listnode *item2;

    pthread_mutex_lock(&adev->input_list_lock);
    if(!list_empty(&adev->active_input_list)){
        list_for_each_safe(item, item2,&adev->active_input_list){
            in = node_to_item(item, struct tiny_stream_in, node);
            if(in!=NULL){
                pthread_mutex_lock(&in->lock);
                in->devices=device;
                pthread_mutex_unlock(&in->lock);
            }
        }
    }
    pthread_mutex_unlock(&adev->input_list_lock);
    return 0;
}

static int out_set_parameters(struct audio_stream *stream, const char *kvpairs)
{
    struct tiny_stream_out *out = (struct tiny_stream_out *)stream;
    struct tiny_audio_device *adev = out->dev;
    struct str_parms *parms;
    char value[32];
    int ret, val = 0;
    audio_devices_t pre_devices=AUDIO_DEVICE_NONE;

    pthread_mutex_lock(&adev->lock);
    check_boot_status(adev);
    pthread_mutex_unlock(&adev->lock);

    parms = str_parms_create_str(kvpairs);

    LOG_I("out_set_parameters type:%d:%s",out->audio_app_type,kvpairs);
    ret =
        str_parms_get_str(parms, AUDIO_PARAMETER_STREAM_ROUTING, value,
                          sizeof(value));
    if (ret >= 0) {
        val = atoi(value);
        if(CHECK_OUT_DEVICE_IS_INVALID(val) || (val == 0)) {
            LOG_E(" %s out_device is invalid, out_device = 0x%x", __FUNCTION__, val);
            ret = -EINVAL;
            goto error;
        }
        if(val!=0){
            pthread_mutex_lock(&out->lock);
            out->apm_devices=val;
            pthread_mutex_unlock(&out->lock);
        }

        if(adev->boot_completed==false){
            if(((AUDIO_DEVICE_OUT_WIRED_HEADPHONE|AUDIO_DEVICE_OUT_WIRED_HEADSET)&val)
             ||(AUDIO_DEVICE_OUT_ALL_SCO&val)){
                LOG_I("out_set_parameters connect devices, boot completed");
                set_audio_boot_completed(adev);
            }else{
                check_boot_status(adev);
                if(adev->boot_completed==false){
                    val=0;
                    LOG_I("system is not ready, can not set audio devices form audiopolicymanager");
                    goto error;
                }
            }
        }

        pthread_mutex_lock(&out->lock);
        if (val != 0 && out->devices != (unsigned int)val) {
            pre_devices=out->devices;
            out->devices = val;
        }
        pthread_mutex_unlock(&out->lock);
        LOG_D("out_set_parameters dev_ctl:%p out:%p",adev->dev_ctl,out);

        pthread_mutex_lock(&adev->lock);

        if(is_call_active_unlock(adev)){
            if((pre_devices&AUDIO_DEVICE_OUT_ALL_SCO)&&(0==(val&AUDIO_DEVICE_OUT_ALL_SCO))){
                force_in_standby(adev,AUDIO_HW_APP_BT_RECORD);
            }else if((val&AUDIO_DEVICE_OUT_ALL_SCO)&&(0==(pre_devices&AUDIO_DEVICE_OUT_ALL_SCO))){
                force_in_standby(adev,AUDIO_HW_APP_NORMAL_RECORD);
            }
        }
        do_set_output_device(adev->dev_ctl,val);
        if((AUDIO_MODE_IN_CALL == adev->call_mode)&& (false==adev->call_start) &&(is_primary_output(out))){
            adev->call_start=true;
            pthread_mutex_unlock(&adev->lock);
            send_cmd_to_dsp_thread(adev->dev_ctl->agdsp_ctl,AUDIO_CTL_START_VOICE,NULL);
        }else{
            pthread_mutex_unlock(&adev->lock);
        }
        ret = 0;
    }

#ifdef SPRD_AUDIO_HIDL_CLIENT
    ret = str_parms_get_str(parms,"hidlstream", value, sizeof(value));
    if(ret >= 0){
        val = atoi(value);
        if(val){
            pthread_mutex_lock(&out->lock);
            out->audio_app_type = AUDIO_HW_APP_HIDL_OUTPUT;
            out->stream.write = hidl_out_write;
            adev->test_ctl.ouput_test.hild_outputstream=out;
            adev->test_ctl.loop_test.hild_outputstream=out;
            pthread_mutex_unlock(&out->lock);
         }
    }

    ret = str_parms_get_str(parms,"hidldiagstream", value, sizeof(value));
    if(ret >= 0){
        val = atoi(value);
        if(val){
            pthread_mutex_lock(&out->lock);
            out->audio_app_type = AUDIO_HW_APP_HIDL_OUTPUT;
            out->stream.write = diag_write;
            audiotester_connected(&adev->audio_param,-1);
            pthread_mutex_unlock(&out->lock);
         }
    }

    ret = str_parms_get_str(parms, "SprdAudioNpiWriteDataSize", value, sizeof(value));
    if (ret >= 0) {
        val = strtoul(value,NULL,0);
        pthread_mutex_lock(&adev->lock);
        malloc_endpoint_outputtest_buffer(&adev->test_ctl,val);
        pthread_mutex_unlock(&adev->lock);
        LOG_I("SprdAudioNpiWriteDataSize:%d",val);
    }

    ret = str_parms_get_str(parms, "AudioDiagDataSize", value, sizeof(value));
    if (ret >= 0) {
        val = strtoul(value,NULL,0);
        pthread_mutex_lock(&out->lock);
        out->diagsize=val;
        out->written=0;
        pthread_mutex_unlock(&out->lock);
        LOG_I("AudioDiagDataSize:%d",val);
    }

#endif

    if(((AUDIO_HW_APP_PRIMARY==out->audio_app_type)||(AUDIO_HW_APP_OFFLOAD==out->audio_app_type))
        &&(is_usbdevice_connected(adev->dev_ctl))){
        usbout_set_parameters(adev,kvpairs);
    }

    if (out->audio_app_type == AUDIO_HW_APP_OFFLOAD) {
        audio_get_compress_metadata(&out->stream, parms);
    }
error:
    str_parms_destroy(parms);
    return 0;
}

static char *out_get_parameters(const struct audio_stream *stream,
                                const char *keys)
{
    struct tiny_stream_out *out = (struct tiny_stream_out *)stream;
    struct str_parms *query = str_parms_create_str(keys);
    struct tiny_audio_device *adev = out->dev;

    LOG_I("out_get_parameters:%s",keys);

    /* supported sample formats */
    if (str_parms_has_key(query, AUDIO_PARAMETER_STREAM_SUP_FORMATS)){
     if(out->audio_app_type == AUDIO_HW_APP_OFFLOAD){
         str_parms_destroy(query);
         return strdup("sup_formats=AUDIO_FORMAT_MP3");
     }else{
         str_parms_destroy(query);
        if(out->request_config.format == AUDIO_FORMAT_PCM_8_24_BIT){
            return strdup("sup_formats=AUDIO_FORMAT_PCM_8_24_BIT");
        }
        else {
            return strdup("sup_formats=AUDIO_FORMAT_PCM_16_BIT");
        }
     }
    }

    if (str_parms_has_key(query, AUDIO_PARAMETER_STREAM_SUP_SAMPLING_RATES)) {
        char rsp[32]={0};
        snprintf(rsp,sizeof(rsp)-1,"sup_sampling_rates=%d",out->requested_rate);
        str_parms_destroy(query);
        LOG_I("sup_sampling_rates=%s",rsp);
        return strdup(rsp);
    }

    if (str_parms_has_key(query, AUDIO_PARAMETER_STREAM_SUP_CHANNELS)) {
        if(1==popcount(out->request_config.channel_mask)){
            str_parms_destroy(query);
            LOG_I("sup_channels=AUDIO_CHANNEL_OUT_MONO");
            return strdup("sup_channels=AUDIO_CHANNEL_OUT_MONO");
        }else{
            str_parms_destroy(query);
            LOG_I("sup_channels=AUDIO_CHANNEL_OUT_STEREO");
            return strdup("sup_channels=AUDIO_CHANNEL_OUT_STEREO");
        }
    }

    if (str_parms_has_key(query,"isOutPutStartup")){
        bool startup=true;
        str_parms_destroy(query);
        pthread_mutex_lock(&out->lock);
        if((false==out->standby)
            &&(out->pcm!=NULL)
            &&(is_startup(out->pcm))){
            startup=true;
        }else{
            startup=false;
        }
        pthread_mutex_unlock(&out->lock);
        if(true==startup){
            return strdup("isOutPutStartup=1");
        }else{
            return strdup("isOutPutStartup=0");
        }
    }

    str_parms_destroy(query);

    if(((AUDIO_HW_APP_PRIMARY==out->audio_app_type)||(AUDIO_HW_APP_OFFLOAD==out->audio_app_type))
        &&(is_usbdevice_connected(adev->dev_ctl))){
        return usbout_get_parameters(adev,keys);
    }

    return strdup("");
}

static uint32_t out_get_latency(const struct audio_stream_out *stream)
{
    struct tiny_stream_out *out = (struct tiny_stream_out *)stream;
    uint32_t temp_latency=0;

    if (out->audio_app_type == AUDIO_HW_APP_OFFLOAD)
        return 100;

    if(AUDIO_HW_APP_DEEP_BUFFER == out->audio_app_type){
        uint32_t latency_time ;
        latency_time = (out->config->period_size * out->config->period_count * 1000) /
               out->config->rate;
        if(latency_time > 100)
            return latency_time/2;
        return latency_time;
    }
    temp_latency=(out->config->period_size * out->config->period_count * 1000) /
            out->config->rate;
    if(AUDIO_HW_APP_VOIP == out->audio_app_type){
        if(0!=out->primary_latency){
              temp_latency=out->primary_latency;
        }
    }

    LOG_D("latency=%d period_size=%d,period_count=%d,rate=%d",temp_latency,out->config->period_size,out->config->period_count,out->config->rate);

    return temp_latency;
}


 static int out_set_volume(struct audio_stream_out *stream, float left,
                           float right)
{
    struct tiny_stream_out *out = (struct tiny_stream_out *)stream;
    struct tiny_audio_device *adev = out->dev;
    int ret=-ENOSYS;

    if (out->audio_app_type == AUDIO_HW_APP_OFFLOAD) {
        pthread_mutex_lock(&out->lock);
        ret= set_offload_volume(adev->dev_ctl,left,right);
        pthread_mutex_unlock(&out->lock);
    }

    pthread_mutex_lock(&adev->lock);
    adev->dev_ctl->music_volume=left;
    pthread_mutex_unlock(&adev->lock);
    return 0;
}

static bool is_primary_voip_active(struct tiny_audio_device *adev){
    struct tiny_stream_out *out = NULL;

    pthread_mutex_lock(&adev->output_list_lock);
    out=get_output_stream_unlock(adev,AUDIO_HW_APP_VOIP);
    if(NULL!=out){
        pthread_mutex_unlock(&adev->output_list_lock);
        return true;
    }
    pthread_mutex_unlock(&adev->output_list_lock);

    return false;
 }

 int is_call_active_unlock(struct tiny_audio_device *adev)
 {
     voice_status_t call_status;
     int is_call_active;
     call_status =adev->call_status;
     is_call_active = (call_status != VOICE_INVALID_STATUS) && (call_status != VOICE_STOP_STATUS);

     return is_call_active;
 }

static bool out_bypass_data(struct tiny_stream_out *out)
{
    struct tiny_audio_device *adev = out->dev;
    bool by_pass=false;

    if(adev->is_agdsp_asserted) {
        by_pass = true;
    }

    if(adev->is_dsp_loop) {
        by_pass = true;
    }

    if((AUDIO_HW_APP_VOICE_TX==out->audio_app_type) && (false==is_usecase(adev->dev_ctl,UC_CALL))){
        by_pass = true;
    }

    if((AUDIO_HW_APP_VOIP==out->audio_app_type) && (true==adev->fm_record_start)){
        by_pass = true;
    }

    if((AUDIO_DEVICE_OUT_ALL_SCO&out->devices) && (false==adev->bt_sco_status)){
        by_pass = true;
    }

    if(out->testMode){
        by_pass = true;
    }

    if((true==out->standby)&&
        ((AUDIO_DEVICE_OUT_USB_DEVICE==out->devices)||(AUDIO_DEVICE_OUT_USB_HEADSET==out->devices))){
        if(false==is_usbdevice_connected(adev->dev_ctl)){
            by_pass = true;
        }
    }

    return by_pass;
}

static int out_get_presentation_position(const struct audio_stream_out *stream,
        uint64_t *frames, struct timespec *timestamp)
{
    struct tiny_stream_out *out = (struct tiny_stream_out *)stream;
    struct pcm *pcm = NULL;
    int ret = -ENODATA;
    unsigned long dsp_frames;
    pthread_mutex_lock(&out->lock);
    pcm = out->pcm;

    if(out->audio_app_type == AUDIO_HW_APP_OFFLOAD){
        if (out->compress != NULL) {
            compress_get_tstamp(out->compress, &dsp_frames,
                            &out->offload_samplerate);
            *frames = dsp_frames;
            ret = 0;
            clock_gettime(CLOCK_MONOTONIC, timestamp);
        }
    } else{
        if (pcm) {
            size_t avail;
            if (pcm_get_htimestamp(pcm, &avail, timestamp) == 0) {
                //LOG_E("out_get_presentation_position out frames %llu timestamp.tv_sec %ld timestamp.tv_nsec %ld",*frames,timestamp->tv_sec,timestamp->tv_nsec);
                out->last_timespec.tv_sec = timestamp->tv_sec;
                out->last_timespec.tv_nsec = timestamp->tv_nsec;
                size_t kernel_buffer_size = out->config->period_size *
                                            out->config->period_count;
                int64_t signed_frames = out->written - kernel_buffer_size + avail;
                if (signed_frames >= 0) {
                    *frames = signed_frames;
                    ret = 0;
                }
            }
        }else{
            //LOG_E("%s  pcm  is null   !!!", __func__);
            uint64_t signed_frames = out->written;
            *frames = signed_frames;
	    timestamp->tv_sec = out->last_timespec.tv_sec;
	    timestamp->tv_nsec = out->last_timespec.tv_nsec;
            ret=0;
        }
    }
    pthread_mutex_unlock(&out->lock);
    //LOG_E("out_get_presentation_position ret %d",ret);
    return ret;
}

#ifdef SPRD_AUDIO_HIDL_CLIENT
static ssize_t hidl_out_write(struct audio_stream_out *stream, const void* buffer,
    size_t bytes){
    struct tiny_stream_out *out = (struct tiny_stream_out *)stream;
    struct tiny_audio_device *adev = out->dev;
    int ret=0;

    pthread_mutex_lock(&out->lock);
    ret=write_endpoint_outputtest_buffer(&adev->test_ctl,buffer,bytes);
    pthread_mutex_unlock(&out->lock);
    return ret;
}

static ssize_t diag_write(struct audio_stream_out *stream, const void* buffer,
    size_t bytes){
    struct tiny_stream_out *out = (struct tiny_stream_out *)stream;
    struct tiny_audio_device *adev = out->dev;
    struct socket_handle *tunning=&adev->audio_param.tunning;
    int writebytes=0;
    int ret=0;

    LOG_I("diag_write:%d diagsize:%d written:%d",
        bytes,out->diagsize,out->written);

    pthread_mutex_lock(&tunning->diag_lock);
    tunning->diag_cmd_process=true;
    pthread_mutex_unlock(&tunning->diag_lock);

    pthread_mutex_lock(&out->lock);
    if(out->written+(int)bytes<out->diagsize){
        writebytes=(int)bytes;
    }else{
        writebytes=out->diagsize-out->written;
    }

    memcpy(tunning->audio_received_buf+out->written,buffer,writebytes);
    out->written+=writebytes;
    if(out->written>=out->diagsize){
        pthread_mutex_unlock(&out->lock);
        ret=handle_received_data(&adev->audio_param,tunning->audio_received_buf,out->diagsize);
        pthread_mutex_lock(&out->lock);
        out->diagsize=0;
        out->written=0;
    }
    pthread_mutex_unlock(&out->lock);

    pthread_mutex_lock(&tunning->diag_lock);
    tunning->diag_cmd_process=false;
    pthread_mutex_unlock(&tunning->diag_lock);
    LOG_I("diag_write exit writebytes:%d rsp size:%d",writebytes,tunning->send_buffer_size);

    return writebytes;
}

#endif
static void dump_outwrite_time(struct tiny_stream_out * out){
    int i=0;
    LOG_I("out app_type:%d current point:%d",
        out->audio_app_type,
        out->timeID);
    for(i=0;i<AUDIO_OUT_TIME_BUFFER_COUNT;i++){
        LOG_I("time[%d]:%u,%u",i,out->time1[i],out->time2[i]);
    }
}

static int out_app_type_check(UNUSED_ATTR struct tiny_audio_device *adev, int audio_app_type,UNUSED_ATTR bool voip_start,
    UNUSED_ATTR audio_devices_t devices,voice_status_t call_status)
{
    int ret_type = audio_app_type;

    if((audio_app_type == AUDIO_HW_APP_VOIP)
            ||(audio_app_type == AUDIO_HW_APP_PRIMARY)
            ||(audio_app_type == AUDIO_HW_APP_VOIP_BT)){
        if(((VOICE_START_STATUS==call_status)||(VOICE_PRE_START_STATUS==call_status))&&
            ((audio_app_type == AUDIO_HW_APP_PRIMARY) || (audio_app_type == AUDIO_HW_APP_VOIP))) {
            ret_type = AUDIO_HW_APP_PRIMARY;
        } else {
            pthread_mutex_lock(&adev->voip_start_lock);
            if(true == adev->voip_record_start){
                ret_type = AUDIO_HW_APP_VOIP;
            } else {
                ret_type = AUDIO_HW_APP_PRIMARY;
            }
            pthread_mutex_unlock(&adev->voip_start_lock);
        }
    }
    return ret_type;
}

ssize_t out_write_test(struct audio_stream_out *stream, const void *buffer,
    size_t bytes)
{
    int ret = 0;
    struct tiny_stream_out *out = (struct tiny_stream_out *)stream;
    struct tiny_audio_device *adev = out->dev;

    LOG_D("out_write_test enter");

    pthread_mutex_lock(&out->lock);
    if (true==out->standby){
        LOG_I("out_write_test start up bytes:%d",bytes);
        ret = start_output_stream(out);
         if (ret != 0) {
            goto exit;
        }
        out->standby = false;
    }

    audiohal_pcmdump(adev->dev_ctl,out->flags,(void *)buffer,bytes,PCMDUMP_PRIMARY_PLAYBACK_MUSIC);
    ret = normal_out_write(out, (void *)buffer, bytes);

exit:
    if((ret < 0) && (NULL!=out->standby_fun)){
        LOG_W("out_write_test failed %p app type:%d",out,out->audio_app_type);
        out->standby_fun(out->dev,out, out->audio_app_type);
    }
    pthread_mutex_unlock(&out->lock);
    if(ret < 0 ) {
        dump_outwrite_time(out);
        usleep(5*1000);
    }
    LOG_D("out_write_test exit");
    return bytes;
}

static ssize_t out_write(struct audio_stream_out *stream, const void *buffer,
                         size_t bytes)
{
    int ret = 0;
    struct tiny_stream_out *out = (struct tiny_stream_out *)stream;
    struct tiny_audio_device *adev = out->dev;
    int frame_size=0;
    voice_status_t call_status;
    int cur_app_type;
    bool voip_start=false;

    out->time1[out->timeID]=getCurrentTimeMs();

    pthread_mutex_lock(&adev->lock);
    pthread_mutex_lock(&out->lock);

    if(adev->boot_completed==false){
        check_headset_status(out);
    }

    call_status =adev->call_status;
    voip_start=adev->voip_start;

    if (out->audio_app_type == AUDIO_HW_APP_MMAP) {
        ret = 0;
        pthread_mutex_unlock(&adev->lock);
        goto exit;
    }

    if (out->audio_app_type == AUDIO_HW_APP_MMAP) {
        ret = 0;
        pthread_mutex_unlock(&adev->lock);
        goto exit;
    }

    call_status =adev->call_status;

    LOG_D("out_write in:%p call_status:%d usecase:0x%x audio_app_type:%x bytes:%d",
          out, call_status, adev->dev_ctl->usecase,
          out->audio_app_type,bytes);

    frame_size = audio_stream_out_frame_size(&out->stream);
    out->written += bytes /frame_size;

    cur_app_type = out_app_type_check(adev,out->audio_app_type,voip_start,out->devices,call_status);

    if(cur_app_type != out->audio_app_type) {
        LOG_W("%s %d out_app_type_check %d %d",__func__,__LINE__,out->audio_app_type,cur_app_type);
        if(out->standby_fun) {
            out->standby_fun(adev,out,out->audio_app_type);
        }
         out->audio_app_type = cur_app_type;
    }

    if (out_bypass_data(out)){
        LOG_I("out_write type:%x bypass call_status:%d,out->devices:%x",out->audio_app_type,call_status,out->devices);
        if(false==out->testMode){
            if((false==out->standby)&&(NULL!=out->standby_fun)){
                out->standby_fun(out->dev,out, out->audio_app_type);
            }
        }
        pthread_mutex_unlock(&out->lock);
        pthread_mutex_unlock(&adev->lock);
        usleep((int64_t) bytes * 1000000 / audio_stream_out_frame_size(stream) / out_get_sample_rate(&stream->common));
        out->time2[out->timeID++]=getCurrentTimeMs();
        out->timeID=out->timeID%AUDIO_OUT_TIME_BUFFER_COUNT;
        return bytes;
    }
    pthread_mutex_unlock(&adev->lock);
#ifdef AWINIC_EFFECT_SUPPORT
   //ALOGE("%s:Awinic skt before is_module_ready =%d!",__func__, adev->awinic_skt.is_module_ready);
   if((out->audio_app_type != AUDIO_HW_APP_OFFLOAD)&&(out->devices & AUDIO_DEVICE_OUT_SPEAKER))
   {
      // ALOGE("%s:Awinic skt Enter is_module_ready =%d!",__func__, adev->awinic_skt.is_module_ready);
       if(out->config->format != PCM_FORMAT_S16_LE)
       {
            if(out->config->format == PCM_FORMAT_S32_LE) {
                adev->awinic_skt.info.bits_per_sample = 32;
                adev->awinic_skt.info.bit_qactor_sample = 32 - 1;
            } else if(out->config->format == PCM_FORMAT_S24_LE) {
                adev->awinic_skt.info.bits_per_sample = 24;
                adev->awinic_skt.info.bit_qactor_sample = 24 - 1;
            }
	    //ALOGE("%s:Awinic bits_per_sample=%d,is_module_ready =%d ",__func__,adev->awinic_skt.info.bits_per_sample,adev->awinic_skt.is_module_ready);
            adev->awinic_skt.info.sampling_rate = out->config->rate;
            adev->awinic_skt.setMediaInfo(adev->awinic_skt.module_context_buffer,&adev->awinic_skt.info); 
       }
       //ALOGE("%s:Awinic skt process before is_module_ready =%d!",__func__, adev->awinic_skt.is_module_ready);
   //     ALOGD("zhangming test out->flags = moduls_buffer = %p\n",adev->awinic_skt.module_context_buffer);
       if(adev->awinic_skt.is_module_ready == true){
          // ALOGE("%s:Awinic skt process!",__func__);
           adev->awinic_skt.process(adev->awinic_skt.module_context_buffer,buffer,bytes);
       }
   }
#endif
    if (true==out->standby){
        LOG_I("out_write start up bytes:%d",bytes);
        if(out->audio_app_type == AUDIO_HW_APP_OFFLOAD){
            ret = audio_start_compress_output(&out->stream);
        }
        else {
            ret = start_output_stream(out);
        }
         if (ret != 0) {
            goto exit;
        }
        out->standby = false;
    }

    if(out->audio_app_type == AUDIO_HW_APP_OFFLOAD){
        ret = out_write_compress(&out->stream, buffer, bytes);
        audiohal_pcmdump(adev->dev_ctl,out->flags,(void *)buffer,bytes,PCMDUMP_OFFLOAD_PLAYBACK_DSP);
    }else if(AUDIO_HW_APP_VOIP==out->audio_app_type){
        audiohal_pcmdump(adev->dev_ctl,out->flags,(void *)buffer,bytes,PCMDUMP_VOIP_PLAYBACK_VBC);
        ret = normal_out_write(out, (void *)buffer, bytes);
    }else if(AUDIO_HW_APP_VOICE_TX==out->audio_app_type){
        audiohal_pcmdump(adev->dev_ctl,out->flags,(void *)buffer,bytes,PCMDUMP_VOICE_TX);
        ret = normal_out_write(out, (void *)buffer, bytes);
    }  else {
        audiohal_pcmdump(adev->dev_ctl,out->flags,(void *)buffer,bytes,PCMDUMP_PRIMARY_PLAYBACK_MUSIC);
        ret = normal_out_write(out, (void *)buffer, bytes);
    }
exit:
    if((ret < 0) && (NULL!=out->standby_fun)){
        LOG_W("out_write failed %p app type:%d",out,out->audio_app_type);
//        dump_all_audio_reg(-1,adev->dump_flag);
        out->standby_fun(out->dev,out, out->audio_app_type);
    }
    out->time2[out->timeID++]=getCurrentTimeMs();
    if(out->timeID>=AUDIO_OUT_TIME_BUFFER_COUNT){
        out->timeID=0;
    }
    pthread_mutex_unlock(&out->lock);
    if(ret < 0 ) {
        dump_outwrite_time(out);
        LOG_E("out_write error");
        usleep(5*1000);
    }
    LOG_D("out_write exit");
    if(out->audio_app_type != AUDIO_HW_APP_OFFLOAD){
        return bytes;
    }else{
        return ret;
    }
}
static int out_get_render_position(const struct audio_stream_out *stream,
                                   uint32_t *dsp_frames)
{
    struct tiny_stream_out *out = (struct tiny_stream_out *)stream;
    if (dsp_frames == NULL)
    { return -1; }

    if (out->audio_app_type == AUDIO_HW_APP_OFFLOAD) {
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
    LOG_W("out_get_render_position failed %p app type:%d",out,out->audio_app_type);
    return -1;
}

static int out_add_audio_effect(UNUSED_ATTR const struct audio_stream *stream,
                                UNUSED_ATTR effect_handle_t effect)
{
    return 0;
}

static int out_remove_audio_effect(UNUSED_ATTR const struct audio_stream *stream,
                                  UNUSED_ATTR effect_handle_t effect)
{
    return 0;
}

/** audio_stream_in implementation **/

static int get_next_buffer(struct resampler_buffer_provider *buffer_provider,
                           struct resampler_buffer *buffer);

static void release_buffer(struct resampler_buffer_provider *buffer_provider,
                           struct resampler_buffer *buffer);

static int out_deinit_resampler(struct tiny_stream_out *out)
{
    if (out->resampler) {
        release_resampler(out->resampler);
        out->resampler = NULL;
    }

    if (out->buffer) {
        free(out->buffer);
        out->buffer = NULL;
    }
    return 0;
}

static int out_init_resampler(struct tiny_stream_out *out)
{
    int ret = 0;
    int size = 0;
    int channel_count = popcount(out->request_config.channel_mask);

    size =
        out->config->period_count*out->config->period_size *
        audio_stream_out_frame_size(&out->stream);

    if(!size ) {
        size = RESAMPLER_BUFFER_SIZE;
    }

    if(NULL!=out->buffer){
        free(out->buffer);
    }

    out->resampler_buffer_size=4*size*out->config->rate*out->config->channels/(out->requested_rate*channel_count);

    out->buffer = malloc(out->resampler_buffer_size);
    if (!out->buffer) {
        LOG_E("in_init_resampler: alloc fail, size: %d", out->resampler_buffer_size);
        ret = -ENOMEM;
        goto err;
    }

    memset(out->buffer,0,out->resampler_buffer_size);

    LOG_I("out_init_resampler malloc buffer size:%d channel_count:%d",
        out->resampler_buffer_size,channel_count);
    ret = create_resampler(out->requested_rate,
                           out->config->rate,
                           channel_count,
                           RESAMPLER_QUALITY_DEFAULT,
                           NULL, &out->resampler);
    if (ret != 0) {
        ret = -EINVAL;
        goto err;
    }

    return ret;

err:
    out_deinit_resampler(out);
    return ret;
}

static int in_deinit_resampler(struct tiny_stream_in *in)
{
    LOG_I("in_deinit_resampler:%p",in->resampler);

    if (in->resampler) {
        release_resampler(in->resampler);
        in->resampler = NULL;
    }
    if (in->buffer) {
        free(in->buffer);
        in->buffer = NULL;
    }
    return 0;
}

static int in_init_resampler(struct tiny_stream_in *in)
{
    int ret = 0;
    int size = 0;
    in->buf_provider.get_next_buffer = get_next_buffer;
    in->buf_provider.release_buffer = release_buffer;
    LOG_I("in_init_resampler");
    size = 4*
        in->config->period_size *
        audio_stream_in_frame_size_l(&in->stream);

    if(NULL==in->buffer){
        in->buffer = malloc(size);
    }

    if (!in->buffer) {
        LOG_E("in_init_resampler: alloc fail, size: %d", size);
        ret = -ENOMEM;
        goto err;
    } else {
        memset(in->buffer, 0, size);
    }

    if(in->resampler!=NULL){
        LOG_W("in_init_resampler resampler is:%p",in->resampler);
    }
    LOG_I("in_init_resampler %d %d %d",
        in->config->rate,in->requested_rate,in->config->channels);
    ret = create_resampler(in->config->rate,
                           in->requested_rate,
                           in->config->channels,
                           RESAMPLER_QUALITY_DEFAULT,
                           &in->buf_provider, &in->resampler);
    if (ret != 0) {
        ret = -EINVAL;
        goto err;
    }

    return ret;

err:
    in_deinit_resampler(in);
    return ret;
}

/*  if bt sco playback stream is not started and bt sco capture stream is started, we will
    start duplicate_thread to write zero data to bt_sco_card.
    we will stop duplicate_thread to write zero data to bt_sco_card if bt sco playback stream
    is started or bt sco capture stream is stoped.
*/

static int do_normal_inputput_standby(void *dev,void * in,UNUSED_ATTR AUDIO_HW_APP_T type)
{
    struct tiny_audio_device *adev=(struct tiny_audio_device *)dev;
    int usecase = UC_UNKNOWN;
    struct tiny_stream_in *normal_in=(struct tiny_stream_in *)in;
    if(normal_in->standby) {
        return -1;
    }
    LOG_I("do_normal_inputput_standby:%p,app_byte:%d",normal_in->pcm, normal_in->audio_app_type);
    if (NULL!=normal_in->pcm) {
        LOG_I("do_normal_inputput_standby pcm_close:%p",normal_in->pcm);
        pcm_close(normal_in->pcm);
        normal_in->pcm = NULL;
    }

    if (NULL!=normal_in->channel_buffer) {
        free(normal_in->channel_buffer);
        normal_in->channel_buffer = NULL;
    }

    if (NULL!=normal_in->proc_buf) {
        free(normal_in->proc_buf);
        normal_in->proc_buf = NULL;
    }
    normal_in->proc_buf_size=0;

    in_deinit_resampler(in);

    if (normal_in->nr_buffer) {
        free(normal_in->nr_buffer);
        normal_in->nr_buffer = NULL;
    }

    if (normal_in->active_rec_proc) {
        AUDPROC_DeInitDp();
        normal_in->active_rec_proc = false;

        if(normal_in->rec_nr_handle) {
            AudioRecordNr_Deinit(normal_in->rec_nr_handle);
            normal_in->rec_nr_handle = NULL;
        }
    }

    normal_in->standby=true;
    usecase = stream_type_to_usecase(normal_in->audio_app_type);
    set_usecase(adev->dev_ctl,usecase,false);
    set_audioparam(adev->dev_ctl,PARAM_USECASE_CHANGE,NULL,false);

    if((AUDIO_HW_APP_NORMAL_RECORD==normal_in->audio_app_type)&&(NULL!=normal_in->recordproc_handle)){
        recordproc_deinit(normal_in->recordproc_handle);
        normal_in->recordproc_handle = NULL;
    }
    pthread_mutex_lock(&adev->voip_start_lock);
    if (normal_in->audio_app_type == AUDIO_HW_APP_VOIP_RECORD) {
        adev->voip_record_start = false;
    }
    pthread_mutex_unlock(&adev->voip_start_lock);
    LOG_I("do_normal_inputput_standby exit");
    return 0;
}

UNUSED_ATTR static bool check_support_nr(struct tiny_audio_device *adev,int rate){
    bool nr_param_enable=true;
    int nr_mask=RECORD_UNSUPPORT_RATE_NR;

    struct audio_record_proc_param *param_data = NULL;
    NR_CONTROL_PARAM_T *nr_control=NULL;

    param_data=(struct audio_record_proc_param *)get_ap_record_param(adev->dev_ctl->audio_param,adev->dev_ctl->in_devices);
    if(NULL==param_data){
        return false;
    }

    nr_control = &param_data->nr_control;

    if((nr_control->nr_switch)){
        nr_param_enable=true;
    }else{
        nr_param_enable=false;
    }

    switch(rate){
        case 48000:
            nr_mask=RECORD_48000_NR;
            break;
        case 44100:
            nr_mask=RECORD_44100_NR;
            break;
        case 16000:
            nr_mask=RECORD_16000_NR;
            break;
        case 8000:
            nr_mask=RECORD_8000_NR;
            break;
        default:
            LOG_I("check_support_nr rate:%d nr:%d",
                rate,adev->dev_ctl->config.enable_other_rate_nr);
            return adev->dev_ctl->config.enable_other_rate_nr;
    }

    if((adev->dev_ctl->config.nr_mask&(1<<nr_mask))&&(true==nr_param_enable)){
        return true;
    }else{
        return false;
    }
}

/* must be called with hw device and input stream mutexes locked */
static int start_input_stream(struct tiny_stream_in *in)
{
    struct tiny_audio_device *adev = in->dev;
    int pcm_devices=-1;
    int ret = 0;
    int usecase = UC_UNKNOWN;
    struct audio_control *control = NULL;
    int old_period_size;

    control = adev->dev_ctl;

    LOG_I("start_input_stream usecase:%x type:%d,adev->voip_start:%d, source:%d",control->usecase,in->audio_app_type,adev->voip_start, in->source);

    if((AUDIO_DEVICE_NONE==in->devices)
        &&(AUDIO_HW_APP_CALL_RECORD!=in->audio_app_type)){
        LOG_W("start_input_stream invalid mic");
        return -3;
    }

    usecase = stream_type_to_usecase(in->audio_app_type);
    ret = set_usecase(control,  usecase, true);
    if(ret < 0) {
        ALOGE("start_input_stream:control->usecase:%x, app_type:%d setusecase failed:%d",control->usecase,in->audio_app_type,ret);
        return -2;
    }

    switch_vbc_route(adev->dev_ctl,in->devices);
    ret = dev_ctl_get_in_pcm_config(control, in->audio_app_type, &pcm_devices, in->config);
    if(ret!= 0){
        goto error;
    }

    if(is_usecase_unlock(control, UC_MM_RECORD|UC_RECOGNITION|UC_MMAP_RECORD)){
        set_record_source(control, in->source);
    }

    if(AUDIO_HW_APP_CALL_RECORD==in->audio_app_type){
        if(in->source == AUDIO_SOURCE_VOICE_UPLINK) {
            ret=mixer_ctl_set_value(control->voice_ul_capture, 0, VOICE_UL_CAPTURE_UPLINK);
            LOG_I("start voice_rx VOICE_UL_CAPTURE_UPLINK");
        } else if(in->source == AUDIO_SOURCE_VOICE_DOWNLINK) {
            ret=mixer_ctl_set_value(control->voice_ul_capture, 0, VOICE_UL_CAPTURE_DOWNLINK);
            LOG_I("start voice_rx VOICE_UL_CAPTURE_DOWNLINK");
        }else{
            ret=mixer_ctl_set_value(control->voice_ul_capture, 0, VOICE_UL_CAPTURE_UPLINK_DOWNLINK);
            LOG_I("start voice_rx VOICE_UL_CAPTURE_UPLINK_DOWNLINK");
        }
        if(ret!=0){
            LOG_E("%s set voice capture type failed:%d",__func__,ret);
        }
    }

    if(AUDIO_HW_APP_RECOGNITION==in->audio_app_type){
        in->active_rec_proc = init_rec_process(&in->stream, in->config->rate);
        LOG_I("start_input_stream check_support_nr:%d", check_support_nr(adev,in->config->rate));
        if((in->config->rate == 48000)
            && check_support_nr(adev,in->config->rate)
            &&(true== in->active_rec_proc)){

            if(NULL==in->nr_buffer){
                in->nr_buffer = malloc(in->config->period_size *
                audio_stream_in_frame_size(&in->stream)*4);
            }

            if (!in->nr_buffer) {
                LOG_E("in_init_resampler: alloc fail");
                ret = -ENOMEM;
                goto error;
            } else {
                memset(in->nr_buffer, 0, in->config->period_size *
                    audio_stream_in_frame_size(&in->stream)*4);
            }

            in->rec_nr_handle = init_nr_process(&in->stream);
        }else{
            in->rec_nr_handle =NULL;
        }
    }else{
        in->rec_nr_handle =NULL;
        in->active_rec_proc=false;
    }

    old_period_size = in->config->period_size;

    if (in->requested_rate != in->config->rate && (in->audio_app_type == AUDIO_HW_APP_NORMAL_RECORD)
        && samplerate_dsp_support(in->requested_rate)){
        in->config->period_size = (((((in->config->period_size * in->config->period_count * 1000)/in->config->rate)
                                                            * in->requested_rate)/(1000 * in->config->period_count))/160)*160;
        in->config->rate = in->requested_rate;
    }

    if ((in->requested_rate != in->config->rate)&&(in->audio_app_type == AUDIO_HW_APP_RECOGNITION)){
        if(in->rec_nr_handle!=NULL){
            in->config->period_size = (((((in->config->period_size * in->config->period_count * 1000)/in->config->rate)
                * 48000)/(1000 * in->config->period_count))/160)*160;
            in->config->rate=48000;
        }else{
            in->config->period_size = (((((in->config->period_size * in->config->period_count * 1000)/in->config->rate)
                * in->requested_rate)/(1000 * in->config->period_count))/160)*160;
            in->config->rate = in->requested_rate;
        }
    }
    if(in->config->start_threshold && old_period_size) {
        in->config->start_threshold = (in->config->start_threshold * in->config->period_size)/old_period_size;
        if(in->config->start_threshold<in->config->period_size){
            in->config->start_threshold =in->config->period_size;
        }
    }
    LOG_I("start_input_stream pcm_open pcm device:%d config rate:%d channels:%d format:%d period_size:%d ,start_threshhold:%d,old period_size:%d",pcm_devices,
        in->config->rate,in->config->channels,in->config->format,in->config->period_size,
        in->config->start_threshold,old_period_size);
    in->pcm =
        pcm_open(adev->dev_ctl->cards.s_tinycard, pcm_devices, PCM_IN | PCM_MMAP | PCM_NOIRQ|PCM_MONOTONIC , in->config);

    LOG_I("start_input_stream pcm_open end");

    if (!pcm_is_ready(in->pcm)) {
        LOG_E("start_input_stream:cannot open pcm err:%s",
              pcm_get_error(in->pcm));
        goto error;
    }

    if(in->resampler!=NULL){
        in_deinit_resampler(in);
        in->resampler=NULL;
    }

    if(in->config->rate !=in->requested_rate){
        LOG_I("start_input_stream create_resampler");
        ret = in_init_resampler(in);
        if (ret) {
            LOG_E(": in_init_resampler failed");
            goto error;
        }
        in->resampler->reset(in->resampler);
        in->frames_in = 0;
    }

    LOG_I("start_input_stream pcm_open devices:%d pcm:%p %d requested_rate:%d requested channels:%d ctrl_devices:%x,in->devices:%x",pcm_devices,in->pcm
        ,in->config->rate,in->requested_rate,in->requested_channels,adev->dev_ctl->in_devices,in->devices);

    if((in->audio_app_type==AUDIO_HW_APP_NORMAL_RECORD)||(in->audio_app_type==AUDIO_HW_APP_VOIP_RECORD)
        || (in->audio_app_type==AUDIO_HW_APP_BT_RECORD)){
        pthread_mutex_lock(&adev->voip_start_lock);
        adev->normal_record_start = true;
        if (in->audio_app_type == AUDIO_HW_APP_VOIP_RECORD) {
            adev->voip_record_start = true;
        } else {
            adev->voip_record_start = false;
        }
        pthread_mutex_unlock(&adev->voip_start_lock);
    }

    if ((!in->pop_mute || !in->pop_mute_bytes)&&
           (in->audio_app_type == AUDIO_HW_APP_NORMAL_RECORD ||
            in->audio_app_type == AUDIO_HW_APP_RECOGNITION)) {
        if(in->requested_rate) {
            in->pop_mute_bytes = RECORD_STANDBY_POP_MIN_TIME_MIC*in->requested_rate/1000*audio_stream_in_frame_size(&(in->stream));
            in->pop_mute = true;
        }
    }

    in->standby_fun = do_normal_inputput_standby;

    select_devices_new(adev->dev_ctl,in->audio_app_type,in->devices,true,false,true, true);

    if(in->audio_app_type == AUDIO_HW_APP_FM_RECORD){
        clear_fm_param_state(adev->dev_ctl);
    }
    set_audioparam(adev->dev_ctl,PARAM_USECASE_DEVICES_CHANGE,NULL,true);

    if(in->audio_app_type == AUDIO_HW_APP_NORMAL_RECORD) {
        struct audio_record_proc_param *param_data=NULL;
        param_data=(struct audio_record_proc_param *)get_ap_record_param(adev->dev_ctl->audio_param,in->devices);
        if(NULL==param_data){
          LOG_I("audio record process param is null");
          goto error;
        }
        in->recordproc_handle= recordproc_init();
        ALOGI("PostProcessing_Init out.");
        recordproc_setparameter(in->recordproc_handle,param_data);
        recordproc_enable(in->recordproc_handle);
    }

#ifdef AUDIO_DEBUG
    if(in->audio_app_type != AUDIO_HW_APP_RECOGNITION){
        debug_dump_start(&adev->debugdump,DEFAULT_REG_DUMP_COUNT);
    }
#endif
    return 0;

error:

    if (in->proc_buf) {
        free(in->proc_buf);
        in->proc_buf = NULL;
    }
    in->proc_buf_size=0;

    if(in->pcm) {
        LOG_E("%s: pcm open error: %s", __func__,
              pcm_get_error(in->pcm));

        pcm_close(in->pcm);
        in->pcm = NULL;
    }

    in_deinit_resampler(in);

    if (in->active_rec_proc) {
        AUDPROC_DeInitDp();
        in->active_rec_proc = false;
    }

    if(in->rec_nr_handle){
        AudioRecordNr_Deinit(in->rec_nr_handle);
        in->rec_nr_handle = NULL;
    }
    set_usecase(control,  usecase, false);
    set_audioparam(adev->dev_ctl,PARAM_USECASE_CHANGE,NULL,false);
    return -1;
}

static uint32_t in_get_sample_rate(const struct audio_stream *stream)
{
    struct tiny_stream_in *in = (struct tiny_stream_in *)stream;
    return in->requested_rate;
}

static int in_set_sample_rate(UNUSED_ATTR struct audio_stream *stream, uint32_t rate)
{
    LOG_I("in_set_sample_rate %d",rate);
    return 0;
}

#ifdef AUDIOHAL_V4
static int in_get_active_microphones(const struct audio_stream_in *stream,
                                     struct audio_microphone_characteristic_t *mic_array,
                                     size_t *mic_count) {
    struct tiny_stream_in *in = (struct tiny_stream_in *)stream;
    struct tiny_audio_device *adev =(struct tiny_audio_device *) in->dev;

    pthread_mutex_lock(&adev->lock);
    pthread_mutex_lock(&in->lock);
    int ret = platform_get_active_microphones(&adev->dev_ctl->config.audio_data,in->devices,
                                              in->config->channels,
                                              in->source,mic_array,mic_count);
    pthread_mutex_unlock(&in->lock);
    pthread_mutex_unlock(&adev->lock);
    return ret;
}
#endif

static size_t in_get_buffer_size(const struct audio_stream *stream)
{
    size_t size;

    struct tiny_stream_in *in = (struct tiny_stream_in *)stream;

    if (check_input_parameters
        (in->requested_rate, AUDIO_FORMAT_PCM_16_BIT,
         in->config->channels) != 0) {
        LOG_I("%p in_get_buffer_size 0 requested_rate:%d channels:%d"
            ,in,in->requested_rate,in->config->channels);
        return 0;
    }

    /*  take resampling into account and return the closest majoring
        multiple of 16 frames, as audioflinger expects audio buffers to
        be a multiple of 16 frames */
    if(in->requested_rate != in->config->rate){
        if(in->audio_app_type == AUDIO_HW_APP_NORMAL_RECORD || in->audio_app_type == AUDIO_HW_APP_FM_RECORD){
            in->config->period_size = (((((in->config->period_size * in->config->period_count * 1000)/in->config->rate)
                                                                * in->requested_rate)/(1000 * in->config->period_count))/160)*160;
        }else{
            in->config->period_size = (((in->config->period_size * in->config->period_count * 1000)/in->config->rate)
                                                            * in->requested_rate)/(1000 * in->config->period_count);
        }
    }
    size = in->config->period_size;
    size = ((size + 15) / 16) * 16;
    LOG_I("%p in_get_buffer_size:%d",in,size * in->requested_channels * sizeof(short));
    return size * in->requested_channels * sizeof(short);
}

static uint32_t in_get_channels(const struct audio_stream *stream)
{
    struct tiny_stream_in *in = (struct tiny_stream_in *)stream;
  //  LOG_I("%p in_get_channels:%d",in,in->requested_channels);

    if (1==in->requested_channels) {
//   if (1==in->config->channels) {
        return AUDIO_CHANNEL_IN_MONO;
    } else {
        return AUDIO_CHANNEL_IN_STEREO;
    }
}

static audio_format_t in_get_format(UNUSED_ATTR const struct audio_stream *stream)
{
    return AUDIO_FORMAT_PCM_16_BIT;
}

static int in_set_format(UNUSED_ATTR struct audio_stream *stream, UNUSED_ATTR audio_format_t format)
{
    return 0;
}

/* must be called with hw device and input stream mutexes locked */
int do_input_standby(struct tiny_stream_in *in)
{
    struct tiny_audio_device *adev = in->dev;

    pthread_mutex_lock(&adev->voip_start_lock);
    if((in->audio_app_type==AUDIO_HW_APP_NORMAL_RECORD)||(in->audio_app_type==AUDIO_HW_APP_VOIP_RECORD)
        ||(in->audio_app_type==AUDIO_HW_APP_BT_RECORD)){
        adev->normal_record_start = false;
        if(false==in->standby){
           adev->voip_record_start = false;
        }
    }
    pthread_mutex_unlock(&adev->voip_start_lock);

    pthread_mutex_lock(&adev->lock);
    if(in->audio_app_type==AUDIO_HW_APP_FM_RECORD){
        adev->fm_record_start = false;
    }
    pthread_mutex_unlock(&adev->lock);

    if(NULL!=in->standby_fun) {
        pthread_mutex_lock(&in->lock);
        in->standby_fun(adev,in,in->audio_app_type);
        pthread_mutex_unlock(&in->lock);
    }
    return 0;
}

void force_in_standby(struct tiny_audio_device *adev,AUDIO_HW_APP_T audio_app_type)
{
    struct tiny_stream_in *in=NULL;

    struct listnode *item;
    struct listnode *tmp;
    struct listnode *list = NULL;

    LOG_I("force_in_standby audio_app_type:0x%x", audio_app_type);

    pthread_mutex_lock(&adev->input_list_lock);
    list=&adev->active_input_list;
    list_for_each_safe(item, tmp, list){
        in = node_to_item(item, struct tiny_stream_in, node);
        if(in->audio_app_type==audio_app_type){
            if (NULL!=in->standby_fun) {
                pthread_mutex_lock(&in->lock);
                LOG_I("force_in_standby audio_app_type:0x%x in:%p node:%p list:%p,app_byte:%d",
                    audio_app_type,in,&(in->node),list,in->audio_app_type);
                in->standby_fun(adev,in,in->audio_app_type);
                pthread_mutex_unlock(&in->lock);
            }
         }
    }
    pthread_mutex_unlock(&adev->input_list_lock);
    LOG_I("force_in_standby:exit");
}


static int in_standby(struct audio_stream *stream)
{
    int status=0;
    struct tiny_stream_in *in = (struct tiny_stream_in *)stream;

    LOG_I("in_standby:%p",stream);
    if(NULL!=stream){
        status = do_input_standby(in);
        pthread_mutex_lock(&in->lock);
        if(in->requested_rate) {
            in->pop_mute_bytes = RECORD_STANDBY_POP_MIN_TIME_MIC*in->requested_rate/1000*audio_stream_in_frame_size(&(in->stream));
            in->pop_mute = true;
        }
        pthread_mutex_unlock(&in->lock);
    }
    return status;
}

static int in_dump(const struct audio_stream *stream, int fd)
{
    struct tiny_stream_in *in = (struct tiny_stream_in *)stream;
    char buffer[DUMP_BUFFER_MAX_SIZE]={0};
    LOG_I("in_dump enter");

    if(in->standby){
        snprintf(buffer,(DUMP_BUFFER_MAX_SIZE-1),
            "\ninput dump ---->\n"
            "source:%d\n",
            in->source);
        AUDIO_DUMP_WRITE_STR(fd,buffer);
        memset(buffer,0,sizeof(buffer));
    }else{
        snprintf(buffer,(DUMP_BUFFER_MAX_SIZE-1),
            "\ninput dump ---->\n"
            "pcm:%p"
            "requested rate:%d channel:%d"
            "resampler:%p "
            "device:0x%x "
            "frames_in:%d\n",
            in->pcm,
            in->requested_rate,
            in->requested_channels,
            in->resampler,
            in->devices,
            in->frames_in);
        AUDIO_DUMP_WRITE_STR(fd,buffer);
        memset(buffer,0,sizeof(buffer));

        snprintf(buffer,(DUMP_BUFFER_MAX_SIZE-1),
            "buffer:%p %p %p %p "
            "pop_mute:%d "
            "pop_mute_bytes:%d\n",
            in->buffer,
            in->channel_buffer,
            in->nr_buffer,
            in->proc_buf,
            in->pop_mute,
            in->pop_mute_bytes);
        AUDIO_DUMP_WRITE_STR(fd,buffer);
        memset(buffer,0,sizeof(buffer));

        snprintf(buffer,(DUMP_BUFFER_MAX_SIZE-1),
            "read_status:%d "
            "frames_read:%lld "
            "source:%d "
            "active_rec_proc:0x%x "
            "audio_app_type:%d "
            "proc_buf_size:%d "
            "proc_frames_in:%d\n",
            in->read_status,
            in->frames_read,
            in->source,
            in->active_rec_proc,
            in->audio_app_type,
            in->proc_buf_size,
            in->proc_frames_in);
        AUDIO_DUMP_WRITE_STR(fd,buffer);
        memset(buffer,0,sizeof(buffer));
    }

    snprintf(buffer,(DUMP_BUFFER_MAX_SIZE-1),"<----input dump\n");
    AUDIO_DUMP_WRITE_STR(fd,buffer);
    LOG_I("in_dump exit");
    return 0;
}

static int in_set_parameters(struct audio_stream *stream, const char *kvpairs)
{
    struct tiny_stream_in *in = (struct tiny_stream_in *)stream;
    struct tiny_audio_device *adev = in->dev;

    struct str_parms *parms;
    char value[32];
    int ret=0, val = 0;

    parms = str_parms_create_str(kvpairs);
    LOG_I("in_set_parameters:%s",kvpairs);
    ret =
        str_parms_get_str(parms, AUDIO_PARAMETER_STREAM_INPUT_SOURCE, value,
                          sizeof(value));
    if (ret >= 0) {
        val = atoi(value);
        pthread_mutex_lock(&in->lock);
        if(in->source != val){
            in->source = val;
            pthread_mutex_unlock(&in->lock);
            LOG_I("in_set_parameters, the current source(%d) has changed!", val);
            do_input_standby(in);
        }else{
            LOG_D("in_set_parameters, the current source(%d) no changed!", val);
            pthread_mutex_unlock(&in->lock);
        }
        //TODO
    }

    ret =
        str_parms_get_str(parms, AUDIO_PARAMETER_STREAM_ROUTING, value,
                          sizeof(value));
    if (ret >= 0) {
        val = atoi(value);
        if(in->audio_app_type == AUDIO_HW_APP_FM_RECORD){
            LOG_I("FM is recording");
        }else{
            pthread_mutex_lock(&in->lock);
            if (val != 0) {
                in->devices = val;
            }
            pthread_mutex_unlock(&in->lock);
            select_devices_new(adev->dev_ctl,in->audio_app_type,val,true,true,false,false);
        }
        ret = 0;
    }

#ifdef SPRD_AUDIO_HIDL_CLIENT
    ret = str_parms_get_str(parms,"hidlstream", value, sizeof(value));
    if(ret >= 0){
        val = atoi(value);
        if(val){
             in->stream.read = hidl_in_read;
         }
    }

    ret = str_parms_get_str(parms,"hidldiagstream", value, sizeof(value));
    if(ret >= 0){
        val = atoi(value);
        if(val){
            pthread_mutex_lock(&in->lock);
            in->stream.read = diag_read;
            pthread_mutex_unlock(&in->lock);
         }
    }


    ret = str_parms_get_str(parms, "SprdAudioNpiReadDataSize", value, sizeof(value));
    if (ret >= 0) {
        val = strtoul(value,NULL,0);
        set_hidl_read_size(adev,val);
        LOG_I("SprdAudioNpiReadDataSize:%d",val);
    }
#endif
    str_parms_destroy(parms);
    return 0;
}

static char *in_get_parameters(const struct audio_stream *stream,
                               const char *keys)
{
     struct str_parms *query = str_parms_create_str(keys);
     struct tiny_stream_in *in = (struct tiny_stream_in *)stream;
     struct tiny_audio_device *adev = in->dev;
     struct socket_handle *tunning=&adev->audio_param.tunning;
     char rsp_buffer[32]={0};
     LOG_I("%p in_get_parameters:%s",in,keys);

    /* supported sample formats */
     if (str_parms_has_key(query, AUDIO_PARAMETER_STREAM_SUP_FORMATS)) {
         str_parms_destroy(query);
         return strdup("sup_formats=AUDIO_FORMAT_PCM_16_BIT");
     }
#ifdef SPRD_AUDIO_HIDL_CLIENT
     if (str_parms_has_key(query, "AudioDiagDataSize")){
        if(in->stream.read == diag_read){
            pthread_mutex_lock(&tunning->diag_lock);
            if((tunning->send_buffer_size==0)&&(tunning->diag_cmd_process==true)){
                tunning->readthreadwait=true;
                pthread_mutex_unlock(&tunning->diag_lock);
                sem_wait(&tunning->sem_wakeup_readthread);
                pthread_mutex_lock(&tunning->diag_lock);
            }
            snprintf(rsp_buffer,sizeof(rsp_buffer)-1,
                "AudioDiagDataSize=%d",tunning->send_buffer_size);
            pthread_mutex_unlock(&tunning->diag_lock);

            str_parms_destroy(query);
            LOG_I("in_get_parameters AudioDiagDataSize:%s",rsp_buffer);
            return strdup(rsp_buffer);
        }
     }

    if (str_parms_has_key(query, "AudioDiagMaxDataSize")){
       if(in->stream.read == diag_read){
           snprintf(rsp_buffer,sizeof(rsp_buffer)-1,
               "%d",adev->dev_ctl->config.audiotester_config.DiagBufferSize);
           str_parms_destroy(query);
           LOG_I("in_get_parameters:%s",rsp_buffer);
           return strdup(rsp_buffer);
       }
    }
#endif
    str_parms_destroy(query);

    return strdup("");
}

static int in_set_gain(UNUSED_ATTR struct audio_stream_in *stream, UNUSED_ATTR float gain)
{
    return 0;
}

static int get_next_buffer(struct resampler_buffer_provider *buffer_provider,
                           struct resampler_buffer *buffer)
{
    struct tiny_stream_in *in;
    struct tiny_audio_device *adev = NULL;
    int ret=-1;

    int period_size=0;
    if (buffer_provider == NULL || buffer == NULL)
    { return -EINVAL; }

    in = container_of(buffer_provider, struct tiny_stream_in, buf_provider);
    adev =(struct tiny_audio_device *) in->dev;

    period_size=in->config->period_size *
        audio_stream_in_frame_size_l((const struct audio_stream_in *)(&in->stream));

    if (in->pcm == NULL) {
        buffer->raw = NULL;
        buffer->frame_count = 0;
        in->read_status = -ENODEV;
        return -ENODEV;
    }

    if (in->frames_in == 0){
        if (in->pcm) {
            if((in->config->rate == 48000) && (in->rec_nr_handle) && (in->active_rec_proc)){
                ret = AudioRecordNr_Proc(in->rec_nr_handle,in->nr_buffer, period_size);
                if(ret <0){
                    LOG_E("AudioRecordNr_Proc  error");
                }

                memcpy(in->buffer,in->nr_buffer,period_size);

                if(AUDIO_HW_APP_NORMAL_RECORD==in->audio_app_type){
                    audiohal_pcmdump(adev->dev_ctl,in->source,(void *)in->nr_buffer,period_size,PCMDUMP_NORMAL_RECORD_NR);
                }

                in->read_status =ret;
            }else{
                in->read_status = _pcm_read(in, in->buffer, period_size);

                if(in->active_rec_proc){
                    aud_rec_do_process(in->buffer, period_size,in->config->channels,
                        in->proc_buf, in->proc_buf_size);
                     if(AUDIO_HW_APP_NORMAL_RECORD==in->audio_app_type){
                        audiohal_pcmdump(adev->dev_ctl,in->source,(void *)in->buffer,period_size,PCMDUMP_NORMAL_RECORD_PROCESS);
                    }
                }
            }
        }

        if (in->read_status != 0) {
            if (in->pcm) {
                LOG_E
                ("get_next_buffer() pcm_read sattus=%d, error: %s",
                 in->read_status, pcm_get_error(in->pcm));
            }
            buffer->raw = NULL;
            buffer->frame_count = 0;
            return in->read_status;
        }
        in->frames_in = in->config->period_size;
    }
    buffer->frame_count = (buffer->frame_count > in->frames_in) ?
                          in->frames_in : buffer->frame_count;
    buffer->i16 = in->buffer + (in->config->period_size - in->frames_in) *
                  in->config->channels;

    return in->read_status;
}

static void release_buffer(struct resampler_buffer_provider *buffer_provider,
                           struct resampler_buffer *buffer)
{
    struct tiny_stream_in *in;

    if (buffer_provider == NULL || buffer == NULL)
    { return; }

    in = container_of(buffer_provider, struct tiny_stream_in, buf_provider);

    in->frames_in -= buffer->frame_count;
}

/*  read_frames() reads frames from kernel driver, down samples to capture rate
    if necessary and output the number of frames requested to the buffer specified */
static ssize_t read_frames(struct tiny_stream_in *in, void *buffer,
                           ssize_t frames)
{
    ssize_t frames_wr = 0;
    while (frames_wr < frames) {
        size_t frames_rd = frames - frames_wr;
        if (in->resampler != NULL) {
            LOG_D("resample_from_provider");
            in->resampler->resample_from_provider(in->resampler,
                (int16_t *) ((char *) buffer +frames_wr *audio_stream_in_frame_size_l(
                (const struct audio_stream_in *)(&in->stream))),&frames_rd);
        } else {
            struct resampler_buffer buf;
            buf.raw=NULL;
            buf.frame_count=frames_rd;
            get_next_buffer(&in->buf_provider, &buf);
            if (buf.raw != NULL) {
                memcpy((char *)buffer +
                       frames_wr *
                       audio_stream_in_frame_size_l((const struct
                                                audio_stream_in
                                                *)(&in->stream.
                                                   common)),
                       buf.raw,
                       buf.frame_count *
                       audio_stream_in_frame_size_l((const struct
                                                audio_stream_in
                                                *)(&in->stream.
                                                   common)));
                frames_rd = buf.frame_count;
            }
            release_buffer(&in->buf_provider, &buf);
        }
        if (in->read_status != 0) {
            return in->read_status;
        }

        frames_wr += frames_rd;
    }
    return frames_wr;
}

static bool in_bypass_data(struct tiny_stream_in *in, voice_status_t voice_status)
{
    struct tiny_audio_device *adev = in->dev;

    if((AUDIO_HW_APP_CALL_RECORD==in->audio_app_type) && (voice_status!=VOICE_START_STATUS)){
        return true;
    }

    if((in->audio_app_type!=AUDIO_HW_APP_CALL_RECORD)&&
        (voice_status!=VOICE_START_STATUS)&&(AUDIO_DEVICE_IN_TELEPHONY_RX==in->devices)){
        return true;
    }

    if((AUDIO_HW_APP_FM_RECORD==in->audio_app_type) && !adev->fm_start) {
        return true;
    }

    if(((AUDIO_SOURCE_VOICE_UPLINK==in->source) ||(AUDIO_SOURCE_VOICE_DOWNLINK==in->source))
        && (voice_status!=VOICE_START_STATUS)){
        return true;
    }

    if(((AUDIO_HW_APP_FM_RECORD==in->audio_app_type) ||
        (AUDIO_HW_APP_NORMAL_RECORD==in->audio_app_type) ||
        (AUDIO_HW_APP_BT_RECORD==in->audio_app_type)) && (is_primary_voip_active(adev))){
        return true;
    }

    if(((AUDIO_HW_APP_NORMAL_RECORD==in->audio_app_type)||(AUDIO_HW_APP_VOIP_RECORD==in->audio_app_type) ||
            (AUDIO_HW_APP_FM_RECORD==in->audio_app_type) || (AUDIO_HW_APP_BT_RECORD==in->audio_app_type))&&
        ((VOICE_PRE_START_STATUS ==voice_status)||(VOICE_START_STATUS ==voice_status) ||
            (VOICE_PRE_STOP_STATUS ==voice_status))){
        return true;
    }

    if(((AUDIO_HW_APP_NORMAL_RECORD==in->audio_app_type) || (AUDIO_HW_APP_VOIP_RECORD==in->audio_app_type)
            || (AUDIO_HW_APP_BT_RECORD==in->audio_app_type))&& adev->fm_record_start){
        return true;
    }

    pthread_mutex_lock(&adev->voip_start_lock);
    if((AUDIO_HW_APP_FM_RECORD==in->audio_app_type) && adev->normal_record_start){
        pthread_mutex_unlock(&adev->voip_start_lock);
        return true;
    }
    pthread_mutex_unlock(&adev->voip_start_lock);

    if(((AUDIO_HW_APP_NORMAL_RECORD==in->audio_app_type) || (AUDIO_HW_APP_VOIP_RECORD==in->audio_app_type)
        || (AUDIO_HW_APP_BT_RECORD==in->audio_app_type))
        && is_usecase_unlock(adev->dev_ctl,UC_LOOP)){
        return true;
    }

    if (is_usecase_unlock(adev->dev_ctl,UC_AGDSP_ASSERT)) {
        return true;
    } else {
        return false;
    }
}

static ssize_t read_pcm_data(void *stream, void* buffer,
        size_t bytes)
{
    size_t ret =0;
    struct audio_stream_in *stream_tmp=(struct audio_stream_in *)stream;
    struct tiny_stream_in *in = (struct tiny_stream_in *)stream;
    struct tiny_audio_device *adev = in->dev;
    size_t frames_rq = bytes / audio_stream_in_frame_size_l((const struct audio_stream_in *)stream_tmp);

    if (in->resampler != NULL) {
        LOG_D("in_read %d start frames_rq:%d,in->config->channels %d",bytes,frames_rq,in->config->channels);
        ret = read_frames(in, buffer, frames_rq);
        if (ret != frames_rq) {
            LOG_E("ERR:in_read0");
            ret = -1;
        } else {
            ret = 0;
        }
        LOG_D("in_read end:%d",ret);
    } else {
            if((in->config->rate == 48000) && (in->rec_nr_handle) && (in->active_rec_proc)){
                ret = AudioRecordNr_Proc(in->rec_nr_handle,buffer, bytes);
                if(ret <0){
                    LOG_E("AudioRecordNr_Proc  error");
                }


                if(AUDIO_HW_APP_NORMAL_RECORD==in->audio_app_type){
                    audiohal_pcmdump(adev->dev_ctl,in->source,(void *)in->nr_buffer,bytes,PCMDUMP_NORMAL_RECORD_NR);
                }
            }
            else{
                ret = _pcm_read(in, buffer, bytes);
                if(in->active_rec_proc){
                    aud_rec_do_process(buffer, bytes, in->config->channels,in->proc_buf, in->proc_buf_size);
                    if(AUDIO_HW_APP_NORMAL_RECORD==in->audio_app_type){
                        audiohal_pcmdump(adev->dev_ctl,in->source,(void *)buffer,bytes,PCMDUMP_NORMAL_RECORD_PROCESS);
                    }
                }
            }
    }
    return ret;
}

static int32_t stereo2mono(int16_t *out, int16_t * in, uint32_t in_samples) {
    int i = 0;
    int out_samples =  in_samples >> 1;
    for(i = 0 ; i< out_samples ; i++) {
        out[i] =(in[2*i+1] + in[2*i]) /2;
    }
    return out_samples;
}

int32_t  mono2stereo(int16_t *out, int16_t * in, uint32_t in_samples){
    uint32_t i = 0;
    int out_samples = in_samples<<1;
    for(i = 0 ; i< in_samples; i++) {
        out[2*i] =in[i];
        out[2*i+1] = in[i];
    }
    return out_samples ;
}

#ifdef SPRD_AUDIO_HIDL_CLIENT
static ssize_t hidl_in_read(struct audio_stream_in *stream, void* buffer,
        size_t bytes)
{
    struct tiny_stream_in *in = (struct tiny_stream_in *)stream;
        struct tiny_audio_device *adev = in->dev;
        int ret=0;
        pthread_mutex_lock(&in->lock);
        ret=hidl_stream_read(adev,buffer,bytes);
        pthread_mutex_unlock(&in->lock);
        LOG_I("hidl_in_read:%d ret:%d",bytes,ret);
        return bytes;
}

static ssize_t diag_read(struct audio_stream_in *stream, void* buffer,
        size_t bytes)
{
    struct tiny_stream_in *in = (struct tiny_stream_in *)stream;
    struct tiny_audio_device *adev = in->dev;
    struct socket_handle *tunning=&adev->audio_param.tunning;
    int readbytes=in->readbytes;

    pthread_mutex_lock(&in->lock);
    pthread_mutex_lock(&tunning->diag_lock);
    LOG_I("diag_read enter:%d send_buffer_size:%d",in->readbytes,tunning->send_buffer_size);

    if((bytes==0)||(tunning->send_buffer_size==0)){
        readbytes=0;
        goto out;
    }
    if((in->readbytes+bytes)<tunning->send_buffer_size){
        readbytes=bytes;
    }else{
        readbytes=tunning->send_buffer_size-in->readbytes;
    }
#if 0
    if(tunning->diag_tx_file==NULL){
        tunning->diag_tx_file = fopen("/data/vendor/local/media/audiodiag_tx.hex", "wb");
    }

    if(tunning->diag_tx_file!=NULL){
        fwrite(buffer, 1, readbytes, tunning->diag_tx_file);
    }
#endif
    memcpy(buffer,tunning->diag_send_buffer+in->readbytes,readbytes);
    in->readbytes+=readbytes;

out:
    if(in->readbytes>=tunning->send_buffer_size){
        LOG_I("diag_read read tunning send_buffer success");
        tunning->send_buffer_size=0;
        in->readbytes=0;
    }
    pthread_mutex_unlock(&tunning->diag_lock);
    pthread_mutex_unlock(&in->lock);
    LOG_I("diag_read:%d req bytes:%d",readbytes,bytes);
    return readbytes;
}

#endif

static ssize_t in_read(struct audio_stream_in *stream, void *buffer,
                       size_t bytes)
{
    int ret = 0;
    struct tiny_stream_in *in = (struct tiny_stream_in *)stream;
    struct tiny_audio_device *adev = in->dev;

    LOG_D("in_read type:%d call_mode%d usecase:%d bytes:%d",in->audio_app_type,adev->call_mode,adev->dev_ctl->usecase,bytes);

    pthread_mutex_lock(&adev->lock);
    pthread_mutex_lock(&in->lock);

    if (in->audio_app_type == AUDIO_HW_APP_MMAP_RECORD) {
        ret = -ENOSYS;
        pthread_mutex_unlock(&adev->lock);
        goto error;
    }
    if((in->audio_app_type == AUDIO_HW_APP_NORMAL_RECORD)||
        (in->audio_app_type == AUDIO_HW_APP_VOIP_RECORD) ||
        (in->audio_app_type == AUDIO_HW_APP_BT_RECORD)||
        (in->audio_app_type == AUDIO_HW_APP_CALL_RECORD)){
        int cur_app_type = 0;
        pthread_mutex_lock(&adev->voip_start_lock);
        if(VOICE_START_STATUS ==adev->call_status){
            cur_app_type = AUDIO_HW_APP_CALL_RECORD;
        }else{
            if ((adev->voip_start && ((in->source != AUDIO_SOURCE_VOICE_COMMUNICATION && in->source != AUDIO_SOURCE_CAMCORDER)||
               (in->source == AUDIO_SOURCE_VOICE_COMMUNICATION && (!adev->sprd_aec_effect_valid)))) ||
               (in->source == AUDIO_SOURCE_VOICE_COMMUNICATION &&
               adev->sprd_aec_effect_valid && adev->sprd_aec_on &&
               adev->call_status != VOICE_PRE_START_STATUS &&
               adev->call_status != VOICE_START_STATUS)||(adev->voip_start && ((AUDIO_DEVICE_IN_ALL_SCO &(~AUDIO_DEVICE_BIT_IN)) & in->devices))) {
                cur_app_type = AUDIO_HW_APP_VOIP_RECORD;
            } else {
                if((AUDIO_DEVICE_IN_ALL_SCO &(~AUDIO_DEVICE_BIT_IN)) & in->devices){
                    cur_app_type = AUDIO_HW_APP_BT_RECORD;
                }else{
                    cur_app_type = AUDIO_HW_APP_NORMAL_RECORD;
                }
            }
        }
        pthread_mutex_unlock(&adev->voip_start_lock);
        if(in->audio_app_type != cur_app_type){
            LOG_I("in->audio_app_type:%d,cur_app_type:%d,adev->voip_start:%d",in->audio_app_type,cur_app_type,adev->voip_start);
            if(in->standby_fun){
                in->standby_fun(adev,in,in->audio_app_type);
            }
            in->audio_app_type = cur_app_type;
            LOG_I("in_read change audio_app_type:%d",in->audio_app_type);
        }
    }

    if (in_bypass_data(in,adev->call_status)) {
        if(in->standby_fun) {
            in->standby_fun(adev,in,in->audio_app_type);
            if(AUDIO_HW_APP_FM_RECORD==in->audio_app_type){
                adev->fm_record_start = false;
            }
        }
        pthread_mutex_unlock(&in->lock);
        pthread_mutex_unlock(&adev->lock);
        LOG_D("in_read type:%x bypass voice_status:%d",in->audio_app_type,adev->call_status);
        memset(buffer,0,bytes);
        usleep((int64_t) bytes * 1000000/audio_stream_in_frame_size(stream)/in_get_sample_rate(&stream->common));
        return bytes;
    }
    else {
        if(in->audio_app_type==AUDIO_HW_APP_FM_RECORD){
            adev->fm_record_start = true;
        }
    }
    pthread_mutex_unlock(&adev->lock);
    if (in->standby) {
        ret = start_input_stream(in);
        if (ret < 0) {
            LOG_E("start_input_stream error ret=%d", ret);
            goto error;
        }
        in->standby = false;
    }


    if(NULL == in->pcm){
        LOG_E("in_read pcm err");
        ret=-1;
        goto error;
    }else{
        if(in->requested_channels != in->config->channels){
            if(in->channel_buffer==NULL){
                in->channel_buffer=malloc(bytes*2);
            }

            if((in->requested_channels == 1) && (in->config->channels == 2)) {
                ret = read_pcm_data(in, in->channel_buffer, bytes * 2);
                if(ret < 0) {
                    goto error;
                }
                stereo2mono(buffer,in->channel_buffer,bytes);
            }else if((in->requested_channels == 2) && (in->config->channels == 1)) {
                ret = read_pcm_data(in, in->channel_buffer, bytes /2);
                if(ret < 0) {
                    goto error;
                }
                mono2stereo(buffer,in->channel_buffer,bytes/4);
            }
        }else {
            ret = read_pcm_data(in, buffer, bytes);
        }

        if(ret < 0) {
            goto error;
        }
    }

    pthread_mutex_unlock(&in->lock);
    if((AUDIO_HW_APP_NORMAL_RECORD==in->audio_app_type)||(AUDIO_HW_APP_BT_RECORD==in->audio_app_type)){
        audiohal_pcmdump(adev->dev_ctl,in->source,buffer,bytes,PCMDUMP_NORMAL_RECORD_HAL);
    }else if(AUDIO_HW_APP_VOIP_RECORD==in->audio_app_type){
        audiohal_pcmdump(adev->dev_ctl,in->source,buffer,bytes,PCMDUMP_VOIP_RECORD_VBC);
    }else if(AUDIO_HW_APP_RECOGNITION==in->audio_app_type){
        audiohal_pcmdump(adev->dev_ctl,in->source,buffer,bytes,PCMDUMP_NORMAL_RECORD_HAL);
    }else if(AUDIO_HW_APP_CALL_RECORD==in->audio_app_type){
        audiohal_pcmdump(adev->dev_ctl,in->source,buffer,bytes,PCMDUMP_VOICE_RX);
    }

    if(in->pop_mute) {
        if((AUDIO_DEVICE_IN_ALL_USB&~AUDIO_DEVICE_BIT_IN)&in->devices){
            in->pop_mute = false;
            LOG_I("usb record do not mute");
        }else if (in->pop_mute_bytes){
            int16_t *pcm_data=(int16_t *)buffer;
            int mute_bytes = in->pop_mute_bytes > bytes ? bytes : in->pop_mute_bytes;
            int pcm_size = mute_bytes/sizeof(int16_t);
            int i=0;
            for(i=0;i<pcm_size;i++){
                pcm_data[i]=0x1;
            }
            if((in->pop_mute_bytes -= mute_bytes) <= 0) {
                in->pop_mute = false;
                in->pop_mute_bytes = 0;
            }
            LOG_I("set mute in_read bytes %d in->pop_mute_bytes %d",bytes,in->pop_mute_bytes);
        } else {
            in->pop_mute = false;
        }
    }

    if (bytes > 0) {
        in->frames_read += bytes/audio_stream_in_frame_size(stream);
    }
    return bytes;

error:

    if (ret < 0){
        LOG_I("in_read do_normal_inputput_standby:%p", in->pcm);
        if (NULL != in->pcm) {
            LOG_E("in_read  do_normal_inputput_standby pcm_close:%p, error: %s", in->pcm, pcm_get_error(in->pcm));
        }
        if(in->standby==false)
            in->standby_fun(adev,in,in->audio_app_type);
        memset(buffer, 0,bytes);
        pthread_mutex_unlock(&in->lock);
        usleep((int64_t) bytes * 1000000 / audio_stream_in_frame_size(stream) / in_get_sample_rate(&stream->common));
        return  bytes;
    }else{
        LOG_D("in_read:%d", bytes);
        if(AUDIO_HW_APP_NORMAL_RECORD==in->audio_app_type){
            audiohal_pcmdump(adev->dev_ctl,in->source,buffer,bytes,PCMDUMP_NORMAL_RECORD_HAL);
        }
    }
    pthread_mutex_unlock(&in->lock);
    LOG_D("in_read exit");
    return  bytes;
}

static uint32_t in_get_input_frames_lost(UNUSED_ATTR struct audio_stream_in *stream)
{
    return 0;
}

static int in_get_capture_position(const struct audio_stream_in *stream,
                                   int64_t *frames, int64_t *time)
{
    if (stream == NULL || frames == NULL || time == NULL) {
        return -EINVAL;
    }
    struct tiny_stream_in *in = (struct tiny_stream_in *)stream;
    int ret = -ENOSYS;

    pthread_mutex_lock(&in->lock);
    if (in->pcm) {
        struct timespec timestamp;
        unsigned int avail;
        if (pcm_get_htimestamp(in->pcm, &avail, &timestamp) == 0) {
            if(in->config->rate != 0)
                avail = (avail * in->requested_rate)/in->config->rate;
            if(in->requested_channels == 1)
                avail /= 2;

            *frames = in->frames_read + avail;
            *time = timestamp.tv_sec * 1000000000LL + timestamp.tv_nsec;
            ret = 0;
        }
    }
    pthread_mutex_unlock(&in->lock);
    return ret;
}

/*
 * Extract the card and device numbers from the supplied key/value pairs.
 *   kvpairs    A null-terminated string containing the key/value pairs or card and device.
 *              i.e. "card=1;device=42"
 *   card   A pointer to a variable to receive the parsed-out card number.
 *   device A pointer to a variable to receive the parsed-out device number.
 * NOTE: The variables pointed to by card and device return -1 (undefined) if the
 *  associated key/value pair is not found in the provided string.
 *  Return true if the kvpairs string contain a card/device spec, false otherwise.
 */
static bool parse_card_device_params(const char *kvpairs, int *card, int *device)
{
    struct str_parms * parms = str_parms_create_str(kvpairs);
    char value[32];
    int param_val;

    // initialize to "undefined" state.
    *card = -1;
    *device = -1;

    param_val = str_parms_get_str(parms, "card", value, sizeof(value));
    if (param_val >= 0) {
        *card = atoi(value);
    }

    param_val = str_parms_get_str(parms, "device", value, sizeof(value));
    if (param_val >= 0) {
        *device = atoi(value);
    }

    str_parms_destroy(parms);

    return *card >= 0 && *device >= 0;
}

static int usbout_set_parameters(void *dev, const char *kvpairs)
{
    LOG_D("usbout_set_parameters() keys:%s", kvpairs);
    struct tiny_audio_device *adev = (struct tiny_audio_device *)dev;
    struct usbaudio_ctl *usb_ctl=(struct usbaudio_ctl *)&adev->usb_ctl;

    int ret_value = 0;
    int card = -1;
    int device = -1;

    if (!parse_card_device_params(kvpairs, &card, &device)) {
        // nothing to do
        return ret_value;
    }

    /* Lock the device because that is where the profile lives */
    pthread_mutex_lock(&usb_ctl->lock);
    if (!profile_is_cached_for(&usb_ctl->out_profile, card, device)) {
        int saved_card = usb_ctl->out_profile.card;
        int saved_device = usb_ctl->out_profile.device;
        usb_ctl->out_profile.card = card;
        usb_ctl->out_profile.device = device;
        ret_value = profile_read_device_info(&usb_ctl->out_profile) ? 0 : -EINVAL;
        if (ret_value != 0) {
            usb_ctl->out_profile.card = saved_card;
            usb_ctl->out_profile.device = saved_device;
        }
    }
    pthread_mutex_unlock(&usb_ctl->lock);
    return ret_value;
}

static char *device_get_parameters(const alsa_device_profile *profile, const char * keys)
{
    if (profile->card < 0 || profile->device < 0) {
        return strdup("");
    }

    struct str_parms *query = str_parms_create_str(keys);
    struct str_parms *result = str_parms_create();

    /* These keys are from hardware/libhardware/include/audio.h */
    /* supported sample rates */
    if (str_parms_has_key(query, AUDIO_PARAMETER_STREAM_SUP_SAMPLING_RATES)) {
        char* rates_list = profile_get_sample_rate_strs(profile);
        str_parms_add_str(result, AUDIO_PARAMETER_STREAM_SUP_SAMPLING_RATES,
                          rates_list);
        free(rates_list);
    }

    /* supported channel counts */
    if (str_parms_has_key(query, AUDIO_PARAMETER_STREAM_SUP_CHANNELS)) {
        char* channels_list = profile_get_channel_count_strs(profile);
        str_parms_add_str(result, AUDIO_PARAMETER_STREAM_SUP_CHANNELS,
                          channels_list);
        free(channels_list);
    }

    /* supported sample formats */
    if (str_parms_has_key(query, AUDIO_PARAMETER_STREAM_SUP_FORMATS)) {
        char * format_params = profile_get_format_strs(profile);
        str_parms_add_str(result, AUDIO_PARAMETER_STREAM_SUP_FORMATS,
                          format_params);
        free(format_params);
    }
    str_parms_destroy(query);

    char* result_str = str_parms_to_str(result);
    str_parms_destroy(result);

    LOG_D("device_get_parameters = %s", result_str);

    return result_str;
}

static char * usbout_get_parameters(void *dev, const char *keys)
{
    struct tiny_audio_device *adev = (struct tiny_audio_device *)dev;
    struct usbaudio_ctl *usb_ctl=(struct usbaudio_ctl *)&adev->usb_ctl;
    char * rsp=NULL;
    pthread_mutex_lock(&usb_ctl->lock);
    rsp=device_get_parameters(&usb_ctl->out_profile, keys);
    pthread_mutex_unlock(&usb_ctl->lock);
    return rsp;
}

bool check_supprot_typec_offload(void *dev,const char *kvpairs){
    struct tiny_audio_device *adev = (struct tiny_audio_device *)dev;
    struct audio_control *control = (struct audio_control *)adev->dev_ctl;
    struct usbaudio_ctl *usb_ctl=(struct usbaudio_ctl *)&adev->usb_ctl;
    int ret=0;
    int card=0;
    int device=0;
    struct mixer *usb_mixer=NULL;
    struct mixer_ctl *usb_kctl=NULL;
    bool usb_mic_connected=false;
    int usb_suspend=0;
    if(true==is_usbmic_connected(control)){
        usb_mic_connected=true;
    }

    if (!parse_card_device_params(kvpairs, &card, &device)) {
        LOG_W("%s fail - invalid address %s", __func__, kvpairs);
        return false;
    }
    pthread_mutex_lock(&usb_ctl->lock);
    if(true==usb_ctl->support_offload){
        LOG_I("check_supprot_typec_offload usb output aready prepared");
        pthread_mutex_unlock(&usb_ctl->lock);
        return true;
    }

    usb_ctl->out_profile.card=card;
    usb_ctl->out_profile.device=device;
    usb_ctl->support_offload=false;

    usb_mixer=mixer_open(usb_ctl->out_profile.card);

    if(usb_mixer!=NULL){
        usb_kctl=mixer_get_ctl_by_name(usb_mixer, "USB_AUD_OFLD_P_EN");
        if(NULL!=usb_kctl){
            if(true==adev->issupport_usb){
            ret=mixer_ctl_set_value(usb_kctl,0,1);
            if(0!=ret){
                LOG_W("Set 'USB_AUD_OFLD_P_EN' to '1' Failed")
            }else{
                LOG_I("Set 'USB_AUD_OFLD_P_EN' to '1' ");
            }
            profile_read_device_info(&usb_ctl->out_profile);

            if((profile_is_sample_rate_valid(&usb_ctl->out_profile,USB_OFFLOAD_SAMPLERATE))
                &&(profile_is_format_valid(&usb_ctl->out_profile,USB_OFFLOAD_FORMAT))
                &&(control->pcm_handle.play[AUD_PCM_MM_NORMAL].channels==profile_get_default_channel_count(&usb_ctl->out_profile))){
                usb_ctl->support_offload=true;
                }
            }
        }
    }

    if(usb_ctl->support_offload==true){
        LOG_I("check_supprot_typec_offload support_offload");
    }else{
        LOG_I("check_supprot_typec_offload use default channel card:%d device:%d",
            usb_ctl->out_profile.card,usb_ctl->out_profile.device);
        usb_ctl->support_offload=false;
        if(NULL!=usb_kctl){
            ret=mixer_ctl_set_value(usb_kctl,0,0);
            if(0!=ret){
                LOG_W("Set 'USB_AUD_OFLD_P_EN' to '0' Failed")
            }else{
                LOG_I("Set 'USB_AUD_OFLD_P_EN' to '0' ");
            }
        }
    }

    if(true==usb_ctl->support_offload){
        if(true==usb_mic_connected){
            if(true==usb_ctl->support_record){
                usb_suspend=1;
            }else{
                usb_suspend=0;
            }
        }else{
            usb_suspend=1;
        }
    }

    usb_kctl=mixer_get_ctl_by_name(usb_mixer, "USB_AUD_SHOULD_SUSPEND");
    if(NULL!=usb_kctl){
        ret=mixer_ctl_set_value(usb_kctl,0,usb_suspend);
        if(0!=ret){
            LOG_W("Set 'USB_AUD_SHOULD_SUSPEND' to '%d' Failed",usb_suspend)
        }else{
            LOG_I("Set 'USB_AUD_SHOULD_SUSPEND' to '%d' ",usb_suspend);
        }
    }

    if(NULL!=usb_mixer){
        mixer_close(usb_mixer);
    }
    pthread_mutex_unlock(&usb_ctl->lock);
    return usb_ctl->support_offload;

}

bool check_supprot_typec_record(void *dev,const char *kvpairs){
    struct tiny_audio_device *adev = (struct tiny_audio_device *)dev;
    struct audio_control *control = (struct audio_control *)adev->dev_ctl;
    struct usbaudio_ctl *usb_ctl=(struct usbaudio_ctl *)&adev->usb_ctl;
    int ret=0;
    int card=0;
    int device=0;
    struct mixer *usb_mixer=NULL;
    struct mixer_ctl *usb_kctl=NULL;

    bool usb_out_connected=false;
    int usb_suspend=0;
    if(true==is_usbdevice_connected(control)){
        usb_out_connected=true;
    }

    if (!parse_card_device_params(kvpairs, &card, &device)) {
        LOG_W("%s fail - invalid address %s", __func__, kvpairs);
        return false;
    }
    pthread_mutex_lock(&usb_ctl->lock);
    if(true==usb_ctl->support_record){
        LOG_I("check_supprot_typec_record usb input aready prepared");
        pthread_mutex_unlock(&usb_ctl->lock);
        return true;
    }

    usb_ctl->in_profile.card=card;
    usb_ctl->in_profile.device=device;
    usb_ctl->support_record=false;


    usb_mixer=mixer_open(usb_ctl->in_profile.card);

    if(usb_mixer!=NULL){
        usb_kctl=mixer_get_ctl_by_name(usb_mixer, "USB_AUD_OFLD_C_EN");
        if(NULL!=usb_kctl){
            if(true==adev->issupport_usb){
            ret=mixer_ctl_set_value(usb_kctl,0,1);
            if(0!=ret){
                LOG_W("Set 'USB_AUD_OFLD_C_EN' to '1' Failed")
            }else{
                LOG_I("Set 'USB_AUD_OFLD_C_EN' to '1' ");
            }
            profile_read_device_info(&usb_ctl->in_profile);

            if((profile_is_sample_rate_valid(&usb_ctl->in_profile,USB_OFFLOAD_SAMPLERATE))
                &&(profile_is_format_valid(&usb_ctl->in_profile,USB_OFFLOAD_FORMAT))
                &&(control->pcm_handle.record[AUD_RECORD_PCM_NORMAL].channels==profile_get_default_channel_count(&usb_ctl->in_profile))){
                LOG_I("%s line:%d support record",__func__,__LINE__);
                usb_ctl->support_record=true;
            }else{
                LOG_W("%s line:%d do not support record",__func__,__LINE__);
                usb_ctl->support_record=false;
                }
            }
        }
    }

    if(usb_ctl->support_record==true){
        LOG_I("check_supprot_typec_record support_record");
        LOG_I("check_supprot_typec_record card:%d device:%d",
            usb_ctl->in_profile.card,usb_ctl->in_profile.device);
    }else{
        LOG_I("check_supprot_typec_record do not support record card:%d device:%d",
            usb_ctl->in_profile.card,usb_ctl->in_profile.device);
        usb_ctl->support_record=false;
        if(NULL!=usb_kctl){
            ret=mixer_ctl_set_value(usb_kctl,0,0);
            if(0!=ret){
                LOG_W("Set 'USB_AUD_OFLD_C_EN' to '0' Failed")
            }else{
                LOG_I("Set 'USB_AUD_OFLD_C_EN' to '0' ");
            }
        }
    }

    if(usb_ctl->support_record==true){
        if(true==usb_out_connected){
            if(true==usb_ctl->support_offload){
                usb_suspend=1;
            }else{
                usb_suspend=0;
            }
        }else{
            usb_suspend=1;
        }
    }

    usb_kctl=mixer_get_ctl_by_name(usb_mixer, "USB_AUD_SHOULD_SUSPEND");
    if(NULL!=usb_kctl){
        ret=mixer_ctl_set_value(usb_kctl,0,usb_suspend);
        if(0!=ret){
            LOG_W("Set 'USB_AUD_SHOULD_SUSPEND' to '%d' Failed",usb_suspend)
        }else{
            LOG_I("Set 'USB_AUD_SHOULD_SUSPEND' to '%d' ",usb_suspend);
        }
    }

    if(NULL!=usb_mixer){
        mixer_close(usb_mixer);
    }
    pthread_mutex_unlock(&usb_ctl->lock);
    return usb_ctl->support_record;

}

int open_usbinput_channel(void *dev)
{
    struct tiny_audio_device *adev = (struct tiny_audio_device *)dev;
    struct usbaudio_ctl *usb_ctl=(struct usbaudio_ctl *)&adev->usb_ctl;
    struct audio_control *control = (struct audio_control *)adev->dev_ctl;
    int ret=0;
    pthread_mutex_lock(&usb_ctl->lock);
    LOG_I("open_usbinput_channel:%d",usb_ctl->input_status);
    if(true==usb_ctl->support_record){
        if((USB_CHANNEL_OPENED!=usb_ctl->input_status)
            &&(USB_CHANNEL_STARTED!=usb_ctl->input_status)){

            struct pcm_config config;
            memcpy(&config,&control->pcm_handle.record[AUD_RECORD_PCM_NORMAL],sizeof(struct pcm_config));
            config.rate=USB_OFFLOAD_SAMPLERATE;
            config.format=USB_OFFLOAD_FORMAT;

            LOG_I("open_usbinput_channel use default channel card:%d device:%d",
                usb_ctl->in_profile.card,usb_ctl->in_profile.device);

            LOG_I("open_usbinput_channel record:%d %d %d %d",
                control->pcm_handle.record[AUD_RECORD_PCM_NORMAL].rate,
                control->pcm_handle.record[AUD_RECORD_PCM_NORMAL].period_size,
                control->pcm_handle.record[AUD_RECORD_PCM_NORMAL].period_count,
                control->pcm_handle.record[AUD_RECORD_PCM_NORMAL].channels
                );

            ret=proxy_prepare(&usb_ctl->in_proxy, &usb_ctl->in_profile, &config);
            if(ret==0) {
                LOG_I("open_usbinput_channel proxy_prepare success");
                usb_ctl->input_status=USB_CHANNEL_OPENED;
            }else {
                 LOG_E("open_usbinput_channel:proxy_prepare error %d", ret);
                 LOG_I("open_usbinput_channel card:%d device:%d %d",
                     usb_ctl->in_proxy.profile->card,usb_ctl->in_proxy.profile->device,
                     usb_ctl->in_proxy.profile->direction);
                 LOG_I("open_usbinput_channel pcm:%d %d %d %d",
                     usb_ctl->in_proxy.alsa_config.rate,
                     usb_ctl->in_proxy.alsa_config.period_size,
                     usb_ctl->in_proxy.alsa_config.period_count,
                     usb_ctl->in_proxy.alsa_config.channels);
            }
        }
    }
    pthread_mutex_unlock(&usb_ctl->lock);
    return ret;
}

int open_usboutput_channel(void *dev)
{
    struct tiny_audio_device *adev = (struct tiny_audio_device *)dev;
    struct usbaudio_ctl *usb_ctl=(struct usbaudio_ctl *)&adev->usb_ctl;
    struct audio_control *control = (struct audio_control *)adev->dev_ctl;
    int ret = 0;
    LOG_I("open_usboutput_channel:%d",usb_ctl->output_status);
    pthread_mutex_lock(&usb_ctl->lock);
    if(true==usb_ctl->support_offload){
        if((USB_CHANNEL_OPENED!=usb_ctl->output_status)
            &&(USB_CHANNEL_STARTED!=usb_ctl->output_status)){

            struct pcm_config config;
            memcpy(&config,&control->pcm_handle.record[AUD_RECORD_PCM_NORMAL],sizeof(struct pcm_config));
            config.rate=USB_OFFLOAD_SAMPLERATE;
            config.format=USB_OFFLOAD_FORMAT;

            LOG_I("open_usboutput_channel");
            ret = proxy_prepare(&usb_ctl->out_proxy, &usb_ctl->out_profile, &config);
            if(0==ret) {
                LOG_I("open_usboutput_channel proxy_prepare success");
                usb_ctl->output_status=USB_CHANNEL_OPENED;
            }else {
                 LOG_E("open_usboutput_channel:proxy_prepare error %d", ret);
                 LOG_I("open_usboutput_channel card:%d device:%d %d",
                     usb_ctl->out_proxy.profile->card,usb_ctl->out_proxy.profile->device,
                     usb_ctl->out_proxy.profile->direction);
                 LOG_I("open_usboutput_channel pcm:%d %d %d %d",
                     usb_ctl->out_proxy.alsa_config.rate,
                     usb_ctl->out_proxy.alsa_config.period_size,
                     usb_ctl->out_proxy.alsa_config.period_count,
                     usb_ctl->out_proxy.alsa_config.channels);
            }
        }
    }
    pthread_mutex_unlock(&usb_ctl->lock);
    return ret;
}

static int proxy_start(alsa_device_proxy * proxy)
{
    if((proxy!=NULL) && (proxy->pcm!=NULL)){
        return pcm_start(proxy->pcm);
    }
    return -1;
}

void disconnect_usb(struct usbaudio_ctl *usb_ctl,bool is_output){
    stop_usb_channel(usb_ctl,is_output);
    pthread_mutex_lock(&usb_ctl->lock);
    if(is_output==true){
        LOG_I("disconnect_usb output");
        usb_ctl->support_offload=false;
        usb_ctl->output_status=USB_CHANNEL_CLOSED;
    }else{
        LOG_I("disconnect_usb input");
        usb_ctl->support_record=false;
        usb_ctl->input_status=USB_CHANNEL_CLOSED;
    }
    pthread_mutex_unlock(&usb_ctl->lock);
}

int start_usb_channel(struct usbaudio_ctl *usb_ctl,bool is_output)
{
    int ret=0;
    pthread_mutex_lock(&usb_ctl->lock);

    if(is_output==true){
        if(usb_ctl->support_offload==false){
            pthread_mutex_unlock(&usb_ctl->lock);
            return -1;
        }
        LOG_I("%s line:%d output status :%d",__func__,__LINE__,
            usb_ctl->output_status);
        if(USB_CHANNEL_STARTED!=usb_ctl->output_status){
            LOG_I("start_usb_channel output");
            usb_ctl->out_proxy.alsa_config.stop_threshold=-1;
            ret= proxy_open(&usb_ctl->out_proxy);
            if(ret==0){
                ret= proxy_start(&usb_ctl->out_proxy);
                if(!ret) {
                    usb_ctl->output_status=USB_CHANNEL_STARTED;
                }
                else {
                    LOG_E("%s line:%d output status :%d proxy_start Failed ret:%d",__func__,__LINE__,
                    usb_ctl->output_status,ret);
                    proxy_close(&usb_ctl->out_proxy);
                }
            }else{
                LOG_E("%s line:%d output status :%d proxy_open Failed",__func__,__LINE__,
                    usb_ctl->output_status);

            }
        }
    }else{
        if(usb_ctl->support_record==false){
            pthread_mutex_unlock(&usb_ctl->lock);
            return -1;
        }
        LOG_I("%s line:%d input status :%d",__func__,__LINE__,
            usb_ctl->input_status);
        if(USB_CHANNEL_STARTED!=usb_ctl->input_status){
            LOG_I("start_usb_channel input");
            usb_ctl->in_proxy.alsa_config.stop_threshold=-1;
            ret= proxy_open(&usb_ctl->in_proxy);
            if(ret==0){
                ret= proxy_start(&usb_ctl->in_proxy);
                if(!ret) {
                    usb_ctl->input_status=USB_CHANNEL_STARTED;
                }
                else {
                    LOG_E("%s line:%d intput status :%d proxy_start Failed",__func__,__LINE__,
                    usb_ctl->input_status);
                    proxy_close(&usb_ctl->in_proxy);
                }
                usb_ctl->input_status=USB_CHANNEL_STARTED;
            }else{
                LOG_E("%s line:%d input status :%d proxy_open Failed",__func__,__LINE__,
                    usb_ctl->input_status);
                LOG_I("start_usb_channel use default channel card:%d device:%d",
                    usb_ctl->in_profile.card,usb_ctl->in_profile.device);

                LOG_I("start_usb_channel use default channel card:%d device:%d %d",
                    usb_ctl->in_proxy.profile->card,usb_ctl->in_proxy.profile->device,
                    usb_ctl->in_proxy.profile->direction);

                LOG_I("start_usb_channel pcm:%d %d %d %d",
                    usb_ctl->in_proxy.alsa_config.rate,
                    usb_ctl->in_proxy.alsa_config.period_size,
                    usb_ctl->in_proxy.alsa_config.period_count,
                    usb_ctl->in_proxy.alsa_config.channels
                    );
            }
        }
    }
    pthread_mutex_unlock(&usb_ctl->lock);
    return ret;
}

int stop_usb_channel(struct usbaudio_ctl *usb_ctl,bool is_output)
{
    int ret=0;
    pthread_mutex_lock(&usb_ctl->lock);

    if(true==is_output){
        if(usb_ctl->support_offload==false){
            pthread_mutex_unlock(&usb_ctl->lock);
            return -1;
        }
        LOG_I("%s line:%d output status :%d",__func__,__LINE__,
            usb_ctl->output_status);
        if(USB_CHANNEL_STARTED == usb_ctl->output_status){
            LOG_I("stop_usb_channel output");
            proxy_close(&usb_ctl->out_proxy);
            usb_ctl->output_status=USB_CHANNEL_STOPED;
        }
    }else{
        if(usb_ctl->support_record==false){
            pthread_mutex_unlock(&usb_ctl->lock);
            return -1;
        }
        LOG_I("%s line:%d input status:%d",__func__,__LINE__,
            usb_ctl->input_status);
        if(USB_CHANNEL_STARTED==usb_ctl->input_status){
            LOG_I("stop_usb_channel input");
            proxy_close(&usb_ctl->in_proxy);
            usb_ctl->input_status=USB_CHANNEL_STOPED;
        }
    }
    LOG_I("stop_usb_channel out");
    pthread_mutex_unlock(&usb_ctl->lock);
    return ret;
}

static void init_usbctl(struct usbaudio_ctl *usb_ctl){
    profile_init(&usb_ctl->out_profile, PCM_OUT);
    profile_init(&usb_ctl->in_profile, PCM_IN);
    usb_ctl->output_status=USB_CHANNEL_INITED;
    usb_ctl->support_offload=false;
    usb_ctl->support_record=false;
    pthread_mutex_init(&usb_ctl->lock, NULL);
}

static void deinit_usbctl(struct usbaudio_ctl *usb_ctl){
    usb_ctl->output_status=USB_CHANNEL_INITED;
    usb_ctl->support_offload=false;
    usb_ctl->support_record=false;
    pthread_mutex_destroy(&usb_ctl->lock);
}

static int adev_open_output_stream(struct audio_hw_device *dev,
                                   UNUSED_ATTR audio_io_handle_t handle,
                                   audio_devices_t devices,
                                   audio_output_flags_t flags,
                                   struct audio_config *config,
                                   struct audio_stream_out **stream_out,
                                   UNUSED_ATTR const char* address)
{
    struct tiny_audio_device *adev = (struct tiny_audio_device *)dev;
    struct tiny_stream_out *out=NULL;
    struct audio_control *control = NULL;
    int ret=0;
    control = adev->dev_ctl;

    LOG_I("%s, devices = 0x%x flags:0x%x rate:%d", __func__, devices,flags,config->sample_rate);

    out =
        (struct tiny_stream_out *)calloc(1, sizeof(struct tiny_stream_out));
    if (!out) {
        LOG_E("adev_open_output_stream calloc fail, size:%d",
              sizeof(struct tiny_stream_out));
        return -ENOMEM;
    }
    LOG_I("adev_open_output_stream out:%p",out);

    memset(out, 0, sizeof(struct tiny_stream_out));
    out->apm_devices=devices;
    out->devices = devices;

    pthread_mutex_lock(&adev->lock);
    if(adev->boot_completed==false){
        if(devices&(AUDIO_DEVICE_OUT_WIRED_HEADSET|AUDIO_DEVICE_OUT_WIRED_HEADPHONE)){
            LOG_I("adev_open_output_stream headset connected");
            set_audio_boot_completed(adev);
        }else{
            devices=get_headset_device(adev);
            if(devices){
                out->devices=devices;
                LOG_I("adev_open_output_stream use hal devices:0x%x", out->devices);
            }
        }
    }
    pthread_mutex_unlock(&adev->lock);

    out->config = (struct pcm_config *) malloc(sizeof(struct pcm_config));
    if(NULL==out->config){
        LOG_E("adev_open_output_stream malloc pcm_config failed");
        goto err_open;
    }
    pthread_mutex_init(&out->lock, NULL);
    out->dev = adev;
    memcpy(&out->request_config,config,sizeof(struct audio_config));
    memcpy(out->config, &(control->pcm_handle.play[AUD_PCM_MM_NORMAL]), sizeof(struct pcm_config));

    if (flags & AUDIO_OUTPUT_FLAG_COMPRESS_OFFLOAD){
        /* check offload supported information */
        if (config->offload_info.version != 1 ||
            config->offload_info.size != AUDIO_INFO_INITIALIZER.size) {
            LOG_E("%s: offload information is not supported ", __func__);
            ret = -EINVAL;
            goto err_open;
        }
        if (!audio_is_offload_support_format(config->offload_info.format)) {
            LOG_E("%s: offload audio format(%d) is not supported ",
                  __func__, config->offload_info.format);
            ret = -EINVAL;
            goto err_open;
        }
        /*  codec type and parameters requested */
        out->compress_config.codec = (struct snd_codec *)calloc(1,
                                     sizeof(struct snd_codec));
        out->offload_format = config->format;
        out->offload_samplerate = config->sample_rate;
        out->offload_channel_mask = config->channel_mask;

        out->stream.set_callback = out_offload_set_callback;
        out->stream.pause = out_offload_pause;
        out->stream.resume = out_offload_resume;
        out->stream.drain = out_offload_drain;
        out->stream.flush = out_offload_flush;

        out->compress_config.codec->id =
            audio_get_offload_codec_id(config->offload_info.format);

        out->audio_app_type = AUDIO_HW_APP_OFFLOAD;
        out->compress_config.fragment_size = control->pcm_handle.compress.fragment_size;
        out->compress_config.fragments = control->pcm_handle.compress.fragments;

        out->compress_config.codec->sample_rate =
            compress_get_alsa_rate(config->offload_info.sample_rate);
        out->compress_config.codec->bit_rate =
            config->offload_info.bit_rate;
        out->compress_config.codec->ch_in =
            popcount(config->channel_mask);
        out->compress_config.codec->ch_out = out->compress_config.codec->ch_in;

        LOG_I("compress_config fragment_size:0x%x fragments:0x%x",
            out->compress_config.fragment_size,out->compress_config.fragments);

        if (flags & AUDIO_OUTPUT_FLAG_NON_BLOCKING) {
            out->is_offload_nonblocking = 1;
        }
        out->is_offload_need_set_metadata = 1;
        audio_offload_create_thread(&out->stream);
    }  else if (out->devices == AUDIO_DEVICE_OUT_TELEPHONY_TX)  {
        out->audio_app_type = AUDIO_HW_APP_VOICE_TX;
        memcpy(out->config, &(control->pcm_handle.play[AUD_PCM_VOICE_TX]), sizeof(struct pcm_config));
    } else if (flags & AUDIO_OUTPUT_FLAG_MMAP_NOIRQ) {
        out->audio_app_type = AUDIO_HW_APP_MMAP;
        out->stream.start = out_start;
        out->stream.stop = out_stop;
        out->stream.create_mmap_buffer = out_create_mmap_buffer;
        out->stream.get_mmap_position = out_get_mmap_position;
    } else if ((flags & AUDIO_OUTPUT_FLAG_VOIP_RX) &&(flags & AUDIO_OUTPUT_FLAG_DIRECT)){
        out->audio_app_type = AUDIO_HW_APP_VOIP;
        memcpy(out->config, &(control->pcm_handle.play[AUD_PCM_VOIP]), sizeof(struct pcm_config));
    } else if (flags & AUDIO_OUTPUT_FLAG_FAST){
        out->audio_app_type = AUDIO_HW_APP_FAST;
        memcpy(out->config, &(control->pcm_handle.play[AUD_PCM_FAST]), sizeof(struct pcm_config));
    }  else if (flags == 0x2000){
        out->audio_app_type = AUDIO_HW_APP_HIDL_OUTPUT;
        memcpy(out->config, &(control->pcm_handle.play[AUD_PCM_MM_NORMAL]), sizeof(struct pcm_config));
        out->config->rate=config->sample_rate;
        out->config->channels=audio_channel_count_from_out_mask(config->channel_mask);
    } else {
        out->buffer = malloc(RESAMPLER_BUFFER_SIZE);
        if (NULL == out->buffer) {
            LOG_E("adev_open_output_stream out->buffer alloc fail, size:%d",
                  RESAMPLER_BUFFER_SIZE);
            goto err_open;
        } else {
            memset(out->buffer, 0, RESAMPLER_BUFFER_SIZE);
        }
        if(flags & AUDIO_OUTPUT_FLAG_DEEP_BUFFER){
            out->audio_app_type = AUDIO_HW_APP_DEEP_BUFFER;
            memcpy(out->config, &(control->pcm_handle.play[AUD_PCM_DEEP_BUFFER]), sizeof(struct pcm_config));
        }else{
            out->audio_app_type = AUDIO_HW_APP_PRIMARY;
            memcpy(out->config, &(control->pcm_handle.play[AUD_PCM_MM_NORMAL]), sizeof(struct pcm_config));
        }
    }
    out->format = out->config->format;
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


    LOG_I("adev_open_output_stream pcm config:%d %d %d", out->config->rate, out->config->period_size,
              out->config->period_size);
    out->standby = true;
    out->flags = flags;

    out->requested_rate = config->sample_rate;
    out->testMode= false;

    audio_add_output(adev,out);

    if((out->audio_app_type == AUDIO_HW_APP_PRIMARY)
        &&(AUDIO_OUTPUT_FLAG_PRIMARY&flags)){
        audio_endpoint_test_init(adev,out);
    }

    LOG_I("adev_open_output_stream Successful audio_app_type:%d out:%p",out->audio_app_type,out);
    *stream_out = &out->stream;
    return 0;

err_open:
    LOG_E("adev_open_output_stream Failed");
    if(out->config){
        free(out->config);
        out->config = NULL;
    }
    free(out);
    *stream_out = NULL;
    return ret;
}

static void adev_close_output_stream(struct audio_hw_device *dev,
                                     struct audio_stream_out *stream)
{
    struct tiny_stream_out *out = (struct tiny_stream_out *)stream;
    struct tiny_audio_device *adev = (struct tiny_audio_device *)dev;

    LOG_I("adev_close_output_stream type:%d",out->audio_app_type);
    out_standby(&stream->common);
    audio_del_output((struct tiny_audio_device *)dev,out);

#ifdef AWINIC_EFFECT_SUPPORT
    if(adev->awinic_skt.is_module_ready == true)
        adev->awinic_skt.reset(adev->awinic_skt.module_context_buffer);
#endif

    pthread_mutex_lock(&out->lock);
    if (out->buffer) {
        free(out->buffer);
    }
    if (out->format_buffer) {
        free(out->format_buffer);
    }
    if (out->resampler) {
        release_resampler(out->resampler);
        out->resampler = NULL;
    }

    if (out->audio_app_type == AUDIO_HW_APP_OFFLOAD) {
        pthread_mutex_unlock(&out->lock);
        audio_offload_destroy_thread(&out->stream);
        pthread_mutex_lock(&out->lock);
        if (out->compress_config.codec != NULL)
        { free(out->compress_config.codec); }
    }

    if(out->config){
        free(out->config);
        out->config = NULL;
    }

#ifdef SPRD_AUDIO_HIDL_CLIENT
    if((out->stream.write == diag_write)&&(out->dev!=NULL)){
        audiotester_disconnected(&out->dev->audio_param);
    }

    if(stream==(struct audio_stream_out *)adev->test_ctl.ouput_test.hild_outputstream){
        adev->test_ctl.ouput_test.hild_outputstream=NULL;
    }

    if(stream==(struct audio_stream_out *)adev->test_ctl.input_test.hild_outputstream){
        adev->test_ctl.input_test.hild_outputstream=NULL;
    }

    if(stream==(struct audio_stream_out *)adev->test_ctl.loop_test.hild_outputstream){
        adev->test_ctl.loop_test.hild_outputstream=NULL;
    }
#endif
    pthread_mutex_unlock(&out->lock);
    pthread_mutex_destroy(&out->lock);
    free(stream);
}

static int adev_set_parameters(struct audio_hw_device *dev, const char *kvpairs)
{
    struct tiny_audio_device *adev=(struct tiny_audio_device *)dev;
    struct str_parms *parms;
    char value[128];
    int ret;
    int val=0;

    pthread_mutex_lock(&adev->lock);
    check_boot_status(dev);
    pthread_mutex_unlock(&adev->lock);

    /*set Empty Parameter then return 0*/
    if (0 == strncmp(kvpairs, "", sizeof(char))) {
        return 0;
    }

    parms = str_parms_create_str(kvpairs);
    ret = str_parms_get_str(parms,"connect", value, sizeof(value));
    if(ret >= 0){
        val = strtoul(value,NULL,0);
        ALOGI("adev_set_parameters connect:0x%x kvpairs:%s",val,kvpairs);
        if(val){
            pthread_mutex_lock(&adev->lock);
            set_available_outputdevices(adev->dev_ctl,val,true);
            if(0==(AUDIO_DEVICE_BIT_IN&val)){
                if(is_usbdevice_connected(adev->dev_ctl)){
                    if(true==check_supprot_typec_offload(dev,kvpairs)){
                        ret=open_usboutput_channel(adev);
                    }
                }
            }else{
                if(is_usbmic_connected(adev->dev_ctl)){
                    if(check_supprot_typec_record(dev,kvpairs)){
                        ret=open_usbinput_channel(adev);
                        if(ret==0){
                            noity_usbmic_connected_for_call(adev->dev_ctl);
                        }
                    }
                }
            }
            if(adev->boot_completed==false){
                set_audio_boot_completed(adev);
            }
            pthread_mutex_unlock(&adev->lock);
         }
    }else{
        ext_contrtol_process((struct tiny_audio_device *)dev,kvpairs);
    }
    str_parms_destroy(parms);
    return 0;
}

static void dump_tinymix_infor(int fd,int dump_flag,struct mixer *mixer){
    unsigned int max_size = 20000;
    unsigned int read_size = max_size;

    if(dump_flag&(1<<ADEV_DUMP_TINYMIX)){
        void *buffer = (void*)malloc(max_size);
        if(NULL!=buffer){
            tinymix_list_controls(mixer,buffer,&read_size);
            AUDIO_DUMP_WRITE_BUFFER(fd,buffer,read_size);
            free(buffer);
            buffer=NULL;
        }
    }
}

static char *adev_get_parameters(const struct audio_hw_device *dev,
                                 const char *keys)
{
    struct str_parms *query = str_parms_create_str(keys);
    struct tiny_audio_device *adev = (struct tiny_audio_device *)dev;

    LOG_I("adev_get_parameters:%s",keys);

    if (str_parms_has_key(query, "isUsboffloadSupported")) {
        struct usbaudio_ctl *usb_ctl=(struct usbaudio_ctl *)&adev->usb_ctl;
        str_parms_destroy(query);
        if(usb_ctl->support_offload){
            return strdup("UsboffloadSupported=1");
        }else{
            return strdup("UsboffloadSupported=0");
        }
    }

    if (str_parms_has_key(query, "isUsbRecordSupported")) {
        struct usbaudio_ctl *usb_ctl=(struct usbaudio_ctl *)&adev->usb_ctl;
        str_parms_destroy(query);
        if(usb_ctl->support_record){
            return strdup("UsbRecordSupported=1");
        }else{
            return strdup("UsbRecordSupported=0");
        }
    }

    if (str_parms_has_key(query, "FM_WITH_DSP")) {
         str_parms_destroy(query);
         pthread_mutex_lock(&adev->lock);
        if (adev->fm_bydsp) {
            pthread_mutex_unlock(&adev->lock);
            return strdup("FM_WITH_DSP=1");
        } else {
            pthread_mutex_unlock(&adev->lock);
            return strdup("FM_WITH_DSP=0");
        }
    }

    if (str_parms_has_key(query, "isAudioDspExist")) {
        str_parms_destroy(query);
        LOG_I("adev_get_parameters: isAudioDspExist=1");
        return strdup("isAudioDspExist=1");
    }

    if (str_parms_has_key(query, "SmartAmpCalibration")){
        str_parms_destroy(query);
        if(false==is_support_smartamp_calibrate(&adev->dev_ctl->smartamp_ctl)){
            return strdup("SmartAmpCalibration=unsupport");
        }else{
            if(access(AUDIO_SMARTAMP_CALI_PARAM_FILE, R_OK) != 0){
                return strdup("SmartAmpCalibration=1");
            }else{
                return strdup("SmartAmpCalibration=0");
            }
        }
    }

    if (str_parms_has_key(query, "SmartAmpCalibrationValues")){
        str_parms_destroy(query);
        if(false==is_support_smartamp_calibrate(&adev->dev_ctl->smartamp_ctl)){
            return strdup("SmartAmpCalibrationValues=null");
        }else{
             struct smart_amp_cali_param cali_values;
            int ret=0;
            char rsp_str[128]={0};
            memset(rsp_str,0,sizeof(rsp_str));
            cali_values.Re_T_cali_out=-1;
            cali_values.postfilter_flag=-1;
            ret=get_smartapm_cali_values(&adev->dev_ctl->smartamp_ctl,&cali_values);
            if(ret==sizeof(cali_values)){
                snprintf(rsp_str,sizeof(rsp_str)-1,"SmartAmpCalibrationValues=%u",cali_values.Re_T_cali_out);
            }else{
                snprintf(rsp_str,sizeof(rsp_str)-1,"SmartAmpCalibrationValues=null");
            }
            LOG_I("%s",rsp_str);
            return strdup(rsp_str);
        }
    }

    if (str_parms_has_key(query, "headphonestate")) {
        str_parms_destroy(query);
        char deltaStr[200] = {0};
        char *str = NULL;
        uint32_t device = 0;
        int usb_cardnum = -1;
        int usb_pdev = -1;
        char cardname[512];
        device = get_headset_device(adev);
        if(!device) {
            usb_cardnum = usb_card_parse(cardname, sizeof(cardname));
            if(usb_cardnum >= 0) {
                usb_pdev = usb_pdev_parse(usb_cardnum);
            }
        }
        if(usb_pdev >= 0){
            snprintf(deltaStr, sizeof(deltaStr), "headphonestate=%d;card=%d;device=%d;cardname=%s",
                AUDIO_DEVICE_OUT_USB_HEADSET, usb_cardnum, usb_pdev, cardname);
        }
        else {
            snprintf(deltaStr, sizeof(deltaStr), "headphonestate=%x", device);
        }
        LOG_I("headphonestate:%s",deltaStr);
        str = strdup(deltaStr);
        return str;
     }

    if (str_parms_has_key(query, "callstatus")) {
        str_parms_destroy(query);
        if((is_usecase(adev->dev_ctl,UC_CALL))
            &&(adev->call_status == VOICE_START_STATUS)){
            return strdup("callstatus=true");
        }else{
            return strdup("callstatus=false");
        }
    }

    if (str_parms_has_key(query, "voice_txup_on")) {
        str_parms_destroy(query);
        if(is_usecase(adev->dev_ctl,UC_VOICE_TX)){
            ALOGI("%s line:%d",__func__,__LINE__);
            return strdup("voice_txup_on=true");
        }else{
            ALOGI("%s line:%d",__func__,__LINE__);
            return strdup("voice_txup_on=false");
        }
    }

    if (str_parms_has_key(query, "voice_txdl_on")) {
        str_parms_destroy(query);
        if(is_usecase(adev->dev_ctl,UC_NORMAL_PLAYBACK|UC_FAST_PLAYBACK|UC_DEEP_BUFFER_PLAYBACK)){
            ALOGI("%s line:%d",__func__,__LINE__);
            return strdup("voice_txdl_on=true");
        }else{
            ALOGI("%s line:%d",__func__,__LINE__);
            return strdup("voice_txdl_on=false");
        }
    }

    if (str_parms_has_key(query, "voice_rxup_on")) {
        str_parms_destroy(query);
        if(is_usecase(adev->dev_ctl,UC_VOICE_RECORD)){
            struct tiny_stream_in *in=get_input_stream(adev,AUDIO_HW_APP_CALL_RECORD);
            if((NULL!=in)&&(AUDIO_SOURCE_VOICE_UPLINK==in->source)){
                ALOGI("%s line:%d",__func__,__LINE__);
                return strdup("voice_rxup_on=true");
            }
            ALOGI("%s line:%d",__func__,__LINE__);
        }
        ALOGI("%s line:%d",__func__,__LINE__);
        return strdup("voice_rxup_on=false");
    }

    if (str_parms_has_key(query, "voice_rxdl_on")) {
        str_parms_destroy(query);
        if(is_usecase(adev->dev_ctl,UC_VOICE_RECORD)){
            struct tiny_stream_in *in=get_input_stream(adev,AUDIO_HW_APP_CALL_RECORD);
            if((NULL!=in)&&(AUDIO_SOURCE_VOICE_DOWNLINK==in->source)){
                ALOGI("%s line:%d",__func__,__LINE__);
                return strdup("voice_rxdl_on=true");
            }
            ALOGI("%s line:%d",__func__,__LINE__);
        }
        ALOGI("%s line:%d",__func__,__LINE__);
        return strdup("voice_rxdl_on=false");
    }

    if (str_parms_has_key(query, "voice_rxdu_on")) {
        str_parms_destroy(query);
        if(is_usecase(adev->dev_ctl,UC_VOICE_RECORD)){
            struct tiny_stream_in *in=get_input_stream(adev,AUDIO_HW_APP_CALL_RECORD);
            if((NULL!=in)&&(AUDIO_SOURCE_VOICE_CALL==in->source)){
                ALOGI("%s line:%d",__func__,__LINE__);
                return strdup("voice_rxdu_on=true");
            }
            ALOGI("%s line:%d",__func__,__LINE__);
        }
        ALOGI("%s line:%d",__func__,__LINE__);
        return strdup("voice_rxdu_on=false");
    }

    str_parms_destroy(query);

    return strdup("");
}

static int adev_init_check(UNUSED_ATTR const struct audio_hw_device *dev)
{
    LOG_I("adev_init_check");
    return 0;
}

int adev_set_voice_volume(struct audio_hw_device *dev, float volume)
{
    struct tiny_audio_device *adev = (struct tiny_audio_device *)dev;
    int voice_volume = 0;
    pthread_mutex_lock(&adev->lock);
    voice_volume = (int)(volume * MAX_VOICE_VOLUME);

    if(voice_volume >= MAX_VOICE_VOLUME){
        voice_volume = MAX_VOICE_VOLUME;
    }
    adev->voice_volume=voice_volume;
    LOG_I("adev_set_voice_volume volume:%f level:%d", volume,voice_volume);

    /*Send at command to dsp side */
    pthread_mutex_lock(&adev->voip_start_lock);
    if((is_call_active_unlock(adev)) || (true==adev->voip_start)) {
        pthread_mutex_unlock(&adev->voip_start_lock);
        set_dsp_volume(adev->dev_ctl,voice_volume);
    }
    else {
        pthread_mutex_unlock(&adev->voip_start_lock);
    }
    pthread_mutex_unlock(&adev->lock);
    return 0;
}

static int adev_set_master_volume(UNUSED_ATTR struct audio_hw_device *dev, UNUSED_ATTR float volume)
{
    return -ENOSYS;
}

int set_voice_status(struct audio_hw_device *dev, voice_status_t status){
    struct tiny_audio_device *adev = (struct tiny_audio_device *)dev;
   // pthread_mutex_lock(&adev->lock);
    adev->call_status = status;
    LOG_I("set_voice_status:%d",status);
   // pthread_mutex_unlock(&adev->lock);
    return 0;
}

static int adev_set_mode(struct audio_hw_device *dev, audio_mode_t mode)
{
    struct tiny_audio_device *adev = (struct tiny_audio_device *)dev;

    LOG_I("adev_set_mode:%d adev mode:%d",mode,adev->call_mode);
    if(true==is_usecase(adev->dev_ctl,UC_LOOP)){
        LOG_E("adev_set_mode looping now, can not start voice call");
        return 0;
    }
    pthread_mutex_lock(&adev->lock);
    if (adev->call_mode != mode){
        adev->call_mode = mode;
        if (((mode == AUDIO_MODE_NORMAL) || (mode == AUDIO_MODE_IN_COMMUNICATION))&&(true== adev->call_start)){
            bool is_bt=false;
            adev->call_start=false;
            is_bt=is_bt_voice(adev);
            pthread_mutex_unlock(&adev->lock);
            if(true==is_bt){
                stop_voice_call(adev->dev_ctl);
            }else{
                send_cmd_to_dsp_thread(adev->dev_ctl->agdsp_ctl,AUDIO_CTL_STOP_VOICE,NULL);
            }
            return 0;
        }
    }
    pthread_mutex_unlock(&adev->lock);
    return 0;
}


int ext_setVoiceMode(void * dev,UNUSED_ATTR struct str_parms *parms,int mode, UNUSED_ATTR char * val){
    struct tiny_audio_device *adev = (struct tiny_audio_device *)dev;
    adev_set_mode(dev,mode);
    if(AUDIO_MODE_IN_CALL == mode){
        char value[AUDIO_EXT_CONTROL_PIPE_MAX_BUFFER_SIZE]={0};
        int ret=0;
        int voice_device=AUDIO_DEVICE_OUT_EARPIECE;
        ret = str_parms_get_str(parms,"test_out_stream_route", value, sizeof(value));
        if(ret >= 0){
            struct tiny_stream_out *voice_out=NULL;
            voice_device= strtoul(value,NULL,0);
            voice_out=get_output_stream(adev,AUDIO_HW_APP_CALL);
            if((NULL!=voice_out)&&(voice_device>0)){
                pthread_mutex_lock(&voice_out->lock);
                voice_out->devices=voice_device;
                LOG_I("ext_setVoiceMode devices:%x",voice_out->devices);
                pthread_mutex_unlock(&voice_out->lock);
            }
        }
        pthread_mutex_lock(&adev->lock);
        adev->call_start=true;
        pthread_mutex_unlock(&adev->lock);
        send_cmd_to_dsp_thread(adev->dev_ctl->agdsp_ctl,AUDIO_CTL_START_VOICE,NULL);
    }
    return 0;
}

static int adev_set_master_mute(struct audio_hw_device *dev, bool mute)
{
    struct tiny_audio_device *adev = (struct tiny_audio_device *)dev;
    pthread_mutex_lock(&adev->lock);
    if(mute!=adev->master_mute){
        LOG_I("adev_set_master_mute:%d",mute);
        adev->master_mute = mute;
    }
    pthread_mutex_unlock(&adev->lock);
    return 0;
}

static int adev_get_master_mute(struct audio_hw_device *dev, bool *mute)
{
    struct tiny_audio_device *adev = (struct tiny_audio_device *)dev;
    pthread_mutex_lock(&adev->lock);
    *mute = adev->master_mute;
    pthread_mutex_unlock(&adev->lock);
    return 0;
}

static int adev_set_mic_mute(struct audio_hw_device *dev, bool mute)
{
    struct tiny_audio_device *adev = (struct tiny_audio_device *)dev;
    int ret=-ENOSYS;
    pthread_mutex_lock(&adev->lock);
    if(mute!=adev->mic_mute){
        LOG_I("adev_set_mic_mute:%d",mute);
        ret=set_voice_ul_mute(adev->dev_ctl,mute);
        if(ret==0){
            LOG_I("adev_set_mic_mute:%d success",mute);
            adev->dsp_mic_mute = mute;
        }else{
            LOG_W("adev_set_mic_mute failed:%d :%d",mute,adev->mic_mute);
        }
        adev->mic_mute = mute;
    }
    pthread_mutex_unlock(&adev->lock);
    return 0;
}

static int adev_get_mic_mute(const struct audio_hw_device *dev, bool *state)
{
    struct tiny_audio_device *adev = (struct tiny_audio_device *)dev;
    pthread_mutex_lock(&adev->lock);
    *state = adev->mic_mute;
    pthread_mutex_unlock(&adev->lock);
    return 0;
}

static size_t adev_get_input_buffer_size(const struct audio_hw_device *dev,
        const struct audio_config *config)
{
    int channel_count = popcount(config->channel_mask);

    if (check_input_parameters
        (config->sample_rate, config->format, channel_count) != 0) {
        return 0;
    }

    return get_input_buffer_size((struct tiny_audio_device *)dev,
                                 config->sample_rate, config->format,
                                 channel_count);
}

static int in_add_audio_effect(UNUSED_ATTR const struct audio_stream *stream,
                               UNUSED_ATTR effect_handle_t effect)
{
    return 0;
}

static int in_remove_audio_effect(UNUSED_ATTR const struct audio_stream *stream,
                                  UNUSED_ATTR effect_handle_t effect)
{
    return 0;
}

int adev_open_input_stream(struct audio_hw_device *dev,
                                  UNUSED_ATTR audio_io_handle_t handle,
                                  audio_devices_t devices,
                                  struct audio_config *config,
                                  struct audio_stream_in **stream_in,
                                  audio_input_flags_t flags,
                                  const char *address __unused,
                                  audio_source_t source )
{
    struct tiny_audio_device *adev = (struct tiny_audio_device *)dev;
    struct tiny_stream_in *in;
    struct audio_control *control = NULL;
    int pcm_devices;
    int ret = 0;
    int channel_count = popcount(config->channel_mask);
    control = adev->dev_ctl;

    LOG_I
    ("adev_open_input_stream,devices=0x%x,sample_rate=%d, channel_count=%d flags:%x source:%d",
     devices, config->sample_rate, channel_count,flags,source);

    in = (struct tiny_stream_in *)calloc(1, sizeof(struct tiny_stream_in));
    if (!in) {
        LOG_E("adev_open_input_stream alloc fail, size:%d",
              sizeof(struct tiny_stream_in));
        return -ENOMEM;
    }
    memset(in, 0, sizeof(struct tiny_stream_in));
    pthread_mutex_init(&in->lock, NULL);
    in->stream.common.get_sample_rate = in_get_sample_rate;
    in->stream.common.set_sample_rate = in_set_sample_rate;
    in->stream.common.get_buffer_size = in_get_buffer_size;
    in->stream.common.get_channels = in_get_channels;
    in->stream.common.get_format = in_get_format;
    in->stream.common.set_format = in_set_format;
    in->stream.common.standby = in_standby;
    in->stream.common.dump = in_dump;
    in->stream.common.set_parameters = in_set_parameters;
    in->stream.common.get_parameters = in_get_parameters;
    in->stream.common.add_audio_effect = in_add_audio_effect;
    in->stream.common.remove_audio_effect = in_remove_audio_effect;
    in->stream.set_gain = in_set_gain;
    in->stream.read = in_read;
    in->stream.get_input_frames_lost = in_get_input_frames_lost;
    in->stream.get_capture_position = in_get_capture_position;

#ifdef AUDIOHAL_V4
    in->stream.get_active_microphones = in_get_active_microphones;
#endif

    in->requested_rate = config->sample_rate;
    in->requested_channels = channel_count;

    in->resampler = NULL;
    in->source=source;
    in->config = (struct pcm_config *) malloc(sizeof(struct pcm_config));
    if(!in->config){
        LOG_E(" adev_open_input_stream malloc failed");
        goto err;
    }

    pthread_mutex_lock(&adev->lock);
    in->dev = adev;

    if ((flags & AUDIO_INPUT_FLAG_MMAP_NOIRQ) &&
         (config->sample_rate == MM_FULL_POWER_SAMPLING_RATE)) {
        in->audio_app_type = AUDIO_HW_APP_MMAP_RECORD;
        in->stream.start = in_start;
        in->stream.stop = in_stop;
        in->stream.create_mmap_buffer = in_create_mmap_buffer;
        in->stream.get_mmap_position = in_get_mmap_position;
    } else {
        if((AUDIO_SOURCE_VOICE_CALL==in->source)||
            (AUDIO_SOURCE_VOICE_UPLINK==in->source)||
            (AUDIO_SOURCE_VOICE_DOWNLINK==in->source)){
            in->audio_app_type =AUDIO_HW_APP_CALL_RECORD;
        }else if(AUDIO_SOURCE_VOICE_RECOGNITION==in->source){
            in->audio_app_type=AUDIO_HW_APP_RECOGNITION;
        }else if((AUDIO_SOURCE_FM_TUNER==in->source)){
            in->audio_app_type =AUDIO_HW_APP_FM_RECORD;
        }else if(AUDIO_SOURCE_CAMCORDER==in->source){
            in->audio_app_type =AUDIO_HW_APP_NORMAL_RECORD;
        }else if(true==adev->voip_start){
            in->audio_app_type =AUDIO_HW_APP_VOIP_RECORD;
        }else if(is_usecase(adev->dev_ctl,UC_VOIP)){
            in->audio_app_type =AUDIO_HW_APP_VOIP_RECORD;
        }else if((AUDIO_DEVICE_IN_ALL_SCO &(~AUDIO_DEVICE_BIT_IN)) & devices){
            in->audio_app_type =AUDIO_HW_APP_BT_RECORD;
        }else{
            in->audio_app_type =AUDIO_HW_APP_NORMAL_RECORD;
        }

        in->standby_fun=do_normal_inputput_standby;
    }

    pthread_mutex_unlock(&adev->lock);

    ret = dev_ctl_get_in_pcm_config(control, in->audio_app_type, &pcm_devices, in->config);
    if(ret != 0 ){
        goto err;
    }

    if(in->requested_rate) {
        if(source == AUDIO_SOURCE_CAMCORDER){
            in->pop_mute_bytes = RECORD_POP_MIN_TIME_CAM*in->requested_rate/1000*audio_stream_in_frame_size(&(in->stream));
        } else {
            in->pop_mute_bytes = RECORD_POP_MIN_TIME_MIC*in->requested_rate/1000*audio_stream_in_frame_size(&(in->stream));
        }
    }
    in->standby = true;
    in->devices = devices;
    in->pop_mute = true;

    *stream_in = &in->stream;

     audio_add_input(adev,in);
     LOG_I
    ("adev_open_input_stream,devices=0x%x,sample_rate=%d, channel_count=%d",
     devices, config->sample_rate, in->config->channels);
    LOG_D("Successfully, adev_open_input_stream.");
    return 0;

err:

    LOG_E("Failed(%d), adev_open_input_stream.", ret);
    if (in->buffer) {
        free(in->buffer);
        in->buffer = NULL;
    }
    if (in->resampler) {
        release_resampler(in->resampler);
        in->resampler = NULL;
    }
    if (in->proc_buf) {
        free(in->proc_buf);
        in->proc_buf = NULL;
    }
    if(in->config){
        free(in->config);
        in->config = NULL;
    }
    in->proc_buf_size=0;

    free(in);
    *stream_in = NULL;
    return ret;
}

static void adev_close_input_stream(struct audio_hw_device *dev,
                                    struct audio_stream_in *stream)
{
    struct tiny_stream_in *in = (struct tiny_stream_in *)stream;
    struct tiny_audio_device *adev = (struct tiny_audio_device *)dev;

    LOG_D("adev_close_input_stream");

    in_standby(&stream->common);
    audio_del_input((struct tiny_audio_device *)dev,in);

    pthread_mutex_lock(&in->lock);
    if (in->resampler) {
        free(in->buffer);
        release_resampler(in->resampler);
        in->resampler=NULL;
    }

    if (in->nr_buffer) {
        free(in->nr_buffer);
        in->nr_buffer = NULL;
    }

    if (in->proc_buf) {
        free(in->proc_buf);
        in->proc_buf = NULL;
    }
    in->proc_buf_size = 0;

    if(in->config){
        free(in->config);
        in->config = NULL;
    }


#ifdef SPRD_AUDIO_HIDL_CLIENT
    if(stream==(struct audio_stream_in *)adev->test_ctl.ouput_test.hidl_inputstream){
        adev->test_ctl.ouput_test.hidl_inputstream=NULL;
    }

    if(stream==(struct audio_stream_in *)adev->test_ctl.input_test.hidl_inputstream){
        adev->test_ctl.input_test.hidl_inputstream=NULL;
    }

    if(stream==(struct audio_stream_in *)adev->test_ctl.loop_test.hidl_inputstream){
        adev->test_ctl.loop_test.hidl_inputstream=NULL;
    }
#endif
    pthread_mutex_unlock(&in->lock);
    pthread_mutex_destroy(&in->lock);
    LOG_I("%s %d",__func__,__LINE__);
    free(stream);
    return;
}

#ifdef AUDIOHAL_V4
static int adev_get_microphones(const struct audio_hw_device *dev,
                                struct audio_microphone_characteristic_t *mic_array,
                                size_t *mic_count) {
    struct tiny_audio_device *adev = (struct tiny_audio_device *)dev;
    LOG_I("adev_get_microphones enter:%d",*mic_count);

    pthread_mutex_lock(&adev->lock);
    int ret = platform_get_microphones(&adev->dev_ctl->config.audio_data, mic_array, mic_count);
    pthread_mutex_unlock(&adev->lock);
    LOG_I("adev_get_microphones exit:%d",*mic_count);
    return ret;
}
#endif

#ifdef AUDIOHAL_V4
static void audio_port_config_log(const struct audio_port_config *config, const char *tag_str){
    unsigned int numValues=0;
    unsigned int i=0;

    switch(config->type){
        case AUDIO_PORT_TYPE_DEVICE:
            LOG_I("AUDIO_PORT_TYPE_DEVICE %s device type:0x%x hw_module:%d\n",tag_str
                ,config->ext.device.type,config->ext.device.hw_module);
            break;

        case AUDIO_PORT_TYPE_MIX:
            LOG_I("AUDIO_PORT_TYPE_MIX %s handle:%d hw_module:%d stream:%d  source:%d\n",tag_str
                ,config->ext.mix.handle,config->ext.mix.hw_module
                ,config->ext.mix.usecase.stream
                ,config->ext.mix.usecase.source);
            break;

        case AUDIO_PORT_TYPE_SESSION:
            LOG_I("AUDIO_PORT_TYPE_SESSION %s session:%d\n",tag_str,
                config->ext.session.session);
            break;

        case AUDIO_PORT_TYPE_NONE:
            LOG_I("AUDIO_PORT_TYPE_NONE %s session:%d\n",tag_str,
                config->ext.session.session);
            break;

        default:
            LOG_I("AUDIO_PORT_TYPE_INVALID %s\n",tag_str);
            break;
    }

    LOG_D("%s audio_port_config id:%d role:%d type:%d config_mask:%d sample_rate:%d channel_mask id:%d format:%d\n",
        tag_str,
        config->id, config->role,config->type, config->config_mask,
        config->sample_rate,config->channel_mask,  config->format);

    LOG_D("%s gain index:%d mode:0x%x channel_mask:0x%x ramp_duration_ms:%d\n",tag_str,
        config->gain.index,config->gain.mode,config->gain.channel_mask,
        config->gain.ramp_duration_ms);

    numValues = audio_channel_count_from_in_mask(config->channel_mask);
    if(numValues<audio_channel_count_from_out_mask(config->channel_mask)){
        numValues=audio_channel_count_from_out_mask(config->channel_mask);
    }

    if(numValues>=sizeof(audio_channel_mask_t) * 8){
        numValues=sizeof(audio_channel_mask_t) * 8;
    }
    for(i=0;i<numValues;i++){
        LOG_D("%s gain[%d]:0x%x\n",tag_str,i,config->gain.values[i]);
    }

}

static int adev_set_audio_port_config(UNUSED_ATTR struct audio_hw_device *dev,
        const struct audio_port_config *config) {
    audio_port_config_log(config,"adev_set_audio_port_config");
    return 0;
}

static int adev_create_audio_patch(struct audio_hw_device *dev,
        unsigned int num_sources,
        const struct audio_port_config *sources,
        unsigned int num_sinks,
        const struct audio_port_config *sinks,
        audio_patch_handle_t *handle) {
    struct tiny_audio_device *adev = (struct tiny_audio_device *)dev;
    unsigned int i=0;

    audio_devices_t sources_device_type=0;
    audio_devices_t sinks_device_type=0;

    if(false==adev->boot_completed){
        pthread_mutex_lock(&adev->lock);
        check_boot_status(dev);
        pthread_mutex_unlock(&adev->lock);
    }

    for (i = 0; i < num_sources; i++) {
        LOG_I("%s: source[%d] type=%d address=%s", __func__, i, sources[i].type,
                sources[i].type == AUDIO_PORT_TYPE_DEVICE
                ? sources[i].ext.device.address
                : "");
        audio_port_config_log(&sources[i],"source");
        if((sources[i].type == AUDIO_PORT_TYPE_DEVICE)
            &&sources[i].ext.device.type>0){
            sources_device_type |=sources[i].ext.device.type;
        }
    }

    for (i = 0; i < num_sinks; i++) {
        LOG_I("%s: sink[%d] type=%d address=%s", __func__, i, sinks[i].type,
                sinks[i].type == AUDIO_PORT_TYPE_DEVICE ? sinks[i].ext.device.address
                : "N/A");
        audio_port_config_log(&sinks[i],"sink");
        if((sinks[i].type == AUDIO_PORT_TYPE_DEVICE)
            &&sinks[i].ext.device.type>0){
            sinks_device_type |=sinks[i].ext.device.type;
        }
    }

    LOG_I("sources_device_type:%x sinks_device_type:%x",sources_device_type,sinks_device_type);

    LOG_I("sinks type:%x sources type:%x",sinks[0].type,sources[0].type);

    if ((num_sinks == 1 && sinks[0].type == AUDIO_PORT_TYPE_MIX
        &&(sinks[0].ext.mix.usecase.source>=0)
        &&(sources_device_type>0)&&(sources_device_type!=AUDIO_DEVICE_IN_TELEPHONY_RX))){
        pthread_mutex_lock(&adev->lock);
        if(false==is_usbmic_offload_supported(adev->dev_ctl)){
            sources_device_type &= ~ALL_USB_INPUT_DEVICES;
            sources_device_type |= AUDIO_DEVICE_BIT_IN;
        }
        adev_in_devices_check(adev,sources_device_type);
        adev->in_devices=sources_device_type;
        select_devices_new(adev->dev_ctl,AUDIO_HW_APP_INVALID,sources_device_type,true,true,false,false);
        pthread_mutex_unlock(&adev->lock);
    }

    if ((num_sources == 1 && sources[0].type == AUDIO_PORT_TYPE_MIX
        &&(sinks_device_type>0)&&(sinks_device_type!=AUDIO_DEVICE_OUT_TELEPHONY_TX))){
        pthread_mutex_lock(&adev->lock);

        if(false==adev->usb_ctl.support_offload){
            sinks_device_type &= ~ALL_USB_OUTPUT_DEVICES;
        }

        if(!sinks_device_type) {
            LOG_E("sinks_device_type is 0");
            pthread_mutex_unlock(&adev->lock);
            return 0;
        }

        if((adev->bt_sco_status==false)&&(sinks_device_type&AUDIO_DEVICE_OUT_ALL_SCO)){
            LOG_I("bt sco is off, do not set bt sco");
            pthread_mutex_unlock(&adev->lock);
            return 0;
        }

        if(adev->boot_completed==false){
            if(((AUDIO_DEVICE_OUT_WIRED_HEADPHONE|AUDIO_DEVICE_OUT_WIRED_HEADSET)&sinks_device_type)
                ||(AUDIO_DEVICE_OUT_ALL_SCO&sinks_device_type)){
                LOG_I("out_set_parameters connect devices, boot completed");
                set_audio_boot_completed(adev);
            }else{
                check_boot_status(adev);
                if(adev->boot_completed==false){
                    sinks_device_type=0;
                    LOG_I("system is not ready, can not set audio devices form audiopolicymanager");
                    pthread_mutex_unlock(&adev->lock);
                    return 0;
                }
            }
        }

        if(is_call_active_unlock(adev)){
            if((adev->out_devices&AUDIO_DEVICE_OUT_ALL_SCO)&&(0==(sinks_device_type&AUDIO_DEVICE_OUT_ALL_SCO))){
                force_in_standby(adev,AUDIO_HW_APP_BT_RECORD);
            }else if((sinks_device_type&AUDIO_DEVICE_OUT_ALL_SCO)&&(0==(adev->out_devices&AUDIO_DEVICE_OUT_ALL_SCO))){
                force_in_standby(adev,AUDIO_HW_APP_NORMAL_RECORD);
            }
        }

        do_set_output_device(adev->dev_ctl,sinks_device_type);

        if((AUDIO_MODE_IN_CALL == adev->call_mode)&& (false==adev->call_start)){
            adev->call_start=true;
            adev->out_devices=sinks_device_type;
            pthread_mutex_unlock(&adev->lock);
            send_cmd_to_dsp_thread(adev->dev_ctl->agdsp_ctl,AUDIO_CTL_START_VOICE,NULL);
        }else{
            adev->out_devices=sinks_device_type;
            pthread_mutex_unlock(&adev->lock);
        }
    }

    if (num_sources == 1 && num_sinks == 1 &&
            sources[0].type == AUDIO_PORT_TYPE_DEVICE &&
            sinks[0].type == AUDIO_PORT_TYPE_DEVICE) {
        // The same audio_patch_handle_t will be passed to release_audio_patch
        *handle = 42;
        LOG_D("%s: handle: %d loop", __func__, *handle);
    }else{
        *handle=adev->patch_handle++;
        if(*handle==42){
            *handle=adev->patch_handle++;
        }
        if(adev->patch_handle>=0xfffffff){
            adev->patch_handle=1;
        }
        LOG_D("%s: handle: %d", __func__,*handle);
    }
    return 0;
}

static int adev_release_audio_patch(UNUSED_ATTR struct audio_hw_device *dev,
        audio_patch_handle_t handle) {
    LOG_D("%s: handle: %d", __func__, handle);
    return 0;
}
#endif

static int adev_dump(const audio_hw_device_t *device, int fd)
{
    char buffer[DUMP_BUFFER_MAX_SIZE]={0};
    char tmp[128]={0};
    int ret=-1;
    struct tiny_audio_device *adev = (struct tiny_audio_device *)device;
    pthread_mutex_lock(&adev->lock);

    ret=property_get(DUMP_AUDIO_PROPERTY, tmp, "0");
    if (ret < 0) {
        LOG_I("adev_dump property_get Failed");
    }else{
        adev->dump_flag=strtoul(tmp,NULL,0);
    }

    snprintf(buffer,(DUMP_BUFFER_MAX_SIZE-1),"\nadev_dump ===>\n");
    AUDIO_DUMP_WRITE_STR(fd,buffer);
    memset(buffer,0,sizeof(buffer));
    LOG_I("adev_dump enter");

    snprintf(buffer,(DUMP_BUFFER_MAX_SIZE-1),
        "mixer:%p "
        "mode:%d "
        "call_status:%d "
        "low_power:%d "
        "mic_mute:%d "
        "master_mute:%d\n",
        adev->mixer,
        adev->call_mode,
        adev->call_status,
        adev->low_power,
        adev->mic_mute,
        adev->master_mute);
    AUDIO_DUMP_WRITE_STR(fd,buffer);
    memset(buffer,0,sizeof(buffer));

    snprintf(buffer,(DUMP_BUFFER_MAX_SIZE-1),
        "bluetooth_nrec:%d "
        "bluetooth_type:%d "
        "bt_wbs:%d "
        "voice_volume:%d "
        "offload_on:%d "
        "fm_record:%d "
        "voip_start:0x%x\n",
        adev->bluetooth_nrec,
        adev->bluetooth_type,
        adev->bt_wbs,
        adev->voice_volume,
        adev->offload_on,
        adev->fm_record,
        adev->voip_start);
    AUDIO_DUMP_WRITE_STR(fd,buffer);
    memset(buffer,0,sizeof(buffer));

    snprintf(buffer,(DUMP_BUFFER_MAX_SIZE-1),
        "call_start:%d "
        "is_agdsp_asserted:%d "
        "dsp_loop:%d ",
        adev->call_start,
        adev->is_agdsp_asserted,
        adev->is_dsp_loop);
    AUDIO_DUMP_WRITE_STR(fd,buffer);
    memset(buffer,0,sizeof(buffer));

    dump_audio_control(fd,buffer,DUMP_BUFFER_MAX_SIZE,adev->dev_ctl);
    dump_audio_param(fd,buffer,DUMP_BUFFER_MAX_SIZE,&adev->audio_param);

    /* dump tinymix */
    dump_tinymix_infor(fd,adev->dump_flag,adev->mixer);

#ifdef AUDIOHAL_V4
    snprintf(buffer,(DUMP_BUFFER_MAX_SIZE-1),"microphones_count:%d \nadev_dump <===\n",
        adev->dev_ctl->config.audio_data.microphones_count);
#else
    snprintf(buffer,(DUMP_BUFFER_MAX_SIZE-1),"\nadev_dump <===\n");
#endif
    AUDIO_DUMP_WRITE_STR(fd,buffer);
    LOG_I("adev_dump exit");

    pthread_mutex_unlock(&adev->lock);
    return 0;
}

static void close_all_stream(void *dev){
    struct tiny_stream_out *out;
    struct tiny_stream_in *in;
    struct listnode *item;
    struct listnode *item2;
    struct tiny_audio_device *adev = (struct tiny_audio_device *)dev;
    LOG_I("close_all_stream");
    pthread_mutex_lock(&adev->output_list_lock);
    if(!list_empty(&adev->active_out_list)){
        list_for_each_safe(item, item2,&adev->active_out_list){
            out = node_to_item(item, struct tiny_stream_out, node);
            pthread_mutex_unlock(&adev->output_list_lock);
            adev_close_output_stream(&adev->hw_device,&out->stream);
            pthread_mutex_lock(&adev->output_list_lock);
        }
    }
    pthread_mutex_unlock(&adev->output_list_lock);

    pthread_mutex_lock(&adev->input_list_lock);
    if(!list_empty(&adev->active_input_list)){
        list_for_each_safe(item, item2,&adev->active_input_list){
            in = node_to_item(item, struct tiny_stream_in, node);
            pthread_mutex_unlock(&adev->input_list_lock);
            adev_close_input_stream(&adev->hw_device,&in->stream);
            pthread_mutex_lock(&adev->input_list_lock);
        }
    }
    pthread_mutex_unlock(&adev->input_list_lock);
    LOG_I("close_all_stream:exit");
}

static int adev_close(hw_device_t *device)
{
    struct tiny_audio_device *adev = (struct tiny_audio_device *)device;
    int wait_count=0;
    pthread_mutex_lock(&adev_init_lock);

    LOG_I("adev_close refcount:%d",primary_dev_count);
    while(false==is_all_audio_param_ready(&adev->audio_param)) {
        usleep(300000);
        wait_count++;
        LOG_I("%s usleep line:%d status:0x%x wait_count:%d",__func__,__LINE__,
            adev->audio_param.load_status,wait_count);
        if(wait_count>=20){
            break;
        }
    }
    pthread_mutex_lock(&adev->lock);

    if(primary_dev_count>0){
        LOG_I("adev_close return primary_dev_count:%d",primary_dev_count);
        primary_dev_count--;
    }

    if(primary_dev_count) {
        pthread_mutex_unlock(&adev->lock);
        pthread_mutex_unlock(&adev_init_lock);
        return 0;
    }
    primary_dev=NULL;
    pthread_mutex_unlock(&adev->lock);
    LOG_I("%s line:%d",__func__,__LINE__);
    LOG_I("%s line:%d",__func__,__LINE__);
    force_all_standby(adev);
    LOG_I("%s line:%d",__func__,__LINE__);
    close_all_stream(adev);
    deinit_usbctl(&adev->usb_ctl);
    LOG_I("%s line:%d",__func__,__LINE__);
    stream_routing_manager_close(adev->dev_ctl);
#ifdef AUDIO_DEBUG
    LOG_I("%s line:%d",__func__,__LINE__);
    debug_dump_close(&adev->debugdump);
#endif
    LOG_I("%s line:%d",__func__,__LINE__);
    ext_control_close(adev);
    LOG_I("%s line:%d",__func__,__LINE__);
    free_audio_param(&adev->audio_param);
    LOG_I("%s line:%d",__func__,__LINE__);
    free_audio_control(adev->dev_ctl);
    LOG_I("%s line:%d",__func__,__LINE__);
    modem_monitor_close(&adev->modem_monitor);

    if(adev->bootup_timer.created) {
       timer_delete(adev->bootup_timer.timer_id);
       adev->bootup_timer.created = false;
    }
    pthread_mutex_destroy(&adev->input_list_lock);
    pthread_mutex_destroy(&adev->output_list_lock);
    pthread_mutex_destroy(&adev->voip_start_lock);

    pthread_mutex_destroy(&adev->lock);
	
#ifdef AWINIC_EFFECT_SUPPORT
    if(adev->awinic_skt.audio_data_buffer != NULL){
        adev->awinic_skt.end(adev->awinic_skt.module_context_buffer);
        free(adev->awinic_skt.audio_data_buffer);
		
		
    }
    if(adev->awinic_skt.module_context_buffer != NULL)
      free(adev->awinic_skt.module_context_buffer); 

    if(adev->awinic_skt.awinic_lib != NULL){
	 	dlclose(adev->awinic_skt.awinic_lib);
	 	adev->awinic_skt.awinic_lib = NULL;
	}
#endif

    free(adev->dev_ctl);
    free(device);
    pthread_mutex_unlock(&adev_init_lock);

    LOG_I("%s line:%d success primary_dev_count %d",__func__,__LINE__, primary_dev_count);
    return 0;
}

static uint32_t adev_get_supported_devices(UNUSED_ATTR const struct audio_hw_device *dev)
{
    return (    /* OUT */
               AUDIO_DEVICE_OUT_EARPIECE |
               AUDIO_DEVICE_OUT_SPEAKER |
               AUDIO_DEVICE_OUT_WIRED_HEADSET |
               AUDIO_DEVICE_OUT_WIRED_HEADPHONE |
               AUDIO_DEVICE_OUT_AUX_DIGITAL |
               AUDIO_DEVICE_OUT_ANLG_DOCK_HEADSET |
               AUDIO_DEVICE_OUT_DGTL_DOCK_HEADSET |
               AUDIO_DEVICE_OUT_ALL_SCO |
               ALL_USB_OUTPUT_DEVICES |
               AUDIO_DEVICE_OUT_FM | AUDIO_DEVICE_OUT_DEFAULT |
               /* IN */
               AUDIO_DEVICE_IN_COMMUNICATION |
               AUDIO_DEVICE_IN_AMBIENT |
               AUDIO_DEVICE_IN_BUILTIN_MIC |
               AUDIO_DEVICE_IN_WIRED_HEADSET |
               AUDIO_DEVICE_IN_AUX_DIGITAL |
               AUDIO_DEVICE_IN_BACK_MIC |
               AUDIO_DEVICE_IN_BLUETOOTH_SCO_HEADSET |
               AUDIO_DEVICE_IN_ALL_SCO | AUDIO_DEVICE_IN_VOICE_CALL |
               ALL_USB_INPUT_DEVICES |
               //AUDIO_DEVICE_IN_LINE_IN | //zzjtodo
               AUDIO_DEVICE_IN_DEFAULT);
}

/*
    Read audproc params from nv and config.
    return value: TRUE:success, FALSE:failed
*/
bool init_rec_process(struct audio_stream_in *stream, int sample_rate)
{
    bool ret1 = false;
    bool ret2 = false;
    struct tiny_stream_in *in = (struct tiny_stream_in *)stream;
    struct tiny_audio_device *adev = in->dev;
    struct audio_record_proc_param *param_data=NULL;

    LOG_I("init_rec_process");

    int size = 0;
    int buf_size = 0;
    size = in->config->period_size;
    size = ((size + 15) / 16) * 16;
    buf_size = size * 2 * sizeof(short);
    if (in->proc_buf_size < (size_t) buf_size) {
        if (in->proc_buf)
        { free(in->proc_buf); }
        in->proc_buf = malloc(buf_size);
        if (!in->proc_buf) {
            LOG_E("init_rec_process:%d",__LINE__);
            return false;
        }
        in->proc_buf_size = buf_size;
    }


    param_data=(struct audio_record_proc_param *)get_ap_record_param(adev->dev_ctl->audio_param,in->devices);

    if(NULL==param_data){
      LOG_I("audio record process param is null");
      return false;
    }

    LOG_D("init_rec_process AUDPROC_initDp DP_input_gain:%x COMPRESSOR_threshold:%x DP_lcf_gain_r:%x",
        param_data->dp_control.DP_input_gain,param_data->dp_control.COMPRESSOR_threshold,param_data->dp_control.DP_lcf_gain_r);
    ret1 = AUDPROC_initDp(&param_data->dp_control, sample_rate);
    if(true!=ret1){
        LOG_W("init_rec_process AUDPROC_initDp failed");
    }
    LOG_D("init_rec_process AUDPROC_initRecordEq RECORDEQ_sw_switch:%x RECORDEQ_band_para[2]:%x RECORDEQ_band_para[5]:%x",
        param_data->record_eq.RECORDEQ_sw_switch,param_data->record_eq.RECORDEQ_band_para[2].gain,param_data->record_eq.RECORDEQ_band_para[5].gain);

    ret2 = AUDPROC_initRecordEq(&param_data->record_eq, sample_rate);
    if(true!=ret2){
        LOG_W("init_rec_process AUDPROC_initRecordEq failed");
    }

    return (ret1 || ret2);
}


int aud_dsp_assert_set(void * dev, bool asserted)
{
    struct tiny_audio_device *adev  = dev;
    if(asserted) {
        adev->is_agdsp_asserted = true;
    }
    else {
        adev->is_agdsp_asserted = false;
        if(adev->call_mode  ==AUDIO_MODE_IN_CALL) {
            send_cmd_to_dsp_thread(adev->dev_ctl->agdsp_ctl,AUDIO_CTL_START_VOICE,NULL);
        }
    }
    return 0;
}

int aud_rec_do_process(void *buffer, size_t bytes, int channels,void *tmp_buffer,
                              size_t tmp_buffer_bytes)
{
    int16_t *temp_buf = NULL;
    size_t read_bytes = bytes;
    unsigned int dest_count = 0;
    temp_buf = (int16_t *) tmp_buffer;
    if (temp_buf && (tmp_buffer_bytes >= 2)) {
        do {
            if (tmp_buffer_bytes <= bytes) {
                read_bytes = tmp_buffer_bytes;
            } else {
                read_bytes = bytes;
            }
            bytes -= read_bytes;
            LOG_D("aud_rec_do_process");
            if(1==channels){
                AUDPROC_ProcessDp((int16_t *) buffer,
                                  (int16_t *) buffer, read_bytes >> 1,
                                  temp_buf, temp_buf, &dest_count);
                memcpy(buffer, temp_buf, read_bytes);
             }else if(2==channels){
                //AUDPROC_ProcessDp_2((int16 *) buffer, read_bytes >> 1, 2, temp_buf, &dest_count);
                //memcpy(buffer, temp_buf, read_bytes);
                AUDPROC_ProcessDp_2((int16 *) buffer, read_bytes >> 1, 2, buffer, &dest_count);
            }else{
                LOG_W("aud_rec_do_process do not supprot channels=%d",channels);
            }
            buffer = (uint8_t *) buffer + read_bytes;
        } while (bytes);
    } else {
        LOG_E("temp_buf malloc failed.(len=%d)", (int)read_bytes);
        return -1;
    }
    return 0;
}

static int adev_open(const hw_module_t *module, const char *name,
                     hw_device_t **device)
{
    struct tiny_audio_device *adev;
    struct sysinfo info;
    LOG_I("adev_open primary_dev_count:%d",primary_dev_count);

    if (strcmp(name, AUDIO_HARDWARE_INTERFACE) != 0) {
        return -EINVAL;
    }
    pthread_mutex_lock(&adev_init_lock);

    primary_dev_count++;

    if(primary_dev!=NULL){
        *device = &primary_dev->hw_device.common;
        adev=(struct tiny_audio_device *)(*device);
        set_audio_boot_completed(adev);
        LOG_I("adev_open:aready open  refcount:%d",primary_dev_count);
        pthread_mutex_unlock(&adev_init_lock);

        return 0;
    }

    adev = calloc(1, sizeof(struct tiny_audio_device));
    if (!adev) {
        LOG_E("malloc tiny_audio_device failed, size:%d",
              sizeof(struct tiny_audio_device));
        primary_dev_count--;
        pthread_mutex_unlock(&adev_init_lock);
        return -ENOMEM;
    }
    memset(adev, 0, sizeof(struct tiny_audio_device));

#ifdef AUDIOHAL_V4
    adev->hw_device.common.version = AUDIO_DEVICE_API_VERSION_CURRENT;
#else
    adev->hw_device.common.version = AUDIO_DEVICE_API_VERSION_2_0;
#endif

    adev->hw_device.common.tag = HARDWARE_DEVICE_TAG;
    adev->hw_device.common.module = (struct hw_module_t *)module;
    adev->hw_device.common.close = adev_close;

    adev->hw_device.get_supported_devices = adev_get_supported_devices;
    adev->hw_device.init_check = adev_init_check;
    adev->hw_device.set_voice_volume = adev_set_voice_volume;
    adev->hw_device.set_master_volume = adev_set_master_volume;
    adev->hw_device.set_mode = adev_set_mode;
    adev->hw_device.set_master_mute = adev_set_master_mute;
    adev->hw_device.get_master_mute = adev_get_master_mute;
    adev->hw_device.set_mic_mute = adev_set_mic_mute;
    adev->hw_device.get_mic_mute = adev_get_mic_mute;
    adev->hw_device.set_parameters = adev_set_parameters;
    adev->hw_device.get_parameters = adev_get_parameters;
    adev->hw_device.get_input_buffer_size = adev_get_input_buffer_size;
    adev->hw_device.open_output_stream = adev_open_output_stream;
    adev->hw_device.close_output_stream = adev_close_output_stream;
    adev->hw_device.open_input_stream = adev_open_input_stream;
    adev->hw_device.close_input_stream = adev_close_input_stream;
    adev->hw_device.dump = adev_dump;

#ifdef AUDIOHAL_V4
    adev->hw_device.get_microphones = adev_get_microphones;

    // New in AUDIO_DEVICE_API_VERSION_3_0
    adev->hw_device.set_audio_port_config = adev_set_audio_port_config;
    adev->hw_device.create_audio_patch = adev_create_audio_patch;
    adev->hw_device.release_audio_patch = adev_release_audio_patch;
    adev->patch_handle=1;
#endif

    adev->call_start = false;
    adev->call_mode = AUDIO_MODE_NORMAL;
    adev->call_status = VOICE_INVALID_STATUS;
    adev->mic_mute = false;
    adev->dsp_mic_mute = false;
    adev->low_power = false;
    adev->master_mute=false;
    adev->fm_record_start=false;
    adev->normal_record_start=false;
    adev->voip_record_start = false;
    adev->bt_sco_status=false;
    adev->issupport_usb=true;
    init_usbctl(&adev->usb_ctl);

    pthread_mutex_init(&adev->input_list_lock,NULL);
    pthread_mutex_init(&adev->output_list_lock,NULL);
    pthread_mutex_init(&adev->voip_start_lock,NULL);
    pthread_mutex_init(&adev->lock,NULL);

    adev->dev_ctl = init_audio_control(adev);
    if (adev->dev_ctl == NULL) {
        LOG_E("adev_open Init audio control failed ");
        goto error_malloc;
    }

    pthread_mutex_lock(&adev->lock);
    init_audio_param(adev);

    pthread_mutex_unlock(&adev->lock);
    adev->audio_param.tunning.running=false;
    adev->audio_param.tunning.wire_connected=false;

    *device = &adev->hw_device.common;

    ext_control_init(adev);
    pthread_mutex_init(&adev->output_list_lock, NULL);
    pthread_mutex_init(&adev->input_list_lock, NULL);
    list_init(&adev->active_out_list);
    list_init(&adev->active_input_list);

#if defined(MINI_AUDIO) && defined(NORMAL_AUDIO_PLATFORM)
    LOG_I("line:%d test",__LINE__);
#else
    modem_monitor_open(adev,&adev->modem_monitor);
#endif

#ifdef AUDIO_DEBUG
    debug_dump_open(&adev->debugdump);
#endif

#if defined(MINI_AUDIO) && defined(NORMAL_AUDIO_PLATFORM)
    LOG_I("line:%d test",__LINE__);
#else
    audio_dsp_loop_open(adev);
    voice_open(adev);
    fm_open(adev);
#endif

    adev->boot_completed=false;
    if (sysinfo(&info)) {
        LOG_E( "Failed to get sysinfo, errno:%u, reason:%s\n", errno, strerror(errno));
    }else{
        LOG_I( "adev_open get sysinfo, uptime:%ld\n", info.uptime);
        if(info.uptime>60){
            LOG_I("adev_open set boot completed");
            set_audio_boot_completed(adev);
        }
    }

    if(false==adev->boot_completed){
        create_bootup_timer(adev,12);//check boot up status 12s
    }

#ifdef AWINIC_EFFECT_SUPPORT
  int ret =0;

  adev->awinic_skt.is_module_ready = true;
  adev->awinic_skt.audio_data_buffer = (char*)calloc(16*1024,sizeof(char));
  if(adev->awinic_skt.audio_data_buffer == NULL)
  {
     ALOGE("%s: Awinic alloc data buffer failed !",__func__);
     adev->awinic_skt.is_module_ready = false;
     return 0;
  } 
  ALOGE("%s: Awinic alloc data buffer success is_module_ready =%d!",__func__, adev->awinic_skt.is_module_ready);
  adev->awinic_skt.awinic_lib = dlopen(AWINIC_LIB_PATH,RTLD_NOW);
  if(adev->awinic_skt.awinic_lib == NULL)
  {
    ALOGE("%s: Awinic Dlopen lib Failed !",__func__);
    ALOGE("%s: Awinic Dlopen - %s !",__func__, dlerror());
    adev->awinic_skt.is_module_ready = false;
    free(adev->awinic_skt.audio_data_buffer);
	 adev->awinic_skt.audio_data_buffer = NULL;
    return 0;
  }
  ALOGE("%s: Awinic open lib success is_module_ready =%d!",__func__, adev->awinic_skt.is_module_ready);

  adev->awinic_skt.getSize = (AwGetSize_t)dlsym(adev->awinic_skt.awinic_lib,"AwinicGetSize");
  adev->awinic_skt.init    = (AwInit_t)dlsym(adev->awinic_skt.awinic_lib,"AwinicInit");
  adev->awinic_skt.end     = (AwEnd_t)dlsym(adev->awinic_skt.awinic_lib,"AwinicEnd");
  adev->awinic_skt.reset     = (AwEnd_t)dlsym(adev->awinic_skt.awinic_lib,"AwinicReset");
  adev->awinic_skt.process = (AwHandle_t)dlsym(adev->awinic_skt.awinic_lib,"AwinicHandle");
  adev->awinic_skt.setMediaInfo = (AwSetMediaInfo_t)dlsym(adev->awinic_skt.awinic_lib,"AwinicSetMediaInfo");
  
  if(adev->awinic_skt.getSize == NULL || adev->awinic_skt.init == NULL|| adev->awinic_skt.setMediaInfo==NULL
      || adev->awinic_skt.end == NULL || adev->awinic_skt.process == NULL || adev->awinic_skt.reset == NULL)
  {
     ALOGE("%s: Awinic get skt function Failed !",__func__);
    adev->awinic_skt.is_module_ready = false;
    free(adev->awinic_skt.audio_data_buffer);
	 adev->awinic_skt.audio_data_buffer = NULL;
	 dlclose(adev->awinic_skt.awinic_lib);
	 adev->awinic_skt.awinic_lib = NULL;
    return 0;
  }
  
  ALOGE("%s: Awinic open lib function success is_module_ready =%d!",__func__, adev->awinic_skt.is_module_ready);
  unsigned long awinic_mem_size = 0;
  awinic_mem_size = adev->awinic_skt.getSize();
  if(awinic_mem_size == 0)
  {
    ALOGE("%s: Awinic get skt memory Failed !",__func__);
    adev->awinic_skt.is_module_ready = false;
    free(adev->awinic_skt.audio_data_buffer);
    adev->awinic_skt.audio_data_buffer=NULL;
	 dlclose(adev->awinic_skt.awinic_lib);
	 adev->awinic_skt.awinic_lib = NULL;
    return 0;

  }
  LOG_I("%s: Awinic getsiz success is_module_ready =%d!",__func__, adev->awinic_skt.is_module_ready);
  adev->awinic_skt.module_context_buffer = (char*)calloc(awinic_mem_size,sizeof(char));
  if(adev->awinic_skt.module_context_buffer == NULL)
  {
    ALOGE("%s: Awinic alloc skt  memory Failed !",__func__);
    adev->awinic_skt.is_module_ready = false;
    free(adev->awinic_skt.audio_data_buffer);
    adev->awinic_skt.audio_data_buffer=NULL;
	 dlclose(adev->awinic_skt.awinic_lib);
	 adev->awinic_skt.awinic_lib = NULL;
    return 0;
  }

  ALOGE("%s: Awinic config mem success is_module_ready =%d!",__func__, adev->awinic_skt.is_module_ready);
  media_info_t *mediaInfo = &adev->awinic_skt.info;
  mediaInfo->num_channels = 2;
  mediaInfo->bits_per_sample = 16;
  mediaInfo->bit_qactor_sample = 16 -1 ;
  mediaInfo->sampling_rate = 44100;
  ret = adev->awinic_skt.init(adev->awinic_skt.module_context_buffer,AWINIC_PARAMS_PATH);
  if(ret !=0)
  {
     ALOGE("%s: Awinic init Failed !",__func__);
    adev->awinic_skt.is_module_ready = false;
    free(adev->awinic_skt.audio_data_buffer);
    adev->awinic_skt.audio_data_buffer = NULL;
    free(adev->awinic_skt.module_context_buffer);
    adev->awinic_skt.module_context_buffer =NULL;
	 dlclose(adev->awinic_skt.awinic_lib);
	 adev->awinic_skt.awinic_lib = NULL;
    return 0;
  }
 
  adev->awinic_skt.setMediaInfo(adev->awinic_skt.module_context_buffer,mediaInfo);

  ALOGE("%s: Awinic init success is_module_ready =%d!",__func__, adev->awinic_skt.is_module_ready);
#endif

    primary_dev=adev;

    LOG_I("adev_open success primary_dev_count %d", primary_dev_count);
    pthread_mutex_unlock(&adev_init_lock);
    return 0;
error_malloc:
    if (adev)
    { free(adev); }
    primary_dev_count--;
    if(primary_dev_count==0){
        primary_dev=NULL;
    }
    LOG_E("adev_open failed");
    pthread_mutex_unlock(&adev_init_lock);
    return -EINVAL;
}

static struct hw_module_methods_t hal_module_methods = {
    .open = adev_open,
};

struct audio_module HAL_MODULE_INFO_SYM = {
    .common = {
        .tag = HARDWARE_MODULE_TAG,
        .module_api_version = AUDIO_MODULE_API_VERSION_0_1,
        .hal_api_version = HARDWARE_HAL_API_VERSION,
        .id = AUDIO_HARDWARE_MODULE_ID,
        .name = "Spreadtrum Audio HW HAL",
        .author = "The Android Open Source Project",
        .methods = &hal_module_methods,
    },
};
