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
#define LOG_TAG "audio_hw_monitor"

#include "audio_hw.h"
#include "audio_control.h"
#include <system/audio.h>
#include <hardware/audio.h>
#include "audio_debug.h"
#include <cutils/sockets.h>

#define DEFAULT_REGDUMP_DIFF_MS    100

extern int audio_agdsp_reset(void * dev,struct str_parms *parms,bool is_start);
static char *mystrstr(char *s1 , char *s2)
{
  if(*s1==0)
  {
    if(*s2) return(char*)NULL;
    return (char*)s1;
  }
  while(*s1)
  {
    int i=0;
    while(1)
   {
      if(s2[i]==0) return s1;
      if(s2[i]!=s1[i]) break;
      i++;
    }
    s1++;
  }
  return (char*)NULL;
}

static void *modem_monitor_routine(void *arg)
{
    int fd = -1;
    int numRead = 0;
    char buf[128] = {0};
    int try_count=10;
    int maxfd=-1;
    fd_set fds_read;
    int result=-1;
    int err_count=0;

    struct modem_monitor_handler *modem_monitor=(struct modem_monitor_handler *)arg;

    struct tiny_audio_device *adev = (struct tiny_audio_device *)modem_monitor->dev ;
    LOG_I("modem_monitor_routine in");
    if( !adev) {
        LOG_E("modem_monitor_routine:error adev is null");
        goto Exit;
    }

reconnect:
    try_count=10;
    do {
        try_count--;
        fd = socket_local_client("modemd",
            ANDROID_SOCKET_NAMESPACE_ABSTRACT, SOCK_STREAM);
        if(fd < 0 ) {
            LOG_E("modem_monitor_routine:socket_local_client failed %d", errno);
            usleep(2000 * 1000);
        }
    } while((fd < 0) && (try_count > 0));

    if(fd<0){
        LOG_E("modem_monitor_routine connect modemd failed");
        goto Exit;
    }

    if((fd >= MAX_SELECT_FD)||(modem_monitor->pipe_fd[0]>= MAX_SELECT_FD)){
        LOG_E("ORTIFY: FD_SET: file descriptor %d %d >= FD_SETSIZE %d",
            fd,modem_monitor->pipe_fd[0],MAX_SELECT_FD);
        goto Exit;
    }
    pthread_mutex_lock(&modem_monitor->lock);
    modem_monitor->is_running=true;
    pthread_mutex_unlock(&modem_monitor->lock);
    while(1) {
        memset (buf, 0 , sizeof(buf));
        FD_ZERO(&fds_read);
        FD_SET(modem_monitor->pipe_fd[0],&fds_read);
        FD_SET(fd,&fds_read);
        maxfd = (fd > modem_monitor->pipe_fd[0]) ? fd + 1 : modem_monitor->pipe_fd[0] + 1;
        LOG_I("%s line:%d maxfd:%d wait",__func__,__LINE__,maxfd);
        result = select(maxfd,&fds_read,NULL,NULL,NULL);
        LOG_I("%s line:%d maxfd:%d wakeup",__func__,__LINE__,maxfd);
        if(result < 0){
            LOG_E("select error ");
            usleep(10*1000);
            continue;
        }
        if(FD_ISSET(modem_monitor->pipe_fd[0],&fds_read))
        {
            char buff[16];
            read(modem_monitor->pipe_fd[0], &buff, sizeof(buff));
            goto Exit;
        }
        numRead = read(fd, buf, sizeof(buf));

        if(numRead <= 0) {
            LOG_E("modem_monitor: error: got to reconnect err_count:s%d",err_count);
            err_count++;
            if(err_count>=10){
                goto Exit;
            }
            close(fd);
            fd=-1;
            usleep(40*1000);
            goto reconnect;
        }
        err_count=0;
        LOG_I("modem_monitor: %s", (char *)buf);
        if(mystrstr(buf, "Assert") || mystrstr(buf, "Reset")
           || mystrstr(buf, "Blocked")) {
            LOG_E("modem_monitor assert:stop voice call start");
            adev->hw_device.set_mode(&adev->hw_device,AUDIO_MODE_NORMAL);
            //audio_agdsp_reset(adev,NULL,true);
            LOG_E("modem_monitor assert:stop voice call end");
        }
    }

Exit:
    if(fd>=0){
        close(fd);
    }

    if(modem_monitor->pipe_fd[0]>0){
        close(modem_monitor->pipe_fd[0]);
        modem_monitor->pipe_fd[0]=-1;
    }
    if(modem_monitor->pipe_fd[1]>0){
        close(modem_monitor->pipe_fd[1]);
        modem_monitor->pipe_fd[1]=-1;
    }

    pthread_mutex_lock(&modem_monitor->lock);
    modem_monitor->is_running=false;
    pthread_mutex_unlock(&modem_monitor->lock);
    return NULL;
}

