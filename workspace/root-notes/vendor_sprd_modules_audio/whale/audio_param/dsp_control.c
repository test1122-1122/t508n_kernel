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
#define LOG_TAG "audio_hw_dsp"
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
#include <cutils/sockets.h>
#include "dsp_control.h"
#include "audio_debug.h"
#include "audio_hw.h"

#include "audio_control.h"
#include "atci.h"
#include <linux/ioctl.h>

#define DSPLOG_CMD_MARGIC 'X'

#define DSPLOG_CMD_LOG_ENABLE               _IOW(DSPLOG_CMD_MARGIC, 0, int)
#define DSPLOG_CMD_LOG_PATH_SET             _IOW(DSPLOG_CMD_MARGIC, 1, int)
#define DSPLOG_CMD_LOG_PACKET_ENABLE        _IOW(DSPLOG_CMD_MARGIC, 2, int)
#define DSPLOG_CMD_PCM_PATH_SET             _IOW(DSPLOG_CMD_MARGIC, 3, int)
#define DSPLOG_CMD_PCM_ENABLE               _IOW(DSPLOG_CMD_MARGIC, 4, int)
#define DSPLOG_CMD_PCM_PACKET_ENABLE        _IOW(DSPLOG_CMD_MARGIC, 5, int)
#define DSPLOG_CMD_DSPASSERT                _IOW(DSPLOG_CMD_MARGIC, 6, int)
#define DSPLOG_CMD_DSPDUMP_ENABLE           _IOW(DSPLOG_CMD_MARGIC, 7, int)
#define DSPLOG_CMD_TIMEOUTDUMP_ENABLE       _IOW(DSPLOG_CMD_MARGIC, 8, int)


#define AUDIO_PIPE_MARGIC 'A'
#define AUDIO_PIPE_WAKEUP             _IOW(AUDIO_PIPE_MARGIC, 0, int)
#define	AUDIO_PIPE_BTHAL_STATE_GET   _IOR(AUDIO_PIPE_MARGIC, 0, int)


#define SPRD_AUD_DSPASSERT_MEM "/dev/audio_dsp_mem"
#define SPRD_AUD_DSPASSERT_LOG "/dev/audio_dsp_log"
#define SPRD_AUD_DSPASSERT_PCM "/dev/audio_dsp_pcm"

#define SPRD_AUD_MINIAP_2_AP "/dev/spipe_nr6"
#define AGDSP_ASSERT_NOTIFYMODEMD_MSG  "AGDSP Assert:"

#define RESET_AUDIO_PROPERTY     "persist.vendor.media.audio.dspreset"

#define BTHAL_STATE_RUNNING     0
#define BTHAL_STATE_IDLE        1

extern void agdsp_boot(void);
extern int stop_voice_call(struct audio_control *ctl);
extern int start_voice_call(struct audio_control *actl);
extern int audiotester_updata_audioparam(AUDIO_PARAM_T *audio_param,int opt, bool is_ram);

static unsigned int agdsp_auto_reset_property() {
    uint8_t tmp[PROPERTY_VALUE_MAX] = {0};
    int ret = 0;
    unsigned int reset_dsp = 0;
    ret=property_get(RESET_AUDIO_PROPERTY, (char *)tmp, NULL);
    if (ret) {
        reset_dsp=(unsigned int)strtoul((const char*)tmp,NULL,0);
    }
    LOG_I("agdsp_auto_reset:reset_dsp_property:%d",reset_dsp);
    return reset_dsp;
}

int agdsp_send_msg(struct dsp_control_t * dsp_ctl,struct dsp_smsg *msg){
    int ret=-1;
    LOG_I("agdsp_send_msg cmd:0x%x param:0x%x 0x%x 0x%x 0x%x",
        msg->command,
        msg->parameter0,
        msg->parameter1,
        msg->parameter2,
        msg->parameter3);
    memset(&dsp_ctl->msg,0,sizeof(struct dsp_smsg));
    ret=write(dsp_ctl->agdsp_pipd_fd,msg,sizeof(struct dsp_smsg));
    return 0;
}

static int dsp_ctrl_dsp_assert_notify(struct dsp_control_t * dsp_ctl ,bool stop_phone,bool reset_modem,const char * info,int info_len)
{
    const char *err_str = NULL;
    int ret = 0;
    LOG_E("peter:dsp_ctrl_notify_modemd in");
    if(((dsp_ctl->fd_dsp_assert_mem>0)  &&  (dsp_ctl->auto_reset_dsp == 0)
        &&  (1==agdsp_auto_reset_property()))
            || (dsp_ctl->dsp_assert_force_notify)){
        ret = ioctl(dsp_ctl->fd_dsp_assert_mem, DSPLOG_CMD_DSPASSERT, 1);
        LOG_E(" dsp_ctrl_notify_modemd notify slogd ret: %d",ret);
    }
    else {
        LOG_E("dsp_ctrl_notify_modemd notify slogd failed,dsp_ctl->fd_dsp_assert_mem  %d",dsp_ctl->fd_dsp_assert_mem);
    }
    if(reset_modem) {
        err_str = sendCmd(0, "AT+SPATASSERT=1");
    }
    if(stop_phone) {
        err_str = sendCmd(0, "ATH");
    }

    if(dsp_ctl->fd_modemd_notify
        && (dsp_ctl->dsp_assert_notify_ok < 10)
        && dsp_ctl->dsp_assert_force_notify){

        int len = 0;
        char* buf = NULL;
        len = sizeof(AGDSP_ASSERT_NOTIFYMODEMD_MSG) + info_len;

        buf = malloc(len);
        if(buf) {
            memset(buf, 0,len);
            strcat(buf,AGDSP_ASSERT_NOTIFYMODEMD_MSG);
            if(info) {
                strcat(buf,(const char *)info);
            }
            ret=write(dsp_ctl->fd_modemd_notify,buf,sizeof(AGDSP_ASSERT_NOTIFYMODEMD_MSG) + info_len);
            LOG_E("dsp_ctrl_notify_modemd write ret  %d,%s",ret,buf);
            dsp_ctl->dsp_assert_notify_ok++;
        }
        else {
            LOG_E(" dsp_assert_notify malloc failed len is %d", len);
        }

        if(buf) {
            free(buf);
        }
    }
    else {
        LOG_E("dsp_ctrl_notify_modemd dsp_ctl->fd_modemd_notify %d",dsp_ctl->fd_modemd_notify);
    }
    return ret;
}

