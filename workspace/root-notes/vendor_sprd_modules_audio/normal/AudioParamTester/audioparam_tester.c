#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <log/log.h>
#include "audioparam_tester.h"
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include "sprd_fts_type.h"
#include <cutils/properties.h>

#define AUDIO_AT_LOGI   ALOGI
#define AUDIO_AT_LOGV   ALOGV
#define AUDIO_AT_LOGE  ALOGE
#undef LOG_TAG
#define LOG_TAG "audio_hw_param"
#ifdef SPRD_AUDIO_HIDL_CLIENT
extern int audioclient_setParameters(const char *str);
#endif

//defined by libnvexchange
extern int adev_get_audiomodenum4eng(void);
extern void stringfile2nvstruct(char *filename, void *para_ptr, int lenbytes);

//defined by libvbeffect
extern int parse_vb_effect_params(void *audio_params_ptr,
                                  unsigned int params_size);
extern void nvstruct2stringfile(char *filename, void *para_ptr, int lenbytes);

static const char *at_sadm = "AT+SADM4AP";
static const char *at_spenha = "AT+SPENHA";
static const char *at_calibr = "AT+CALIBR";
static const char *at_phoneinfo = "AT+PEINFO";
static const char *at_phoneloop = "AT+PELOOP";

#define AUDIO_NV_ARM_INDI_FLAG 0x02
#define AUDIO_ENHA_EQ_INDI_FLAG 0x04
#define ENG_DIAG_SIZE 4096

// static int at_sadm_cmd_to_handle[] = {7,8,9,10,11,12,-1};
static int at_sadm_cmd_to_handle[] = {7, 8, 9, 10, 11, 12, -1};
// static int at_spenha_cmd_to_handle[] = {0,1,2,3,4,-1};
static int at_spenha_cmd_to_handle[] = {0, 1, 2, 3, 4, -1};

struct audio_param_tunning_ctl {
    int g_is_data;;
    int g_index;
    int g_indicator;
    unsigned char tun_data[376];
    AUDIO_TOTAL_T *audio_total;
    struct eng_callback * reg;
    int engaudio_num;
} ;

struct audio_param_tunning_ctl audiotester_ctl;
static int ascii2bin(unsigned char *dst, unsigned char *src, unsigned long size) {
  unsigned char h, l;
  unsigned long count = 0;

  if ((NULL == dst) || (NULL == src)) return -1;

  while (count < size) {
    if ((*src >= '0') && (*src <= '9')) {
      h = *src - '0';
    } else {
      h = *src - 'A' + 10;
    }

    src++;

    if ((*src >= '0') && (*src <= '9')) {
      l = *src - '0';
    } else {
      l = *src - 'A' + 10;
    }

    src++;
    count += 2;

    *dst = (unsigned char)(h << 4 | l);
    dst++;
  }

  return 0;
}

static int bin2ascii(unsigned char *dst, unsigned char *src, unsigned long size) {
  unsigned char semi_octet;
  unsigned long count = 0;

  if ((NULL == dst) || (NULL == src)) return -1;

  while (count < size) {
    semi_octet = ((*src) & 0xf0) >> 4;
    if (semi_octet <= 9) {
      *dst = semi_octet + '0';
    } else {
      *dst = semi_octet + 'A' - 10;
    }

    dst++;

    semi_octet = ((*src) & 0x0f);
    if (semi_octet <= 9) {
      *dst = semi_octet + '0';
    } else {
      *dst = semi_octet + 'A' - 10;
    }

    dst++;

    src++;
    count++;
  }

  return 0;
}

static void skipWhiteSpace(char **p_cur) {
  if (*p_cur == NULL) return;

  while (**p_cur != '\0' && isspace(**p_cur)) {
    (*p_cur)++;
  }
}

static void skipNextComma(char **p_cur) {
  if (*p_cur == NULL) return;

  while (**p_cur != '\0' && **p_cur != ',') {
    (*p_cur)++;
  }

  if (**p_cur == ',') {
    (*p_cur)++;
  }
}

static char *nextTok(char **p_cur) {
  char *ret = NULL;

  skipWhiteSpace(p_cur);

  if (*p_cur == NULL) {
    ret = NULL;
  } else if (**p_cur == '"') {
    (*p_cur)++;
    ret = strsep(p_cur, "\"");
    skipNextComma(p_cur);
  } else {
    ret = strsep(p_cur, ",");
  }

  return ret;
}

static int at_tok_equel_start(char **p_cur) {
  if (*p_cur == NULL) {
    return -1;
  }

  // skip prefix
  // consume "^[^:]:"

  *p_cur = strchr(*p_cur, '=');

  if (*p_cur == NULL) {
    return -1;
  }

  (*p_cur)++;

  return 0;
}