int modem_monitor_open(void *dev,struct modem_monitor_handler *modem_monitor)
{
    int rc;
    pthread_attr_t attr;
    modem_monitor->dev=dev;

    if(pipe(modem_monitor->pipe_fd) < 0) {
        LOG_E("modem_monitor_open create pipe failed");
        return -1;
    }

    pthread_mutex_init(&modem_monitor->lock, NULL);

    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
    modem_monitor->is_running=false;
    if((modem_monitor->pipe_fd[0]<0)||(modem_monitor->pipe_fd[1]<0)){
        LOG_E("modem_monitor_open pipe fd error");
        return -2;
    }
    if(modem_monitor->pipe_fd[0]>= MAX_SELECT_FD){
        LOG_E("modem_monitor_open: FD_SET: file descriptor %d >= FD_SETSIZE %d",
            modem_monitor->pipe_fd[0],MAX_SELECT_FD);
        close(modem_monitor->pipe_fd[0]);
        modem_monitor->pipe_fd[0]=-1;

        close(modem_monitor->pipe_fd[1]);
        modem_monitor->pipe_fd[1]=-1;
        pthread_attr_destroy(&attr);
        pthread_mutex_destroy(&modem_monitor->lock);
        return -3;
    }

    rc = pthread_create((pthread_t *) & (modem_monitor->thread_id), &attr,
                        modem_monitor_routine, (void *)modem_monitor);
    if (rc) {
        LOG_E("modem_monitor_open,pthread_create failed, rc=0x%x", rc);
    }
    pthread_attr_destroy(&attr);
    LOG_I("modem_monitor_open,pthread_create ok, rc=0x%x", rc);
    return rc;
}

void modem_monitor_close(struct modem_monitor_handler *modem_monitor)
{
    bool is_running=true;
    pthread_mutex_lock(&modem_monitor->lock);
    is_running=modem_monitor->is_running;
    pthread_mutex_unlock(&modem_monitor->lock);

    LOG_I("modem_monitor_close is_running:%d %d %d"
        ,is_running,modem_monitor->pipe_fd[0],modem_monitor->pipe_fd[1]);
    if(is_running==true){
        if(modem_monitor->pipe_fd[1]>0){
            write(modem_monitor->pipe_fd[1], "exit", strlen("exit"));
        }
        pthread_join(modem_monitor->thread_id, NULL);
        modem_monitor->thread_id=-1;
        pthread_mutex_destroy(&modem_monitor->lock);
    }
    LOG_I("modem_monitor_close success");
}


static int dump_audio_reg_to_buffer(void *buffer,int max_size,const char *reg_file_name){
    FILE *reg_file=NULL;
    int ret=0;

    if((NULL==buffer)||(NULL==reg_file_name)){
        return -1;
    }
    reg_file=fopen(reg_file_name,"rb");
    if(NULL!=reg_file){
        ret=fread(buffer, 1, max_size, reg_file);
        ALOGI("read reg size:%d %d",max_size,ret);
        fclose(reg_file);
        reg_file=NULL;
    }else{
       ret=-1;
    }
    if (reg_file) {
        fclose(reg_file);
        reg_file = NULL;
    }
    return ret;
}

static  void dump_audio_reg(int fd,const char *reg_file_name){
    int max_size=2048;
    void *buffer = (void*)malloc(max_size);
    int ret=0;

    LOG_I("\ndump_audio_reg:%s",reg_file_name);
    if(NULL!=buffer){
        memset(buffer,0,max_size);
        ret=dump_audio_reg_to_buffer(buffer,max_size-1,reg_file_name);
        if(ret>0){
            AUDIO_DUMP_WRITE_BUFFER(fd,buffer,ret);
        }else{
            LOG_I("dump_audio_reg open:%s failed",reg_file_name);
            snprintf(buffer,max_size-1,
                "\ndump_audio_reg open:%s failed\n",reg_file_name);
            AUDIO_DUMP_WRITE_STR(fd,buffer);
        }
        free(buffer);
        buffer=NULL;
    }
}

void dump_all_audio_reg(int fd,int dump_flag){
    /* dump codec reg */
    if(dump_flag&(1<<ADEV_DUMP_CODEC_REG)){
        dump_audio_reg(fd,SPRD_CODEC_REG_FILE);
    }

    /* dump vbc reg */
    if(dump_flag&(1<<ADEV_DUMP_VBC_REG)){
        dump_audio_reg(fd,VBC_REG_FILE);
    }

    /* dump dma reg */
    if(dump_flag&(1<<ADEV_DUMP_DMA_REG)){
        dump_audio_reg(fd,AUDIO_DMA_REG_FILE);
    }
}