static int agdsp_net_msg_process(struct audio_control *dev_ctl,struct dsp_smsg *msg){
    struct voice_net_t net_infor;

    net_infor.net_mode=(aud_net_m)msg->parameter0;
    net_infor.rate_mode=msg->parameter1;

   return set_audioparam(dev_ctl,PARAM_NET_CHANGE,&net_infor,false);
}

static int agdsp_msg_process(struct dsp_control_t * agdsp_ctl ,struct dsp_smsg *msg){
    int ret=0;
    int is_time_out = 0;
    LOG_I("agdsp_msg_process cmd:0x%x parameter:0x%x 0x%x 0x%x",
        msg->command,msg->parameter0,msg->parameter1,msg->parameter2);

    if(msg->channel!=2){
        LOG_W("agdsp_msg_process channel:%d",msg->channel);
        return 0;
    }

    switch(msg->command){
        case AGDSP_CMD_NET_MSG:
            agdsp_net_msg_process(agdsp_ctl->dev_ctl,msg);
            break;
        case AGDSP_CMD_TIMEOUT:
            LOG_E("dsp timeout!!!!!");
            agdsp_ctl->dsp_assert_time_out= true;
        case AGDSP_CMD_ASSERT:
            {
                uint8_t tmp[40] = {0};
                int len = 0;
                agdsp_ctl->dsp_assert = true;
                aud_dsp_assert_set(agdsp_ctl->dev, true);
                LOG_E("dsp asserted timeout %d",is_time_out);
                len = sprintf((char *)tmp,":%x,%x,%x,%x",msg->parameter0,msg->parameter1,msg->parameter2,msg->parameter3);
                set_usecase(agdsp_ctl->dev_ctl, UC_AGDSP_ASSERT, true);
                if(agdsp_ctl->dsp_assert_time_out) {
                    if(is_usecase(agdsp_ctl->dev_ctl, UC_CALL)) {
                        dsp_ctrl_dsp_assert_notify(agdsp_ctl,1, agdsp_ctl->reset_modem, "time out", sizeof("time out"));
                    }
                    else
                        dsp_ctrl_dsp_assert_notify(agdsp_ctl,0, agdsp_ctl->reset_modem, "time out", sizeof("time out"));
                }
                else {
                    dsp_ctrl_dsp_assert_notify(agdsp_ctl,0,agdsp_ctl->reset_modem,(const char *)tmp, len);
                }
                LOG_E("peter: force all standby a");
                force_all_standby(agdsp_ctl->dev);
                if(agdsp_ctl->auto_reset_dsp || agdsp_auto_reset_property()) {
                    int bthal_state;
                    do {
                        ret = ioctl(agdsp_ctl->agdsp_pipd_fd, AUDIO_PIPE_BTHAL_STATE_GET, &bthal_state);
                        if(ret < 0) {
                            ALOGE("ioctl AUDIO_PIPE_BTHAL_STATE_SET:error:%d", errno);
                            break;
                        }
                        if(bthal_state != BTHAL_STATE_IDLE) {
                            usleep(100000);
                        }
                        ALOGI("waiting bt audio hal to clean up");
                    } while(bthal_state != BTHAL_STATE_IDLE);
                    agdsp_boot();
                }
                LOG_E("peter: force all standby e");
            }
            break;
        case AGDSP_CMD_ASSERT_EPIPE:
            {
                if(agdsp_ctl->dsp_assert_time_out || agdsp_ctl->dsp_assert){
                    LOG_E("dsp already time out or assert!!!");
                    usleep(2000000);
                    break;
                }
                uint8_t temp[40] = {0};
                int len_t = 0;
                aud_dsp_assert_set(agdsp_ctl->dev, true);
                LOG_E("dsp asserted!!!");
                len_t = snprintf((char *)temp,sizeof(temp),":%x,%x,%x,%x",msg->parameter0,msg->parameter1,msg->parameter2,msg->parameter3);
                set_usecase(agdsp_ctl->dev_ctl, UC_AGDSP_ASSERT, true);
                dsp_ctrl_dsp_assert_notify(agdsp_ctl,0,agdsp_ctl->reset_modem,(const char *)temp,len_t);

                LOG_E("peter: force all standby in");
                force_all_standby(agdsp_ctl->dev);
                if(agdsp_ctl->auto_reset_dsp || agdsp_auto_reset_property()) {
                    int bthal_state;
                    do {
                        ret = ioctl(agdsp_ctl->agdsp_pipd_fd, AUDIO_PIPE_BTHAL_STATE_GET, &bthal_state);
                        if(ret < 0) {
                            ALOGE("ioctl AUDIO_PIPE_BTHAL_STATE_SET:error:%d", errno);
                            break;
                        }
                        if(bthal_state != BTHAL_STATE_IDLE) {
                            usleep(100000);
                        }
                    } while(bthal_state != BTHAL_STATE_IDLE);
                    agdsp_boot();
                }
                if(agdsp_ctl->dsp_assert_force_notify){
                    usleep(2000000);
                }
                LOG_E("peter: force all standby out");
            }
            break;
        case AGDSP_CMD_STATUS_CHECK:
        case AGDSP_CMD_BOOT_OK:
            LOG_E("peter: check dsp status start");
            agdsp_ctl->dsp_assert_notify_ok = 0;
            agdsp_ctl->dsp_assert = false;
            agdsp_ctl->dsp_assert_time_out = false;
            memcpy(&agdsp_ctl->msg,msg,sizeof(struct dsp_smsg));
            sem_post(&agdsp_ctl->rx.sem);
            set_usecase(agdsp_ctl->dev_ctl,UC_AGDSP_ASSERT, false);
            aud_dsp_assert_set(agdsp_ctl->dev, false);
            break;
        case AGDSP_CMD_SMARTAMP_CALI:
            {
                struct audio_control *dev_ctl=(struct audio_control *)agdsp_ctl->dev_ctl;
                set_smartamp_cali_values(&dev_ctl->smartamp_ctl,msg->parameter0,msg->parameter1);
            }
            break;
        case AGDSP_CMD_DA_NO_INT:
        case AGDSP_CMD_AD_NO_INT:
            LOG_E("AGDSP moniter %s NO interrupt, lost count %d, dump VBC & Codec reg!",
                  (msg->command == AGDSP_CMD_DA_NO_INT) ? "DA" : "AD", msg->parameter0);
            dump_all_audio_reg(-1, (1<<ADEV_DUMP_VBC_REG) | (1<<ADEV_DUMP_CODEC_REG));
            break;
        default:
            break;
    }
    return ret;
}

