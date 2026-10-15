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
#define LOG_TAG "audio_hw_dsp_loop"

#include <sys/wait.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <sys/select.h>
#include <fcntl.h>
#include <signal.h>
#include <semaphore.h>
#include "audio_debug.h"
#include "audio_hw.h"
#include "audio_control.h"

#define TEST_AUDIO_LOOP_MIN_DATA_MS 500
#define LOOPBACK_ENCODER_PER_BUFFER_SIZE    160*4    //MCDT_LOOP_C_FRAGMENT

#define AUDIO_STRUCTURE_SAMPLERATE_OFFSET    0
extern unsigned int producer_proc(struct ring_buffer *ring_buf,unsigned char * buf,unsigned int size);
extern int calculation_ring_buffer_size(int ms,struct pcm_config *config);
extern int apply_dsploop_control(struct private_dsploop_control *dsploop_ctl, int type,int rate,int mode);

static void *get_dsploop_ap_param(AUDIO_PARAM_T *audio_param,audio_devices_t in_devices,audio_devices_t out_devices){
    int id=-1;
    struct audio_param_res  res;
    struct audio_ap_param *ap_param=NULL;
    int loopid=0; 

    res.net_mode=AUDIO_NET_LOOP;
    res.usecase=UC_LOOP;
    res.in_devices=in_devices;
    res.out_devices=out_devices;

    id=get_loopback_param(&audio_param->dev_ctl->config.audiotester_config,out_devices,in_devices);
    if((id<0)||(id>=PROFILE_MODE_MAX)){
        LOG_W("get_dsploop_ap_param Failed in_devices:%x out_devices:%x",in_devices,out_devices);
        return NULL;
    }

    ap_param=(struct audio_ap_param *)(audio_param->param[SND_AUDIO_PARAM_RECORD_PROCESS_PROFILE].data);

    loopid=(int)(((AUDIO_PARAM_PROFILE_MODE_E)id-PROFILE_LOOP_MODE_START));
    if((loopid>=0)&&(loopid<LOOP_AP_PARAM_COUNT)){
        return  &(ap_param->loop[loopid]);
    }else{
        ALOGE("%s line:%d invalid loop param:%d id:%d",__func__,__LINE__,loopid,id);
        return NULL;
    }
}

static int get_dsploop_samplerate(AUDIO_PARAM_T *audio_param,audio_devices_t in_devices,audio_devices_t out_devices){
    int id=-1;
    uint8_t param_id_tmp=PROFILE_MODE_MAX;
    int offset=AUDIO_PARAM_INVALID_8BIT_OFFSET;

    int samplerate=8000;
    short *samplerate_ptr;
    id=get_loopback_param(&audio_param->dev_ctl->config.audiotester_config,out_devices,in_devices);
    if((id<0)||(id>=PROFILE_MODE_MAX)){
        LOG_W("get_dsploop_samplerate Failed in_devices:%x out_devices:%x",in_devices,out_devices);
        return samplerate;
    }

    param_id_tmp=check_shareparam(audio_param->dev_ctl,SND_AUDIO_PARAM_AUDIO_STRUCTURE_PROFILE,id);

    offset=(uint8_t)get_audio_param_mode(audio_param,SND_AUDIO_PARAM_AUDIO_STRUCTURE_PROFILE,param_id_tmp);
    if(AUDIO_PARAM_INVALID_8BIT_OFFSET!=offset){
        struct vbc_fw_header header;
        int bytesRead=0;
        FILE *file;
        char *buffer=NULL;

        file = fopen("/vendor/firmware/audio_structure", "r");
        if(NULL==file){
            LOG_E("open /vendor/firmware/audio_structure failed err:%s",
                strerror(errno));
            goto out;
        }
        fseek(file,0,SEEK_SET);

        bytesRead = fread(&header, 1, sizeof(header), file);
        if(bytesRead<=0){
            LOG_W("read vbc_fw_header failed bytesRead:%d err:%s"
                ,bytesRead,strerror(errno));
            fclose(file);
            goto out;
        }

        buffer=(char *)malloc(header.len*header.num_mode+sizeof(header));
        if(NULL==buffer){
            LOG_W("malloc buffer failed");
            fclose(file);
            goto out;
        }

        memcpy(buffer,&header,sizeof(header));

        bytesRead = fread(buffer+sizeof(header),1,header.len*header.num_mode, file);
        if(bytesRead!=header.len*header.num_mode){
            LOG_W("read config file failed bytesRead:%d",bytesRead);
            free(buffer);
            fclose(file);
            goto out;
        }

        fclose(file);

        samplerate_ptr= (short *)(buffer+sizeof(header)+offset*header.len);
        if(0x08==(*samplerate_ptr)){
            samplerate=8000;
        }else if(0x10==(*samplerate_ptr)){
            samplerate=16000;
        }else if(0x20==(*samplerate_ptr)){
            samplerate=32000;
        }else if(0x30==(*samplerate_ptr)){
            samplerate=48000;
        }
        LOG_I("%s samplerate:%x %d",__func__,(*samplerate_ptr),samplerate);
        free(buffer);
    }

out:
    return samplerate;
}

