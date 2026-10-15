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

#define LOG_TAG "audio_hw_tester"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <stdint.h>
#include <sys/types.h>
#include <semaphore.h>
#include <pthread.h>
#ifdef AUDIOHAL_V4
#include <log/log.h>
#else
#include <cutils/log.h>
#endif
#include "audio_hw.h"
#include "audio_param/audio_param.h"
#include <cutils/properties.h>

#include "audiotester_server.h"

extern int save_audio_param(AUDIO_PARAM_T *audio_param,int profile, bool isram);
int handle_audio_cmd_data(AUDIO_PARAM_T *audio_param,
                          uint8_t *buf, int len, int sub_type, int data_state);
extern int connect_audiotester_process(struct socket_handle *tunning_handle,int sockfd,int max_buffer_size,void *fun);
extern int disconnect_audiotester_process(struct socket_handle *tunning_handle);

int check_diag_header_and_tail(uint8_t *received_buf, int rev_len)
{
    uint8_t diag_header;
    uint8_t diag_tail;
    diag_header = received_buf[0];
    diag_tail = received_buf[rev_len - 1];
    if (diag_header != 0x7e || diag_tail != 0x7e) {
        LOG_E("the diag header or tail is wrong  header=%x, tail=%x rev_len:%x\n", diag_header,
              diag_tail,rev_len);
        return -1;
    }
    return 0;
}


/*
return -1: diag err
return 0: diag ok
return 1:need next package

typedef enum {
    DATA_SINGLE = 1,
    DATA_START,
    DATA_END,
    DATA_MIDDLE,
    DATA_STATUS_OK,
    DATA_STATUS_ERROR,
} AUDIO_DATA_STATE;
*/


int is_sprd_full_diag_check(struct socket_handle * tunning,uint8_t *rx_buf_ptr, int rx_buf_len)
{
    AUDIO_MSG_HEAD_T *msg_head;
    uint8_t *msg_start=NULL;
    uint8_t *cmd_buf_ptr=tunning->audio_cmd_buf;

    static int size=0;
    int ret=0;

    if(rx_buf_ptr[0]==0x7e){
        msg_start=(uint8_t *)(rx_buf_ptr + 1);
        msg_head = (AUDIO_MSG_HEAD_T *)(msg_start);
        size=msg_head->len+2;
        if((rx_buf_len>=msg_head->len) && (0x7e==*(msg_start+msg_head->len))){
            LOG_D("is_sprd_full_diag_check one pack size:%x rx_buf_len:%x",size,rx_buf_len);
            size=0;
            ret=0;
        }else{
            LOG_D("is_sprd_full_diag_check mutl pack size:%x",size);
            ret=1;
        }
        tunning->rx_packet_len=0;
        memcpy((void *)&cmd_buf_ptr[tunning->rx_packet_len], rx_buf_ptr, rx_buf_len);
        tunning->rx_packet_len+=rx_buf_len;
    }else{
        if(size<tunning->rx_packet_len+rx_buf_len){
            LOG_E("is_sprd_full_diag_check data err size:%d rx_packet_len:%d len:%d",
                size,tunning->rx_packet_len,rx_buf_len);
            ret=-1;
        }else{
            if(0x7e==*(rx_buf_ptr+rx_buf_len-1)){
                memcpy((void *)&cmd_buf_ptr[tunning->rx_packet_len], rx_buf_ptr, rx_buf_len);
                tunning->rx_packet_len+=rx_buf_len;
                ret=0;
                LOG_D("is_sprd_full_diag_check pack end");
            }else{
                memcpy((void *)&cmd_buf_ptr[tunning->rx_packet_len], rx_buf_ptr, rx_buf_len);
                tunning->rx_packet_len+=rx_buf_len;
                LOG_D("is_sprd_full_diag_check pack continue");
                ret=1;
            }
        }
    }

    if(ret<0){
        tunning->rx_packet_len=0;
        size=0;
    }
    return ret;
}