int agdsp_send_msg_test(void * arg,UNUSED_ATTR void * params,int opt,UNUSED_ATTR char *val){
    struct tiny_audio_device *adev = (struct tiny_audio_device *)arg ;
    struct dsp_smsg _msg;
    _msg.channel=2;
    _msg.command=opt;
    _msg.parameter0=opt+1;
    _msg.parameter1=opt+2;
    _msg.parameter2=opt+3;
    agdsp_send_msg(adev->dev_ctl->agdsp_ctl,&_msg);
    return 0;
}

bool agdsp_check_status(struct dsp_control_t * dsp_ctl){
    struct dsp_smsg _msg;
    _msg.channel=2;
    _msg.command=AGDSP_CMD_STATUS_CHECK;
    _msg.parameter0=AGDSP_CMD_STATUS_CHECK+1;
    _msg.parameter1=AGDSP_CMD_STATUS_CHECK+2;
    _msg.parameter2=AGDSP_CMD_STATUS_CHECK+3;
    agdsp_send_msg(dsp_ctl,&_msg);
    sem_wait(&dsp_ctl->rx.sem);
    if((_msg.command !=dsp_ctl->msg.command) ||
       (_msg.parameter0 !=dsp_ctl->msg.parameter0) ||
       (_msg.parameter1 !=dsp_ctl->msg.parameter1) ||
       (_msg.parameter2 !=dsp_ctl->msg.parameter2)
    ){
        LOG_E("check_agdsp_status failed send:0x%x 0x%x 0x%x 0x%x recv: 0x%x 0x%x 0x%x 0x%x",
            _msg.command,_msg.parameter0,_msg.parameter1,_msg.parameter2,
            dsp_ctl->msg.command,dsp_ctl->msg.parameter0,dsp_ctl->msg.parameter1,dsp_ctl->msg.parameter2);
        return false;
    }
    return true;
}

typedef enum {
    AUD_CMD_EXIT,
    AUD_CMD_NETMODE,
    AUD_CMD_NETMODE_RSP,
    AUD_CMD_CLOSE,
}AUD_CMD_T;

struct parameters_head_t
{
    char        tag[4];   /* "VBC" */
    unsigned int    cmd_type;
    unsigned int    paras_size; /* the size of Parameters Data, unit: bytes*/
};

struct netmode_t {
    struct parameters_head_t head;
    struct voice_net_t netmode;
};

/*
 * local functions definition.
 */
static int read_nonblock(int fd,void *buf,int bytes)
{
    int ret = 0;
    int bytes_to_read = bytes;

    if((fd > 0) && (buf != NULL)) {
        do {
            ret = read(fd, (uint8_t *)buf+bytes_to_read-bytes, bytes);
            if( ret > 0) {
                if(ret <= bytes) {
                    bytes -= ret;
                }
            } else if((!((errno == EAGAIN) || (errno == EINTR))) || (0 == ret)) {
                LOG_E("pipe read error %d,bytes read is %d",errno,bytes_to_read - bytes);
                break;
            } else {
                LOG_W("pipe_read_warning: %d,ret is %d",errno,ret);
            }
        } while(bytes);
    }

    if(bytes == bytes_to_read)
        return ret ;
    else
        return (bytes_to_read - bytes);

}
static int write_nonblock(int fd,void *buf,int bytes)
{
    int ret = -1;
    int bytes_to_read = bytes;

    if((fd > 0) && (buf != NULL)) {
        do {
            ret = write(fd, (uint8_t *)buf+(bytes_to_read-bytes), bytes);
            if( ret > 0) {
                if(ret <= bytes) {
                    bytes -= ret;
                }
            } else if((!((errno == EAGAIN) || (errno == EINTR))) || (0 == ret)) {
                LOG_E("pipe write error %d,bytes read is %d",errno,bytes_to_read - bytes);
                break;
            } else {
                LOG_W("pipe_write_warning: %d,ret is %d",errno,ret);
            }
        } while(bytes);
    }

    if(bytes == bytes_to_read)
        return ret ;
    else
        return (bytes_to_read - bytes);

}


static void dspctl_empty_pipe(int fd) {
    char buff[16];
    int ret;

    do {
        ret = read(fd, &buff, sizeof(buff));
    } while (ret > 0 || (ret < 0 && errno == EINTR));
}

static int  ReadParas_Head(int fd_pipe,  struct parameters_head_t *head_ptr)
{
    int ret = 0;
    ret = read_nonblock(fd_pipe,(void *)head_ptr,sizeof(struct parameters_head_t));
    if(ret != sizeof(struct parameters_head_t))
            ret = -1;
    return ret;
}


static int netmode_cmd_process(struct dsp_control_t * dsp_ctl,struct voice_net_t netmode) {
    return set_audioparam(dsp_ctl->dev_ctl,PARAM_NET_CHANGE,&netmode,false);
}