static int at_tok_nextint_base(char **p_cur, int *p_out, int base, int uns) {
  char *ret;

  if (*p_cur == NULL) {
    return -1;
  }

  ret = nextTok(p_cur);

  if (ret == NULL) {
    return -1;
  } else {
    long l;
    char *end;

    if (uns)
      l = strtoul(ret, &end, base);
    else
      l = strtol(ret, &end, base);

    *p_out = (int)l;

    if (end == ret) {
      return -1;
    }
  }

  return 0;
}

/**
 * Parses the next base 10 integer in the AT response line
 * and places it in *p_out
 * returns 0 on success and -1 on fail
 * updates *p_cur
 */
int at_tok_nextint(char **p_cur, int *p_out) {
  return at_tok_nextint_base(p_cur, p_out, 10, 0);
}

static AUDIO_TOTAL_T *eng_regetpara(void) {
  int srcfd;
  char *filename = NULL;
  // eng_getparafromnvflash();
  AUDIO_AT_LOGV("wangzuo eng_regetpara 1");

  AUDIO_TOTAL_T *aud_params_ptr=NULL;
  int len = sizeof(AUDIO_TOTAL_T) * adev_get_audiomodenum4eng();

  aud_params_ptr = calloc(1, len);
  if (!aud_params_ptr){
    return NULL;
  }
  memset(aud_params_ptr, 0, len);
  srcfd = open((char *)(ENG_AUDIO_PARA_DEBUG), O_RDONLY);
  filename = (srcfd < 0) ? (ENG_AUDIO_PARA) : (ENG_AUDIO_PARA_DEBUG);
  if (srcfd >= 0) {
    close(srcfd);
  }

  AUDIO_AT_LOGV("eng_regetpara %s", filename);  ////done,into
  stringfile2nvstruct(filename, aud_params_ptr, len);

  return aud_params_ptr;
}

static void eng_setpara(AUDIO_TOTAL_T *ptr) {  // to do
  int len = sizeof(AUDIO_TOTAL_T) * adev_get_audiomodenum4eng();
  AUDIO_AT_LOGV("eng_setpara");
  nvstruct2stringfile(ENG_AUDIO_PARA_DEBUG, ptr, len);
}

static int eng_notify_mediaserver_updatapara(int ram_ops, int index,
                                             AUDIO_TOTAL_T *aud_params_ptr) {
  int result = 0;
  int fifo_id = -1;
  int receive_fifo_id = -1;
  int ret;
  int length = 0;
  AUDIO_AT_LOGI("eng_notify_mediaserver_updatapara E,%d:%d!\n", ram_ops, index);
  fifo_id = open(AUDFIFO, O_WRONLY);
  if (fifo_id != -1) {
    int buff = 1;
    AUDIO_AT_LOGI("eng_notify_mediaserver_updatapara notify OPS!\n");
    result = write(fifo_id, &ram_ops, sizeof(int));
    if (ram_ops & ENG_RAM_OPS) {
      result = write(fifo_id, &index, sizeof(int));
      result = write(fifo_id, aud_params_ptr, sizeof(AUDIO_TOTAL_T));
      AUDIO_AT_LOGI("eng_notify_mediaserver_updatapara,index:%d,size:%d!\n", index,
            sizeof(AUDIO_TOTAL_T));
    }
    if (ram_ops & ENG_PHONEINFO_OPS) {
      receive_fifo_id = open(AUDFIFO_2, O_RDONLY);
      if (receive_fifo_id != -1) {
        result = read(receive_fifo_id, &length, sizeof(int));
        if (result < 0) {
          goto error;
        }
        sprintf((char *)aud_params_ptr, "%d", length);
        result =
            read(receive_fifo_id, (void *)aud_params_ptr + sizeof(int), length);
        if (result < 0) {
          goto error;
        }
        result += sizeof(int);
        close(receive_fifo_id);
        receive_fifo_id = -1;
        AUDIO_AT_LOGI("eng_notify_mediaserver_updatapara,result:%d,received:%d!\n",
              result, length);
      } else {
        AUDIO_AT_LOGI("%s open audio FIFO_2 error %s,fifo_id:%d\n", __FUNCTION__,strerror(errno), fifo_id);
      }
    }
    close(fifo_id);
    fifo_id = -1;
  } else {
    AUDIO_AT_LOGE("%s open audio FIFO error %s,fifo_id:%d\n", __FUNCTION__, strerror(errno), fifo_id);
  }
  AUDIO_AT_LOGI("eng_notify_mediaserver_updatapara X,result:%d,length:%d!\n", result,length);
  return result;
error:
  AUDIO_AT_LOGE("eng_notify_mediaserver_updatapara X,ERROR,result:%d!\n", result);
  if (receive_fifo_id != -1) {
    close(receive_fifo_id);
    receive_fifo_id = -1;
  }

  if (fifo_id != -1) {
    close(fifo_id);
    fifo_id = -1;
  }
  return result;
}