int is_full_diag_check(struct socket_handle * tunning,uint8_t *rx_buf_ptr, int rx_buf_len,
                       uint8_t *cmd_buf_ptr,  int *cmd_len)
{
    int rtn;
    uint8_t packet_header = rx_buf_ptr[0];
    uint16_t packet_len = (rx_buf_ptr[6] << 0x08) | (rx_buf_ptr[5]);
    uint8_t packet_end = rx_buf_ptr[rx_buf_len - 1];
    int cmd_buf_offset = tunning->rx_packet_len;

    rtn = 0;
    *cmd_len = rx_buf_len;
    LOG_D("is_full_diag_check %p %p",rx_buf_ptr,cmd_buf_ptr);
    if ((0x7e == packet_header)
        && (0x00 == tunning->rx_packet_total_len)
        && (0x00 == tunning->rx_packet_len)) {
        tunning->rx_packet_total_len = packet_len + 2;
    }

    if ((0x7e == packet_header)
        && (0x7e == packet_end)) {
        /* one packet */
        LOG_D("%s %d", __func__, __LINE__);
    } else { /* mul packet */
        tunning->rx_packet_len += rx_buf_len;
        LOG_D("%s %d", __func__, __LINE__);
        if ((0x7e == packet_end)
            && (tunning->rx_packet_len == tunning->rx_packet_total_len)) {
            *cmd_len = tunning->rx_packet_len;
            LOG_D("%s %d", __func__, __LINE__);
        } else {
            LOG_D("%s %d not find end: offset:0x%x len:0x%x packet_size:0x%x",
                  __func__, __LINE__, cmd_buf_offset, rx_buf_len, tunning->rx_packet_total_len);
            rtn = 1;
        }
    }

    if(rx_buf_len+cmd_buf_offset>AUDIO_COMMAND_BUF_SIZE-1){
        LOG_E("receive buffer is too long:cur_offset:0x%x size:0x%x",cmd_buf_offset,rx_buf_len);
        return -1;
    }

    memcpy((void *)&cmd_buf_ptr[cmd_buf_offset], rx_buf_ptr, rx_buf_len);

    if (0 == rtn) {
        tunning->rx_packet_len = 0x00;
        tunning->rx_packet_total_len = 0x00;
        LOG_I("%s %d", __func__, __LINE__);
    }

    return rtn;
}

int handle_received_data(void *param, uint8_t *received_buf,int rev_len)
{
    int ret, data_len;
    AUDIO_MSG_HEAD_T *msg_head;
    DATA_HEAD_T *data_command;
    uint8_t *audio_data_buf;
    AUDIO_PARAM_T  *audio_param=(AUDIO_PARAM_T  *)(param);

    LOG_I("%02x %02x %02x %02x %02x %02x %02x %02x",
    received_buf[0],
    received_buf[1],
    received_buf[2],
    received_buf[3],
    received_buf[4],
    received_buf[5],
    received_buf[6],
    received_buf[7]);

    LOG_I("%02x %02x %02x %02x %02x %02x %02x %02x",
    received_buf[8],
    received_buf[9],
    received_buf[10],
    received_buf[11],
    received_buf[12],
    received_buf[13],
    received_buf[14],
    received_buf[15]);

    ret = check_diag_header_and_tail(received_buf, rev_len);
    if (ret != 0) {
        LOG_E("Diag Header or Tail error");
        ret=-3;
        return ret;
    }
    msg_head = (AUDIO_MSG_HEAD_T *)(received_buf + 1);

    LOG_I("seq:%08x len:%04x type:%02x subtype:%02x",
          msg_head->seq_num, msg_head->len, msg_head->type, msg_head->subtype);
    //0x99 is audio tunning specification define
    if (msg_head->type != AUDIO_CMD_TYPE) {
        LOG_E("the audio data's type is not equal 0X99, type=%d\n", msg_head->type);
        return -1;
    }

    if(received_buf[1 + msg_head->len] != 0x7e) {
        LOG_E("date format err:%x", received_buf[1 + msg_head->len]);
        return -2;
    }

    data_len = msg_head->len - sizeof(AUDIO_MSG_HEAD_T) - sizeof(DATA_HEAD_T);
    data_command = (DATA_HEAD_T *)(received_buf + sizeof(AUDIO_MSG_HEAD_T) + 1);
    audio_data_buf = received_buf + sizeof(AUDIO_MSG_HEAD_T) + sizeof(DATA_HEAD_T) + 1;

    ret = handle_audio_cmd_data(audio_param, audio_data_buf,
                                data_len, msg_head->subtype, data_command->data_state);

    LOG_I("handle_received_data exit");
    return ret;
}