void *thread_routine_miniap(void *arg)
{
    struct dsp_control_t * dsp_ctl = (struct dsp_control_t *)arg;
    int open_count = 0;
    int maxfd;
    fd_set fds_read;
    int ret = 0;
    int dsp_fd = -1;
    struct parameters_head_t read_common_head;
    struct parameters_head_t write_common_head;

    LOG_I("%s enter", __func__);
    dsp_ctl->pipe_fd_miniap = -2;
    while (dsp_ctl->pipe_fd_miniap < 0) {
        dsp_ctl->pipe_fd_miniap = open(SPRD_AUD_MINIAP_2_AP, O_RDWR);
        if (dsp_ctl->pipe_fd_miniap < 0) {
            LOG_I("vbc_ctrl_thread_routine open fail, pipe_name:%s, %d.", SPRD_AUD_MINIAP_2_AP, errno);
            usleep(200000);
            open_count ++;
            if(open_count > 100) {
                LOG_E("peter:dsp_ctl->pipe_fd_miniap %s, open failed",SPRD_AUD_MINIAP_2_AP);
                return 0;
            }
        } else {
            LOG_I("dspctl:pipe_name(%s) pipe_fd_miniap(%d) open successfully.", SPRD_AUD_MINIAP_2_AP, dsp_ctl->pipe_fd_miniap);
            if(dsp_ctl->pipe_fd_miniap >= MAX_SELECT_FD){
                LOG_E("thread_routine_miniap ORTIFY: FD_SET: file descriptor %d >= FD_SETSIZE %d",dsp_ctl->pipe_fd_miniap,MAX_SELECT_FD);
                close(dsp_ctl->pipe_fd_miniap);
                dsp_ctl->pipe_fd_miniap=true;
                return NULL;
            }
        }

        if(true==dsp_ctl->isMiniAPExit){
            break;
        }
    }

    if(true==dsp_ctl->isMiniAPExit){
        if(dsp_ctl->pipe_fd_miniap>0){
            close(dsp_ctl->pipe_fd_miniap);
            dsp_ctl->pipe_fd_miniap=-1;
            return NULL;
        }
        return NULL;
    }

        dsp_fd = dsp_ctl->pipe_fd[0];
        if(dsp_fd<=0){
        ALOGW("%s line:%d pipe is not ready",__func__,__LINE__);
        maxfd = dsp_ctl->pipe_fd_miniap + 1;
        if((fcntl(dsp_ctl->pipe_fd_miniap,F_SETFL,O_NONBLOCK))<0)
        {
            LOG_D("dspctl:bpipe_name(%s) pipe_fd(%d) fcntl error.", SPRD_AUD_MINIAP_2_AP, dsp_ctl->pipe_fd_miniap);
        }
    } else {
        maxfd = (dsp_ctl->pipe_fd_miniap > dsp_fd) ? dsp_ctl->pipe_fd_miniap + 1 : dsp_fd + 1;
        if((fcntl(dsp_ctl->pipe_fd_miniap,F_SETFL,O_NONBLOCK))<0)
        {
            LOG_D("dspctl:bpipe_name(%s) pipe_fd(%d) fcntl error.", SPRD_AUD_MINIAP_2_AP, dsp_ctl->pipe_fd_miniap);
        }

        if((fcntl(dsp_fd,F_SETFL,O_NONBLOCK))<0)
        {
            ALOGD("dspctl:bpipe_name(%s) dsp_fd(%d) fcntl error.", SPRD_AUD_MINIAP_2_AP, dsp_fd);
        }
    }

     while(!dsp_ctl->isMiniAPExit) {
        FD_ZERO(&fds_read);
        FD_SET(dsp_ctl->pipe_fd_miniap,&fds_read);
        if(dsp_fd>0)
        FD_SET(dsp_fd,&fds_read);
        ret = select(maxfd,&fds_read,NULL,NULL,NULL);
        if(ret < 0) {
            LOG_E("dspctl:select error %d",errno);
            continue;
        } else if(!ret) {
            LOG_E("dspctl:select timeout");
        }

        if(FD_ISSET(dsp_fd,&fds_read)) {
             dspctl_empty_pipe(dsp_fd);
        } else {
             LOG_E("dspctl:select ok but no fd is set");
        }

        if(FD_ISSET(dsp_ctl->pipe_fd_miniap, &fds_read) <= 0) {
             LOG_E("dspctl:select ok but no fd is set too");
             continue;
        }

        ret = ReadParas_Head(dsp_ctl->pipe_fd_miniap, &read_common_head);
        LOG_I("miniap readparas, pipe_name:%d, ret:%d.", dsp_ctl->pipe_fd_miniap, ret);
        if(ret < 0) {
            LOG_E("%s read head failed(%s), pipe_name:%d, need to read again ",
                __func__, strerror(errno), dsp_ctl->pipe_fd_miniap);
            dspctl_empty_pipe(dsp_ctl->pipe_fd_miniap);
            continue;
        }

        switch (read_common_head.cmd_type)
        {
            case AUD_CMD_NETMODE:
            {
                struct voice_net_t netmode;
                if(read_common_head.paras_size != sizeof(struct voice_net_t)) {
                    LOG_E("read_common_head.paras_size error: size:%d",read_common_head.paras_size);
                    dspctl_empty_pipe(dsp_ctl->pipe_fd_miniap);
                    break;
                }
                ret = read_nonblock(dsp_ctl->pipe_fd_miniap,&netmode, sizeof(struct voice_net_t));
                if(ret != sizeof(struct voice_net_t)) {
                    LOG_E("read_nonblock.voice_net_t  read error: size:%d",read_common_head.paras_size);
                    dspctl_empty_pipe(dsp_ctl->pipe_fd_miniap);
                    break;;
                }
                write_common_head.cmd_type = AUD_CMD_NETMODE_RSP;
                ret = write_nonblock(dsp_ctl->pipe_fd_miniap,(void *)&write_common_head, sizeof(struct parameters_head_t));
                if(ret != sizeof(struct parameters_head_t)) {
                    LOG_E("write_nonblock.voice_net_t  write error: size:%d",read_common_head.paras_size);
                }
                netmode_cmd_process(dsp_ctl,netmode);
                break;
            }
            default:
                LOG_E("unknow aud cmd_type %d",read_common_head.cmd_type);
                break;
        }
    }
    dsp_ctl->exit_miniap = true;
    return NULL;
}

int agdsp_auto_reset(void * arg,UNUSED_ATTR void * params,int opt,UNUSED_ATTR char * val){
    struct tiny_audio_device *adev = (struct tiny_audio_device *)arg ;
    adev->dev_ctl->agdsp_ctl->auto_reset_dsp = opt;
    LOG_I(" agdsp_auto_reset auto_reset_dsp opt: %d", opt);
    return 0;
}

int agdsp_autoreset_property_set(UNUSED_ATTR void * arg,UNUSED_ATTR struct str_parms *parms,int opt,UNUSED_ATTR char * val){
    int ret = 0;
    if(opt) {
        ret = property_set(RESET_AUDIO_PROPERTY,"1");
    }
    else {
        ret = property_set(RESET_AUDIO_PROPERTY,"0");
    }
    LOG_I(" agdsp_autoreset_property_set: %d,ret:%d", opt,ret);
    return 0;
}