static int audio_apdata_process (struct audio_param_tunning_ctl *ctl,char *buf){
      int fd;
      int wlen, rlen, ret = 0;
      MSG_HEAD_T *head_ptr = NULL;
      char *ptr = NULL;
      AUDIO_TOTAL_T *audio_ptr;
      int ram_ofs = 0;

      head_ptr = (MSG_HEAD_T *)(buf + 1);
      ptr = buf + 1 + sizeof(MSG_HEAD_T);
      AUDIO_AT_LOGI("audio_apdata_process g_is_data:%d index:%d"
        ,ctl->g_is_data,ctl->g_index);
      if (ctl->g_is_data) {
        wlen = head_ptr->len - sizeof(MSG_HEAD_T) - 1;

        audio_ptr = (AUDIO_TOTAL_T *)eng_regetpara();
        if (NULL== audio_ptr ) {
          AUDIO_AT_LOGE("%s eng_regetpara failed %s", __FUNCTION__,strerror(errno));
          return -1;
        }

        if (ctl->g_is_data & AUDIO_NV_ARM_DATA_MEMORY) {
          ram_ofs |= ENG_RAM_OPS;
          ctl->g_is_data &= (~AUDIO_NV_ARM_DATA_MEMORY);
          ctl->g_indicator |= AUDIO_NV_ARM_INDI_FLAG;
          ascii2bin(
              (unsigned char *)(&ctl->audio_total[ctl->g_index]
                                     .audio_nv_arm_mode_info.tAudioNvArmModeStruct),
              (unsigned char *)ptr, wlen);
        }
        if (ctl->g_is_data & AUDIO_NV_ARM_DATA_FLASH) {
          ram_ofs |= ENG_FLASH_OPS;
          ctl->g_is_data &= (~AUDIO_NV_ARM_DATA_FLASH);
          ctl->g_indicator |= AUDIO_NV_ARM_INDI_FLAG;
          ascii2bin(
              (unsigned char *)(&ctl->audio_total[ctl->g_index]
                                     .audio_nv_arm_mode_info.tAudioNvArmModeStruct),
              (unsigned char *)ptr, wlen);
          audio_ptr[ctl->g_index].audio_nv_arm_mode_info.tAudioNvArmModeStruct =
              ctl->audio_total[ctl->g_index].audio_nv_arm_mode_info.tAudioNvArmModeStruct;
        }
        if (ctl->g_is_data & AUDIO_ENHA_DATA_MEMORY) {
          ram_ofs |= ENG_RAM_OPS;
          ctl->g_is_data &= (~AUDIO_ENHA_DATA_MEMORY);
          ctl->g_indicator |= AUDIO_ENHA_EQ_INDI_FLAG;
          ascii2bin((unsigned char *)(&ctl->audio_total[ctl->g_index].audio_enha_eq),
                    (unsigned char *)ptr, wlen);
        }
        if (ctl->g_is_data & AUDIO_ENHA_DATA_FLASH) {
          ram_ofs |= ENG_FLASH_OPS;
          ctl->g_is_data &= (~AUDIO_ENHA_DATA_FLASH);
          ctl->g_indicator |= AUDIO_ENHA_EQ_INDI_FLAG;
          ascii2bin((unsigned char *)(&ctl->audio_total[ctl->g_index].audio_enha_eq),
                    (unsigned char *)ptr, wlen);
          audio_ptr[ctl->g_index].audio_enha_eq = ctl->audio_total[ctl->g_index].audio_enha_eq;
        }
        if (ctl->g_is_data & AUDIO_ENHA_TUN_DATA_MEMORY) {
          ram_ofs |= ENG_RAM_OPS;
          ctl->g_is_data &= (~AUDIO_ENHA_TUN_DATA_MEMORY);
          ascii2bin((unsigned char *)ctl->tun_data, (unsigned char *)ptr, wlen);
        }

        AUDIO_AT_LOGI("audio_apdata_process  g_indicator:0x%x",ctl->g_indicator);
        if (audio_ptr) {
          if (ram_ofs & ENG_FLASH_OPS) {
            eng_setpara(audio_ptr);
          }

          if (ctl->g_indicator) {
            ram_ofs |= ENG_PGA_OPS;
          }
          eng_notify_mediaserver_updatapara(ram_ofs,ctl->g_index,
                                            &ctl->audio_total[ctl->g_index]);
          free(audio_ptr);
        }

        if (ctl->g_indicator) {
          AUDIO_AT_LOGI("data is ready!g_indicator = 0x%x,g_index:%d\n",ctl->g_indicator,
                  ctl->g_index);
          ctl->g_indicator = 0;
          parse_vb_effect_params((void *)ctl->audio_total, adev_get_audiomodenum4eng() *
                                                          sizeof(AUDIO_TOTAL_T));
        }
      }
      return 0;
}


