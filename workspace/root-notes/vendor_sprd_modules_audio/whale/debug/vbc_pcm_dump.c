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
#define LOG_TAG "audio_hw_vbcdump"

#include <sys/wait.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <sys/select.h>
#include <fcntl.h>
#include "audio_debug.h"
#include "audio_hw.h"
#include "audio_control.h"

struct vbc_dump_mode_t {
    PCM_DUMP_TYPE type;
    const char *name;
};

static const struct vbc_dump_mode_t  vbc_dump_mode_table[] = {
    { VBC_DISABLE_DMUP,           "disable"         },
    { DUMP_POS_DAC0_E,            "vbc_dac0"        },
    { DUMP_POS_DAC1_E,            "vbc_dac1"        },
    { DUMP_POS_A4,                "vbc_a4"          },
    { DUMP_POS_A3,                "vbc_a3"          },
    { DUMP_POS_A2,                "vbc_a2"          },
    { DUMP_POS_A1,                "vbc_a1"          },
    { DUMP_POS_V2,                "vbc_v2"          },
    { DUMP_POS_V1,                "vbc_v1"          },
};

static void *vbc_pcm_dump_thread(void *args){
    struct audio_control *dev_ctl=(struct audio_control *)args;
    struct vbc_dump_ctl * vbc_dump=&dev_ctl->vbc_dump;
    struct pcm *pcm=NULL;
    char *buffer=NULL;
    int size=0;

    LOG_I("vbc_pcm_dump_thread enter dump_name:%s",vbc_dump->dump_name);
    pthread_mutex_lock(&dev_ctl->lock);
    vbc_dump->is_exit=false;
    pthread_mutex_unlock(&dev_ctl->lock);
    set_usecase(dev_ctl,UC_VBC_PCM_DUMP, true);
    set_vbc_dump_control(dev_ctl,vbc_dump->dump_name,true);
    vbc_dump->pcm_devices=dev_ctl->pcm_handle.record_devices[AUD_RECORD_PCM_DUMP];

    pcm = pcm_open(0, vbc_dump->pcm_devices, PCM_IN | PCM_MMAP | PCM_NOIRQ, &vbc_dump->config);
    if(NULL==pcm){
        LOG_E("Unable to open PCM device:%d\n",vbc_dump->pcm_devices);
        goto out;
    }else if (!pcm_is_ready(pcm)) {
        LOG_E("Unable to open PCM device:%d (%s)\n",vbc_dump->pcm_devices, pcm_get_error(pcm));
        goto out;
    }

    size = pcm_frames_to_bytes(pcm, pcm_get_buffer_size(pcm));
    buffer = malloc(size);
    if (!buffer) {
        LOG_E("Unable to allocate %d bytes\n", size);
        goto out;
    }

    while(vbc_dump->is_exit==false){
        if(!pcm_mmap_read(pcm, buffer, size)){
            if(vbc_dump->dump_fd>0){
                write(vbc_dump->dump_fd,buffer,size);
            }
        }else{
            pcm_close(pcm);
            set_vbc_dump_control(dev_ctl,vbc_dump->dump_name,true);
            pcm = pcm_open(0, vbc_dump->pcm_devices, PCM_IN | PCM_MMAP | PCM_NOIRQ, &vbc_dump->config);
            if(NULL==pcm){
                LOG_E("Unable to open PCM device:%d\n",vbc_dump->pcm_devices);
                goto out;
            }else if (!pcm_is_ready(pcm)) {
                LOG_E("Unable to open PCM device:%d (%s)\n",vbc_dump->pcm_devices, pcm_get_error(pcm));
                goto out;
            }
        }
    }
    LOG_I("vbc_pcm_dump_thread Exit");
out:
    if(buffer)
        free(buffer);

    if(pcm)
        pcm_close(pcm);

    if(vbc_dump->dump_fd){
        close(vbc_dump->dump_fd);
        vbc_dump->dump_fd=-1;
    }

    pthread_mutex_lock(&dev_ctl->lock);
    vbc_dump->is_exit=true;
    pthread_mutex_unlock(&dev_ctl->lock);
    set_vbc_dump_control(dev_ctl,vbc_dump->dump_name,false);
    set_usecase(dev_ctl,UC_VBC_PCM_DUMP, false);
    return NULL;
}

