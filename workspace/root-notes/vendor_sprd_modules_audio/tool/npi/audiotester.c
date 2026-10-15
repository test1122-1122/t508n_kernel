#define LOG_TAG "audio_npi_param"
#include "audiotester_server.h"
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
#include <linux/ioctl.h>
#include <string.h>
#include <audio_diag.h>
#define MAX_DIAG_SIZE  (65535-32)
#define ENG_DIAG_NO_RESPONSE (-255)
extern int audioclient_setParameters(const char *str);
extern int openStreamOut(
        int devices,
        int sample_rate,
        int channels);
extern ssize_t StreamOutwrite(const void *buffer, size_t numBytes);
extern int StreamOutClose(void);
extern int StreamOutsetParameters(const char *str);
extern int openStreamIn(
        int devices,
        int sample_rate,
        int channels);
extern int StreamInClose(void);
extern int StreamInsetParameters(const char *str);
extern int StreamIngetParameters(const char *str, char *rsp);
ssize_t StreamInread( void *buffer, size_t numBytes);

extern void dump_data(char *tag, char *buf, int len);

extern struct eng_audio_ctl engaudio;

struct eng_audiotester_handle {
    pthread_t read_ser_id;
    char *diag_rsp_buffer;
    bool is_exit;
    sem_t read_sem;
    bool connect;
    bool writting;
    bool reading;
    pthread_mutex_t lock;
    bool inited;
    int max_size;
};

struct eng_audiotester_handle audiotester_handle;

static bool get_read_status(struct eng_audiotester_handle *tester_handle){
    bool read_status=false;
    pthread_mutex_lock(&tester_handle->lock);
    read_status=tester_handle->reading;
    pthread_mutex_unlock(&tester_handle->lock);
    return read_status;
}

static void set_read_status(struct eng_audiotester_handle *tester_handle,bool status){
    pthread_mutex_lock(&tester_handle->lock);
    ALOGI("%s status:%d",__func__,status);
    tester_handle->reading=status;
    pthread_mutex_unlock(&tester_handle->lock);
}

static bool get_write_status(struct eng_audiotester_handle *tester_handle){
    bool status=false;
    pthread_mutex_lock(&tester_handle->lock);
    status=tester_handle->writting;
    pthread_mutex_unlock(&tester_handle->lock);
    return status;
}

static void set_write_status(struct eng_audiotester_handle *tester_handle,bool status){
    pthread_mutex_lock(&tester_handle->lock);
    ALOGI("%s status:%d",__func__,status);
    tester_handle->writting=status;
    pthread_mutex_unlock(&tester_handle->lock);
}

static int read_diag_rsp(struct eng_audiotester_handle *tester_handle,FILE *file){
    char diag_buffer[64];
    int byteread=0;
    char *diag_rsp_buffer=NULL;
    int diag_rsp_size=0;
    int req_read=0;

    memset((void *)diag_buffer,0,sizeof(diag_buffer));
    StreamIngetParameters("AudioDiagDataSize",diag_buffer);
    diag_rsp_size=strtoul(diag_buffer+strlen("AudioDiagDataSize="),NULL,0);
    if(diag_rsp_size>tester_handle->max_size){
        ALOGE("read_diag_rsp invalid diag buffer size:%d > max_size:%d",
            diag_rsp_size,tester_handle->max_size);
    }
    ALOGI("%s (%s) diag_rsp_size:%d",__func__,diag_buffer,diag_rsp_size);
    if(diag_rsp_size==0){
        return diag_rsp_size;
    }

    req_read=diag_rsp_size;
    diag_rsp_buffer=tester_handle->diag_rsp_buffer;
    while(req_read>0){
        byteread=StreamInread(diag_rsp_buffer,req_read);
        if(byteread>0){
            req_read-=byteread;
            diag_rsp_buffer+=byteread;
        }else{
            break;
        }
    }

    if(diag_rsp_size-req_read>0){
        if(NULL!=file){
            fwrite(tester_handle->diag_rsp_buffer, 1, diag_rsp_size-req_read, file);
            usleep(20*1000);
        }else{
            audio_diag_write(&engaudio,tester_handle->diag_rsp_buffer,diag_rsp_size-req_read);
        }
    }
    ALOGI("%s audio_diag_write:%d diag_rsp_size:%d exit",__func__,diag_rsp_size-req_read,diag_rsp_size);
    return diag_rsp_size;
}

static void *read_thread(void *args){
    struct eng_audiotester_handle *tester_handle=(struct eng_audiotester_handle *)args;
    FILE *file=NULL;
    int ret=0;
    int no_data_count=0;

    ALOGI("read_thread enter");
    //file = fopen("/data/vendor/local/media/diag_tx.hex", "wb");
    while(tester_handle->is_exit==false){
        set_read_status(tester_handle,false);
        ALOGI("read_thread wait");
        sem_wait(&tester_handle->read_sem);
        ALOGI("read_thread wakeup");
        no_data_count=0;
        set_read_status(tester_handle,true);
        while(get_write_status(tester_handle)==true){
            ret=read_diag_rsp(tester_handle,file);
            if(ret<0){
                ALOGE("%s line:%d read failed:%d",__func__,__LINE__,ret);
                break;
            }else if(ret==0){
                usleep(20*1000);
                no_data_count++;
                if(no_data_count>=50){
                    break;
                }
            }else{
                no_data_count=0;
            }
        }
        //read the last buffer
        usleep(20*1000);
        read_diag_rsp(tester_handle,file);
    }

    if(NULL!=file){
        fclose(file);
        file=NULL;
    }
    ALOGI("read_thread exit");
    
    return NULL;
}