static int audio_peloop_process (struct audio_param_tunning_ctl *ctl,
    char *buf, char *rsp)
{
    char *ptr = NULL;
    int loop_enable=0;
    int loop_route=0;
    int fifo_id = -1;
    int ram_ops=ENG_PHONELOOP_OPS;
    int ret=-1;

    if((NULL==buf)||(NULL==rsp)){
        return 0;
    }
    ptr = buf + 1 + sizeof(MSG_HEAD_T);
    at_tok_equel_start(&ptr);
    if(0!=at_tok_nextint(&ptr,&loop_enable)){
        ALOGE("%s line:%d error",__func__,__LINE__);
    }
    if(0!=at_tok_nextint(&ptr,&loop_route)){
        ALOGE("%s line:%d error",__func__,__LINE__);
    }

    AUDIO_AT_LOGI("AUDIO_PELOOP_AT loop_enable:%d loop_route:%d",
        loop_enable,loop_route);

    fifo_id = open(AUDFIFO, O_WRONLY);
    if (fifo_id != -1) {
        ret = write(fifo_id, &ram_ops, sizeof(int));
        ret = write(fifo_id,&loop_enable,sizeof(int));
        ret = write(fifo_id,&loop_route,sizeof(int));
        close(fifo_id);
        fifo_id = -1;
        sprintf(rsp,"\r\nOK\r\n");
    } else {
        sprintf(rsp, "\r\nERROR\r\n");
        AUDIO_AT_LOGE("%s open audio FIFO error %s,fifo_id:%d\n", __FUNCTION__, strerror(errno), fifo_id);
    }
    return strlen(rsp);
}

static int AUDIO_PELOOP_AT (char *req, char *rsp)
{
       return  audio_peloop_process(&audiotester_ctl,req,rsp);
}

static int audio_spenha_process (struct audio_param_tunning_ctl *ctl,char *buf,char *rsp)
{
    int cmd_type=0;
    int eq_or_tun_type=0;
    AUDIO_TOTAL_T *audio_ptr=NULL;
    char *ptr = NULL;
    int i=0;

    if (NULL == buf) {
        AUDIO_AT_LOGE("%s,null pointer", __FUNCTION__);
        return 0;
    }
    ptr = buf + 1 + sizeof(MSG_HEAD_T);
    AUDIO_AT_LOGI("audio_spenha_process:%s",ptr);

    if(ctl->g_is_data){
        audio_apdata_process(ctl,buf);
        sprintf(rsp, "\r\nOK\r\n");
        goto out;
    }

    at_tok_equel_start(&ptr);
    at_tok_nextint(&ptr, &eq_or_tun_type);
    at_tok_nextint(&ptr, &cmd_type);

    AUDIO_AT_LOGI("AUDIO_SPENHA_AT cmd_type:%d eq_or_tun_type:%d ptr:%s",
        cmd_type,eq_or_tun_type,ptr);

    for (i = 0; i < sizeof(at_spenha_cmd_to_handle) / sizeof(int); i += 1) {
      if (-1 == at_spenha_cmd_to_handle[i]) {
          AUDIO_AT_LOGE("SPENHA line:%d exit error",__LINE__);
        return 0;
      }

      if (at_spenha_cmd_to_handle[i] == cmd_type) {
        if (GET_AUDIO_ENHA_MODE_COUNT != cmd_type) {
          at_tok_nextint(&ptr, &ctl->g_index);
          AUDIO_AT_LOGV("SPENHA g_index:%d line:%d",ctl->g_index,__LINE__);
          if ((ctl->g_index > adev_get_audiomodenum4eng()) || (ctl->g_index <= 0)) {
            ALOGI("SPENHA line:%d exit error",__LINE__);
            return 0;
          }
            ctl->g_index--;
          AUDIO_AT_LOGV("SPENHA index = %x", ctl->g_index);
        }
        break;
      }
    }

    if (1 == eq_or_tun_type) {
      switch (cmd_type) {
        case GET_AUDIO_ENHA_MODE_COUNT:
          sprintf(rsp, "+SPENHA: 1");
          break;
        case SET_AUDIO_ENHA_DATA_TO_MEMORY:
        case SET_AUDIO_ENHA_DATA_TO_FLASH:
          ctl->g_is_data |= AUDIO_ENHA_TUN_DATA_MEMORY;
          sprintf(rsp, "\r\n> \r\nPENDING\r\n");
          break;
        case GET_AUDIO_ENHA_DATA_FROM_FLASH:
        case GET_AUDIO_ENHA_DATA_FROM_MEMORY:
          sprintf(rsp, "+SPENHA: %d,", ctl->g_index);
          bin2ascii((unsigned char *)(rsp + strlen(rsp)),
                    (unsigned char *)ctl->tun_data, sizeof(ctl->tun_data));
          break;
        default:
          break;
      }
    } else {
      switch (cmd_type) {
        case GET_AUDIO_ENHA_MODE_COUNT:
          sprintf(rsp, "+SPENHA: %d", adev_get_audiomodenum4eng());
          break;
        case SET_AUDIO_ENHA_DATA_TO_MEMORY:
          ctl->g_is_data |= AUDIO_ENHA_DATA_MEMORY;
          sprintf(rsp, "\r\n> \r\nPENDING\r\n");
          break;
      case SET_AUDIO_ENHA_DATA_TO_FLASH:
          ctl->g_is_data |= AUDIO_ENHA_DATA_FLASH;
          sprintf(rsp, "\r\n> \r\nPENDING\r\n");
          break;
        case GET_AUDIO_ENHA_DATA_FROM_FLASH:
          audio_ptr = (AUDIO_TOTAL_T *)eng_regetpara();
          if (NULL != audio_ptr) {
            ctl->audio_total[ctl->g_index].audio_enha_eq =audio_ptr[ctl->g_index].audio_enha_eq;
            free(audio_ptr);
          }
        // there is no break in this case,'cause it will share the code with the
        // following case
        case GET_AUDIO_ENHA_DATA_FROM_MEMORY:
          sprintf(rsp, "+SPENHA: %d,", ctl->g_index);
          bin2ascii((unsigned char *)(rsp + strlen(rsp)),
                    (unsigned char *)(&ctl->audio_total[ctl->g_index].audio_enha_eq),
                    sizeof(AUDIO_ENHA_EQ_STRUCT_T));
          break;
        default:
          break;
      }
    }

out:
    return rsp != NULL ? strlen(rsp) : 0;
}

