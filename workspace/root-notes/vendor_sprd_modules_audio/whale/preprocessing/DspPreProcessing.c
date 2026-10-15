/*
 * Copyright (C) 2011 The Android Open Source Project
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
#define LOG_TAG "audio_hw_record_processsing"
#define LOG_NDEBUG 0
#include <stdlib.h>
#include <string.h>
#include <log/log.h>
#include "DspPreProcessing.h"
#include "DspCommandCtl.h"
#define UNUSED_ATTR __attribute__((unused))

typedef struct ns_param_msg {
    DSP_PRE_PROCESS_MSG dspMsg;
    struct audio_record_proc_param nsParamData;
}NS_PARAM_MSG;

typedef struct audio_record_proc_param RECORD_NS_PARAM;

typedef struct ns_config {
    int samplingRate;
    int channels;
}NS_CONFIG;

typedef struct ns_config_msg {
    DSP_PRE_PROCESS_MSG dspMsg;
    NS_CONFIG config;
}NS_CONFIG_MSG;

typedef struct ns_switch {
    int switching;
}NS_SWITCH;

typedef struct ns_switch_msg {
    DSP_PRE_PROCESS_MSG dspMsg;
    NS_SWITCH switching;
}NS_SWITCH_MSG;


struct record_proc_t {
    int is_enabled;
};

void RecordProcEnable()
{
    ALOGV("NsEnable effect");
    //@TODO: to add NR recorder enable;
    NS_SWITCH_MSG msg;
    NS_SWITCH switching;
    switching.switching = SWITCH_ON;
    MAKE_DSP_MSG(msg.dspMsg, DSP_COMMAND_RECORD_PROCESS_TYPE, NS_SWITCH_TYPE, sizeof(NS_SWITCH), &switching);
    SendDspPreProcessMsg((DSP_PRE_PROCESS_MSG *)&msg);
    return;

}

void RecordProcDisable()
{
    ALOGV("NsDisable effect");
    //@TODO: to add NR recorder disable;
    NS_SWITCH_MSG msg;
    NS_SWITCH switching;
    switching.switching = SWITCH_OFF;
    MAKE_DSP_MSG(msg.dspMsg, DSP_COMMAND_RECORD_PROCESS_TYPE, NS_SWITCH_TYPE, sizeof(NS_SWITCH), &switching);
    SendDspPreProcessMsg((DSP_PRE_PROCESS_MSG *)&msg);
    return;
}

int recordproc_setparameter(UNUSED_ATTR recordproc_handle handle, struct audio_record_proc_param * data)
{
    ALOGI("recordproc_setparameter");

    NS_PARAM_MSG msg;
    /*send msg to dsp*/
    //GetNsParameter(mode, &msg.nsParamData);
    memcpy(&msg.nsParamData, (RECORD_NS_PARAM*)data,  sizeof(RECORD_NS_PARAM));
    MAKE_DSP_MSG_EXT(msg.dspMsg, DSP_COMMAND_RECORD_PROCESS_TYPE, \
        NS_PARAMTER_TYPE, sizeof(RECORD_NS_PARAM));
    SendDspPreProcessMsg((DSP_PRE_PROCESS_MSG *)&msg);
    return 0;
}

int recordproc_enable(recordproc_handle handle)
{
    struct record_proc_t  *record_proc = (struct record_proc_t *)handle;
    ALOGI("recordproc_enable ");
    if(record_proc) {
        RecordProcEnable();
        record_proc->is_enabled = 1;
    }
    return 0;
}


int recordproc_disable(recordproc_handle handle)
{
    struct record_proc_t  *record_proc = (struct record_proc_t *)handle;
    if(record_proc->is_enabled) {
        RecordProcDisable();
        record_proc->is_enabled = 0;
    }
    return 0;
}


recordproc_handle recordproc_init()
{
    recordproc_handle *recordproc=NULL;
    recordproc = (recordproc_handle*)malloc(sizeof(struct record_proc_t));

    ALOGI("recordproc_Init ");

    return (void *)recordproc;
}


void recordproc_deinit(recordproc_handle handle)
{
    struct record_proc_t  *record_proc = (struct record_proc_t *)handle;
    if(handle) {
        if(record_proc->is_enabled) {
            recordproc_disable(handle);
            record_proc->is_enabled = 0;
        }
        free(handle);
    }
    ALOGI("recordproc_deinit");
}