#define MAX_HIDL_BUFFER  20480
static int start_read_thread(struct eng_audiotester_handle *tester_handle)
{
    pthread_attr_t attr;
    char diag_buffer[16];

    int ret;
    ALOGI("%s line:%d",__func__,__LINE__);
    sem_init(&tester_handle->read_sem, 0, 0);

    StreamIngetParameters("AudioDiagMaxDataSize",diag_buffer);
    tester_handle->max_size=strtoul(diag_buffer,NULL,0);
    tester_handle->max_size/=4;
    tester_handle->max_size*=4;//aligned 4bytes

    tester_handle->diag_rsp_buffer=malloc(tester_handle->max_size);
    if(NULL==tester_handle->diag_rsp_buffer){
        ALOGI("%s line:%d",__func__,__LINE__);
        return -1;
    }

    //init inputStream DataQueue  size
    if(tester_handle->max_size>=MAX_HIDL_BUFFER){
        StreamInread(tester_handle->diag_rsp_buffer,MAX_HIDL_BUFFER);
    }else{
        StreamInread(tester_handle->diag_rsp_buffer,tester_handle->max_size);
    }
    tester_handle->writting=false;
    tester_handle->reading=false;

    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    tester_handle->is_exit=false;
    ret = pthread_create(&tester_handle->read_ser_id, &attr, read_thread, tester_handle);
    if (ret < 0) {
        ALOGE("create audio tunning server thread failed");
    }
    pthread_attr_destroy(&attr);
    ALOGI("%s line:%d max_size:%d",__func__,__LINE__,tester_handle->max_size);
    return ret;
}

static void stop_read_thread(struct eng_audiotester_handle *tester_handle)
{
    tester_handle->is_exit=true;
    sem_post(&tester_handle->read_sem);
    ALOGI("%s line:%d",__func__,__LINE__);
    pthread_join(tester_handle->read_ser_id, NULL);
    sem_destroy(&tester_handle->read_sem);
    if(NULL!=tester_handle->diag_rsp_buffer){
        free(tester_handle->diag_rsp_buffer);
        tester_handle->diag_rsp_buffer=NULL;
    }
    ALOGI("%s line:%d",__func__,__LINE__);
    audiotester_handle.inited=false;
}

void audiotester_init(void){
    audiotester_handle.inited=false;
}

static void audiotester_connect(void){
    openStreamIn(0,48000,2);
    openStreamOut(0,48000,2);
    StreamOutsetParameters("hidldiagstream=1");
    StreamInsetParameters("hidldiagstream=1");
    audiotester_handle.inited=true;
    start_read_thread(&audiotester_handle);
}

static void audiotester_disconnect(void){
    StreamOutClose();
    StreamInClose();
    stop_read_thread(&audiotester_handle);
    audiotester_handle.inited=false;
}

int audiotester_fun(char *received_buf, int buf_len, char *rsp,UNUSED_ATTR int rsplen){

    MSG_HEAD_T *msg_head;
    DATA_HEAD_T *data_head;
    char cmd_tmp[32]={0};
    int wait_count=0;
    int ret=0;
    int written=0;
    msg_head = (MSG_HEAD_T *)(rsp + 1);
    data_head = (DATA_HEAD_T *)(rsp + (1+sizeof(MSG_HEAD_T)));
    memcpy((void *)rsp,(const void *)received_buf,(sizeof(MSG_HEAD_T) + sizeof(DATA_HEAD_T)+2));
    data_head->data_state = DATA_STATUS_ERROR;
    msg_head->len = sizeof(MSG_HEAD_T) + sizeof(DATA_HEAD_T);

    ALOGI("%s line:%d subtype:%d seq_num:%d len:%d type:%d"
        ,__func__,__LINE__,
        msg_head->subtype,
        msg_head->seq_num,
        msg_head->len,
        msg_head->type);

    if(buf_len<128){
        dump_data("audiotester_fun", received_buf, buf_len);
    }else{
        dump_data("audiotester_fun", received_buf,64);
    }

    if(audiotester_handle.inited!=true){
        audiotester_connect();
    }

    snprintf(cmd_tmp,sizeof(cmd_tmp),"AudioDiagDataSize=%d",buf_len);
    StreamOutsetParameters(cmd_tmp);

    if(CONNECT_AUDIOTESTER==msg_head->subtype){
        StreamOutwrite(received_buf,buf_len);
    }else if(DIS_CONNECT_AUDIOTESTER==msg_head->subtype){
        StreamOutwrite(received_buf,buf_len);
        audiotester_disconnect();
    }else{
        set_write_status(&audiotester_handle,true);
        sem_post(&audiotester_handle.read_sem);
        written=0;
        do{
            ret=StreamOutwrite(received_buf+written,buf_len-written);
            ALOGI("StreamOutwrite size:%d written:%d ret:%d",buf_len-written,written,ret);
            if(ret>0){
                written+=ret;
            }else{
                ALOGE("StreamOutwrite size:%d written:%d Failed",buf_len-written,written);
                audiotester_disconnect();
                return ENG_DIAG_NO_RESPONSE;
            }
        }while(written<buf_len);
        set_write_status(&audiotester_handle,false);

        while(get_read_status(&audiotester_handle)==true){
            usleep(20*1000);
            if(wait_count++>=100){
                ALOGE("audiotester_fun wait read status failed");
                return 0;
            }else{
                ALOGI("audiotester_fun cmd:%d wait count:%d",
                    msg_head->subtype,wait_count);
            }
        }
        return ENG_DIAG_NO_RESPONSE;
    }
    return ENG_DIAG_NO_RESPONSE;
}