static int AUDIO_SPENHA_AT (char *req, char *rsp)
{
       return  audio_spenha_process(&audiotester_ctl,req,rsp);
}

static int audio_peinfo_process (struct audio_param_tunning_ctl *ctl,
    char *buf, char *rsp)
{
    int length = 0;
    char bin_tmp[2 * ENG_DIAG_SIZE];
    int rsplen_tmp=0;
    int ret=0;

    memcpy(rsp, "+PEINFO:", sizeof("+PEINFO:"));
    length = eng_notify_mediaserver_updatapara(ENG_PHONEINFO_OPS, 0, bin_tmp);
    if (length > 0) {
    bin2ascii(rsp + strlen("+PEINFO:"), bin_tmp, length);
    } else {
        sprintf(rsp, "\r\nERROR\r\n");
    }
    return rsp != NULL ? strlen(rsp) : 0;
}

static int AUDIO_PEINFO_AT (char *req, char *rsp)
{
#ifdef SPRD_AUDIO_HIDL_CLIENT
    audioclient_setParameters("AudioTesterInit=true");
#endif
   return  audio_peinfo_process(&audiotester_ctl,req,rsp);
}

static int audio_sadm4ap_process (struct audio_param_tunning_ctl *ctl,char *buf,char *rsp)
{
    int cmd_type=0;
    AUDIO_TOTAL_T *audio_ptr=NULL;
    char *ptr = NULL;
    int i=0;

    if((NULL==buf)||(NULL==rsp)){
        return 0;
    }

    ptr = buf + 1 + sizeof(MSG_HEAD_T);
    AUDIO_AT_LOGI("audio_sadm4ap_process:%s",ptr);

    AUDIO_AT_LOGI("AUDIO_SADM4AP_AT cmd_type:%d index:%d ptr:%s",
        cmd_type,ctl->g_index,ptr);
    if(ctl->g_is_data){
        audio_apdata_process(ctl,buf);
        sprintf(rsp, "\r\nOK\r\n");
        goto out;
    }

    at_tok_equel_start(&ptr);
    at_tok_nextint(&ptr, &cmd_type);

    for (i = 0; i < sizeof(at_sadm_cmd_to_handle) / sizeof(int); i += 1) {
      if (-1 == at_sadm_cmd_to_handle[i]) {
        AUDIO_AT_LOGE("audio_sadm4ap_process end of at_sadm_cmd_to_handle");
        return 0;
      }
      if (at_sadm_cmd_to_handle[i] == cmd_type) {
        if (GET_ARM_VOLUME_MODE_COUNT != cmd_type) {
          ctl->g_index = atoi(ptr);
          AUDIO_AT_LOGV("SADM4AP g_index:%d line:%d",ctl->g_index,__LINE__);
          if (ctl->g_index >= adev_get_audiomodenum4eng()) {
              AUDIO_AT_LOGE("audio_sadm4ap_process g_index:%d error", __LINE__);
            return 0;
          }
        }
        break;
      }
    }

    switch (cmd_type) {
      case GET_ARM_VOLUME_MODE_COUNT:
        sprintf(rsp, "+SADM4AP: %d", adev_get_audiomodenum4eng());
        break;
      case GET_ARM_VOLUME_MODE_NAME:
        sprintf(rsp, "+SADM4AP: %d,\"%s\"", ctl->g_index,
                ctl->audio_total[ctl->g_index].audio_nv_arm_mode_info.ucModeName);
        break;
      case SET_ARM_VOLUME_DATA_TO_RAM:
        ctl->g_is_data |= AUDIO_NV_ARM_DATA_MEMORY;
        sprintf(rsp, "\r\n> \r\nPENDING\r\n");
        break;
      case SET_ARM_VOLUME_DATA_TO_FLASH:
        ctl->g_is_data |= AUDIO_NV_ARM_DATA_FLASH;
        sprintf(rsp, "\r\n> \r\nPENDING\r\n");
        break;
      case GET_ARM_VOLUME_DATA_FROM_FLASH:
        audio_ptr = (AUDIO_TOTAL_T *)eng_regetpara();
        if (((AUDIO_TOTAL_T *)(-1) != audio_ptr) &&
        ((AUDIO_TOTAL_T *)(0) != audio_ptr)) {
            ctl->audio_total[ctl->g_index].audio_nv_arm_mode_info.tAudioNvArmModeStruct =
            audio_ptr[ctl->g_index].audio_nv_arm_mode_info.tAudioNvArmModeStruct;
            free(audio_ptr);
        }
      // there is no break in this case,'cause it will share the code with the
      // following case
      case GET_ARM_VOLUME_DATA_FROM_RAM:
        sprintf(rsp, "+SADM4AP: 0,\"%s\",",
                ctl->audio_total[ctl->g_index].audio_nv_arm_mode_info.ucModeName);
        bin2ascii((unsigned char *)(rsp + strlen(rsp)),
                  (unsigned char *)(&ctl->audio_total[ctl->g_index]
                                         .audio_nv_arm_mode_info
                                         .tAudioNvArmModeStruct),
                  sizeof(AUDIO_NV_ARM_MODE_STRUCT_T));
        break;

      default:
        sprintf(rsp, "\r\nERROR\r\n");
        break;
    }

out:
    return strlen(rsp);
}