#ifdef AUDIO_DEBUG
static void *debugdump_routine(void *arg)
{
    struct debug_dump_handler *debugdump=(struct debug_dump_handler *)arg;

    pthread_mutex_lock(&debugdump->cond_lock);
    LOG_I("%s Enter",__func__);
    while(!debugdump->is_close){
        int ret=-1;

        if (debugdump->dumpcount <= 0) {
            LOG_I("%s wait cur_time:%d",__func__,getCurrentTimeMs());
            ret=pthread_cond_wait(&debugdump->cond, &debugdump->cond_lock);
            LOG_I("%s wakeup ret:%d cur_time:%d count:%d",__func__,ret,getCurrentTimeMs(),debugdump->dumpcount);
        } else {
            struct timespec ts= {0, 0};

            clock_gettime(CLOCK_MONOTONIC, &ts);
            ts.tv_nsec += (debugdump->timems%1000) * 1000000;
            ts.tv_sec += debugdump->timems/1000;
            if(ts.tv_nsec >=1000000000) {
                ts.tv_nsec -= 1000000000;
                ts.tv_sec += 1;
            }
            LOG_I("%s wait tv_nsec:%ld tv_sec:%ld",__func__,ts.tv_nsec,ts.tv_sec);
            ret=pthread_cond_timedwait(&debugdump->cond, &debugdump->cond_lock, &ts);
            LOG_I("%s wakeup ret:%d cur_time:%d dumpcount:%d",__func__,
                ret,getCurrentTimeMs(),debugdump->dumpcount);
        }

        if ((!ret || ret == ETIMEDOUT) &&
            (debugdump->dumpcount > 0)) {
            dump_all_audio_reg(-1,1<<ADEV_DUMP_CODEC_REG);
            debugdump->dumpcount--;
        }
    }
    pthread_mutex_unlock(&debugdump->cond_lock);
    LOG_I("%s Exit",__func__);
    return NULL;
}

int debug_dump_open(struct debug_dump_handler *debugdump)
{
    int rc=0;

    pthread_condattr_t attr1;
    pthread_attr_t attr2;
    pthread_condattr_init(&attr1);
    pthread_condattr_setclock(&attr1, CLOCK_MONOTONIC);
    pthread_mutex_init(&debugdump->lock, NULL);
    pthread_mutex_init(&debugdump->cond_lock, NULL);
    pthread_cond_init(&debugdump->cond, (const pthread_condattr_t *)&attr1);
    pthread_condattr_destroy(&attr1);
    pthread_attr_init(&attr2);
    pthread_attr_setdetachstate(&attr2, PTHREAD_CREATE_JOINABLE);
    debugdump->is_close=false;
    debugdump->dumpcount=0;
    debugdump->timems=DEFAULT_REGDUMP_DIFF_MS;

    rc = pthread_create((pthread_t *) & (debugdump->thread_id), &attr2,
                        debugdump_routine, (void *)debugdump);
    if (rc) {
        LOG_E("audioreg_dump_monitor_open,pthread_create failed, rc=0x%x", rc);
        pthread_attr_destroy(&attr2);
        return -1;
    }
    pthread_attr_destroy(&attr2);
    LOG_I("audioreg_dump_monitor_open,pthread_create ok, rc=0x%x", rc);
    return 0;
}

void debug_dump_close(struct debug_dump_handler *debugdump)
{
    pthread_mutex_lock(&debugdump->lock);
    pthread_mutex_lock(&debugdump->cond_lock);
    debugdump->is_close=true;
    debugdump->dumpcount=0;
    pthread_cond_signal(&debugdump->cond);
    pthread_mutex_unlock(&debugdump->cond_lock);
    pthread_mutex_unlock(&debugdump->lock);
    pthread_join(debugdump->thread_id, NULL);
    pthread_cond_destroy(&debugdump->cond);
    pthread_mutex_destroy(&debugdump->lock);
    pthread_mutex_destroy(&debugdump->cond_lock);
    debugdump->thread_id=-1;
    LOG_I("debug_dump_close success");
}

void debug_dump_start(struct debug_dump_handler *debugdump,int count)
{
    pthread_mutex_lock(&debugdump->lock);
    if(true==debugdump->is_close){
        pthread_mutex_unlock(&debugdump->lock);
        return;
    }
    debugdump->is_start = true;
    pthread_mutex_lock(&debugdump->cond_lock);
    if(debugdump->dumpcount <= count) {
        debugdump->dumpcount=count;
    }
    pthread_cond_signal(&debugdump->cond);
    pthread_mutex_unlock(&debugdump->cond_lock);
    LOG_I("debug_dump_start dumpcount:%d %d",debugdump->dumpcount,count);
    pthread_mutex_unlock(&debugdump->lock);
}

void debug_dump_stop(struct debug_dump_handler *debugdump)
{
    LOG_D("debug_dump_stop dumpcount:%d",debugdump->dumpcount);
    pthread_mutex_lock(&debugdump->lock);
    if(debugdump->is_start){
        pthread_mutex_lock(&debugdump->cond_lock);
        debugdump->is_start = false;
        debugdump->dumpcount=0;
        pthread_cond_signal(&debugdump->cond);
        pthread_mutex_unlock(&debugdump->cond_lock);
    }
    pthread_mutex_unlock(&debugdump->lock);
}
#endif