int agdsp_timeout_dump_set(void * arg,UNUSED_ATTR void * params,int opt,UNUSED_ATTR char * val){
    struct tiny_audio_device *adev = (struct tiny_audio_device *)arg ;
    struct dsp_control_t *agdsp_ctl = adev->dev_ctl->agdsp_ctl;
    int ret = 0;
    if(agdsp_ctl->fd_dsp_assert_log > 0) {
        ret = ioctl(agdsp_ctl->fd_dsp_assert_mem, DSPLOG_CMD_TIMEOUTDUMP_ENABLE, opt);
        LOG_E(" agdsp_timeout_dump_set ret: %d, opt: %d",ret, opt);
    }
    return 0;
}

int agdsp_force_assert_notify(void * arg,UNUSED_ATTR void * params,int opt,UNUSED_ATTR char * val){
    struct tiny_audio_device *adev = (struct tiny_audio_device *)arg ;
    adev->dev_ctl->agdsp_ctl->dsp_assert_force_notify = opt;
    LOG_D(" agdsp_force_assert_notify lodsp_assert_force_notifyg set opt: %d", opt);
    return 0;
}

int agdsp_reboot(UNUSED_ATTR void * arg,UNUSED_ATTR void * params,int opt,UNUSED_ATTR char *val) {
    LOG_D(" agdsp_reboot  set opt: %d", opt);
    if(opt) {
        agdsp_boot();
    }
    return 0;
}

int agdsp_log_set(void * arg,UNUSED_ATTR void * params,int opt,UNUSED_ATTR char *val){
    struct tiny_audio_device *adev = (struct tiny_audio_device *)arg ;
    struct dsp_control_t *agdsp_ctl = adev->dev_ctl->agdsp_ctl;
    int ret = 0;
    if(agdsp_ctl->fd_dsp_assert_log > 0) {
        ret = ioctl(agdsp_ctl->fd_dsp_assert_log, DSPLOG_CMD_LOG_ENABLE, opt);
        LOG_E(" agdsp_log_set log set ret: %d, opt: %d",ret, opt);
    }
    return 0;
}

int agdsp_pcmdump_set(void * arg,UNUSED_ATTR void * params,int opt,UNUSED_ATTR char *val){
    struct tiny_audio_device *adev = (struct tiny_audio_device *)arg ;
    struct dsp_control_t *agdsp_ctl = adev->dev_ctl->agdsp_ctl;
    int ret = 0;
    if(agdsp_ctl->fd_dsp_assert_pcm > 0) {
        ret = ioctl(agdsp_ctl->fd_dsp_assert_pcm, DSPLOG_CMD_PCM_ENABLE, opt);
        LOG_E(" agdsp_pcmdump_set log set ret: %d, opt: %d",ret, opt);
    }
    return 0;
}

int agdsp_check_status_test(void * arg,UNUSED_ATTR void * params,UNUSED_ATTR int opt,UNUSED_ATTR char *val){
    struct tiny_audio_device *adev = (struct tiny_audio_device *)arg ;
    agdsp_check_status(adev->dev_ctl->agdsp_ctl);
    return 0;
}

static void *agdsp_pipe_process(void *arg){
    int ret = 0;
    struct dsp_control_t * dsp_ctl=(struct dsp_control_t *)arg;
    struct dsp_smsg msg;

    LOG_I("begin to receive agdsp pipe message is_exit:%d",dsp_ctl->rx.is_exit);
    memset(&msg,0,sizeof(struct dsp_smsg));
    ret = read(dsp_ctl->agdsp_pipd_fd,&msg,sizeof(struct dsp_smsg));
    if(ret < 0){
        LOG_E("read data err errno = %d is_exit:%d", errno,dsp_ctl->rx.is_exit);
        if (EINTR != errno && (!dsp_ctl->rx.is_exit)) {
            usleep(200000);
        }
    }else{
        agdsp_msg_process(dsp_ctl,&msg);
    }

    LOG_D("agdsp_pipe_process exit");
    return NULL;
}


static void *dsp_ctrl_rx_thread_routine(void *arg)
{
    struct dsp_control_t * dsp_ctl=(struct dsp_control_t *)arg;


    setpriority(PRIO_PROCESS, 0, ANDROID_PRIORITY_AUDIO);

    prctl(PR_SET_NAME, (unsigned long)"Audio DspRx Thread", 0, 0, 0);

    dsp_ctl->agdsp_pipd_fd = open(AGDSP_CTL_PIPE, O_RDWR);
    if(dsp_ctl->agdsp_pipd_fd < 0){
        LOG_E("%s, open pipe error!! ",__func__);
        dsp_ctl->rx.is_exit = true;
        return NULL;
    }

    while(!dsp_ctl->rx.is_exit) {
        agdsp_pipe_process(dsp_ctl);
    }
    close(dsp_ctl->agdsp_pipd_fd);
    dsp_ctl->agdsp_pipd_fd=-1;
    dsp_ctl->rx.thread_exit = true;
    LOG_W("dsp_ctrl_rx_thread_routine exit!!!");
    return NULL;
}

int send_cmd_to_dsp_thread(struct dsp_control_t *agdsp_ctl,int cmd,void* parameter){
    LOG_D("send_cmd_to_dsp_thread:%d %p",cmd,agdsp_ctl);
    pthread_mutex_lock(&agdsp_ctl->tx.lock);
    agdsp_ctl->tx.cmd=cmd;
    agdsp_ctl->tx.parameter=parameter;
    pthread_mutex_unlock(&agdsp_ctl->tx.lock);
    sem_post(&agdsp_ctl->tx.sem);
    return 0;
}

