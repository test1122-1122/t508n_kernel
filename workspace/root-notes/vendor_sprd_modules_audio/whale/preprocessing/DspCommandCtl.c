#define LOG_TAG "audio_hw_record_processsing"
#define LOG_NDEBUG 0

#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <log/log.h>
#include <errno.h>
#include "DspCommandCtl.h"

#define COMMAND_CHANNAL "/dev/audio_pipe_recordproc"

typedef struct dsp_command_control{
    int fileFd;
}DSP_COMMAND_CTL;

static DSP_COMMAND_CTL *pComandCtl = NULL;

int InitDspCommandCtl(DSP_COMMAND_CTL **pCtl)
{
    int rc = 0;
    int fd = -1;
    DSP_COMMAND_CTL *pTmpCtl = NULL;

    if (pCtl == NULL) {
        ALOGE("%s is fail!", __FUNCTION__);
        rc = -1;
        goto error;
    }

    if (*pCtl == NULL) {
        pTmpCtl = (DSP_COMMAND_CTL *)malloc(sizeof(DSP_COMMAND_CTL));
        fd = open(COMMAND_CHANNAL, O_RDWR);
        if (fd < 0) {
            rc = -2;
            goto error;
        } else {
            pTmpCtl->fileFd = fd;
            *pCtl = pTmpCtl;
        }
    } else {
        ALOGI("%s already is init! %p", __FUNCTION__, *pCtl);
    }
    return rc;

error:
    if (pCtl != NULL && *pCtl != NULL) {
        free(*pCtl);
        *pCtl = NULL;
    }
    ALOGE("%s is error rc = %d error = %s", __FUNCTION__, rc, strerror(errno));
    return rc;
}

int SendDspCommand(DSP_COMMAND_CTL *pCtl, DSP_PRE_PROCESS_MSG *pDspMsg)
{
    int rc = 0;
    int wLength = 0;

    if ((pCtl == NULL) || pDspMsg == NULL) {
        ALOGE("%s argin is error! pCtl: %p, pDspMsg: %p", __FUNCTION__, pCtl, pDspMsg);
        rc = -1;
        goto error;
    }


    if (pCtl->fileFd < 0 || pDspMsg->tlvMsg.length <= 0 || pDspMsg->tlvMsg.type >=DSP_COMMAND_TYPE_INVAILD) {
        ALOGE("%s the fd: %d length: %d type: %d pMsgData: %p is error", __FUNCTION__, pCtl->fileFd,
            pDspMsg->tlvMsg.length, pDspMsg->tlvMsg.type, pDspMsg->tlvMsg.msgData);
        rc = -2;
        goto error;
    }

    int msgLength = sizeof(DSP_PRE_PROCESS_MSG) + pDspMsg->tlvMsg.length;

    do {
        wLength = write(pCtl->fileFd, pDspMsg, msgLength);
        if (msgLength != wLength) {
            ALOGE("%s send dsp msg is fail! wLength: %d,msgLength:%d,pCtl->fileFd:%d", __FUNCTION__, wLength,msgLength,pCtl->fileFd);
            rc = -3;
            goto error;
        }
        ALOGI("%s send dsp msg is ok! wLength: %d,msgLength:%d,pCtl->fileFd:%d", __FUNCTION__, wLength,msgLength,pCtl->fileFd);
    }while(0);
    return rc;

error:
    ALOGE("%s is fail! rc: %d", __FUNCTION__, rc);
    return rc;
}

int DeInitDspCommandCtl(DSP_COMMAND_CTL *pCtl)
{
    int rc = 0;
    if (pCtl == NULL) {
        ALOGE("%s argin is error!", __FUNCTION__);
        rc = -1;
        goto error;
    }
    rc = close(pCtl->fileFd);
    if (0 != rc) {
        ALOGE("%s close fd is fail! rc: %d", __FUNCTION__, rc);
        rc = -2;
        goto error;
    }
    free(pCtl);
    pCtl = NULL;
    return rc;
error:
    ALOGE("%s is fail! rc: %d", __FUNCTION__, rc);
    return rc;
}


int SendDspPreProcessMsg(DSP_PRE_PROCESS_MSG *pMsg)
{
    int rc = 0;
    if (pComandCtl == NULL) {
        rc = InitDspCommandCtl(&pComandCtl);
        if (rc != 0) {
            goto error;
        }
    }

    rc = SendDspCommand(pComandCtl, pMsg);
    if (rc != 0) {
        goto error;
    }
    return rc;

error:
    ALOGE("%s is fail! rc: %d", __FUNCTION__, rc);
    DeInitDspCommandCtl(pComandCtl);
    return rc;
}