static void *dsp_loop_rx_thread(void *args){
    struct loop_ctl_t *in=(struct loop_ctl_t *)args;
    struct pcm *pcm=in->pcm;
    struct tiny_audio_device *adev=(struct tiny_audio_device *)in->dev;
    char *buffer=NULL;
    int num_read = 0;
    int size=0;
    int bytes_read=0;

    pthread_mutex_lock(&(in->lock));
    if(NULL==in->pcm){
        LOG_E("dsp_loop_rx_thread pcm is null");
        goto ERR;
    }

    size = pcm_frames_to_bytes(pcm, adev->dev_ctl->pcm_handle.record[AUD_RECORD_PCM_DSP_LOOP].period_size);

    buffer = (char *)malloc(size);
    if (!buffer) {
        LOG_E("Unable to allocate %d bytes\n", size);
        goto ERR;
    }
    LOG_I("dsp_loop_rx_thread %d\n", size);
    sem_wait(&in->sem);
    while(in->is_exit==false){
        num_read=pcm_mmap_read(pcm, buffer, size);
        LOG_D("dsp_loop_rx_thread read:%d req:%d",num_read,size);
        if (!num_read){
            bytes_read += size;
            audiohal_pcmdump(adev->dev_ctl,0xff,buffer,size,PCMDUMP_LOOP_PLAYBACK_RECORD);
            producer_proc(in->ring_buf, (unsigned char *)buffer, (unsigned int)size);
            LOG_V("dsp_loop_rx_thread capture 0x%x total:0x%x",size,bytes_read);
        }else{
            LOG_E("dsp_loop_rx_thread Error reading:%s\n",pcm_get_error(pcm));
            pcm_close(pcm);
            in->pcm = pcm_open(adev->dev_ctl->cards.s_tinycard, adev->dev_ctl->pcm_handle.record_devices[AUD_RECORD_PCM_DSP_LOOP],
                PCM_IN |PCM_MMAP | PCM_NOIRQ|PCM_MONOTONIC,  &adev->dev_ctl->pcm_handle.record[AUD_RECORD_PCM_DSP_LOOP]);

            if (!pcm || !pcm_is_ready(in->pcm)) {
                LOG_E("dsp_loop_rx_thread Unable to open PCM device %u (%s)\n",
                      0, pcm_get_error(in->pcm));
                goto ERR;
            }
            pcm=in->pcm;
        }
    }
    in->state=false;

    if(buffer){
        free(buffer);
        buffer=NULL;
    }

    pthread_mutex_unlock(&(in->lock));
    LOG_I("dsp_loop_rx_thread exit success");
    return NULL;

ERR:
    if(buffer){
        free(buffer);
        buffer=NULL;
    }

    if(in->pcm){
        pcm_close(in->pcm);
        in->pcm=NULL;
    }
    in->state=false;
    pthread_mutex_unlock(&(in->lock));

    LOG_E("dsp_loop_rx_thread exit err");
    return NULL;
}