static void *dsp_ctrl_tx_thread_routine(void *arg)
{
    struct dsp_control_t * dsp_ctl=(struct dsp_control_t *)arg;
    pthread_attr_t attr;
    struct sched_param m_param;
    int newprio = 39;
    int ret = 0;

    LOG_D("dsp_ctrl_tx_thread_routine enter");
    pthread_attr_init(&attr);
    pthread_attr_setscope(&attr, PTHREAD_SCOPE_SYSTEM);
    pthread_attr_setschedpolicy(&attr, SCHED_FIFO);
    pthread_attr_getschedparam(&attr, &m_param);
    m_param.sched_priority = newprio;
    pthread_attr_setschedparam(&attr, &m_param);
    ret = pthread_setschedparam(dsp_ctl->tx.thread_id,SCHED_FIFO, &m_param);
    LOG_I("peter: pthread_setschedparam dsp_ctrl_tx_thread_routine ret %d", ret);

    while(!dsp_ctl->tx.is_exit) {
        LOG_I("dsp_ctrl_tx_thread_routine wait begin");
        sem_wait(&dsp_ctl->tx.sem);
        LOG_I("dsp_ctrl_tx_thread_routine wait end cmd:%d",dsp_ctl->tx.cmd);
        pthread_mutex_lock(&dsp_ctl->tx.lock);
        switch(dsp_ctl->tx.cmd){
            case AUDIO_CTL_STOP_VOICE:
                stop_voice_call(dsp_ctl->dev_ctl);
                break;
            case AUDIO_CTL_START_VOICE:{
                    start_voice_call(dsp_ctl->dev_ctl);
                }
                break;
            case AUDIO_TESTER_UPDATAE_AUDIO_PARAM_TO_RAM:    {
                    audio_param_dsp_cmd_t *res=(audio_param_dsp_cmd_t *)dsp_ctl->tx.parameter;
                    if(NULL!=res){
                        struct audio_control *ctl=(struct audio_control *)dsp_ctl->dev_ctl;
                        audiotester_updata_audioparam(res->res,res->opt,true);
                        free(res);
                        force_in_standby(ctl->adev,AUDIO_HW_APP_NORMAL_RECORD);
                    }
                }
                break;
            case AUDIO_TESTER_UPDATAE_AUDIO_PARAM_TO_FLASH:    {
                    audio_param_dsp_cmd_t *res=(audio_param_dsp_cmd_t *)dsp_ctl->tx.parameter;
                    if(NULL!=res){
                        struct audio_control *ctl=(struct audio_control *)dsp_ctl->dev_ctl;
                        audiotester_updata_audioparam(res->res,res->opt,false);
                        free(res);
                        force_in_standby(ctl->adev,AUDIO_HW_APP_NORMAL_RECORD);
                    }
                }
                break;
            case RIL_NET_MODE_CHANGE:
                break;
            default:
                break;
        }
        pthread_mutex_unlock(&dsp_ctl->tx.lock);
    }
    LOG_W("dsp_ctrl_tx_thread_routine exit");
    return 0;
}

static void *dsp_ctrl_modemd_notify_thread_routine(void *arg)
{
    int try_count=100;
    int fd = 0;
    struct dsp_control_t * dsp_ctl=(struct dsp_control_t *)arg;
    LOG_D("dsp_ctrl_modemd_notify_thread_routine enter");
    do {
        try_count--;
        if(true==dsp_ctl->tx.is_exit){
            break;
        }

        fd = socket_local_client("modemd",
                                 ANDROID_SOCKET_NAMESPACE_ABSTRACT, SOCK_STREAM);
        if(fd < 0 ) {
            LOG_E("modem_monitor_routine:socket_local_client failed %d", errno);
            usleep(200 * 1000);
        }
    } while((fd < 0) && (try_count > 0));

    if(fd<0){
        return NULL;
    }
    dsp_ctl->fd_modemd_notify = fd;
    dsp_ctl->modemd.thread_exit = true;
    LOG_I("dsp_ctrl_modemd_notify_thread_routine exit,dsp_ctl->fd_modemd_notify %d",dsp_ctl->fd_modemd_notify);
    return NULL;
}

int dsp_sleep_ctrl_l(struct dsp_control_t * dsp_ctl ,bool on_off){
    int ret=0;
    if(NULL ==dsp_ctl->dsp_sleep_ctl){
        LOG_E("dsp_sleep_ctrl_l  failed mixer is null");
        ret= -1;
        goto exit;
    }

    ret = mixer_ctl_set_value(dsp_ctl->dsp_sleep_ctl, 0, on_off);
    if (ret != 0) {
        LOG_E("dsp_sleep_ctrl_l Failed %d\n", on_off);
    }else{
        LOG_I("dsp_sleep_ctrl_l:%d",on_off);
    }

exit:
    return ret;
}

int dsp_sleep_ctrl_pair(struct dsp_control_t * dsp_ctl ,bool on_off){
    int ret=0;

    pthread_mutex_lock(&dsp_ctl->lock);
    if(on_off) {
        if(!dsp_ctl->agdsp_access_cnt &&
        !dsp_ctl->agdsp_sleep_status) {
            ret = dsp_sleep_ctrl_l(dsp_ctl, on_off);
            if(ret) {
                    LOG_E("dsp_sleep_ctrl_pair:error:ret:%d,on_off:%d",ret, on_off);
                    pthread_mutex_unlock(&dsp_ctl->lock);
                    return ret;
            }
        }
        dsp_ctl->agdsp_access_cnt++;
    }
    else {
        dsp_ctl->agdsp_access_cnt--;
        if(!dsp_ctl->agdsp_access_cnt &&
            !dsp_ctl->agdsp_sleep_status){
            ret = dsp_sleep_ctrl_l(dsp_ctl, on_off);
            if(ret) {
                dsp_ctl->agdsp_access_cnt++;
                LOG_E("dsp_sleep_ctrl_pair:error:ret:%d,on_off:%d",ret, on_off);
            }
        }
    }
    LOG_I("dsp_sleep_ctrl_pair: count:%d,on_off:%d,dsp_ctl->agdsp_sleep_status:%d",dsp_ctl->agdsp_access_cnt,on_off,dsp_ctl->agdsp_sleep_status);
    pthread_mutex_unlock(&dsp_ctl->lock);
    return ret;
}

int dsp_sleep_ctrl(struct dsp_control_t * dsp_ctl ,bool on_off){
    int ret=0;

    pthread_mutex_lock(&dsp_ctl->lock);
    dsp_ctl->agdsp_sleep_status=on_off;
    if(on_off) {
        if(!dsp_ctl->agdsp_access_cnt) {
            ret = dsp_sleep_ctrl_l(dsp_ctl, on_off);
            if(ret) {
                LOG_E("dsp_sleep_ctrl:dsp_sleep_ctrl_l error:%d,on_off:%d",ret,on_off);
                dsp_ctl->agdsp_sleep_status = false;
            }
        }
    }
    else {
        if(!dsp_ctl->agdsp_access_cnt) {
            ret = dsp_sleep_ctrl_l(dsp_ctl, on_off);
            if(ret){
                dsp_ctl->agdsp_access_cnt = true;
                LOG_E("dsp_sleep_ctrl:dsp_sleep_ctrl_l error:%d,on_off:%d",ret,on_off);
            }
        }
    }
    LOG_I("dsp_sleep_ctrl: count:%d,on_off:%d,dsp_ctl->agdsp_sleep_status:%d",dsp_ctl->agdsp_access_cnt,on_off,dsp_ctl->agdsp_sleep_status);
    pthread_mutex_unlock(&dsp_ctl->lock);
    return ret;
}

