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
#define LOG_TAG "a2dp_offload_hw"

#include <log/log.h>
#include <errno.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <sys/select.h>
#include <fcntl.h>
#include <signal.h>
#include <semaphore.h>
#include "dsp_monitor.h"
#include <linux/ioctl.h>
#include <cutils/sched_policy.h>
#include <sys/prctl.h>
#include <cutils/str_parms.h>
#include <system/thread_defs.h>
#include <string.h>


#define AUDIO_PIPE_MARGIC 'A'
#define	AUDIO_PIPE_WAKEUP		_IOW(AUDIO_PIPE_MARGIC, 0, int)
#define	AUDIO_PIPE_BTHAL_STATE_SET		_IOW(AUDIO_PIPE_MARGIC, 1, int)
#define	AUDIO_PIPE_BTHAL_STATE_GET		_IOR(AUDIO_PIPE_MARGIC, 0, int),

#define BTHAL_STATE_RUNNING     0
#define BTHAL_STATE_IDLE        1


#define SPRD_AUD_DSP_BTHAL   "/dev/audio_pipe_bthal" 

typedef enum {
    AGDSP_CMD_NET_MSG = 0x0,
    AGDSP_CMD_ASSERT = 0x25,
    AGDSP_CMD_TIMEOUT=0x26,
    AGDSP_CMD_ASSERT_EPIPE = 0x27,
    AGDSP_CMD_SMARTAMP_CALI = 0x28,
    AGDSP_CMD_STATUS_CHECK = 0x1234,
    AGDSP_CMD_BOOT_OK =0xbeee,
} AGDSP_CMD;

#define AGDSP_MSG_SIZE  sizeof(struct dsp_smsg)

volatile int log_level = 3;
#define LOG_V(...)  ALOGV_IF(log_level >= 5,__VA_ARGS__);
#define LOG_D(...)  ALOGD_IF(log_level >= 4,__VA_ARGS__);
#define LOG_I(...)  ALOGI_IF(log_level >= 3,__VA_ARGS__);
#define LOG_W(...)  ALOGW_IF(log_level >= 2,__VA_ARGS__);
#define LOG_E(...)  ALOGE_IF(log_level >= 1,__VA_ARGS__);


struct dsp_smsg {
    uint16_t        command;    /* command */
    uint16_t        channel;    /* channel index */
    uint32_t        parameter0;    /* msg parameter0 */
    uint32_t        parameter1;    /* msg parameter1 */
    uint32_t        parameter2;    /* msg parameter2 */
    uint32_t        parameter3;    /* msg parameter3 */
};

struct dsp_monitor_t {
    pthread_t thread_id;
    bool is_exit;
    int dsp_pipe_fd;
    bool dsp_assert;
    bool dsp_assert_time_out;
};

extern void adev_a2dp_force_stanby(bool forcestandby);

static int agdsp_msg_process(struct dsp_monitor_t * agdsp_ctl ,struct dsp_smsg *msg){
    int ret=0;
    int is_time_out = 0;
    LOG_I("agdsp_msg_process cmd:0x%x parameter:0x%x 0x%x 0x%x",
        msg->command,msg->parameter0,msg->parameter1,msg->parameter2);

    switch(msg->command){

        case AGDSP_CMD_TIMEOUT:
            LOG_E("dsp timeout!!!!!");
            agdsp_ctl->dsp_assert_time_out= true;
        case AGDSP_CMD_ASSERT:
            {
                LOG_E("force all standby a");
                int bthal_ready = BTHAL_STATE_IDLE;
                adev_a2dp_force_stanby(true);
                ret = ioctl(agdsp_ctl->dsp_pipe_fd, AUDIO_PIPE_BTHAL_STATE_SET,&bthal_ready);
                if(ret < 0) {
					ALOGE("ioctl AUDIO_PIPE_BTHAL_STATE_SET:error:%d", errno);
                }
                LOG_E("force all standby e");
            }
            break;
        case AGDSP_CMD_ASSERT_EPIPE:
            {
            	int bthal_ready = BTHAL_STATE_IDLE;
                if(agdsp_ctl->dsp_assert_time_out || agdsp_ctl->dsp_assert){
                    LOG_E("dsp already time out or assert!!!");
                    usleep(2000000);
                    break;
                }
               adev_a2dp_force_stanby(true);
               ret = ioctl(agdsp_ctl->dsp_pipe_fd, AUDIO_PIPE_BTHAL_STATE_SET,&bthal_ready);
                if(ret < 0) {
					ALOGE("ioctl AUDIO_PIPE_BTHAL_STATE_SET:error:%d", errno);
                }
                LOG_I("force all standby out");
            }
            break;
        case AGDSP_CMD_STATUS_CHECK:
        case AGDSP_CMD_BOOT_OK:
            LOG_E("peter: check dsp status start");
            int bthal_state = BTHAL_STATE_RUNNING;
            agdsp_ctl->dsp_assert = false;
            agdsp_ctl->dsp_assert_time_out = false;
            adev_a2dp_force_stanby(false);
            ret = ioctl(agdsp_ctl->dsp_pipe_fd, AUDIO_PIPE_BTHAL_STATE_SET,&bthal_state);
            if(ret < 0) {
				ALOGE("ioctl AUDIO_PIPE_BTHAL_STATE_SET:error:%d", errno);
            }
            break;
        default:
            break;
    }
    return ret;
}