static void *dsp_loop_tx_thread(void *args){
    struct loop_ctl_t *out=(struct loop_ctl_t *)args;
    struct pcm *pcm=out->pcm;
    struct tiny_audio_device *adev=(struct tiny_audio_device *)out->dev;
    char *buffer=NULL;
    int num_read = 0;
    int size=0;
    int ret=-1;
    int write_count=0;
    int no_data_count=0;

    pthread_mutex_lock(&(out->lock));
    out->state=true;
    if(NULL==out->pcm){
        LOG_E("dsp_loop_tx_thread pcm is null");
        goto ERR;
    }

    size = pcm_frames_to_bytes(pcm, adev->dev_ctl->pcm_handle.play[AUD_PCM_DSP_LOOP].period_size);

    buffer = (char *)malloc(size);
    if (!buffer) {
        LOG_E("dsp_loop_tx_thread Unable to allocate %d bytes\n", size);
        goto ERR;
    }

    sem_wait(&out->sem);
    LOG_I("dsp_loop_tx_thread size:%d",size);
    while(out->is_exit==false){
        num_read=ring_buffer_get(out->ring_buf, (void *)buffer, size);
        LOG_D("dsp_loop_tx_thread read:0x%x req:0x%x",num_read,size);

        if(num_read > 0){
            no_data_count=0;
            ret = pcm_mmap_write(pcm, buffer,num_read);
            if (ret) {
                int ret = 0;
                LOG_E("dsp_loop_tx_thread Error playing sample:%s\n",pcm_get_error(pcm));
                ret = pcm_close(pcm);
                out->pcm = pcm_open(adev->dev_ctl->cards.s_tinycard, adev->dev_ctl->pcm_handle.playback_devices[AUD_PCM_DSP_LOOP],
                        PCM_OUT | PCM_MMAP | PCM_NOIRQ |PCM_MONOTONIC , &adev->dev_ctl->pcm_handle.play[AUD_PCM_DSP_LOOP]);
                if (!pcm || !pcm_is_ready(out->pcm)) {
                    LOG_E("dsp_loop_tx_thread Unable to open PCM device %u (%s)\n",
                          0, pcm_get_error(out->pcm));
                    goto ERR;
                }
                pcm=out->pcm;
            }else{
                write_count+=num_read;
                LOG_V("dsp_loop_tx_thread write:0x%x total:0x%x",num_read,write_count);
                audiohal_pcmdump(adev->dev_ctl,0xff,buffer,size,PCMDUMP_LOOP_PLAYBACK_DSP);
            }
        }else{
            usleep(10*1000);
            no_data_count++;
            LOG_I("dsp_loop_tx_thread no data read");
        }
    }

    if(buffer){
        free(buffer);
        buffer=NULL;
    }

    out->state=false;
    pthread_mutex_unlock(&(out->lock));
    LOG_I("dsp_loop_tx_thread exit success");
    return NULL;

ERR:
    if(buffer){
        free(buffer);
        buffer=NULL;
    }

    if(out->pcm){
        pcm_close(out->pcm);
        out->pcm=NULL;
    }
    out->state=false;
    pthread_mutex_unlock(&(out->lock));
    LOG_E("dsp_loop_tx_thread exit err");
    return NULL;
}

static int audio_dsp_loop_standby(void *dev,void * loop_stream,UNUSED_ATTR AUDIO_HW_APP_T type)
{
    struct tiny_audio_device *adev=(struct tiny_audio_device *)dev;
    struct tiny_stream_out *dsp_loop=(struct tiny_stream_out *)loop_stream;
    struct loop_ctl_t *ctl=NULL;
    struct dsp_loop_t *loop=&adev->loop_ctl;

    LOG_I("audio_dsp_loop_stop enter");
    set_mdg_mute(adev->dev_ctl,UC_CALL,true);

    if(false==loop->state){
        LOG_W("audio_dsp_loop_stop failed, loop is not work");
        return -1;
    }

    loop->in.is_exit=true;
    loop->out.is_exit=true;

    /* first:close record pcm devices */

    if (!adev->loop_from_dsp)
        pthread_join(loop->in.thread,  NULL);

    LOG_I("audio_dsp_loop_stop %d",__LINE__);
    loop->out.is_exit=true;
    if (!adev->loop_from_dsp)
        pthread_join(loop->out.thread, NULL);

    LOG_I("audio_dsp_loop_stop %d",__LINE__);
    ctl=&loop->in;
    pthread_mutex_lock(&(ctl->lock));
    ctl->is_exit=true;
    ctl->dev=NULL;
    if(ctl->pcm!=NULL){
        pcm_close(ctl->pcm);
        ctl->pcm=NULL;
        LOG_I("audio_dsp_loop_stop %d",__LINE__);
    }
    pthread_mutex_unlock(&(ctl->lock));
    ctl=&loop->out;
    pthread_mutex_lock(&(ctl->lock));
    ctl->is_exit=true;
    ctl->dev=NULL;
    if(ctl->pcm!=NULL){
        pcm_close(ctl->pcm);
        ctl->pcm=NULL;
        LOG_I("audio_dsp_loop_stop %d",__LINE__);
    }
    pthread_mutex_unlock(&(ctl->lock));

    if(loop->out.ring_buf!=NULL){
        ring_buffer_free(loop->out.ring_buf);
    }

    switch_vbc_iis_route(adev->dev_ctl,UC_LOOP,false);
    loop->state=false;

    dsp_loop->standby=true;

    pthread_mutex_destroy(&(loop->out.lock));
    pthread_mutex_destroy(&(loop->in.lock));

    set_usecase(adev->dev_ctl,UC_LOOP,false);
    set_audioparam(adev->dev_ctl,PARAM_USECASE_CHANGE,NULL,false);
    adev->is_dsp_loop = false;
    LOG_I("audio_dsp_loop_standby exit");

    return 0;
}