int audiotester_connected(AUDIO_PARAM_T *audio_param,int socket_fd){
    int fd=-1;
    int max_buffer_size=0;
    bool is_ready=false;

    struct audiotester_config_handle *audiotester_config=
       &audio_param->dev_ctl->config.audiotester_config;

    if(socket_fd>0){
        max_buffer_size=audiotester_config->DiagBufferSize;
    }else{
        max_buffer_size=audiotester_config->SocketBufferSize;
    }

    fd= open(AUDIO_PARAM_TUNNING_FILE, O_RDWR | O_CREAT,
                  S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);

    if(fd<0){
        LOG_E("audiotester_connected create:%sfailed",AUDIO_PARAM_TUNNING_FILE);
        return -1;
    }
    close(fd);

    if(MAX_SOCKT_LEN<max_buffer_size){
        max_buffer_size=MAX_SOCKT_LEN;
    }

    parse_audioparam_config(&audio_param->dev_ctl->config,true);

    do{
        is_ready=is_all_audio_param_ready(audio_param);
        usleep(200*1000);
        LOG_I("audiotester_connected Wait audio param is ready");
    }while(is_ready==false);
    clear_audio_param_load_status(audio_param->dev_ctl);
    init_sprd_audio_param(audio_param,true);

    if((connect_audiotester_process(&(audio_param->tunning),socket_fd,max_buffer_size,handle_received_data)<0)){
        LOG_E("audiotester_connected failed\n");
        disconnect_audiotester_process(&(audio_param->tunning));
        return -1;
    }

    return 0;
}

int audiotester_disconnected(AUDIO_PARAM_T *audio_param){
    int i=0;
    for(i=0;i<SND_AUDIO_PARAM_PROFILE_MAX;i++){
        release_xml_handle(&(audio_param->param[i].xml));
    }

    clear_audio_param_load_status(audio_param->dev_ctl);
    init_sprd_audio_param(audio_param,true);

    for(i=0;i<SND_AUDIO_PARAM_PROFILE_MAX;i++){
        release_xml_handle(&(audio_param->param[i].xml));
    }

    release_xml_handle(&audio_param->tunning.audio_config_xml);

    disconnect_audiotester_process(&(audio_param->tunning));
    audio_param->tunning.res=NULL;

    return 0;
}

void *tunning_thread(void *args)
{
    AUDIO_PARAM_T *audio_param=(AUDIO_PARAM_T *)args;
    struct socket_handle *tunning=(struct socket_handle*)(&audio_param->tunning);

    int ret, rev_len;
    int sock_fd = tunning->sockfd;
    fd_set rfds;
    struct timeval tv;
    LOG_E("%s", __func__);

    while(true==tunning->wire_connected) {

        FD_ZERO(&rfds);
        FD_SET(sock_fd, &rfds);

        tv.tv_sec = 2; //wait 2 seconds
        tv.tv_usec = 0;
        FD_SET(sock_fd, &rfds);
       // ret = select(sock_fd + 1, &rfds, NULL, NULL, &tv);
        ret = select(sock_fd + 1, &rfds, NULL, NULL, NULL);
        if (ret <= 0) {
            LOG_D("havn't receive any data for 2 seconds from AudioTester:0x%x",sock_fd);
            continue;
        }

        if(FD_ISSET(sock_fd,&rfds)){
            rev_len = recv(sock_fd, tunning->audio_received_buf, tunning->max_len,
                           MSG_DONTWAIT);

            if (rev_len <= 0) {
                LOG_E("communicate with AudioTester error:%d",rev_len);
                break;
            }
            LOG_D("tunning_thread recv len:%d", rev_len);

            ret = is_sprd_full_diag_check(tunning,tunning->audio_received_buf, rev_len);
            if (ret == 0) {
                ret=tunning->process(audio_param, tunning->audio_cmd_buf,tunning->rx_packet_len);
                if(ret==-3){
                    LOG_D("tunning_thread recv err len:%d", tunning->rx_packet_len);
                    dump_data((char *)tunning->audio_cmd_buf,tunning->rx_packet_len);
                }
                tunning->rx_packet_len=0;
            }else if(ret<0){
                /*
                if(NULL!=tunning->debug){
                    LOG_E("tunning_thread rec data err:%x %x dump",tunning->rx_packet_len,rev_len);
                    if(tunning->debug->rx_debug.err_fd>0){
                        ret = write(tunning->debug->rx_debug.err_fd,tunning->audio_cmd_buf, tunning->rx_packet_len);

                        ret = write(tunning->debug->rx_debug.err_fd, tunning->audio_received_buf, rev_len);
                    }
                }
                */
                tunning->rx_packet_len=0;
            }else{
                sleep(2);
            }
        }
    }
    return NULL;
}


