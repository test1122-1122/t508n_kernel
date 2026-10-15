/*******************************************************************
**  File Name: vc_main.c                                         
**  Author: Fei Dong (VA-Sys team)                                        
**  Date: 22/03/2016                                       
**  Copyright: Spreadtrum, Incorporated. All Rights Reserved.
**  Description: ISC_R0_Cali
**                   Visual Studio Main
********************************************************************
********************************************************************
**  Edit History                                                   
**-----------------------------------------------------------------
**  DATE            NAME        DESCRIPTION                    
**  25/10/2018 Fei Dong  1st alpha version      

*******************************************************************/
#define LOG_TAG "audio_hw_SmartAmp"

#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <math.h>
#include <string.h>
#include <unistd.h>

#include <sys/errno.h>
#include <errno.h>
#include "smartamp.h"
#include "audio_debug.h"
#include "audio_param/audio_param.h"
#include "audio_control.h"

int read_smartamp_cali_param(struct smart_amp_cali_param *cali_values){
    int ret=0;
    FILE *file;

    file = fopen(AUDIO_SMARTAMP_CALI_PARAM_FILE, "r");
    if(NULL==file){
        LOG_I("open %s failed err:%s"
            ,AUDIO_SMARTAMP_CALI_PARAM_FILE,strerror(errno));
        return 0;
    }

    fseek(file,0,SEEK_SET);

    ret = fread(cali_values, 1,sizeof(cali_values),file);
    if(ret!=sizeof(cali_values)){
        LOG_W("read_smartamp_cali_param failed ret:%d err:%s",
            ret,strerror(errno));
    }
    fclose(file);
    return ret;
}

int write_smartamp_cali_param(struct smart_amp_cali_param *cali_out){
    int ret=0;
    FILE *file;
    file = fopen(AUDIO_SMARTAMP_CALI_PARAM_FILE, "wb");
    if(NULL==file){
        LOG_E("set_smartamp_cali_param open %s failed err:%s"
            ,AUDIO_SMARTAMP_CALI_PARAM_FILE,strerror(errno));
        return 0;
    }
    fseek(file,0,SEEK_SET);

    ret = fwrite(cali_out, 1,sizeof(struct smart_amp_cali_param), file);
    if(ret<=0){
        LOG_W("set_smartamp_cali_param failed ret:%d err:%s"
            ,ret,strerror(errno));
    }

    fclose(file);
    return ret;
}

int read_smart_firmware(void **param){
    struct vbc_fw_header header;
    int bytesRead=0;
    FILE *file;
    char *buffer=NULL;

    file = fopen("/vendor/firmware/dsp_smartamp", "r");
    if(NULL==file){
        LOG_E("open /vendor/firmware/dsp_smartamp failed err:%s",
            strerror(errno));
        return -1;
    }
    fseek(file,0,SEEK_SET);

    bytesRead = fread(&header, 1, sizeof(header), file);
    if(bytesRead<=0){
        LOG_W("read vbc_fw_header failed bytesRead:%d err:%s"
            ,bytesRead,strerror(errno));
        fclose(file);
        return -2;

    }

    LOG_I("%s mode:%d size:%d",__func__,header.len,header.num_mode);

    buffer=(char *)malloc(header.len*header.num_mode+sizeof(header));
    if(NULL==buffer){
        LOG_W("malloc buffer failed");
        fclose(file);
        return -3;
    }

    memcpy(buffer,&header,sizeof(header));

    bytesRead = fread(buffer+sizeof(header),1,header.len*header.num_mode, file);
    if(bytesRead!=header.len*header.num_mode){
        LOG_W("read config file failed bytesRead:%d",bytesRead);
        free(buffer);
        fclose(file);
        return -4;
    }

    fclose(file);
    *param=buffer;

    return header.len*header.num_mode+sizeof(header);
}

void free_smart_firmware_buffer(void *param){
    free(param);
}