static void *dsp_monitor_thread_routine(void *arg)
{
    struct dsp_monitor_t * dsp_ctl=(struct dsp_monitor_t *)arg;
    int ret = 0;
    struct dsp_smsg msg;
    setpriority(PRIO_PROCESS, 0, ANDROID_PRIORITY_AUDIO);
    set_sched_policy(0, SP_FOREGROUND);
    prctl(PR_SET_NAME, (unsigned long)"bthal dspctrl Thread", 0, 0, 0);

    while(!dsp_ctl->is_exit) {
        LOG_I("begin to receive agdsp pipe message");
        memset((void *)&msg,0,sizeof(struct dsp_smsg));
        ret = read(dsp_ctl->dsp_pipe_fd,&msg,sizeof(struct dsp_smsg));
        if(ret < 0){
            LOG_E("read data err errno = %d", errno);
            if (EINTR != errno && (!dsp_ctl->is_exit)) {
                usleep(2000000);
            }
        }else{
            agdsp_msg_process(dsp_ctl,&msg);
        }
    }
    LOG_W("dsp_monitor_rx_thread_routine exit!!!");
    return NULL;
}


void * dsp_monitor_open()
{
    int ret=0;
    int bthal_state = BTHAL_STATE_RUNNING;
    LOG_I("dsp_monitor_open");
    struct dsp_monitor_t * dsp_ctl = (struct dsp_monitor_t *)calloc(1, (sizeof(struct dsp_monitor_t )));
    if(!dsp_ctl) {
        ALOGE("dsp_ctl calloc failed");
        return NULL;
    }

    dsp_ctl->is_exit=false;

    dsp_ctl->dsp_pipe_fd = open(SPRD_AUD_DSP_BTHAL,O_RDWR);
    if(dsp_ctl->dsp_pipe_fd <= 0) {
        LOG_E("dsp_pipe_fd :%s,open failed %d",SPRD_AUD_DSP_BTHAL,dsp_ctl->dsp_pipe_fd);
        goto err;
    }
	ret = ioctl(dsp_ctl->dsp_pipe_fd, AUDIO_PIPE_BTHAL_STATE_SET,&bthal_state);
    if(ret < 0) {
		ALOGE("ioctl AUDIO_PIPE_BTHAL_STATE_SET:error:%d", errno);
    }

    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);

    ret=pthread_create((pthread_t *)(&dsp_ctl->thread_id), &attr,
            dsp_monitor_thread_routine, dsp_ctl);
    if (ret) {
        LOG_E("pthread_createfailed %d", ret);
        dsp_ctl->thread_id = -1;
        goto err;
    }

    return dsp_ctl;

err:
    if(dsp_ctl->dsp_pipe_fd > 0){
        close(dsp_ctl->dsp_pipe_fd);
    }
    free(dsp_ctl);
    LOG_E("dsp_monitor_open failed %d", ret);

    return NULL;
}

void dsp_monitor_close(void * dsp_monitor)
{
    int ret = 0;
    struct dsp_monitor_t * dsp_ctl = (struct dsp_monitor_t *)dsp_monitor;
    LOG_I("dsp_monitor_close enter");
    if(dsp_ctl==NULL){
        return;
    }
    dsp_ctl->is_exit=true;
    if(dsp_ctl->dsp_pipe_fd > 0) {
        ret = ioctl(dsp_ctl->dsp_pipe_fd, AUDIO_PIPE_WAKEUP, 1);
    }
    LOG_I("dsp_monitor_close exit 1");
    if(dsp_ctl->thread_id > 0) {
        pthread_kill(dsp_ctl->thread_id, SIGKILL);
        pthread_join(dsp_ctl->thread_id, NULL);
    }
    if(dsp_ctl->dsp_pipe_fd>0){
        close(dsp_ctl->dsp_pipe_fd);
    }
    LOG_I("dsp_monitor_close exit");
}