int audiotester_exit(AUDIO_PARAM_T *audio_param){
    int i=0;
    for(i=0;i<SND_AUDIO_PARAM_PROFILE_MAX;i++){
        release_xml_handle(&(audio_param->param[i].xml));
    }

    release_xml_handle(&audio_param->tunning.audio_config_xml);

    disconnect_audiotester_process(&(audio_param->tunning));
    audio_param->tunning.res=NULL;

    return 0;
}

static void *listen_thread(void *args)
{
    int ser_fd, tunning_fd, n, ret;
    struct sockaddr_in sock_addr;
    struct sockaddr cli_addr;
    socklen_t addrlen;
    pthread_t tunning_t;
    pthread_attr_t attr;
    AUDIO_PARAM_T *audio_param=(AUDIO_PARAM_T *)args;

    memset(&sock_addr, 0, sizeof (struct sockaddr_in));
    sock_addr.sin_family = AF_INET;
    sock_addr.sin_addr.s_addr = INADDR_ANY;
    sock_addr.sin_port = htons(AUDIO_TUNNING_PORT);
    LOG_E("listen_thread line:%d\n",__LINE__);

    ser_fd = socket(sock_addr.sin_family, SOCK_STREAM, 0);
    LOG_E("listen_thread line:%d\n",__LINE__);

    if (ser_fd < 0) {
        LOG_E("can not create tunning server thread\n");
        return NULL;
    }
    LOG_E("listen_thread line:%d\n",__LINE__);

    n=1;
    if (setsockopt(ser_fd, SOL_SOCKET, SO_REUSEADDR, &n, sizeof(n)) != 0) {
        LOG_E("setsockopt SO_REUSEADDR error:%s\n",strerror(errno));
        close(ser_fd);
        return NULL;
    }
    LOG_E("listen_thread line:%d\n",__LINE__);

    if (bind(ser_fd, (struct sockaddr *)&sock_addr,
             sizeof (struct sockaddr_in)) != 0) {
        LOG_E("bind server error:%s\n",strerror(errno));
        close(ser_fd);
        return NULL;
    }
    LOG_E("listen_thread line:%d\n",__LINE__);

    if (listen(ser_fd, 10) < 0) {
        LOG_E("tunning server listen failed error:%s\n",strerror(errno));
        close(ser_fd);
        return NULL;
    }
    pthread_attr_init(&attr);
    LOG_I("listen_thread line:%d\n",__LINE__);

    audio_param->tunning.running=true;
    tunning_fd=-1;
    while (audio_param->tunning.running) {
        void *res;
        addrlen = sizeof(struct sockaddr);
        LOG_D("waiting audiotester connect\n");
        tunning_fd = accept(ser_fd, &cli_addr, &addrlen);
        LOG_I("listen_thread line:%d\n",__LINE__);

        if(tunning_fd<0){
            LOG_I("accept tunning client failed:%s\n",strerror(errno));
            break;
        }

        if(audio_param->tunning.running==false){
            LOG_I("listen_thread break");
            if (tunning_fd > 0) {
                close(tunning_fd);
            }
            audiotester_exit(audio_param);
            break;
        }

        if(audiotester_connected(audio_param,tunning_fd)!=0){
            LOG_E("connect_audiotester_process failed\n");
            if (tunning_fd >= 0) {
                close(tunning_fd);
            }
            continue;
        }
        //this is prevent process be killed by SIGPIPE
        signal(SIGPIPE, SIG_IGN);

        ret = pthread_create(&tunning_t, &attr, tunning_thread, audio_param);
        if (ret < 0) {
            LOG_E("create tunning thread failed");
            if (tunning_fd >= 0) {
                close(tunning_fd);
            }
            break;
        }
        pthread_join(tunning_t , &res);
        if (tunning_fd >= 0) {
            close(tunning_fd);
        }

        audiotester_disconnected(audio_param);
    }
    pthread_attr_destroy(&attr);
    if (ser_fd) {
        close(ser_fd);
    }
    LOG_I("listen_thread line:%d exit\n",__LINE__);
    pthread_detach(pthread_self());

    return NULL;
}