static bool smartamp_cali_calculation(int16 R0_max_offset,int16 R0_normal_offset,
    int R0_dc,int16 Re_T_cali, struct smart_amp_cali_param *cali_out){

    int16  R0_maxup_thrd = 0,R0_maxdown_thrd=0,R0_normalup_thrd=0,R0_normaldown_thrd=0;

    R0_maxup_thrd = (short)(((long int) Re_T_cali)*((1<<10)+R0_max_offset)>>10) ;//R0_max_offset,Q10
    R0_maxdown_thrd =(short)(((long int) Re_T_cali)*( (1<<10)-R0_max_offset)>>10) ; //R0_max_offset,Q10
    R0_normalup_thrd =(short)(((long int) Re_T_cali)*( (1<<10)+R0_normal_offset)>>10) ;//R0_max_offset,Q10
    R0_normaldown_thrd =(short)(((long int) Re_T_cali)*( (1<<10)-R0_normal_offset)>>10);//R0_max_offset,Q10

    cali_out->postfilter_flag=0;
    cali_out->Re_T_cali_out=0;
    if((R0_dc>R0_maxup_thrd)||(R0_dc < R0_maxdown_thrd)) {
        cali_out->Re_T_cali_out = Re_T_cali;
        LOG_W("R0 out of range,calibration fail");
     }else{
        if((R0_dc>R0_normalup_thrd)){
            cali_out->Re_T_cali_out  = R0_normalup_thrd;
            cali_out->postfilter_flag = 1;
        }else{
            cali_out->Re_T_cali_out = R0_dc;
        }
        LOG_I("calibration success R0_init=%f,R0_cali=%f,postfilter_flag = %d \n",
        (float) Re_T_cali/1024, (float) cali_out->Re_T_cali_out/1024, cali_out->postfilter_flag);
        return true;
    }
    return false;

}

int get_smartapm_cali_values(struct smart_amp_ctl *smartamp_ctl,struct smart_amp_cali_param *cali_values){
    int ret=0;
    pthread_mutex_lock(&smartamp_ctl->lock);
    if(true==smartamp_ctl->smart_cali_failed){
        LOG_I("get_smartapm_cali_values smartapm cali failed");
        pthread_mutex_unlock(&smartamp_ctl->lock);
        return -1;
    }

    if(access(AUDIO_SMARTAMP_CALI_PARAM_FILE, R_OK)==0){
        ret= read_smartamp_cali_param(cali_values);
    }else{
        LOG_I("get_smartapm_cali_values access :%s failed",AUDIO_SMARTAMP_CALI_PARAM_FILE);
    }
    pthread_mutex_unlock(&smartamp_ctl->lock);
    return ret;
}

bool is_support_smartamp(struct smart_amp_ctl *smartamp_ctl){
    bool ret=false;
    pthread_mutex_lock(&smartamp_ctl->lock);
    if(((SND_AUDIO_FF_SMARTAMP_MODE==smartamp_ctl->smartamp_support_mode)
        ||(SND_AUDIO_FB_SMARTAMP_MODE==smartamp_ctl->smartamp_support_mode))
        &&(0!=smartamp_ctl->smartamp_usecase)){
        ret=true;
    }else{
        ret=false;
    }
    pthread_mutex_unlock(&smartamp_ctl->lock);
    return ret;
}

bool is_support_fbsmartamp(struct smart_amp_ctl *smartamp_ctl){
    bool ret=false;
    pthread_mutex_lock(&smartamp_ctl->lock);
    LOG_D("is_support_fbsmartamp usecase:%d %d %d",smartamp_ctl->smartamp_usecase,
        smartamp_ctl->smart_cali_failed,smartamp_ctl->calibrating);
    if((SND_AUDIO_FB_SMARTAMP_MODE==smartamp_ctl->smartamp_support_mode)
            &&(0!=smartamp_ctl->smartamp_usecase)){
        if((true==smartamp_ctl->smart_cali_failed)
            &&(false==smartamp_ctl->calibrating)){
            ret=false;
            LOG_I("FbSmartamp is not Calibrated");
        }else{
            ret=true;
        }
    }
    pthread_mutex_unlock(&smartamp_ctl->lock);
    return ret;
}

bool is_support_smartamp_calibrate(struct smart_amp_ctl *smartamp_ctl){
    bool ret=false;
    pthread_mutex_lock(&smartamp_ctl->lock);
    if((SND_AUDIO_FB_SMARTAMP_MODE==smartamp_ctl->smartamp_support_mode)
            &&(0!=smartamp_ctl->smartamp_usecase)){
        ret=true;
    }
    pthread_mutex_unlock(&smartamp_ctl->lock);
    return ret;
}

void set_smartamp_cali_offset(struct smart_amp_ctl *smartamp_ctl,int q10_max, int q10_normal){
    pthread_mutex_lock(&smartamp_ctl->lock);
    smartamp_ctl->R0_max_offset=(int16)q10_max;
    smartamp_ctl->R0_normal_offset=(int16)q10_normal;
    LOG_I("set_smartamp_cali_offset q10_max:%u q10_normal:%u",
        smartamp_ctl->R0_max_offset,smartamp_ctl->R0_normal_offset);
    pthread_mutex_unlock(&smartamp_ctl->lock);
}

