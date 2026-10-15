#ifndef _DSP_COMMAND_CTL_H_
#define _DSP_COMMAND_CTL_H_


#define SWITCH_ON 1
#define SWITCH_OFF 0

typedef enum preprocess_type {
    NS_CONFIG_TYPE = 0x100,
    NS_PARAMTER_TYPE,
    NS_SWITCH_TYPE,
    DSP_COMMAND_TYPE_INVAILD
}PRE_PROCESS_TYPE;

typedef struct preprocess_msg {
    int type; /*see PRE_PROCESS_TYPE*/
    int length; /*length of message data*/
    char msgData[0];
}PRE_PROCESS_MSG;

typedef struct dsp_preprocess_msg {
    int dsp_msg_command; /*dsp_msg_command = 1*/
    PRE_PROCESS_MSG tlvMsg;
}DSP_PRE_PROCESS_MSG;

typedef enum dsp_command_type {
    /*becasue playback effect use value: 0, so record process use vale 1*/
    DSP_COMMAND_RECORD_PROCESS_TYPE = 0x1
}DSP_COMMAND_TYPE;

#ifdef __cplusplus
extern "C" {
#endif

#define MAKE_DSP_MSG(dspMsg, dspCommandType, tlvType, tlvlength, ptrMsgData) \
do { \
    dspMsg.dsp_msg_command = dspCommandType; \
    dspMsg.tlvMsg.type = tlvType; \
    dspMsg.tlvMsg.length = tlvlength; \
    memcpy(dspMsg.tlvMsg.msgData, ptrMsgData, tlvlength); \
}while(0)

#define MAKE_DSP_MSG_EXT(dspMsg, dspCommandType, tlvType, tlvlength) \
do { \
    dspMsg.dsp_msg_command = dspCommandType; \
    dspMsg.tlvMsg.type = tlvType; \
    dspMsg.tlvMsg.length = tlvlength; \
}while(0)

#define DESTROY_MSG(dspMsg) \
do { \
    dspMsg.dsp_msg_command = 0; \
    dspMsg.tlvMsg.type = 0; \
    dspMsg.tlvMsg.length = 0; \
    memset(dspMsg.tlvMsg.msgData, 0, dspMsg.length); \
}while(0)

int SendDspPreProcessMsg(DSP_PRE_PROCESS_MSG *pMsg);

#ifdef __cplusplus
}
#endif


#endif  //_DSP_COMMAND_CTL_H_