static int AUDIO_SADM4AP_AT (char *req, char *rsp)
{
       return  audio_sadm4ap_process(&audiotester_ctl,req,rsp);
}

#ifdef ADC_CALIBRATION
#define ADC_MAGIC (0x4144434D)  // ADCM, header flag of adc data
#define MISCDATA_BASE  (0)
#define ADC_DATA_OFFSET  (512 * 1024)
#define ADC_DATA_START (MISCDATA_BASE + ADC_DATA_OFFSET)

#define CALI_MAGIC (0x49424143)  // CALI
#define CALI_COMP (0x504D4F43)  // COMP

static void enable_calibration(void) {
  CALI_INFO_DATA_T cali_info;
  char miscdata_path[128] = {0};

  if (-1 == property_get("ro.product.partitionpath", miscdata_path, "")){
    AUDIO_AT_LOGI("%s: get miscdata_path fail\n", __FUNCTION__);
    return;
  }else{
    strcat (miscdata_path, "miscdata");
    AUDIO_AT_LOGI("miscdata_path %s\n", miscdata_path);
  }

  int fd = open(miscdata_path, O_RDWR);
  int ret = 0;

  if (fd < 0) {
    AUDIO_AT_LOGI("%s open %s failed\n", __func__, miscdata_path);
    return;
  }
  lseek(fd, ADC_DATA_START + sizeof(unsigned int), SEEK_SET);

  ret = read(fd, &cali_info, sizeof(cali_info));
  if (ret <= 0) {
    AUDIO_AT_LOGI(" %s read failed...\n", __func__);
    close(fd);
    return;
  }
  cali_info.magic = 0xFFFFFFFF;
  cali_info.cali_flag = 0xFFFFFFFF;

  lseek(fd, ADC_DATA_START + sizeof(unsigned int), SEEK_SET);

#ifdef CONFIG_NAND
  __s64 up_sz = sizeof(cali_info);
  ioctl(fd, UBI_IOCVOLUP, &up_sz);
#endif

  write(fd, &cali_info, sizeof(cali_info));
  close(fd);
}