void set_smartamp_support_usecase(struct smart_amp_ctl *smartamp_ctl,int usecase,int mode){
    pthread_mutex_lock(&smartamp_ctl->lock);
    LOG_I("set_smartamp_support_usecase enter usecase:%x mode:%d",
        usecase,mode);

    if((SND_AUDIO_FF_SMARTAMP_MODE==mode)||(SND_AUDIO_FB_SMARTAMP_MODE==mode)){
        smartamp_ctl->smartamp_usecase=usecase;
        smartamp_ctl->smartamp_support_mode=mode;
    }else{
        smartamp_ctl->smartamp_usecase=0;
        smartamp_ctl->smartamp_support_mode=SND_AUDIO_UNSUPPORT_SMARTAMP_MODE;
    }
    LOG_I("set_smartamp_support_usecase usecase:%x support mode:%d",
        smartamp_ctl->smartamp_usecase,smartamp_ctl->smartamp_support_mode);
    pthread_mutex_unlock(&smartamp_ctl->lock);
}

int get_smartamp_support_usecase(struct smart_amp_ctl *smartamp_ctl){
    int usecase=0;

    pthread_mutex_lock(&smartamp_ctl->lock);
    usecase=smartamp_ctl->smartamp_usecase;
    pthread_mutex_unlock(&smartamp_ctl->lock);
    return usecase;
}

bool smartamp_cali_process(struct smart_amp_ctl *smartamp_ctl,struct smart_amp_cali_param *cali_out){
    bool result=false;

    pthread_mutex_lock(&smartamp_ctl->lock);

    LOG_I("smartamp_cali_process offset:%u %u R0_dc:%u Re_T_cali:%u",smartamp_ctl->R0_max_offset,smartamp_ctl->R0_normal_offset,
        smartamp_ctl->R0_dc,smartamp_ctl->Re_T_cali);

    if((0xffff==smartamp_ctl->R0_dc)&&(0xff==smartamp_ctl->Re_T_cali)){
        LOG_W("smartamp_cali_process invalid param");
        smartamp_ctl->smart_cali_failed=true;
    }else{
        result=smartamp_cali_calculation(smartamp_ctl->R0_max_offset,
            smartamp_ctl->R0_normal_offset,
            smartamp_ctl->R0_dc,
            smartamp_ctl->Re_T_cali,cali_out);

        smartamp_ctl->smart_cali_failed=!result;
    }
    pthread_mutex_unlock(&smartamp_ctl->lock);
    return result;
}

void dump_smartamp(int fd,char* buffer,int buffer_size,void *ctl){
    struct smart_amp_ctl *smartamp_ctl=(struct smart_amp_ctl *)ctl;
    if(SND_AUDIO_UNSUPPORT_SMARTAMP_MODE!=smartamp_ctl->smartamp_support_mode){
        snprintf(buffer,(buffer_size-1),
            "smartamp dump: "
            "R0_dc:0x%x Re_T_cali:0x%x\n"
            "R0_max_offset:0x%x R0_normal_offset:0x%x\n"
            "usecase:0x%x support_mode:0x%x cali_failed:0x%x\n",
            smartamp_ctl->R0_dc,
            smartamp_ctl->Re_T_cali,
            smartamp_ctl->R0_max_offset,
            smartamp_ctl->R0_normal_offset,
            smartamp_ctl->smartamp_usecase,
            smartamp_ctl->smartamp_support_mode,
            smartamp_ctl->smart_cali_failed);
        AUDIO_DUMP_WRITE_STR(fd,buffer);
        memset(buffer,0,sizeof(buffer));
    }else{
        snprintf(buffer,(buffer_size-1),
            "smartamp dump: unsupport smartamp\n");
        AUDIO_DUMP_WRITE_STR(fd,buffer);
        memset(buffer,0,sizeof(buffer));
    }
}

int init_smartamp(struct smart_amp_ctl *smartamp_ctl){
    int ret=0;

    pthread_condattr_t attr;
    pthread_condattr_init(&attr);
    pthread_condattr_setclock(&attr, CLOCK_MONOTONIC);

    if(pthread_cond_init(&smartamp_ctl->cond, (const pthread_condattr_t *)&attr)) {
        LOG_E("%s:%d: cond init failed\n", __func__, __LINE__);
    }

    pthread_mutex_init(&smartamp_ctl->lock, NULL);

    smartamp_ctl->smartamp_support_mode=SND_AUDIO_UNSUPPORT_SMARTAMP_MODE;
    smartamp_ctl->calibrating=false;
    smartamp_ctl->smartamp_usecase=0;

    smartamp_ctl->iv_enable =false;
    smartamp_ctl->smartamp_func_enable =false;

    if(access(AUDIO_SMARTAMP_CALI_PARAM_FILE, R_OK)!=0){
        LOG_I("init_smartamp access:%s failed",AUDIO_SMARTAMP_CALI_PARAM_FILE);
        smartamp_ctl->smart_cali_failed=true;
    }else{
        smartamp_ctl->smart_cali_failed=false;
    }
    return ret;
}

void set_smartamp_calibrating(struct smart_amp_ctl *smartamp_ctl,bool on){
    pthread_mutex_lock(&smartamp_ctl->lock);
    smartamp_ctl->calibrating=on;
    pthread_mutex_unlock(&smartamp_ctl->lock);
}