int audio_dsp_loop_open(struct tiny_audio_device *adev){
    struct tiny_stream_out *dsp_loop=NULL;
    dsp_loop=(struct tiny_stream_out *)calloc(1,
                                sizeof(struct tiny_stream_out));
    if(NULL==dsp_loop){
        LOG_E("audio_dsp_loop_open calloc falied");
        goto error;
    }

    LOG_I("audio_dsp_loop_open out:%p",dsp_loop);

    if (pthread_mutex_init(&(dsp_loop->lock), NULL) != 0) {
        LOG_E("Failed pthread_mutex_init dsp_loop->lock,errno:%u,%s",
              errno, strerror(errno));
        goto error;
    }

    dsp_loop->dev=adev;
    dsp_loop->standby=false;
    dsp_loop->audio_app_type=AUDIO_HW_APP_DSP_LOOP;
    dsp_loop->standby_fun = audio_dsp_loop_standby;
    audio_add_output(adev,dsp_loop);

    LOG_I("audio_dsp_loop_open success");
    return 0;

error:

    if(NULL!=dsp_loop){
        free(dsp_loop);
    }

    return -1;
}

static int audio_dsp_loop_start(struct tiny_audio_device *adev,struct dsp_loop_t *loop){
    struct ring_buffer *ring_buf=NULL;
    struct pcm_config in_config;
    struct pcm_config out_config;
    int loop_samplerate=8000;
    struct tiny_stream_out *dsp_loop=NULL;
    int size=0;
    int min_buffer_size=0;
    int delay_size=0;
    int ret=0;
    struct audio_param_res param_res;
    struct dsp_loop_param *loop_ap_param=NULL;

    audio_devices_t out_devices=0;
    audio_devices_t in_devices=0;

    if(true==loop->state){
        LOG_W("audio_dsp_loop_start failed, loop is working now");
        return -1;
    }

    dsp_loop= get_output_stream(adev,AUDIO_HW_APP_DSP_LOOP);
    if(NULL==dsp_loop){
        LOG_E("audio_dsp_loop_start failed");
        return -1;
    }

    adev->is_dsp_loop = true;

   // switch_vbc_iis_route(adev->dev_ctl,UC_LOOP,true);
    ret=set_usecase(adev->dev_ctl,UC_LOOP,true);
    if(ret<0){
        goto error;
    }
    force_all_standby(adev);

    force_in_standby(adev,AUDIO_HW_APP_NORMAL_RECORD);
    force_in_standby(adev,AUDIO_HW_APP_VOIP_RECORD);
    force_in_standby(adev,AUDIO_HW_APP_BT_RECORD);

    pthread_mutex_lock(&adev->dev_ctl->lock);
    out_devices=adev->dev_ctl->debug_out_devices;
    in_devices=adev->dev_ctl->debug_in_devices;
    pthread_mutex_unlock(&adev->dev_ctl->lock);

    switch_vbc_route(adev->dev_ctl,in_devices);
    select_devices_new(adev->dev_ctl,AUDIO_HW_APP_DSP_LOOP,in_devices,true,false,true,true);
    switch_vbc_route(adev->dev_ctl,out_devices);
    select_devices_new(adev->dev_ctl,AUDIO_HW_APP_DSP_LOOP,out_devices,false,false,true,true);

    dsp_loop->standby=false;

    memcpy(&in_config,&adev->dev_ctl->pcm_handle.record[AUD_RECORD_PCM_DSP_LOOP],sizeof(struct pcm_config));
    memcpy(&out_config,&adev->dev_ctl->pcm_handle.play[AUD_PCM_DSP_LOOP],sizeof(struct pcm_config));

    loop_ap_param=(struct  dsp_loop_param *)get_dsploop_ap_param(adev->dev_ctl->audio_param,
        in_devices, out_devices);
    if(NULL==loop_ap_param){
        LOG_E("audio_dsp_loop_start get_dsploop_ap_param failed: in_devices:0x%x out_devices:%d",
            in_devices,out_devices);
        goto error;
    }else{
        LOG_I("audio_dsp_loop_start delay:%dms type:%d",loop_ap_param->delay,loop_ap_param->type);
    }

    if(loop_ap_param->delay>=10000){
        LOG_W("audio_dsp_loop_start invalid delay:%d",loop_ap_param->delay);
        loop_ap_param->delay=50;
    }

    if(loop_ap_param->mode == 1)
        adev->loop_from_dsp = true;
    else
        adev->loop_from_dsp = false;

    loop_samplerate=get_dsploop_samplerate(adev->dev_ctl->audio_param,
        in_devices, out_devices);
    in_config.rate=loop_samplerate;
    out_config.rate=loop_samplerate;
    size=calculation_ring_buffer_size(loop_ap_param->delay*16,&in_config);
    min_buffer_size=(TEST_AUDIO_LOOP_MIN_DATA_MS *in_config.rate*in_config.channels*2/1000);
    if(size<min_buffer_size){
        size=calculation_ring_buffer_size(TEST_AUDIO_LOOP_MIN_DATA_MS,&in_config);
    }

    /* dsp process frame buffer size is 122 word(244bytes),
       the delay buffer size must be a multiple of dsp frame buffer */
    delay_size=loop_ap_param->delay*in_config.rate*in_config.channels*2/1000;
    if(loop_ap_param->type==2)//encoder loopback
    {
        delay_size=(delay_size/LOOPBACK_ENCODER_PER_BUFFER_SIZE+1)*LOOPBACK_ENCODER_PER_BUFFER_SIZE;
    }

    if(size<delay_size){
        size=delay_size*2;
    }

    ring_buf=ring_buffer_init(size,delay_size);
    if(NULL==ring_buf){
        goto error;
    }

    LOG_I("ring_buffer_init total buffer size:%d start delay size:%d",size,delay_size);

    if (pthread_mutex_init(&(loop->in.lock), NULL) != 0) {
        LOG_E("Failed pthread_mutex_init loop->in.lock,errno:%u,%s",
              errno, strerror(errno));
        goto error;
    }

    if (pthread_mutex_init(&(loop->out.lock), NULL) != 0) {
        LOG_E("Failed pthread_mutex_init loop->out.lock,errno:%u,%s",
              errno, strerror(errno));
        goto error;
    }

    loop->in.ring_buf=ring_buf;
    loop->out.ring_buf=ring_buf;

    loop->in.state=true;
    loop->out.state=true;

    loop->in.dev=adev;
    loop->out.dev=adev;

    loop->in.is_exit=false;
    loop->out.is_exit=false;

    sem_init(&loop->in.sem, 0, 1);
    sem_init(&loop->out.sem, 0, 1);

    LOG_I("audio_dsp_loop_start out devices:%d rate:%d card:%d",
        adev->dev_ctl->pcm_handle.playback_devices[AUD_PCM_DSP_LOOP],
        out_config.rate,
        adev->dev_ctl->cards.s_tinycard);

    /*
        dsp loop test needs to open the record pcm devices first and
        then open the playback pcm devices.
    */

    if (adev->loop_from_dsp) {
        out_config.stop_threshold= -1;
        in_config.stop_threshold= -1;
    }
    loop->out.pcm  =
        pcm_open(adev->dev_ctl->cards.s_tinycard, adev->dev_ctl->pcm_handle.playback_devices[AUD_PCM_DSP_LOOP],
        PCM_OUT | PCM_MMAP | PCM_NOIRQ |PCM_MONOTONIC , &out_config);

    if (!pcm_is_ready(loop->out.pcm)) {
        LOG_E("audio_dsp_loop_start playback failed:cannot open pcm : %s",
              pcm_get_error(loop->out.pcm));
        goto error;
    }

    if (adev->loop_from_dsp)
        if( 0 != pcm_start(loop->out.pcm)){
            ALOGE("%s:pcm_start loop->out.pcm start unsucessfully: %s", __func__,pcm_get_error(loop->out.pcm));
            goto error;
        }

    LOG_I("audio_dsp_loop_start in devices:%d rate:%d",
        adev->dev_ctl->pcm_handle.record_devices[AUD_RECORD_PCM_DSP_LOOP],
        adev->dev_ctl->pcm_handle.record[AUD_RECORD_PCM_DSP_LOOP].rate);

    loop->in.pcm  =
        pcm_open(adev->dev_ctl->cards.s_tinycard, adev->dev_ctl->pcm_handle.record_devices[AUD_RECORD_PCM_DSP_LOOP],
        PCM_IN |PCM_MMAP | PCM_NOIRQ|PCM_MONOTONIC, &in_config);

    if (!pcm_is_ready(loop->in.pcm)){
        LOG_E("audio_dsp_loop_start record failed:cannot open pcm : %s",
              pcm_get_error(loop->in.pcm));
        goto error;
    }

    if (adev->loop_from_dsp)
        if( 0 != pcm_start(loop->in.pcm)){
            ALOGE("%s:pcm_start loop->in.pcm start unsucessfully: %s", __func__,pcm_get_error(loop->in.pcm));
            goto error;
        }


    memcpy(&param_res,&adev->dev_ctl->param_res,sizeof(struct audio_param_res));
    param_res.net_mode=AUDIO_NET_LOOP;

    set_audioparam(adev->dev_ctl,PARAM_USECASE_DEVICES_CHANGE,NULL,true);

    apply_dsploop_control(&(adev->dev_ctl->route.dsploop_ctl),loop_ap_param->type,loop_samplerate,loop_ap_param->mode);
    set_mdg_mute(adev->dev_ctl,UC_CALL,false);
    loop->state=true;

    if (!adev->loop_from_dsp) {
        if(pthread_create(&loop->in.thread, NULL, dsp_loop_rx_thread, (void *)&(loop->in))){
            LOG_E("audio_out_devices_test creating rx thread failed !!!!");
            goto error;
        }

        if(pthread_create(&loop->out.thread, NULL, dsp_loop_tx_thread, (void *)&(loop->out))) {
            LOG_E("audio_out_devices_test creating tx thread failed !!!!");
            goto error;
        }
    }
    LOG_I("audio_dsp_loop_start sucess");
    return 0;

error:

    if(NULL!=loop->out.pcm){
        pcm_close(loop->out.pcm);
        loop->in.pcm=NULL;
    }

    if(NULL!=loop->in.pcm){
        pcm_close(loop->in.pcm);
        loop->in.pcm=NULL;
    }

    if(NULL!=ring_buf){
        ring_buffer_free(ring_buf);
    }

    dsp_loop->standby=true;

    set_usecase(adev->dev_ctl,UC_LOOP,false);
    set_audioparam(adev->dev_ctl,PARAM_USECASE_CHANGE,NULL,false);
    loop->state=false;
    adev->is_dsp_loop = false;

    LOG_I("audio_dsp_loop_start failed");
    return -1;
}

int audio_dsp_loop(void * dev,UNUSED_ATTR struct str_parms *parms,int is_start, UNUSED_ATTR char * val){
    int ret=0;
    struct tiny_audio_device * adev=(struct tiny_audio_device *)dev;
    struct tiny_stream_out * out = NULL;

    if(is_start){

        if(is_usecase(adev->dev_ctl, UC_LOOP)){
            return 0;
        }

        ret=audio_dsp_loop_start(adev,&adev->loop_ctl);
    }else{

        if(false==is_usecase(adev->dev_ctl, UC_LOOP)){
            return 0;
        }

        out = get_output_stream(adev,AUDIO_HW_APP_DSP_LOOP);
        if(out) {
            LOG_I("%s %d out:%p type:%d",__func__,__LINE__,out,out->audio_app_type);
            do_output_standby(out);
        }
    }

    return ret;
}