static void disable_calibration(void) {
  CALI_INFO_DATA_T cali_info;
  int ret = 0;
  char miscdata_path[128] = {0};

  if (-1 == property_get("ro.product.partitionpath", miscdata_path, "")){
    AUDIO_AT_LOGI("%s: get miscdata_path fail\n", __FUNCTION__);
    return;
  }else{
    strcat (miscdata_path, "miscdata");
    AUDIO_AT_LOGI("miscdata_path %s\n", miscdata_path);
  }

  int fd = open(miscdata_path, O_RDWR);

  if (fd < 0) {
    AUDIO_AT_LOGI("%s open %s failed\n", __func__, miscdata_path);
    return;
  }
  lseek(fd, ADC_DATA_START + sizeof(unsigned int), SEEK_SET);

  ret = read(fd, &cali_info, sizeof(cali_info));
  if (ret <= 0) {
    AUDIO_AT_LOGI(" %s read failed...\n", __func__);
    close(fd);
    return;
  }

  cali_info.magic = CALI_MAGIC;
  cali_info.cali_flag = CALI_COMP;

  lseek(fd, ADC_DATA_START + sizeof(unsigned int), SEEK_SET);

#ifdef CONFIG_NAND
  __s64 up_sz = sizeof(cali_info);
  ioctl(fd, UBI_IOCVOLUP, &up_sz);
#endif

  write(fd, &cali_info, sizeof(cali_info));
  close(fd);
}

static int audio_calibr_process (struct audio_param_tunning_ctl *ctl,
    audio_at_param *param, char *rsp)
{
    int cmd_type=0;
    cmd_type=*(int *)param->param[0];

    if (SET_CALIBRATION_ENABLE == cmd_type) {
      enable_calibration();
      sprintf(rsp, "\r\nenable_calibration   \r\n");
    } else if (SET_CALIBRATION_DISABLE == cmd_type) {
      disable_calibration();
      sprintf(rsp, "\r\ndisable_calibration   \r\n");
    }
    At_cmd_back_sig();
    return rsp != NULL ? strlen(rsp) : 0;
}

static int AUDIO_CALIBR_AT (char *req, char *rsp)
{
       return  audio_calibr_process(&audiotester_ctl,req,rsp);
}

#endif

static int check_audio_para_file_size(char *config_file) {
  int fileSize = 0;
  int tmpFd;

  AUDIO_AT_LOGI("%s: enter", __FUNCTION__);
  tmpFd = open(config_file, O_RDONLY);
  if (tmpFd < 0) {
    AUDIO_AT_LOGI("%s: open error", __FUNCTION__);
    return -1;
  }
  fileSize = lseek(tmpFd, 0, SEEK_END);
  if (fileSize <= 0) {
    AUDIO_AT_LOGI("%s: file size error", __FUNCTION__);
    close(tmpFd);
    return -1;
  }
  close(tmpFd);
  AUDIO_AT_LOGI("%s: check OK", __FUNCTION__);
  return 0;
}

static int ensure_audio_para_file_exists(char *config_file) {
  char buf[2048];
  int srcfd, destfd;
  struct stat sb;
  int nread;
  int ret;

  AUDIO_AT_LOGI("%s: enter", __FUNCTION__);
  ret = access(config_file, R_OK | W_OK);
  if ((ret == 0) || (errno == EACCES)) {
    if ((ret != 0) &&
        (chmod(config_file, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP) != 0)) {
      AUDIO_AT_LOGI("eng_vdiag Cannot set RW to \"%s\": %s", config_file,
            strerror(errno));
      return -1;
    }
    if (0 == check_audio_para_file_size(config_file)) {
      AUDIO_AT_LOGI("%s: ensure OK", __FUNCTION__);
      return 0;
    }
  } else if (errno != ENOENT) {
    AUDIO_AT_LOGE("%s  Cannot access \"%s\": %s", __FUNCTION__,config_file, strerror(errno));
    return -1;
  }

  srcfd = open((char *)(ENG_AUDIO_PARA), O_RDONLY);
  if (srcfd < 0) {
    AUDIO_AT_LOGE("eng_vdiag Cannot open \"%s\": %s", (char *)(ENG_AUDIO_PARA),
          strerror(errno));
    return -1;
  }

  destfd = open(config_file, O_CREAT | O_RDWR, 0660);
  if (destfd < 0) {
    close(srcfd);
    AUDIO_AT_LOGE("eng_vdiag Cannot create \"%s\": %s", config_file, strerror(errno)
);
    return -1;
  }

  AUDIO_AT_LOGI("%s: start copy", __FUNCTION__);
  while ((nread = read(srcfd, buf, sizeof(buf))) != 0) {
    if (nread < 0) {
      AUDIO_AT_LOGI("eng_vdiag Error reading \"%s\": %s", (char *)(ENG_AUDIO_PARA),
            strerror(errno));
      close(srcfd);
      close(destfd);
      unlink(config_file);
      return -1;
    }
    write(destfd, buf, nread);
  }

  close(destfd);
  close(srcfd);

  /* chmod is needed because open() didn't set permisions properly */
  if (chmod(config_file, 0660) < 0) {
    AUDIO_AT_LOGI("eng_vdiag Error changing permissions of %s to 0660: %s",
        config_file, strerror(errno));
    unlink(config_file);
    return -1;
  }
  AUDIO_AT_LOGI("%s: ensure done", __FUNCTION__);
  return 0;
}