static int init_vbcmixer_playback_dump(struct vbc_dump_ctl *vbc_dump,char *dump_file_name){
    char file_name[256]={0};
    struct   tm     *timenow;
    time_t current_time;
    time(&current_time);
    timenow = localtime(&current_time);
#ifdef AUDIOHAL_V4
    snprintf(file_name,sizeof(file_name),"/data/vendor/local/media/vbc_dump_%s_%04d_%02d_%02d_%02d_%02d_%02d.pcm",
        dump_file_name,timenow->tm_year+1900,timenow->tm_mon+1,timenow->tm_mday,timenow->tm_hour,timenow->tm_min,timenow->tm_sec);
#else
    snprintf(file_name,sizeof(file_name),"/data/local/media/vbc_dump_%s_%04d_%02d_%02d_%02d_%02d_%02d.pcm",
        dump_file_name,timenow->tm_year+1900,timenow->tm_mon+1,timenow->tm_mday,timenow->tm_hour,timenow->tm_min,timenow->tm_sec);
#endif

    vbc_dump->dump_fd=open(file_name, O_WRONLY | O_CREAT |O_TRUNC ,
                S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
    if(vbc_dump->dump_fd<0){
        vbc_dump->is_exit=true;
        LOG_E("init_vbcmixer_playback_dump create:%s failed",file_name);
        return -1;
    }

    memset(&vbc_dump->config,0,sizeof(struct pcm_config));
    vbc_dump->config.channels=2;
    vbc_dump->config.period_count=2;
    vbc_dump->config.period_size=960;
    vbc_dump->config.rate=48000;

    vbc_dump->pcm_devices=0;
    return 0;
}

int disable_vbc_playback_dump(struct audio_control *dev_ctl){
    struct vbc_dump_ctl * vbc_dump=&dev_ctl->vbc_dump;

    if((vbc_dump->is_exit==false)
        && (is_usecase(dev_ctl,UC_VBC_PCM_DUMP))){
        vbc_dump->is_exit=true;
        pthread_join(vbc_dump->thread, NULL);
    }

    if(vbc_dump->dump_fd>0){
        close(vbc_dump->dump_fd);
        vbc_dump->dump_fd=-1;
    }
    vbc_dump->dump_name=NULL;

    return 0;
}

int vbc_playback_dump(void *dev,UNUSED_ATTR struct str_parms *parms,UNUSED_ATTR int opt, char * val){
    struct tiny_audio_device *adev = (struct tiny_audio_device *)dev;
    int ret=-1;
    unsigned int i=0;
    struct vbc_dump_ctl * vbc_dump=&adev->dev_ctl->vbc_dump;
    PCM_DUMP_TYPE type=VBC_INVALID_DMUP;
    char *dump_name=NULL;
    pthread_mutex_lock(&adev->lock);

    if(true==is_usecase_unlock(adev->dev_ctl,UC_MM_RECORD|UC_FM_RECORD|UC_BT_RECORD)){
        LOG_W("vbc_playback_dump can not dump while recording");
    }else{
        for(i=0;i<sizeof(vbc_dump_mode_table)/sizeof(struct vbc_dump_mode_t);i++){
            if(strncmp(val,vbc_dump_mode_table[i].name,strlen(vbc_dump_mode_table[i].name))==0){
                type=vbc_dump_mode_table[i].type;
                dump_name=(char *)vbc_dump_mode_table[i].name;
                break;
            }
        }
    }

    if(VBC_INVALID_DMUP!=type){
        if(VBC_DISABLE_DMUP==type){
            disable_vbc_playback_dump(adev->dev_ctl);
            ret=0;
        }else{
           LOG_I("is_exit:%d dump_name:%s usecase:0x%x",vbc_dump->is_exit,dump_name,adev->dev_ctl->usecase);
            if((vbc_dump->is_exit==true) && (false==is_usecase(adev->dev_ctl,UC_VBC_PCM_DUMP))
                &&(dump_name!=NULL)){
                vbc_dump->dump_name=dump_name;
                ret=init_vbcmixer_playback_dump(vbc_dump,dump_name);
                if((ret==0) && (vbc_dump->dump_name!=NULL)){
                    if(pthread_create(&vbc_dump->thread, NULL, vbc_pcm_dump_thread,adev->dev_ctl)) {
                        LOG_E("vbc_playback_dump creating thread failed !!!!");
                    }
                }
            }
        }
    }else{
        LOG_W("vbc_playback_dump:%s",val);
    }
    pthread_mutex_unlock(&adev->lock);
    return ret;
}