void * dsp_ctrl_open(void *ctl)
{
    int ret=0;
    LOG_I("dsp_ctrl_open");
    struct audio_control *dev_ctl=(struct audio_control *)ctl;
    struct dsp_control_t * dsp_ctl = (struct dsp_control_t *)calloc(1, (sizeof(struct dsp_control_t )));

    if(NULL==dsp_ctl){
        LOG_E("dsp_ctrl_open failed");
        return NULL;
    }
    memset(dsp_ctl,0,sizeof(struct dsp_control_t));
    dsp_ctl->pipe_fd[0] = -1;
    dsp_ctl->pipe_fd[1] = -1;

    dsp_ctl->dev=dev_ctl->adev;
    dsp_ctl->dev_ctl = dev_ctl;

#ifdef AUDIO_DEBUG
    dsp_ctl->auto_reset_dsp = 0;
    dsp_ctl->reset_modem = 0;
    dsp_ctl->dsp_assert_force_notify = 1;
#else
    dsp_ctl->auto_reset_dsp = 1;
    dsp_ctl->reset_modem = 0;
    dsp_ctl->dsp_assert_force_notify = 0;
#endif

    dsp_ctl->dsp_assert_notify_ok = 0;
    dsp_ctl->dsp_assert = false;
    dsp_ctl->dsp_assert_time_out = false;

    if(NULL == dsp_ctl->dsp_sleep_ctl){
        dsp_ctl->dsp_sleep_ctl= mixer_get_ctl_by_name(dev_ctl->mixer, "agdsp_access_en");
    }

    if(NULL == dsp_ctl->codec_dig_access_dis){
        dsp_ctl->codec_dig_access_dis= mixer_get_ctl_by_name(dev_ctl->mixer, "Codec Digital Access Disable");
    }

    if (pthread_mutex_init(&dsp_ctl->lock, NULL) != 0) {
        LOG_E("dsp_ctrl_open pthread_mutex_init,errno:%u,%s",
              errno, strerror(errno));
        ret=-1;
        goto err;
    }

    ret = sem_init(&dsp_ctl->rx.sem, 0, 0);
    if (ret) {
        LOG_E("sem_init falied, code is %s", strerror(errno));
        goto err;
    }

    dsp_ctl->isMiniAPExit = false;
    dsp_ctl->exit_miniap = false;
    pthread_attr_t attr_miniap;
    pthread_attr_init(&attr_miniap);
    pthread_attr_setdetachstate(&attr_miniap, PTHREAD_CREATE_JOINABLE);

    if (pipe(dsp_ctl->pipe_fd) < 0) {
        ALOGE("dsp_ctrl_open create pipe failed");
    }

    ret = pthread_create((pthread_t *)&dsp_ctl->thread_id_miniap, NULL,
            thread_routine_miniap, (void *)dsp_ctl);
    if (ret) {
        LOG_E("thread_routine_miniap create failed %d", ret);
        dsp_ctl->thread_id_miniap = -1;
        dsp_ctl->exit_miniap = true;
    }
    pthread_attr_destroy(&attr_miniap);
    dsp_ctl->rx.is_exit=false;
    dsp_ctl->rx.thread_exit = false;
    dsp_ctl->agdsp_pipd_fd=-1;
#ifndef NORMAL_AUDIO_PLATFORM
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
    ret=pthread_create((pthread_t *)(&dsp_ctl->rx.thread_id), &attr,
            dsp_ctrl_rx_thread_routine, dsp_ctl);
    if (ret) {
        LOG_E("vbc_ctrl_open rx failed %d", ret);
        ret=-3;
        dsp_ctl->rx.thread_id = -1;
        sem_destroy(&dsp_ctl->rx.sem);
        dsp_ctl->rx.is_exit=true;
        dsp_ctl->rx.thread_exit = true;
        goto err;
    }
    pthread_attr_destroy(&attr);
#endif
    ret = sem_init(&dsp_ctl->tx.sem, 0, 0);
    if (ret) {
        LOG_E("sem_init falied, code is %s", strerror(errno));
        goto err;
    }

    if (pthread_mutex_init(&dsp_ctl->tx.lock, NULL) != 0) {
        LOG_E("dsp_ctrl_open pthread_mutex_init tx lock,errno:%u,%s",
              errno, strerror(errno));
        ret=-1;
        goto err;
    }

    dsp_ctl->tx.is_exit=false;
    dsp_ctl->tx.cmd=AUDIO_CTL_INVALID;
#ifndef NORMAL_AUDIO_PLATFORM

    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
    ret=pthread_create((pthread_t *)(&dsp_ctl->tx.thread_id), &attr,
            dsp_ctrl_tx_thread_routine, dsp_ctl);
    if (ret) {
        LOG_E("vbc_ctrl_open tx failed %d", ret);
        dsp_ctl->tx.thread_id = -1;
        ret=-3;
        goto err;
    }
    pthread_attr_destroy(&attr);

    dsp_ctl->fd_dsp_assert_mem = open(SPRD_AUD_DSPASSERT_MEM,O_RDWR);
    if(dsp_ctl->fd_dsp_assert_mem <= 0) {
        LOG_E("dsp_ctl->fd_dsp_assert_mem open failed %d",dsp_ctl->fd_dsp_assert_mem);
    }

    dsp_ctl->fd_dsp_assert_log = open(SPRD_AUD_DSPASSERT_LOG,O_RDWR);
    if(dsp_ctl->fd_dsp_assert_log <= 0) {
        LOG_E("dsp_ctl->fd_dsp_assert_log open failed %d",dsp_ctl->fd_dsp_assert_log);
    }

    dsp_ctl->fd_dsp_assert_pcm = open(SPRD_AUD_DSPASSERT_PCM,O_RDWR);
    if(dsp_ctl->fd_dsp_assert_pcm <= 0) {
        LOG_E("dsp_ctl->fd_dsp_assert_pcm open failed %d",dsp_ctl->fd_dsp_assert_pcm);
    }

    ret = ioctl(dsp_ctl->fd_dsp_assert_mem, DSPLOG_CMD_DSPDUMP_ENABLE,dsp_ctl->dsp_assert_force_notify);
    LOG_I("dsp_ctrl_notify_dspdump notify ret: %d",ret);

    ret = ioctl(dsp_ctl->fd_dsp_assert_mem, DSPLOG_CMD_TIMEOUTDUMP_ENABLE,0);
    LOG_I("dsp_ctrl_notify_dspdump notify ret: %d",ret);

    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
    dsp_ctl->fd_modemd_notify=-1;
    dsp_ctl->modemd.thread_exit = false;
    ret=pthread_create((pthread_t *)(&dsp_ctl->modemd.thread_id), &attr,
    dsp_ctrl_modemd_notify_thread_routine, dsp_ctl);
    if (ret) {
        LOG_E("vbc_ctrl_open rx failed %d", ret);
        ret=-3;
        dsp_ctl->modemd.thread_id = -1;
        dsp_ctl->modemd.thread_exit = true;
        goto err;
    }
#endif
    return dsp_ctl;
err:

    pthread_attr_destroy(&attr);

    if(dsp_ctl!=NULL){
        if(dsp_ctl->fd_dsp_assert_mem>0){
            close(dsp_ctl->fd_dsp_assert_mem);
            dsp_ctl->fd_dsp_assert_mem=-1;
        }

        if(dsp_ctl->fd_dsp_assert_log>0){
            close(dsp_ctl->fd_dsp_assert_log);
            dsp_ctl->fd_dsp_assert_log=-1;
        }

        if(dsp_ctl->fd_dsp_assert_pcm>0){
            close(dsp_ctl->fd_dsp_assert_pcm);
            dsp_ctl->fd_dsp_assert_pcm=-1;
        }

        free(dsp_ctl);
        dsp_ctl=NULL;
    }

    LOG_E("dsp_ctrl_open failed %d", ret);

    return NULL;
}