void start_audio_tunning_server(struct tiny_audio_device *adev)
{
    pthread_t ser_id;
    pthread_attr_t attr;
    int ret;
    struct socket_handle *tunning=&adev->audio_param.tunning;
    LOG_I("start_audio_tunning_server enter\n");

    if(adev->audio_param.tunning.running==true){
        LOG_W("AudioTester aready running");
        return;
    }
    tunning->readthreadwait=false;
    sem_init(&tunning->sem_wakeup_readthread, 0, 0);

    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    ret = pthread_create(&ser_id, &attr, listen_thread, &adev->audio_param);
    if (ret < 0) {
        LOG_E("create audio tunning server thread failed");
        pthread_attr_destroy(&attr);
        return;
    }
    pthread_attr_destroy(&attr);
    LOG_I("start_audio_tunning_server exit\n");
}


void stop_audio_tunning_server(struct tiny_audio_device *adev)
{
    int socket_fd=-1;
    struct sockaddr_in s_add;
    struct socket_handle *tunning=&adev->audio_param.tunning;

    int index = 0;
    AUDIO_MSG_HEAD_T m_head;
    DATA_HEAD_T data_head;
    char data_buf[64];

    memset(&m_head,0,sizeof(AUDIO_MSG_HEAD_T));
    memset(&data_head,0,sizeof(DATA_HEAD_T));

    LOG_I("stop_audio_tunning_server enter");

    tunning->running=false;

    socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if(-1 == socket_fd){
        LOG_W("%s create socket failed",__func__);
        return ;
    }

    memset(&s_add,0,sizeof(struct sockaddr_in));
    s_add.sin_family=AF_INET;
    s_add.sin_addr.s_addr= INADDR_ANY;
    s_add.sin_port=htons(AUDIO_TUNNING_PORT);


    if(-1 == connect(socket_fd,(struct sockaddr *)(&s_add), sizeof(struct sockaddr))){
        LOG_W("%s connect socket failed",__func__);
        close(socket_fd);
        return ;
    }

    *(data_buf + index) = 0x7e;
    index += 1;
    m_head.seq_num = 0;
    m_head.type = 0x99;//spec
    m_head.subtype = DIS_CONNECT_AUDIOTESTER;//spec

    m_head.len = sizeof(AUDIO_MSG_HEAD_T) + sizeof(DATA_HEAD_T);
    memcpy(data_buf + index, &m_head, sizeof(AUDIO_MSG_HEAD_T));
    index += sizeof(AUDIO_MSG_HEAD_T);

    data_head.data_state = DATA_STATUS_OK;
    memcpy(data_buf + index, &data_head, sizeof(DATA_HEAD_T));
    index += sizeof(DATA_HEAD_T);
    *(data_buf + index) = 0x7e;
    index += 1;

    write(socket_fd, data_buf, index);
    close(socket_fd);
    LOG_I("stop_audio_tunning_server exit");
}