bool get_smartamp_cali_iv(void *dsp_ctl,struct smart_amp_ctl *smartamp_ctl,int timeout_ms){
    struct dsp_control_t *agdsp_ctl=(struct dsp_control_t *)dsp_ctl;
    bool result=false;
    struct timespec ts= {0, 0};
    int ret=0;
    struct dsp_smsg _msg;
    _msg.channel=2;
    _msg.command=AGDSP_CMD_SMARTAMP_CALI;
    _msg.parameter0=AGDSP_CMD_SMARTAMP_CALI+1;
    _msg.parameter1=AGDSP_CMD_SMARTAMP_CALI+2;
    _msg.parameter2=AGDSP_CMD_SMARTAMP_CALI+3;

    pthread_mutex_lock(&smartamp_ctl->lock);

    LOG_I("wait_smartamp_cali_rsp Start timeout_ms:%d",timeout_ms);

    clock_gettime(CLOCK_MONOTONIC, &ts);
    LOG_I("get_smartamp_cali_iv CLOCK_MONOTONIC tv_nsec:%ld tv_sec:%ld",ts.tv_nsec,ts.tv_sec);

    agdsp_send_msg(agdsp_ctl,&_msg);

    ts.tv_nsec += (timeout_ms%1000) * 1000000;
    ts.tv_sec += timeout_ms/1000;
    if(ts.tv_nsec >=1000000000) {
        ts.tv_nsec -= 1000000000;
        ts.tv_sec += 1;
    }

    do{
        LOG_I("get_smartamp_cali_iv wait tv_nsec:%ld tv_sec:%ld",ts.tv_nsec,ts.tv_sec);
        ret=pthread_cond_timedwait(&smartamp_ctl->cond, &smartamp_ctl->lock, &ts);
        LOG_I("get_smartamp_cali_iv wakeup ret:%d",ret);
    }while((ret!=0)&&(ret != ETIMEDOUT));

    if(ret == ETIMEDOUT){
        LOG_W("get_smartamp_cali_iv pthread_cond_timedwait timeout");
        result=false;
    }else{
        result=true;
    }
    pthread_mutex_unlock(&smartamp_ctl->lock);
    return result;
}

void set_smartamp_cali_values(struct smart_amp_ctl *smartamp_ctl,uint32_t value1,uint32_t value2){
    LOG_I("set_smartamp_cali_values");
    pthread_mutex_lock(&smartamp_ctl->lock);
    smartamp_ctl->R0_dc=value1;
    smartamp_ctl->Re_T_cali=(int16)(value2&0xffff);
    pthread_cond_signal(&smartamp_ctl->cond);
    LOG_I("set_smartamp_cali_values signal");
    pthread_mutex_unlock(&smartamp_ctl->lock);
}

void enable_smartamp_func(void *dsp_ctl,bool on){
    struct dsp_control_t *agdsp_ctl=(struct dsp_control_t *)dsp_ctl;
    struct dsp_smsg _msg;
    _msg.channel=2;
    _msg.command=AGDSP_CMD_SMARTAMP_FUNC;

    if(true==on){
        _msg.parameter0=1;
    }else{
        _msg.parameter0=0;
    }
    _msg.parameter1=0;
    _msg.parameter2=0;

    agdsp_send_msg(agdsp_ctl,&_msg);
}

void enable_smartamp_pt(void *dsp_ctl,bool on){
    struct dsp_control_t *agdsp_ctl=(struct dsp_control_t *)dsp_ctl;
    struct dsp_smsg _msg;
    _msg.channel=2;
    _msg.command=AGDSP_CMD_SMARTAMP_PT;

    if(true==on){
        _msg.parameter0=1;
    }else{
        _msg.parameter0=0;
    }
    _msg.parameter1=0;
    _msg.parameter2=0;

    agdsp_send_msg(agdsp_ctl,&_msg);
}


bool is_smartamp_func_enable(struct smart_amp_ctl *smartamp_ctl){
    bool ret=false;
    pthread_mutex_lock(&smartamp_ctl->lock);
    if((0!=smartamp_ctl->smartamp_usecase)&&(true==smartamp_ctl->smartamp_func_enable)){
        if(SND_AUDIO_FB_SMARTAMP_MODE==smartamp_ctl->smartamp_support_mode){
            if((true==smartamp_ctl->smart_cali_failed)
                &&(false==smartamp_ctl->calibrating)){
                ret=false;
            }else{
                ret=true;
            }
        }

        if(SND_AUDIO_FF_SMARTAMP_MODE==smartamp_ctl->smartamp_support_mode){
            ret=true;
        }
    }
    pthread_mutex_unlock(&smartamp_ctl->lock);
    return ret;
}