void dsp_ctrl_close(struct dsp_control_t * dsp_ctl)
{
    int ret = 0;
    char f_exit[]="exit";
    LOG_I("dsp_ctrl_close enter")
    if(dsp_ctl==NULL){
        return;
    }

    dsp_ctl->isMiniAPExit = true;
    LOG_I("dsp_ctrl_close close rx thead:%ld",dsp_ctl->rx.thread_id);
    dsp_ctl->rx.is_exit=true;
    if(dsp_ctl->agdsp_pipd_fd  <= 0) {
        LOG_I("dsp_ctrl_close open pipe failed");
    }
    else {
        ret = ioctl(dsp_ctl->agdsp_pipd_fd, AUDIO_PIPE_WAKEUP, 1);
    }

    if(dsp_ctl->pipe_fd_miniap  <= 0) {
        LOG_I("dsp_ctrl_close open pipe miniap failed");
    } else {
        ret = ioctl(dsp_ctl->pipe_fd_miniap, AUDIO_PIPE_WAKEUP, 1);
    }

    LOG_I("dsp_ctrl_close exit 1");

    if(dsp_ctl->pipe_fd[1]>0)
    write(dsp_ctl->pipe_fd[1], f_exit, sizeof(f_exit));

    if(dsp_ctl->exit_miniap)
       pthread_join(dsp_ctl->thread_id_miniap, NULL);

    if(dsp_ctl->pipe_fd[0]>0) {
       close(dsp_ctl->pipe_fd[0]);
    }
       dsp_ctl->pipe_fd[0] = -1;

    if(dsp_ctl->pipe_fd[1]>0) {
       close(dsp_ctl->pipe_fd[1]);
    }
       dsp_ctl->pipe_fd[1] = -1;

    if(dsp_ctl->rx.thread_exit) {
       pthread_join(dsp_ctl->rx.thread_id, NULL);
       sem_destroy(&dsp_ctl->rx.sem);
       pthread_mutex_destroy(&dsp_ctl->rx.lock);
    }

    LOG_I("dsp_ctrl_close close tx thead:%ld",dsp_ctl->tx.thread_id);
    if(false==dsp_ctl->tx.is_exit){
        dsp_ctl->tx.is_exit=true;
        sem_post(&dsp_ctl->tx.sem);
        pthread_join(dsp_ctl->tx.thread_id, NULL);
        sem_destroy(&dsp_ctl->tx.sem);
    }
    pthread_mutex_destroy(&dsp_ctl->tx.lock);

    LOG_I("dsp_ctrl_close exit 3");
    if(dsp_ctl->modemd.thread_exit) {
       pthread_join(dsp_ctl->modemd.thread_id, NULL);
    }
    pthread_mutex_destroy(&dsp_ctl->modemd.lock);

    LOG_I("dsp_ctrl_close exit 4");
    if(dsp_ctl->agdsp_pipd_fd>0){
        close(dsp_ctl->agdsp_pipd_fd);
        dsp_ctl->agdsp_pipd_fd=-1;
    }

    LOG_I("dsp_ctrl_close exit 5");
    if(dsp_ctl->pipe_fd_miniap>0){
        close(dsp_ctl->pipe_fd_miniap);
        dsp_ctl->pipe_fd_miniap=-1;
    }

    if(dsp_ctl->fd_dsp_assert_mem>0){
        close(dsp_ctl->fd_dsp_assert_mem);
        dsp_ctl->fd_dsp_assert_mem=-1;
    }

    if(dsp_ctl->fd_dsp_assert_log>0){
        close(dsp_ctl->fd_dsp_assert_log);
        dsp_ctl->fd_dsp_assert_log=-1;
    }

    if(dsp_ctl->fd_dsp_assert_pcm>0){
        close(dsp_ctl->fd_dsp_assert_pcm);
        dsp_ctl->fd_dsp_assert_pcm=-1;
    }

    if(dsp_ctl->fd_modemd_notify>0){
        close(dsp_ctl->fd_modemd_notify);
        dsp_ctl->fd_modemd_notify=-1;
    }
    pthread_mutex_destroy(&dsp_ctl->lock);
    free(dsp_ctl);
    LOG_I("dsp_ctrl_close exit");
}