static AUDIO_TOTAL_T * malloc_audioparam(void){
    int ret=0;
    AUDIO_TOTAL_T *audio_total=NULL;
    int audio_total_size=sizeof(AUDIO_TOTAL_T) * adev_get_audiomodenum4eng();
    ret = ensure_audio_para_file_exists((char *)(ENG_AUDIO_PARA_DEBUG));
    if(0==ret){
        audio_total = calloc(1, sizeof(AUDIO_TOTAL_T) * adev_get_audiomodenum4eng());
        if (!audio_total) {
            AUDIO_AT_LOGE("eng_vdiag_wthread malloc audio_total memory error\n");
            return NULL;
        }else{
            int srcfd=-1;
            char *filename = NULL;
            memset(audio_total, 0, audio_total_size);
            srcfd = open((char *)(ENG_AUDIO_PARA_DEBUG), O_RDONLY);
            filename = (srcfd < 0) ? (ENG_AUDIO_PARA) : (ENG_AUDIO_PARA_DEBUG);
            if (srcfd >= 0) {
              close(srcfd);
            }
            stringfile2nvstruct(filename, audio_total,
                                audio_total_size);
        }
    }
    return audio_total;
}

void register_this_module_ext(struct eng_callback * reg, int *num)
{
    int modules_num=0;
#ifdef SPRD_AUDIO_HIDL_CLIENT
    char prop_t[PROPERTY_VALUE_MAX] = {0};
#endif
    ALOGI("AUDIO register_this_module_ext enter");

    memset(&audiotester_ctl,0,sizeof(struct audio_param_tunning_ctl ));
    audiotester_ctl.audio_total=malloc_audioparam();

    sprintf((reg+modules_num)->at_cmd, "%s", at_sadm);
    (reg+modules_num)->eng_linuxcmd_func = AUDIO_SADM4AP_AT;
    AUDIO_AT_LOGV("register_this_module_ext AUDIO_SADM4AP_AT:%p",
        AUDIO_SADM4AP_AT);
    modules_num++;

    sprintf((reg+modules_num)->at_cmd, "%s", at_spenha);
    (reg+modules_num)->eng_linuxcmd_func = AUDIO_SPENHA_AT;
    modules_num++;
    AUDIO_AT_LOGV("register_this_module_ext AUDIO_SPENHA_AT:%p",
        AUDIO_SPENHA_AT);

    sprintf((reg+modules_num)->at_cmd, "%s", at_phoneinfo);
    (reg+modules_num)->eng_linuxcmd_func = AUDIO_PEINFO_AT;
    modules_num++;
    AUDIO_AT_LOGV("register_this_module_ext AUDIO_PEINFO_AT:%p",
        AUDIO_PEINFO_AT);

    sprintf((reg+modules_num)->at_cmd, "%s", at_phoneloop);
    (reg+modules_num)->eng_linuxcmd_func = AUDIO_PELOOP_AT;
    modules_num++;
    AUDIO_AT_LOGV("register_this_module_ext AUDIO_PELOOP_AT:%p",
        AUDIO_PELOOP_AT);

#ifdef ADC_CALIBRATION
    sprintf((reg+modules_num)->at_cmd, "%s", at_calibr);
    (reg+modules_num)->eng_linuxcmd_func = AUDIO_CALIBR_AT;
    modules_num++;
#endif
    *num =modules_num;
#ifdef SPRD_AUDIO_HIDL_CLIENT
    if(property_get("ro.bootmode", prop_t, "") && (0 == strcmp(prop_t, "autotest"))){
        AUDIO_AT_LOGI("ro.bootmode is autotest");
    }else{
        audioclient_setParameters("AudioTesterInit=true");
    }
#endif
    ALOGI("AUDIO register_this_module_ext  num:%d",*num);
}
