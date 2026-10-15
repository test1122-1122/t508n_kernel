#ifndef _AUDIO_DIAG_H_
#define _AUDIO_DIAG_H_

#include <stdlib.h>
#include "sprd_fts_type.h"
#include "sprd_fts_cb.h"


#define DIAG_CMD_AUDIO_IN              0x0A
#define DIAG_CMD_AUDIO_OUT             0x0B
#define DIAG_CMD_AUDIO_HEADSET_CHECK             0x0C

#define AUDIO_TEST_FILE     "/data/local/media/aploopback.pcm"
static const char *default_pcm_file = "/system/media/engtest_sample.pcm";

#define AUDIO_WIRED_HEADPHONE_DISCONNECTED   0
#define  AUDIO_WIRED_HEADSET_CONNECTED     1
#define  AUDIO_WIRED_HEADPHONE_CONNECTED    2

static const char *at_play = "AT+AUDIOPLAY";
static const char *at_headsettype = "AT+HEADSETTYPE";
static const char *at_audiocploop = "AT+AUDIONPILOOP";
static const char *at_audiofm = "AT+AUDIOFM";
static const char *at_audiopipe = "AT+AUDIOPIPE";
static const char *at_headsetcheck = "AT+HEADSETCHECK";
static const char *at_calibration = "AT+AUDIO_CALIBRATION";

static const char *at_play_test = "AT+AUDIOPLAY=1,2,44100,2,\"/system/media/engtest_sample.pcm\"";
static const char *at_play_test_stop = "AT+AUDIOPLAY=0";

#define FM_START_PROCESS_TIME_MS     700
#define DEFAULT_PROCESS_TIME_MS     100
#define MAX_PROCESS_TIME_MS     1000
#define MIN_PROCESS_TIME_MS     10
#define UNUSED_ATTR __attribute__((unused))

enum aud_outdev_e
{
    AUD_OUTDEV_SPEAKER = 0,
    AUD_OUTDEV_EARPIECE,
    AUD_OUTDEV_HEADSET,
};

enum aud_indev_e
{
    AUD_INDEV_BUILTIN_MIC = 0,
    AUD_INDEV_HEADSET_MIC,
    AUD_INDEV_BACK_MIC,
};

#define RECORD_SAMPLE_RATE  16000

#define RECORD_MAX_SIZE    1600
#define DIAG_MAX_DATA_SIZE  400 // bytes
#define NPI_CMD_FAILED    1
#define NPI_CMD_SUCCESS    0

typedef void   *(*SEND_AT_FUN)(int phoneId, char *cmd, char *buf, unsigned int buf_len, int wait);

struct eng_audio_ctl
{
    char *recordBuf;
    int record_pcm_pipe;
    SEND_AT_FUN atc_fun;
    struct fw_callback eng_cb;
    DYMIC_WRITETOPC_FUNC eng_diagcb;
    long test_time;
} ;

#ifdef SPRD_AUDIO_HIDL_CLIENT
extern int audioclient_setParameters(const char *str);
int audioclient_getParameters(const char *str,char *rsp);
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
ssize_t StreamInread( void *buffer, size_t numBytes);
#endif
long getCurrentTimeMs();
int audio_diag_write(struct eng_audio_ctl *ctl,char *rsp, int len);
#endif
