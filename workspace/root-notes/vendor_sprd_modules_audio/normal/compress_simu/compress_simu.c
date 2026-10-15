#define LOG_TAG "audio_hw_cps"
#define _GNU_SOURCE

#include "compress_simu.h"
#include<sched.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <stdbool.h>
#include <errno.h>
#include <fcntl.h>
#include <tinyalsa/asoundlib.h>
#include <cutils/log.h>
#include <pthread.h>
#include <semaphore.h>
#include "mp3_dec_api.h"
#include <sound/asound.h>
#include "sound/compress_params.h"
#include "sound/compress_offload.h"
#include "tinycompress/tinycompress.h"
#include "cutils/list.h"
#include "ringbuffer.h"
#include <cutils/str_parms.h>
#include <cutils/properties.h>
#include <expat.h>
#include <system/thread_defs.h>
#include <cutils/sched_policy.h>
#include <sys/prctl.h>
#include "sys/resource.h"
#include <time.h>
#include <sys/time.h>
#include <linux/ioctl.h>
#include <audio_utils/resampler.h>
#include "dumpdata.h"


#if 0
#undef ALOGE
extern "C"
{
extern void  peter_log(const char *tag,const char *fmt, ...);
#define ALOGE(...)  peter_log(LOG_TAG,__VA_ARGS__);
}
#endif



#define COMPR_ERR_MAX 128
#define MP3_MAX_DATA_FRAME_LEN  (1536)  //unit by bytes
#define MP3_DEC_FRAME_LEN       (1152)  //pcm samples number

#define AUDIO_OFFLOAD_PLAYBACK_VOLUME_MAX 0x1000

enum{
    ERROR_NO_DATA = 100,
    ERROR_NO_PCM_DATA,
    ERROR_DECODE,
    ERROR_OTHER,
};


enum {
    STATE_OPEN =1,
    STATE_START,
    STATE_PAUSE,
    STATE_RESUME,
    STATE_STOP,
    STATE_STOP_DONE,
    STATE_DRAIN,
    STATE_DRAIN_DONE,
    STATE_PARTIAL_DRAIN,
    STATE_PARTIAL_DRAIN_DONE,
    STATE_CLOSE,
    STATE_CHANGE_DEV,
    STATE_EXIT,
};

typedef enum {
    COMPRESS_CMD_EXIT,               /* exit compress offload thread loop*/
    COMPRESS_CMD_DRAIN,              /* send a full drain request to driver */
    COMPRESS_CMD_PARTIAL_DRAIN,      /* send a partial drain request to driver */
    COMPRESS_WAIT_FOR_BUFFER,    /* wait for buffer released by driver */
    COMPERSS_CMD_PAUSE,
    COMPERSS_CMD_RESUME,
    COMPERSS_CMD_START,
    COMPERSS_CMD_STOP,
    COMPERSS_CMD_SEND_DATA,
    COMPERSS_CMD_CLOSE,
    COMPERSS_CMD_CHANGE_DEV,
    COMPERSS_CMD_NEXTTRACK,
}COMPRESS_CMD_T;


struct nexttrack_param {
        int stream_id;
};


struct dev_set_param {
        uint32_t card;
        uint32_t device;
        struct pcm_config  *config;
        void * mixerctl;
};


struct compress_cmd {
    struct listnode node;
    COMPRESS_CMD_T cmd;
    bool is_sync;
    sem_t   sync_sem;
    int ret;
    void* param;
};

#define PARTIAL_DRAIN_ACK_EARLY_BY_MSEC  200000000


#define LARGE_PCM_BUFFER_TIME   400

#define STREAM_CACHE_MAX   2048
#define SPRD_AUD_DRIVER "/dev/audio_offloadf"


#define SPRD_AUD_IOCTL_MAGIC 	'A'

#define AUD_WAIT      			   _IO(SPRD_AUD_IOCTL_MAGIC, 1)
#define AUD_LOCK                  _IOW(SPRD_AUD_IOCTL_MAGIC, 2,int)
#define AUD_WAKEUP                _IO(SPRD_AUD_IOCTL_MAGIC, 3)


/* Stream id switches between 1 and 2 */
#define NEXT_STREAM_ID(stream_id) ((stream_id & 1) + 1)

#define STREAM_ARRAY_INDEX(stream_id) (stream_id - 1)

#define MAX_NUMBER_OF_STREAMS 2

#define MAX_CLEAN_NUM 4

struct compr_gapless_state {
    bool set_next_stream_id;
    int32_t stream_opened[MAX_NUMBER_OF_STREAMS];
    uint32_t initial_samples_drop;
    uint32_t trailing_samples_drop;
    uint32_t gapless_transition;
    uint32_t gapless_transition_delaying;
    uint32_t partial_drain_waiting;
    uint32_t next_song_pcm_bytes;
    uint32_t songs_count;
    bool use_dsp_gapless_mode;
};


struct fade_in
{
    int16_t step;
    int32_t max_count;
};


struct  stream {
    struct ringbuffer * ring;
};



struct compress {
                int32_t aud_fd;
                int ready;
                struct compr_config *config;
                uint32_t flags;
                uint8_t error[2];

                int card;
                int device;
                struct pcm *pcm;
                struct pcm_config pcm_cfg;
                pthread_mutex_t  pcm_mutex;
                pthread_mutex_t  lock;
                pthread_t        thread;
                int running;
                int32_t  state;


                pthread_mutex_t  cond_mutex;
                pthread_cond_t   cond;
                int buffer_ready;

                pthread_mutex_t  drain_mutex;
                pthread_cond_t   drain_cond;
                int drain_ok;
                int drain_waiting;

                pthread_mutex_t  partial_drain_mutex;
                pthread_cond_t   partial_drain_cond;
                int partial_drain_ok;
                int partial_drain_interrupt;
                int partial_drain_waiting;
                int partial_wait_interrupt;
                pthread_mutex_t  partial_wait_mutex;
                pthread_cond_t   partial_wait_cond;

                uint32_t start_time;
                uint32_t samplerate;
                uint32_t channels;
                uint32_t bitrate;

                int32_t is_first_frame;

                pthread_cond_t   cmd_cond;
                pthread_mutex_t  cmd_mutex;

                struct listnode cmd_list;

                void *mMP3DecHandle;
                int16_t mLeftBuf[MP3_DEC_FRAME_LEN];  //output pcm buffer
                int16_t mRightBuf[MP3_DEC_FRAME_LEN];
                uint8_t mMaxFrameBuf[MP3_MAX_DATA_FRAME_LEN];
                int16_t pOutputBuffer[MP3_DEC_FRAME_LEN*2];

                int16_t *pOutPcmBuffer;

                FT_MP3_ARM_DEC_Construct mMP3_ARM_DEC_Construct;
                FT_MP3_ARM_DEC_Deconstruct mMP3_ARM_DEC_Deconstruct;
                FT_MP3_ARM_DEC_InitDecoder mMP3_ARM_DEC_InitDecoder;
                FT_MP3_ARM_DEC_DecodeFrame mMP3_ARM_DEC_DecodeFrame;

                struct  stream  stream;

                uint32_t pcm_bytes_left;
                uint32_t pcm_bytes_offset;
                struct timespec ts;

                struct timespec end_ts;
                uint32_t loop_state;
                struct mixer *mixer;
                struct mixer_ctl *position_ctl;
                unsigned long prev_samples;
                unsigned long pause_position;

                struct  stream  pcm_stream;
                int32_t play_cache_pcm_first;
                struct stream  cache_pcm_stream;

                struct resampler_itfe  *resampler;
                uint32_t resample_in_samplerate;
                uint32_t resample_out_samplerate;
                uint32_t resample_channels;
                int16_t* resample_buffer;
                uint32_t resample_buffer_len;
                uint32_t resample_in_max_bytes;
                int no_src_data;
                uint32_t fade_step;

                uint32_t fade_step_2;
                uint32_t pcm_time_ms;
                long sleep_time;

                struct fade_in  fade_resume;
                struct fade_in  fade_start;

                unsigned long bytes_decoded;
                unsigned long bytes_decoded_after_src;

                uint32_t  noirq_frames_per_msec;
                uint8_t   is_start;

                pthread_mutex_t  position_mutex;

                long left_vol;
                long right_vol;
                AUDIO_PCMDUMP_FUN dumppcmfun;
                int avail_buffer_cleaned;
                bool use_dsp_gapless_mode;
                pthread_mutex_t  gapless_state_mutex;
                struct compr_gapless_state gapless_state;
                pthread_mutex_t  wait_for_stream_avail_mutex;
                pthread_cond_t   wait_for_stream_avail_cond;
                int wait_for_stream_interrupt;
                uint32_t next_stream;
                int wait_for_next_stream;

                unsigned long songs_pcm_bytes;
                unsigned long songs_pcm_decoded_bytes;
                unsigned long played_pcm_bytes;

                //used by client
                int client_stream_id;

                //used by thread
                int cur_stream_id;
                int next_stream_id;

                int pcm_bytes_write_to_dev;
                unsigned int start_threshold;
                int dev_start;
                int is_end;
};

static int compress_wait_for_stream_avail_notify(struct compress *compress,int interrupt);
static int compress_signal(struct compress * compress);
static void compress_write_data_notify(struct compress *compress);
extern int pcm_avail_update(struct pcm *pcm);
extern int pcm_state(struct pcm *pcm);
static int compress_wait_for_stream_avail_notify_no_lock(struct compress *compress,int interrupt);
static void compress_gapless_transition_check(struct compress *compress);
static int compress_pcm_state_check_no_lock(struct compress *compress);


//#define COMPRESS_DEBUG_INTERNAL

#ifdef COMPRESS_DEBUG_INTERNAL

static FILE * g_out_fd = NULL;

static FILE * g_out_fd2 = NULL;

int16_t test_buf[2*1024*1024] = {0};

static int out_dump_create(FILE **out_fd, const char *path)
{
    if (path == NULL) {
        ALOGE("path not assigned.");
        return -1;
    }
    *out_fd = (FILE *)fopen(path, "wb");
    if (*out_fd == NULL ) {
        ALOGE("cannot create file.");
        return -1;
    }
    ALOGI("path %s created successfully.", path);
    return 0;
}


static int out_dump_doing2( const void* buffer, size_t bytes)
{
    int ret;
    if(!g_out_fd) {
#ifdef AUDIOHAL_V4
    out_dump_create(&g_out_fd,"/data/vendor/local/media/peter_track.pcm");
#else
    out_dump_create(&g_out_fd,"/data/local/media/peter_track.pcm");
#endif
    }
    if (g_out_fd) {
        ret = fwrite((uint8_t *)buffer, bytes, 1, g_out_fd);
        if (ret < 0) ALOGW("%d, fwrite failed.", bytes);
    } else {
        ALOGW("out_fd is NULL, cannot write.");
    }
    return 0;
}

static int out_dump_doing3( const void* buffer, size_t bytes)
{
    int ret;
    if(!g_out_fd2) {
#ifdef AUDIOHAL_V4
        out_dump_create(&g_out_fd2,"/data/vendor/local/media/peter_track2.pcm");
#else
        out_dump_create(&g_out_fd2,"/data/local/media/peter_track2.pcm");
#endif
    }
    if (g_out_fd2) {
        ret = fwrite((uint8_t *)buffer, bytes, 1, g_out_fd2);
        if (ret < 0) ALOGW("%d, fwrite failed.", bytes);
    } else {
        ALOGW("out_fd is NULL, cannot write.");
    }
    return 0;
}


#endif




static uint32_t getNextMdBegin(uint8_t *frameBuf) {
    uint32_t header = 0;
    uint32_t result = 0;
    uint32_t offset = 0;

    if (frameBuf) {
        header = (frameBuf[0]<<24)|(frameBuf[1]<<16)|(frameBuf[2]<<8)|frameBuf[3];
        offset += 4;

        unsigned layer = (header >> 17) & 3;

        if (layer == 1) {
            //only for layer3, else next_md_begin = 0.

            if ((header & 0xFFE60000L) == 0xFFE20000L) {
                if (!(header & 0x00010000L)) {
                    offset += 2;
                    if (header & 0x00080000L) {
                        result = ((uint32_t)frameBuf[7]>>7)|((uint32_t)frameBuf[6]<<1);
                    } else {
                        result = frameBuf[6];
                    }
                } else {
                    if (header & 0x00080000L) {
                        result = ((uint32_t)frameBuf[5]>>7)|((uint32_t)frameBuf[4]<<1);
                    } else {
                        result = frameBuf[4];
                    }
                }
            }
        }
    }
    return result;
}


static uint32_t getCurFrameBitRate(uint8_t *frameBuf) {
    uint32_t header = 0;
    uint32_t bitrate = 0;

    if (frameBuf) {
        header = (frameBuf[0]<<24)|(frameBuf[1]<<16)|(frameBuf[2]<<8)|frameBuf[3];

        unsigned layer = (header >> 17) & 3;
        unsigned bitrate_index = (header >> 12) & 0x0f;
        unsigned version = (header >> 19) & 3;

        if (layer == 3) {
            // layer I
            static const int kBitrateV1[] = {
                32, 64, 96, 128, 160, 192, 224, 256,
                288, 320, 352, 384, 416, 448
            };

            static const int kBitrateV2[] = {
                32, 48, 56, 64, 80, 96, 112, 128,
                144, 160, 176, 192, 224, 256
            };

            bitrate =
                (version == 3 /* V1 */)
                ? kBitrateV1[bitrate_index - 1]
                : kBitrateV2[bitrate_index - 1];

        } else {
            // layer II or III
            static const int kBitrateV1L2[] = {
                32, 48, 56, 64, 80, 96, 112, 128,
                160, 192, 224, 256, 320, 384
            };

            static const int kBitrateV1L3[] = {
                32, 40, 48, 56, 64, 80, 96, 112,
                128, 160, 192, 224, 256, 320
            };

            static const int kBitrateV2[] = {
                8, 16, 24, 32, 40, 48, 56, 64,
                80, 96, 112, 128, 144, 160
            };

            if (version == 3 /* V1 */) {
                bitrate = (layer == 2 /* L2 */)
                          ? kBitrateV1L2[bitrate_index - 1]
                          : kBitrateV1L3[bitrate_index - 1];
            } else {
                // V2 (or 2.5)
                bitrate = kBitrateV2[bitrate_index - 1];
            }
        }
    }
    return bitrate;
}


static int set_audio_affinity(pid_t tid){
    cpu_set_t cpu_set;
    int cpu_num = 0;

    CPU_ZERO(&cpu_set);
    CPU_SET(cpu_num, &cpu_set);
    return sched_setaffinity(tid, sizeof(cpu_set), &cpu_set);
}

static bool getMPEGAudioFrameSize(
        uint32_t header, uint32_t *frame_size,
        int *out_sampling_rate, int *out_channels,
        int *out_bitrate, int *out_num_samples) {
    *frame_size = 0;

    if (out_sampling_rate) {
        *out_sampling_rate = 0;
    }

    if (out_channels) {
        *out_channels = 0;
    }

    if (out_bitrate) {
        *out_bitrate = 0;
    }

    if (out_num_samples) {
        *out_num_samples = 1152;
    }

    if ((header & 0xffe00000) != 0xffe00000) {
        return false;
    }

    unsigned version = (header >> 19) & 3;

    if (version == 0x01) {
        return false;
    }

    unsigned layer = (header >> 17) & 3;

    if (layer == 0x00) {
        return false;
    }

    unsigned protection = (header >> 16) & 1;

    unsigned bitrate_index = (header >> 12) & 0x0f;

    if (bitrate_index == 0 || bitrate_index == 0x0f) {
        // Disallow "free" bitrate.
        return false;
    }

    unsigned sampling_rate_index = (header >> 10) & 3;

    if (sampling_rate_index == 3) {
        return false;
    }

    static const int kSamplingRateV1[] = { 44100, 48000, 32000 };
    int sampling_rate = kSamplingRateV1[sampling_rate_index];
    if (version == 2 /* V2 */) {
        sampling_rate /= 2;
    } else if (version == 0 /* V2.5 */) {
        sampling_rate /= 4;
    }

    unsigned padding = (header >> 9) & 1;

    if (layer == 3) {
        // layer I

        static const int kBitrateV1[] = {
            32, 64, 96, 128, 160, 192, 224, 256,
            288, 320, 352, 384, 416, 448
        };

        static const int kBitrateV2[] = {
            32, 48, 56, 64, 80, 96, 112, 128,
            144, 160, 176, 192, 224, 256
        };

        int bitrate =
            (version == 3 /* V1 */)
                ? kBitrateV1[bitrate_index - 1]
                : kBitrateV2[bitrate_index - 1];

        if (out_bitrate) {
            *out_bitrate = bitrate;
        }

        *frame_size = (12000 * bitrate / sampling_rate + padding) * 4;

        if (out_num_samples) {
            *out_num_samples = 384;
        }
    } else {
        // layer II or III

        static const int kBitrateV1L2[] = {
            32, 48, 56, 64, 80, 96, 112, 128,
            160, 192, 224, 256, 320, 384
        };

        static const int kBitrateV1L3[] = {
            32, 40, 48, 56, 64, 80, 96, 112,
            128, 160, 192, 224, 256, 320
        };

        static const int kBitrateV2[] = {
            8, 16, 24, 32, 40, 48, 56, 64,
            80, 96, 112, 128, 144, 160
        };

        int bitrate;
        if (version == 3 /* V1 */) {
            bitrate = (layer == 2 /* L2 */)
                ? kBitrateV1L2[bitrate_index - 1]
                : kBitrateV1L3[bitrate_index - 1];

            if (out_num_samples) {
                *out_num_samples = 1152;
            }
        } else {
            // V2 (or 2.5)

            bitrate = kBitrateV2[bitrate_index - 1];
            if (out_num_samples) {
                *out_num_samples = (layer == 1 /* L3 */) ? 576 : 1152;
            }
        }

        if (out_bitrate) {
            *out_bitrate = bitrate;
        }

        if (version == 3 /* V1 */) {
            *frame_size = 144000 * bitrate / sampling_rate + padding;
        } else {
            // V2 or V2.5
            size_t tmp = (layer == 1 /* L3 */) ? 72000 : 144000;
            *frame_size = tmp * bitrate / sampling_rate + padding;
        }
    }

    if (out_sampling_rate) {
        *out_sampling_rate = sampling_rate;
    }

    if (out_channels) {
        int channel_mode = (header >> 6) & 3;

        *out_channels = (channel_mode == 3) ? 1 : 2;
    }

    return true;
}

static uint32_t U32_AT(const uint8_t *ptr) {
    return ptr[0] << 24 | ptr[1] << 16 | ptr[2] << 8 | ptr[3];
}


static uint32_t  getCurrentTimeUs()
{
   struct timeval tv;
   gettimeofday(&tv,NULL);
   return (tv.tv_sec* 1000000 + tv.tv_usec)/1000;
}



int is_compress_ready(struct compress *compress)
{
    return compress->ready;
}

int is_compress_running(struct compress *compress)
{
    return compress->running ? 1 : 0;
}

const char *compress_get_error(struct compress *compress)
{
    return compress->error;
}


int compress_set_gapless_metadata(struct compress *compress,
    struct compr_gapless_mdata *mdata)
{
        return 0;

}


static int compress_wait_event(struct compress * compress,pthread_mutex_t  *mutex)
{
    int ret = 0;
    if(compress->aud_fd) {
        pthread_mutex_unlock(mutex);
        ret = ioctl(compress->aud_fd, AUD_WAIT, NULL);
        pthread_mutex_lock(mutex);
    }
    return ret;
}

static int compress_signal(struct compress * compress)
{
    int ret = 0;
    pthread_cond_signal(&compress->cmd_cond);
    if(compress->aud_fd > 0) {
        ret = ioctl(compress->aud_fd, AUD_WAKEUP, NULL);
        if(ret) {
            ALOGE("compress_signal error %d",errno);
        }
    }
    return ret;
}


static void fade_init(struct fade_in *fade ,uint32_t max_count)
{
    fade->max_count = max_count;
}

static void fade_enable(struct fade_in *fade )
{
    fade->step = 1;
}

static void fade_disable(struct fade_in *fade )
{
    fade->step = fade->max_count;
}


static void fade_in_process(struct fade_in *fade,int16_t * buf, uint32_t bytes,uint32_t channels)
{
    int i = 0;
    uint32_t frames = 0;

    if(fade->step != fade->max_count) {
        if(channels == 2) {
            frames = bytes >> 2;
            for( i=0; i< frames; i++) {
                buf[2*i] = (int16_t)((int32_t)buf[2*i] *fade->step/fade->max_count);
                buf[2*i+1] = (int16_t)((int32_t)buf[2*i+1]*fade->step/fade->max_count);
                fade->step ++;
                if(fade->step == fade->max_count) {
                    break;
                }
            }
        }
        else if(channels == 1) {
            frames = bytes >> 1;
            for( i=0; i< frames; i++) {
                buf[i] = (int16_t)((int32_t)buf[i] *fade->step/fade->max_count);
                fade->step ++;
                if(fade->step == fade->max_count) {
                    break;
                }
            }
        }
    }
}

static void digital_gain_process(struct compress *compress,int16_t * buf, uint32_t bytes,uint32_t channels)
{
    int i = 0;
    uint32_t frames = 0;

    if((compress->left_vol!= AUDIO_OFFLOAD_PLAYBACK_VOLUME_MAX)
        || (compress->right_vol!= AUDIO_OFFLOAD_PLAYBACK_VOLUME_MAX)){
        if(channels == 2) {
            frames = bytes >> 2;
            for( i=0; i< frames; i++) {
                buf[2*i] = (int16_t)(((int32_t)buf[2*i] *compress->left_vol)>> 12);
                buf[2*i+1] = (int16_t)(((int32_t)buf[2*i+1]*compress->right_vol) >>12);
            }
        }
        else if(channels == 1) {
            frames = bytes >> 1;
            for( i=0; i< frames; i++) {
                buf[i] = (int16_t)(((int32_t)buf[i] *compress->left_vol) >> 12);
            }
        }
    }
}





static  int stream_comsume(struct  stream * stream, uint32_t bytes)
{
    if(stream->ring) {
        return ringbuffer_consume(stream->ring,bytes);
    }
    else {
        return -1;
    }
}

static int stream_copy(struct  stream * dst_stream,struct  stream * src_stream)
{
    dst_stream->ring = ringbuffer_copy(src_stream->ring);
    return 0;
}

static  int stream_write(struct  stream * stream, uint8_t *buf, uint32_t bytes, uint32_t *bytes_written)
{
    int ret = 0;
    ret = ringbuffer_write(stream->ring,buf,bytes);
    if(!ret) {
        *bytes_written = bytes;
    }
    else {
        *bytes_written = 0;
    }
    return ret;
}

static  int stream_read(struct  stream * stream, uint8_t *buf, uint32_t bytes,uint32_t * read)
{
    int ret = 0;
    ret = ringbuffer_read(stream->ring,buf,bytes);
    if(!ret) {
        *read = bytes;
    }
    else {
        *read = 0;
        ret = -1;
    }
    return ret;

}

static  int32_t stream_peek(struct  stream * stream, uint8_t *buf, uint32_t offset,uint32_t bytes)
{
    int ret = 0;
    if(stream->ring) {
        ret = ringbuffer_peek(stream->ring,buf, offset,bytes);
    }
    else {
        ret = -1;
    }
    return ret;
}

static  int stream_clean(struct  stream * stream)
{
    int ret = 0;
    if(stream->ring) {
        ret = ringbuffer_reset(stream->ring);
    }
    return ret;
}

static  uint32_t stream_total_bytes(struct  stream * stream)
{
    if(stream->ring) {
        return ringbuffer_total_write(stream->ring);
     }
    else {
        return 0;
    }
}


static  uint32_t stream_data_bytes(struct  stream * stream)
{
    if(stream->ring) {
        return ringbuffer_data_bytes(stream->ring);
    }
    else {
        return 0;
    }
}

static  uint32_t stream_buffer_bytes(struct  stream * stream)
{
    if(stream->ring) {
        return ringbuffer_buf_bytes(stream->ring);
    }
    else {
        return 0;
    }
}




static  int32_t  mp3_demux_frame(struct  stream  *stream,uint8_t * buf, uint32_t * frame_len,
                    uint32_t   *pNextbegin,uint32_t * pBitrate,uint32_t *pSamplerate,uint32_t *pChannel)
{
        int             ret = 0;
        uint8_t     header_buf[10];
        uint32_t    bytes_read;
        uint32_t    header = 0;
        uint32_t    frame_size;
        int             bitrate;
        int             num_samples;
        int             sample_rate;
        int             channels = 0;
        uint32_t    nextbegin;

        //    ALOGE(" compress->state %d,total_left %d",compress->state,total_left);
        ret = stream_peek(stream,header_buf,0,sizeof(header_buf));
        if(ret) {
            ALOGE("peter: stream_peek error out %d",ret);
            return -ERROR_NO_DATA;
        }
        // ALOGE("peter: stream_peek out %d",ret);
        header = U32_AT(header_buf);
        ret = getMPEGAudioFrameSize(header, &frame_size,&sample_rate, &channels,&bitrate, &num_samples);
        //ALOGE("peter:  frame_size %d, sample_rate: %d,channel: %d, bitrate : %d, num_damples: %d",frame_size,sample_rate,channels,bitrate,num_samples);

        if((ret == false) || (frame_size <= 0) || (frame_size > MP3_MAX_DATA_FRAME_LEN)) {
            ret = stream_read(stream,header_buf,1,&bytes_read);
            //out_dump_doing2(header_buf,bytes_read);
            //ALOGE("peter error framesize %d",frame_size,ret);
            return -ERROR_OTHER;
        }

        ret = stream_peek(stream,header_buf,frame_size,sizeof(header_buf));
        if(ret) {
            ALOGE("peter: stream_peek ret %d",ret);
            return -ERROR_NO_DATA;
        }
        nextbegin =getNextMdBegin(header_buf);

        ret = stream_read(stream,buf,frame_size,&bytes_read);
        if(ret) {
            ALOGE("stream_read error");
            return -ERROR_NO_DATA;;
        }
       // out_dump_doing2(buf,bytes_read);

        *frame_len = frame_size;
        *pBitrate = bitrate;
        *pSamplerate = sample_rate;
        *pChannel = channels;
        *pNextbegin = nextbegin;

        return ret;

}



static int32_t mp3_decode_process(struct compress *compress,int16_t* buf, uint32_t * bytes,uint32_t channels_req)
{
        int             i = 0;
        uint32_t    numOutBytes = 0;
        int             ret = 0;
        uint32_t    frame_size;
        int             bitrate;
        int             samplerate;
        int             channels = 0;
        uint32_t    nextbegin = 0;
        FRAME_DEC_T inputParam ;
        OUTPUT_FRAME_T outputFrame ;
        int  decoderRet = 0;
        int consecutive_err_cnt = 0;
        do {
            ret = mp3_demux_frame(&compress->stream,compress->mMaxFrameBuf, &frame_size,& nextbegin,&bitrate,&samplerate,&channels);
            if(-ERROR_OTHER == ret) {
                consecutive_err_cnt++;
            }
        }while((-ERROR_OTHER == ret) && consecutive_err_cnt < 2048);
        if(ret) {
            ALOGE("mp3_demux_frame error ret %d",ret);
            return ret;
        }

        //ALOGE("peter  ok 1 ");
        memset(&inputParam, 0, sizeof(FRAME_DEC_T));
        memset(&outputFrame, 0, sizeof(OUTPUT_FRAME_T));

        inputParam.next_begin = nextbegin;
        inputParam.bitrate = bitrate; //kbps
        inputParam.frame_buf_ptr = compress->mMaxFrameBuf;
        inputParam.frame_len = frame_size;

        outputFrame.pcm_data_l_ptr = compress->mLeftBuf;
        outputFrame.pcm_data_r_ptr =compress->mRightBuf;

        // ALOGE("peter: mNextMdBegin is %d,frame_size_dec: %d,bitrate:%d",mNextMdBegin,frame_size_dec,bitrate);

        //out_dump_doing2(inputParam.frame_buf_ptr,frame_size_dec);
        MP3_ARM_DEC_DecodeFrame(compress->mMP3DecHandle, &inputParam,&outputFrame, &decoderRet);
        //frames_dec++;
   // ALOGE("peter: decoderRet is %d,outputFrame: %d,frames_dec:%d",decoderRet,outputFrame.pcm_bytes,frames_dec);
        if(decoderRet != MP3_ARM_DEC_ERROR_NONE) { //decoder error
            ALOGE("MP3 decoder returned error %d, substituting silence", decoderRet);
            outputFrame.pcm_bytes = 1152; //samples number
        }

        if(decoderRet != MP3_ARM_DEC_ERROR_NONE) {
            ALOGE("MP3 decoder returned error %d, substituting silence", decoderRet);
            numOutBytes = outputFrame.pcm_bytes * sizeof(int16_t) *2;
            memset(buf, 0, numOutBytes);
        } else {
            for( i=0; i<outputFrame.pcm_bytes; i++) {
                if(channels_req == 1) {
                    numOutBytes = outputFrame.pcm_bytes * sizeof(int16_t) ;
                    if (2 == channels) {
                        buf[i] =(compress->mLeftBuf[i] + compress->mRightBuf[i])/2;
                    } else {
                        buf[i] = compress->mLeftBuf[i];
                    }
                }
                else if(channels_req == 2) {
                    numOutBytes = outputFrame.pcm_bytes * sizeof(int16_t) *2;
                    if (2 == channels) {
                        buf[2*i] =compress->mLeftBuf[i];
                        buf[2*i+1] =compress->mRightBuf[i];
                    } else {
                        buf[2*i] = compress->mLeftBuf[i];
                        buf[2*i+1] = compress->mLeftBuf[i];
                    }
                }
            }
        }

        *bytes = numOutBytes;

        compress->samplerate = samplerate;
        compress->channels = channels;
        compress->bitrate = bitrate;

       return 0;

}

static void compress_pcm_restart(struct compress *compress)
{
    ALOGE("compress_pcm_restart");
    compress->pcm= pcm_open(compress->card, compress->device, PCM_OUT| PCM_MMAP |PCM_NOIRQ |PCM_MONOTONIC, &compress->pcm_cfg);
    if(!pcm_is_ready(compress->pcm)) {
        pcm_close(compress->pcm);
        compress->pcm = NULL;
    }
    if((compress->no_src_data == 1) && (stream_data_bytes(&compress->pcm_stream)< compress->start_threshold)) {
            compress->played_pcm_bytes += stream_data_bytes(&compress->pcm_stream);
            ALOGE("pcm_restart but not enough pcm data :%d,start_threshhold:%d",stream_data_bytes(&compress->pcm_stream),compress->start_threshold);
            stream_comsume(&compress->pcm_stream,stream_data_bytes(&compress->pcm_stream));
            stream_copy(&compress->cache_pcm_stream,&compress->pcm_stream);
            compress->play_cache_pcm_first = 0;
    }
    else {
        if(stream_data_bytes(&compress->pcm_stream)) {
            stream_copy(&compress->cache_pcm_stream,&compress->pcm_stream);
            compress->play_cache_pcm_first = 1;
        }
        fade_enable(&compress->fade_resume);
    }
    compress->prev_samples = 0;
    compress->pcm_bytes_left = 0;
    compress->pcm_bytes_write_to_dev = 0;
    compress->dev_start = 0;

}

extern int pcm_mmap_avail_buffer_clean(struct pcm *pcm);

static int32_t audio_process(struct compress *compress)
{
        int ret = 0;
        int ret1=0;
        uint32_t avail = 0;
        uint32_t frame_size_dec= 0;
        uint32_t bytes_write = 0;
        uint32_t numOutBytes = 0;
        uint32_t bytes_written = 0;
        uint32_t bytes_read = 0;

        pthread_mutex_lock(&compress->pcm_mutex);
        if(compress->pcm_bytes_left ==  0) {
            if(compress->play_cache_pcm_first) {
                ALOGE(" cache pcm: length:%d",stream_data_bytes(&compress->cache_pcm_stream));
                if(stream_data_bytes(&compress->cache_pcm_stream)) {
                    int32_t read_len = stream_data_bytes(&compress->cache_pcm_stream) > (1152*4)? (1152*4):stream_data_bytes(&compress->cache_pcm_stream);
                    ret = stream_read(&compress->cache_pcm_stream,compress->pOutputBuffer,read_len,&numOutBytes);
                    ALOGE(" cache pcm: length:%d,copy:%d,read_bytes:%d,ret: %d",
                    stream_data_bytes(&compress->cache_pcm_stream),read_len,numOutBytes,ret);
                    //out_dump_doing2((uint8_t*)compress->pOutputBuffer,numOutBytes);
                    if(ret) {
                        //out_dump_doing2((uint8_t*)compress->pOutputBuffer,numOutBytes);
                        compress->play_cache_pcm_first = 0;
                        numOutBytes = 0;
                        compress->pOutPcmBuffer = NULL;
                    }
                    else {
                        compress->pOutPcmBuffer = compress->pOutputBuffer;
                    }
                }
                else {
                    compress->play_cache_pcm_first = 0;
                    numOutBytes = 0;
                    compress->pOutPcmBuffer = NULL;
                }
            }
            else if (stream_buffer_bytes(&compress->pcm_stream)){
                pthread_mutex_unlock(&compress->pcm_mutex);
                ret = mp3_decode_process(compress,compress->pOutputBuffer, &numOutBytes,compress->pcm_cfg.channels);
                pthread_mutex_lock(&compress->pcm_mutex);
               // ALOGE("mp3_decode_process out:%d ,numOutBytes:%d,channels:%d",ret, numOutBytes,compress->pcm_cfg.channels);
                if(ret) {
                    numOutBytes = 0;
                    struct timespec cur_ts;
                    if(ret == -ERROR_NO_DATA) {
                        ret1 = ret;
                        compress_write_data_notify(compress);
                        compress->no_src_data = 1;
                        if ((compress->end_ts.tv_sec == 0)&& (compress->end_ts.tv_nsec== 0)) {
                            clock_gettime(CLOCK_MONOTONIC, &compress->end_ts);
                        }
                        pthread_mutex_lock(&compress->gapless_state_mutex);
                        if((compress->gapless_state.set_next_stream_id == true)
                            &&(compress->gapless_state.partial_drain_waiting == 1)
                            &&(compress->gapless_state.gapless_transition == 0)){
                            ALOGE("peter: compress->bytes_decoded:%lu",compress->bytes_decoded);
                            compress->songs_pcm_bytes = compress->bytes_decoded_after_src;
                            compress->songs_pcm_decoded_bytes = compress->bytes_decoded;
                        }
                        pthread_mutex_unlock(&compress->gapless_state_mutex);
                        if(compress->pcm) {
                            int32_t avail = 0;
                            avail = pcm_avail_update(compress->pcm);
                            if(compress->avail_buffer_cleaned < MAX_CLEAN_NUM){
                                pcm_mmap_avail_buffer_clean(compress->pcm);
                                compress->avail_buffer_cleaned ++;
                            }
                        }
                    }
                    clock_gettime(CLOCK_MONOTONIC, &cur_ts);
                    ALOGE("peter: mp3_decode_processs error %d, pcm left : %d, gapless_transition :%d,compress->played_pcm_bytes:%lu,compress->songs_pcm_bytes:%lu",ret,stream_data_bytes(&compress->pcm_stream),compress->gapless_state.gapless_transition,compress->played_pcm_bytes,compress->songs_pcm_bytes);
                    pthread_mutex_lock(&compress->gapless_state_mutex);
                    if((!compress->gapless_state.gapless_transition) && (!compress->gapless_state.partial_drain_waiting )) {
                        if((!stream_data_bytes(&compress->pcm_stream)) || ((cur_ts.tv_sec > (compress->end_ts.tv_sec +1)) && (cur_ts.tv_nsec >= compress->end_ts.tv_nsec))){
                            compress->is_end = 1;
                            pthread_mutex_unlock(&compress->gapless_state_mutex);
                            pthread_mutex_unlock(&compress->pcm_mutex);
                            uint32_t stream_index = STREAM_ARRAY_INDEX(compress->cur_stream_id);
                            if(stream_data_bytes(&compress->pcm_stream)) {
                                stream_comsume(&compress->pcm_stream,stream_data_bytes(&compress->pcm_stream));
                            }
                            ALOGE("peter: pcm data is end and return to end playback %d,stream_index:%d",stream_data_bytes(&compress->pcm_stream),stream_index);
                            return -ERROR_NO_PCM_DATA;
                        }
                    }
                    else {
                        compress_pcm_state_check_no_lock(compress);
                        compress_gapless_transition_check(compress);
                    }
                    pthread_mutex_unlock(&compress->gapless_state_mutex);
                }
                else {
                    compress->avail_buffer_cleaned = 0;
                    compress->end_ts.tv_sec = 0;
                    compress->end_ts.tv_nsec = 0;
                    compress->no_src_data = 0;
                    compress->bytes_decoded += numOutBytes;
                    compress->is_end = 0;

                    if((stream_data_bytes(&compress->stream) <= compress->config->fragment_size)
                           && (!compress->partial_drain_waiting)
                           && (!compress->drain_waiting)) {
                        compress_write_data_notify(compress);
                    }
                }
                if((compress->samplerate != compress->pcm_cfg.rate)&& numOutBytes) {
                    uint32_t in_frames = 0;
                    uint32_t out_frames = 0;
                    if((compress->resampler == NULL)
                        ||(compress->resample_in_samplerate != compress->samplerate)
                        || (compress->resample_out_samplerate != compress->pcm_cfg.rate)
                        || (compress->resample_channels != compress->pcm_cfg.channels)){

                        ALOGI("resampler recreate in samplerate:%d,out samplerate:%d, out channle:%d,orig:in rate:%d, orig: out rate:%d, orig channles:%d,max:%d,inbyts:%d",
                        compress->samplerate,compress->pcm_cfg.rate,compress->pcm_cfg.channels,
                        compress->resample_in_samplerate,compress->resample_out_samplerate,compress->resample_channels,compress->resample_in_max_bytes,numOutBytes);

                        if(compress->resampler) {
                            release_resampler(compress->resampler);
                            compress->resampler = NULL;
                        }
                        ret = create_resampler( compress->samplerate,
                            compress->pcm_cfg.rate,
                            compress->pcm_cfg.channels,
                            RESAMPLER_QUALITY_DEFAULT,
                            NULL,
                            &compress->resampler);
                        if (ret != 0) {
                            ALOGE("resample create failed");
                            compress->resampler = NULL;
                        }
                        else {
                            if(compress->resample_buffer) {
                                free(compress->resample_buffer);
                                compress->resample_buffer = NULL;
                            }
                            if(!compress->resample_buffer) {
                                compress->resample_buffer_len = numOutBytes*8*2;// (compress->pcm_cfg.rate*numOutBytes + compress->samplerate)/compress->samplerate + 1024;
                                compress->resample_buffer = malloc(compress->resample_buffer_len);
                                if(!compress->resample_buffer) {
                                    ALOGE("compress->resample_buffer malloc failed len:%d",compress->resample_buffer_len);
                                }
                                compress->resample_in_max_bytes = numOutBytes;
                            }
                            compress->resample_in_samplerate = compress->samplerate;
                            compress->resample_out_samplerate = compress->pcm_cfg.rate;
                            compress->resample_channels = compress->pcm_cfg.channels;
                        }
                    }

                    if(numOutBytes > compress->resample_in_max_bytes) {
                        if(compress->resample_buffer) {
                            free(compress->resample_buffer);
                            compress->resample_buffer = NULL;
                        }
                        compress->resample_buffer_len = numOutBytes*8*2;// (compress->pcm_cfg.rate*numOutBytes + compress->samplerate)/compress->samplerate + 1024;
                        compress->resample_buffer = malloc(compress->resample_buffer_len);
                        if(!compress->resample_buffer) {
                            ALOGE("compress->resample_buffer malloc failed len:%d",compress->resample_buffer_len);
                        }
                        compress->resample_in_max_bytes = numOutBytes;
                    }
                    in_frames = numOutBytes/(compress->pcm_cfg.channels<<1);
                    out_frames = compress->resample_buffer_len/(compress->pcm_cfg.channels<<1);
                    if(compress->resampler && compress->resample_buffer) {

                        compress->resampler->resample_from_input(compress->resampler,
                        (int16_t *)compress->pOutputBuffer,
                        &in_frames,
                        (int16_t *)compress->resample_buffer,
                        &out_frames);

                    }

                    numOutBytes = out_frames * (compress->pcm_cfg.channels<<1);
                    compress->pOutPcmBuffer = compress->resample_buffer;
                }
                else {
                    compress->pOutPcmBuffer = compress->pOutputBuffer;
                }
                if(ret == 0) {
                    fade_in_process(&compress->fade_start,compress->pOutPcmBuffer,numOutBytes,compress->pcm_cfg.channels);
                }
                ret = stream_write(&compress->pcm_stream,compress->pOutPcmBuffer,numOutBytes,&bytes_written);
                if(ret || (numOutBytes != bytes_written)) {
                    avail= pcm_avail_update(compress->pcm);
                    //avail = pcm_frames_to_bytes(compress->pcm, avail);
                    ALOGE("audio_process:stream_write error ret %d,numOutBytes: %d,bytes_written: %d,avail: %d,data bytes:%d,kernel_data_bytes:%d",
                    ret,numOutBytes,bytes_written,avail,stream_data_bytes(&compress->pcm_stream),pcm_frames_to_bytes(compress->pcm,pcm_get_buffer_size(compress->pcm)- avail));
                }
                compress->bytes_decoded_after_src += numOutBytes;
            }
            else {
                ALOGI("pcm_stream_is_full");
            }
            if(compress->pcm) {
                avail= pcm_avail_update(compress->pcm);
                avail = pcm_frames_to_bytes(compress->pcm, avail);
            }
            else {
                avail = 0;
            }
            bytes_write = numOutBytes>avail ?avail:numOutBytes;
            compress->pcm_bytes_left = numOutBytes - bytes_write;
            compress->pcm_bytes_offset = 0;
        }
        else {
            if(compress->pcm) {
                avail= pcm_avail_update(compress->pcm);
                avail = pcm_frames_to_bytes(compress->pcm, avail);
            }
            else {
                avail = 0;
            }
            bytes_write =compress->pcm_bytes_left> avail ? avail: compress->pcm_bytes_left;
            compress->pcm_bytes_left -= bytes_write;
        }
       // ALOGE("avail %d,bytes_write %d,compress->pcm_bytes_left %d,compress->pcm_bytes_offset:%d,compress->pcm:%x",avail,bytes_write,compress->pcm_bytes_left,compress->pcm_bytes_offset,(uint32_t)compress->pcm);

        if(compress->pcm){
            uint32_t cur_samples = 0;
            uint32_t cur_consumes_bytes = 0;
            //ret = stream_read_l(&compress->pcm_stream,test_buf,(cur_samples - compress->prev_samples)*4,&bytes_read);
            //out_dump_doing2((uint8_t*)test_buf,bytes_read);
            if(compress->pcm_time_ms < LARGE_PCM_BUFFER_TIME) {
                ret = stream_comsume(&compress->pcm_stream,bytes_write);
                cur_consumes_bytes = bytes_write;
            }
            else {
                if(compress->position_ctl){
                    cur_samples = mixer_ctl_get_value(compress->position_ctl, 0);
                }
                cur_consumes_bytes = (cur_samples - compress->prev_samples)*(compress->pcm_cfg.channels<<1);
                ret = stream_comsume(&compress->pcm_stream,cur_consumes_bytes);
                //ALOGE("peter: cur_consumes_bytes:%d,stream_data_bytes:%d",cur_consumes_bytes,stream_data_bytes(&compress->pcm_stream));
            }
            if(ret) {
                ALOGE("stream_comsume_error ret : %d,cur:%d,prev: %lu, bytes:%ld,numOutBytes:%d,avail:%d,compress->bytes_decoded:%lu,compress->no_src_data:%d,songs_pcm_bytes:%lu,played pcm:%lu,transition:%d",
                    ret,cur_samples,compress->prev_samples,(cur_samples - compress->prev_samples)*4,stream_data_bytes(&compress->pcm_stream),avail,
                    compress->bytes_decoded,compress->no_src_data,compress->songs_pcm_bytes,(compress->played_pcm_bytes *(compress->pcm_cfg.channels<<1)),
                    compress->gapless_state.gapless_transition);
                if(stream_data_bytes(&compress->pcm_stream)) {
                    stream_comsume(&compress->pcm_stream,stream_data_bytes(&compress->pcm_stream));
                }
                pthread_mutex_lock(&compress->gapless_state_mutex);
                compress->played_pcm_bytes = compress->bytes_decoded_after_src;
                compress->pcm_bytes_left = 0;
                compress->pcm_bytes_offset = 0;
                if(compress->gapless_state.gapless_transition) {
                    compress_gapless_transition_check(compress);
                }
                else {
                    if(compress->no_src_data == 1) {
                        pthread_mutex_unlock(&compress->gapless_state_mutex);
                        pthread_mutex_unlock(&compress->pcm_mutex);
                        return -ERROR_NO_PCM_DATA;
                    }
                }
                pthread_mutex_unlock(&compress->gapless_state_mutex);
            }
            else {
                compress->played_pcm_bytes += cur_consumes_bytes;
                #if 0
                ALOGI("peter_pcm:played:%d,bytes_decoded_after_src:%d,songs_pcm_bytes:%d,transition:%d,no_src_data:%d,pcm_cached:%d,song_pcm_cached:%d,pcm_left:%d,stream_avail:%d,total_avail:%d,cur_stream_id:%d,next_stream_id:%d,client_stream_id:%d,next_song_bytes:%d,bytes_decoded:%d,sones_pcm_decode_bytes:%d",compress->played_pcm_bytes,compress->bytes_decoded_after_src,compress->songs_pcm_bytes,
                compress->gapless_state.gapless_transition,compress->no_src_data,compress->bytes_decoded_after_src-compress->played_pcm_bytes,compress->songs_pcm_bytes-compress->played_pcm_bytes ,
                compress->pcm_bytes_left,stream_data_bytes(&compress->pcm_stream),compress->pcm_bytes_left+stream_data_bytes(&compress->pcm_stream),compress->cur_stream_id,compress->next_stream_id,compress->client_stream_id,compress->gapless_state.next_song_pcm_bytes,
                compress->bytes_decoded,compress->songs_pcm_decoded_bytes);
                #endif
                pthread_mutex_lock(&compress->gapless_state_mutex);
                compress_gapless_transition_check(compress);
                pthread_mutex_unlock(&compress->gapless_state_mutex);
            }

           // ALOGE("cur_samples %d,prev_samples %d, total bytes:%d,data bytes: %d,cur_samples1:%d,",cur_samples,compress->prev_samples,stream_total_bytes(&compress->pcm_stream),stream_data_bytes(&compress->pcm_stream),(stream_total_bytes(&compress->pcm_stream) - stream_data_bytes(&compress->pcm_stream))/4);
            compress->prev_samples = cur_samples;
          // out_dump_doing2((uint8_t*)compress->pOutputBuffer +compress->pcm_bytes_offset,bytes_write);

           if(bytes_write &&compress->pcm) {
                fade_in_process(&compress->fade_resume,compress->pOutPcmBuffer +compress->pcm_bytes_offset,bytes_write,compress->pcm_cfg.channels);
              //  if(1) {//ss->pcm_cfg.rate == 16000) {
                   // out_dump_doing2((uint8_t*)compress->pOutPcmBuffer +compress->pcm_bytes_offset,bytes_write);
                //}
                if(compress->pcm_time_ms < LARGE_PCM_BUFFER_TIME) {
                    digital_gain_process(compress,(int16_t *)((uint8_t*)compress->pOutPcmBuffer +compress->pcm_bytes_offset), bytes_write,compress->pcm_cfg.channels);
                }
                ret = pcm_mmap_write(compress->pcm,(uint8_t*)compress->pOutPcmBuffer +compress->pcm_bytes_offset,bytes_write);
                if(NULL!=compress->dumppcmfun){
                    compress->dumppcmfun((void *)((uint8_t *)compress->pOutPcmBuffer  +compress->pcm_bytes_offset),
                        bytes_write,DUMP_MUSIC_HWL_OFFLOAD_VBC);
                }
                //ALOGE("peter_pcm:kernel_bytes:%d",pcm_frames_to_bytes(compress->pcm,pcm_get_buffer_size(compress->pcm)- pcm_avail_update(compress->pcm)));
                if(ret < 0) {
                    ALOGE("pcm_mmap_write error");
                    pcm_close(compress->pcm);
                    compress_pcm_restart(compress);
                }
                else {
                    pthread_mutex_lock(&compress->gapless_state_mutex);
                    if(compress->gapless_state.gapless_transition && (compress->gapless_state.partial_drain_waiting==0)) {
                        if((compress->played_pcm_bytes < compress->songs_pcm_bytes) &&(compress->bytes_decoded_after_src > compress->songs_pcm_bytes)){
                            compress->gapless_state.next_song_pcm_bytes += bytes_write;
                        }
                    }
                    pthread_mutex_unlock(&compress->gapless_state_mutex);

                    compress->pcm_bytes_write_to_dev += bytes_write;
                    //ALOGE("compress->pcm_bytes_write_to_dev:%d,compress->start_threshold:%d,stream_data_bytes(&compress->pcm_stream:%d,compress->dev_start:%d,compress->no_src_data:%d,pcm_state:%d",compress->pcm_bytes_write_to_dev,compress->start_threshold,
                        //stream_data_bytes(&compress->pcm_stream),compress->dev_start,compress->no_src_data,pcm_state(compress->pcm));
                    if(!compress->dev_start) {
                        if((compress->no_src_data == 1) && (stream_data_bytes(&compress->pcm_stream)< compress->start_threshold)) {
                            if(compress->pcm_bytes_write_to_dev >= stream_data_bytes(&compress->pcm_stream)) {
                                if (compress->pcm) {
                                    pcm_close(compress->pcm);
                                    compress_pcm_restart(compress);
                                }
                                compress->dev_start = 0;
                                compress->played_pcm_bytes += stream_data_bytes(&compress->pcm_stream);
                                stream_comsume(&compress->pcm_stream,stream_data_bytes(&compress->pcm_stream));
                                ALOGI("pcm_start for not enough pcm data compress->pcm_bytes_write_to_dev:%d",compress->pcm_bytes_write_to_dev);
                            }
                        }
                        else if(compress->pcm_bytes_write_to_dev >= compress->start_threshold) {
                            if(pcm_state(compress->pcm) == PCM_STATE_RUNNING) {
                                compress->dev_start = 1;
                                ALOGI("pcm_start ok compress->pcm_bytes_write_to_dev:%d",compress->pcm_bytes_write_to_dev);
                            }
                            else {
                                if((compress->no_src_data == 1)
                                    &&(compress->pcm_bytes_write_to_dev == stream_data_bytes(&compress->pcm_stream))){
                                    compress->played_pcm_bytes += stream_data_bytes(&compress->pcm_stream);
                                    stream_comsume(&compress->pcm_stream,stream_data_bytes(&compress->pcm_stream));
                                    ALOGE("pcm data too less to make pcm_start to start");
                                }
                            }
                            //ALOGI("pcm_start compress->pcm_bytes_write_to_dev:%d:pcm_state:%d",compress->pcm_bytes_write_to_dev,pcm_state(compress->pcm));
                        }
                    }
                }
                if(compress->pcm_bytes_left) {
                    compress->pcm_bytes_offset += bytes_write;
                }
            }
        }
        pthread_mutex_unlock(&compress->pcm_mutex);

        return (ret1 == -ERROR_NO_DATA)?-ERROR_NO_DATA:0;
}

static unsigned long g_prev_samples = 0;

static void compress_gapless_transition_check(struct compress *compress)
{
    if(compress->gapless_state.gapless_transition) {
        ALOGI("peter: sond_end_notify pcm left : %d,compress->played_pcm_bytes:%lu,compress->songs_pcm_bytes:%lu,cur_stream_id:%d, next_stream_id:%d,pcm_state:%d",stream_data_bytes(&compress->pcm_stream),compress->played_pcm_bytes,
                    compress->songs_pcm_bytes,compress->cur_stream_id,compress->next_stream_id,pcm_state(compress->pcm));
        if(compress->played_pcm_bytes >= compress->songs_pcm_bytes) {
            uint32_t stream_index = STREAM_ARRAY_INDEX(compress->cur_stream_id);
            ALOGI("peter: pcm data is end and return to end playback %u,compress->songs_pcm_bytes:%lu",stream_data_bytes(&compress->pcm_stream),compress->songs_pcm_bytes);
            if (compress->gapless_state.gapless_transition){
                compress->gapless_state.gapless_transition = 0;
            }
            if(compress->gapless_state.set_next_stream_id
                &&compress->gapless_state.stream_opened[stream_index]) {
                compress->gapless_state.set_next_stream_id = false;
                compress->gapless_state.stream_opened[stream_index] = 0;
            }
            compress_wait_for_stream_avail_notify_no_lock(compress,0);
            if(compress->next_stream_id && (compress->next_stream_id != compress->cur_stream_id)) {
                compress->cur_stream_id = compress->next_stream_id;
                compress->next_stream_id = 0;
            }
            compress->gapless_state.next_song_pcm_bytes = 0;
            compress->bytes_decoded -= compress->songs_pcm_decoded_bytes;
            compress->songs_pcm_decoded_bytes = 0;
            compress->played_pcm_bytes -= compress->songs_pcm_bytes;
            compress->bytes_decoded_after_src -= compress->songs_pcm_bytes;
            compress->songs_pcm_bytes = 0;
            g_prev_samples = 0;
        }
    }
}

static int compress_wait_for_stream_avail_notify(struct compress *compress,int interrupt)
{
    pthread_mutex_lock(&compress->gapless_state_mutex);
    if(compress->next_stream) {
        if(interrupt) {
            compress->wait_for_stream_interrupt = 1;
        }
        pthread_cond_signal(&compress->wait_for_stream_avail_cond);
        compress->next_stream = 0;
    }
    pthread_mutex_unlock(&compress->gapless_state_mutex);
    return 0;
}

static int compress_wait_for_stream_avail_notify_no_lock(struct compress *compress,int interrupt)
{
    if(compress->next_stream) {
        if(interrupt) {
            compress->wait_for_stream_interrupt = 1;
        }
        pthread_cond_signal(&compress->wait_for_stream_avail_cond);
        compress->next_stream = 0;
    }
    return 0;
}



static int compress_wait_for_stream_avail(struct compress *compress,pthread_mutex_t  *mutex)
{
    int ret = 0;
    compress->next_stream = 1;
    if(!compress->wait_for_stream_interrupt) {
        pthread_cond_wait(&compress->wait_for_stream_avail_cond, mutex);
    }
    if(compress->wait_for_stream_interrupt){
        ret = -1;
    }
    compress->wait_for_stream_interrupt = 0;
    return ret;
}



static int compress_partial_drain_delay(struct compress *compress,long time)
{
    struct timespec ts;
    int ret = 0;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    ts.tv_nsec += time;
    if(ts.tv_nsec >1000000000) {
        ts.tv_nsec -= 1000000000;
        ts.tv_sec += 1;
    }
    pthread_mutex_lock(&compress->partial_wait_mutex);
    if(!compress->partial_wait_interrupt) {
#ifndef ANDROID4X
        ret = pthread_cond_timedwait(&compress->partial_wait_cond, &compress->partial_wait_mutex, &ts);
#else
        ret = pthread_cond_timedwait_monotonic(&compress->partial_wait_cond, &compress->partial_wait_mutex, &ts);
#endif
                        //ALOGE("wait out:%ld",ret);
        if(ret != ETIMEDOUT){
            ALOGE("pthread_cond_timedwait_monotonic ret no timeout");
        }
    }
    ALOGE("compress_partial_drain_delay,partial_wait_interrupt:%d",compress->partial_wait_interrupt);
    if(compress->partial_wait_interrupt) {
        compress->partial_wait_interrupt = 0;
        ret = -1;
    }
    else
        ret = 0;
    pthread_mutex_unlock(&compress->partial_wait_mutex);
    return ret;
}

static void compress_drain_wait_interrupt(struct compress *compress)
{
    ALOGI("compress_drain_notify in stream_id:%d",compress->cur_stream_id);
    pthread_mutex_lock(&compress->partial_wait_mutex);
    compress->partial_wait_interrupt = 1;
    pthread_cond_signal(&compress->partial_wait_cond);
    pthread_mutex_unlock(&compress->partial_wait_mutex);
    ALOGI("compress_drain_notify out stream_id:%d",compress->cur_stream_id);
}


static long compress_get_wait_time(struct compress *compress)
{
    long wait_time = 0;
    long wait_time2 = 0;
    pthread_mutex_lock(&compress->pcm_mutex);
    if(compress->pcm) {
        uint32_t avail = 0;
        avail = pcm_avail_update(compress->pcm);
        if(compress->no_src_data == 0) {
            if(avail < (uint32_t)compress->pcm_cfg.avail_min) {
                wait_time = (compress->pcm_cfg.avail_min - avail) / compress->noirq_frames_per_msec;
                if(wait_time < 20) {
                    wait_time = 20;
                }
            }
            else {
                wait_time = 50;
            }
        }
        else {
            wait_time = (pcm_get_buffer_size(compress->pcm) - pcm_avail_update(compress->pcm))/compress->noirq_frames_per_msec;
            if(wait_time >= 150) {
                wait_time = wait_time-100;
            }
            else {
                if(compress->pcm_bytes_write_to_dev < stream_data_bytes(&compress->pcm_stream)) {
                    wait_time = 0;
                }
                else
                    wait_time = 50;
            }
            ALOGI("peter: nor src and wait time:%ld",wait_time);
        }
    }
    else {
        wait_time = 50;
    }
    pthread_mutex_lock(&compress->gapless_state_mutex);
    if(compress->gapless_state.gapless_transition) {
        if(compress->songs_pcm_bytes > compress->played_pcm_bytes) {
            if(compress->pcm && (pcm_state(compress->pcm) == PCM_STATE_RUNNING)) {
                wait_time2 = pcm_bytes_to_frames(compress->pcm,compress->songs_pcm_bytes-compress->played_pcm_bytes)/compress->noirq_frames_per_msec;
                wait_time = wait_time > wait_time2 ? wait_time2:wait_time;
                wait_time = wait_time < 10 ? 10:wait_time;
                ALOGI("peter_test:wait_time:%ld,wait_time2:%ld",wait_time,wait_time2);
            }
        }
    }
    pthread_mutex_unlock(&compress->gapless_state_mutex);
    pthread_mutex_unlock(&compress->pcm_mutex);
    return (wait_time * 1000000);
}

static long compress_get_buffer_time(struct compress *compress)
{
    uint32_t wait_time = 0;
    pthread_mutex_lock(&compress->pcm_mutex);
    if(compress->pcm) {
        uint32_t avail = 0;
        uint32_t total = 0;
        total = pcm_get_buffer_size(compress->pcm);
        avail = pcm_avail_update(compress->pcm);
        avail = total-avail;
        wait_time = avail / compress->noirq_frames_per_msec;
    }
    pthread_mutex_unlock(&compress->pcm_mutex);
    return (wait_time*1000000) ;
}


static int compress_pcm_state_check(struct compress *compress)
{
    int state = 0;
    pthread_mutex_lock(&compress->pcm_mutex);
    if(compress->pcm) {
        state = pcm_state(compress->pcm);
       if(pcm_state(compress->pcm) != PCM_STATE_RUNNING) {
            ALOGE("error: pcm_state %d",state);
            pcm_close(compress->pcm);
            compress_pcm_restart(compress);
        }
    }
    else {
        compress->pcm= pcm_open(compress->card, compress->device, PCM_OUT| PCM_MMAP |PCM_NOIRQ |PCM_MONOTONIC, &compress->pcm_cfg);
        if(!pcm_is_ready(compress->pcm)) {
            pcm_close(compress->pcm);
            compress->pcm = NULL;
        }
        compress->prev_samples = 0;
        compress->pcm_bytes_write_to_dev = 0;
        compress->dev_start = 0;
    }
    pthread_mutex_unlock(&compress->pcm_mutex);
    return 0;
}

static int compress_pcm_state_check_no_lock(struct compress *compress)
{
    int state = 0;

    if(compress->pcm) {
        state = pcm_state(compress->pcm);
       if(pcm_state(compress->pcm) != PCM_STATE_RUNNING) {
            ALOGE("error: pcm_state %d",state);
            pcm_close(compress->pcm);
            compress_pcm_restart(compress);
        }
    }
    else {
        compress->pcm= pcm_open(compress->card, compress->device, PCM_OUT| PCM_MMAP |PCM_NOIRQ |PCM_MONOTONIC, &compress->pcm_cfg);
        if(!pcm_is_ready(compress->pcm)) {
            pcm_close(compress->pcm);
            compress->pcm = NULL;
        }
        compress->prev_samples = 0;
        compress->pcm_bytes_write_to_dev = 0;
        compress->dev_start = 0;
    }
    return 0;
}


static void compress_write_data_notify(struct compress *compress)
{
    pthread_mutex_lock(&compress->cond_mutex);
    compress->buffer_ready = 1;
    pthread_cond_signal(&compress->cond);
    pthread_mutex_unlock(&compress->cond_mutex);
}

static int compress_partial_drain_wait(struct compress *compress)
{
    int ret = 0;
    pthread_mutex_lock(&compress->partial_drain_mutex);
    if((!compress->partial_drain_ok) && (!compress->partial_drain_interrupt)){
        ALOGI("%s in 1",__FUNCTION__);
        compress->partial_drain_waiting = 1;
        pthread_cond_wait(&compress->partial_drain_cond, &compress->partial_drain_mutex);
        ALOGI("%s out 2",__FUNCTION__);
        compress->partial_drain_waiting = 0;
    }
    if(compress->partial_drain_interrupt) {
        ret = -1;
        compress->partial_drain_interrupt=0;
    }
    compress->partial_drain_ok = 0;
    pthread_mutex_unlock(&compress->partial_drain_mutex);
    return ret;
}

static void compress_partial_drain_notify(struct compress *compress,int interrupt)
{
    ALOGI("compress_partial_drain_notify in stream_id:%d,interrupt:%d",compress->cur_stream_id,interrupt);
    pthread_mutex_lock(&compress->partial_drain_mutex);
    if(interrupt){
        compress->partial_drain_interrupt = 1;
    }
    else {
        compress->partial_drain_ok = 1;
    }
    if(compress->partial_drain_waiting == 1) {
        pthread_cond_signal(&compress->partial_drain_cond);
    }
    pthread_mutex_unlock(&compress->partial_drain_mutex);
    ALOGI("compress_partial_drain_notify out stream_id:%d",compress->cur_stream_id);
}

static void compress_partial_drain_clear(struct compress *compress)
{
   // ALOGE("compress_partial_drain_clear in stream_id:%d,interrupt:%d",compress->cur_stream_id);
    pthread_mutex_lock(&compress->partial_drain_mutex);
    if(compress->partial_drain_ok)
    	compress->partial_drain_ok = 0;
    pthread_mutex_unlock(&compress->partial_drain_mutex);
   // ALOGE("compress_partial_drain_clear out stream_id",compress->cur_stream_id);
}


static void compress_drain_notify(struct compress *compress)
{
    ALOGI("compress_drain_notify in stream_id:%d",compress->cur_stream_id);
    pthread_mutex_lock(&compress->drain_mutex);
    compress->drain_ok = 1;
    if(compress->drain_waiting == 1) {
        pthread_cond_signal(&compress->drain_cond);
    }
    pthread_mutex_unlock(&compress->drain_mutex);
    ALOGI("compress_drain_notify out stream_id:%d",compress->cur_stream_id);
}



static int compress_cmd_process(struct compress *compress, struct compress_cmd  *cmd)
{
    int ret = 0;
    if(cmd == NULL) {
        return 0;
    }
    if(cmd) {
    ALOGI("compress_cmd_process cmd is %d,compress->is_start:%d",cmd->cmd,compress->is_start);
    switch(cmd->cmd) {
        case COMPERSS_CMD_STOP:
            compress->loop_state = STATE_STOP;
            stream_clean(&compress->stream);
            compress->pcm_bytes_left = 0;
            compress->pcm_bytes_offset = 0;
            compress->no_src_data = 0;
            compress->bytes_decoded = 0;
            compress->bytes_decoded_after_src = 0;
            compress->gapless_state.next_song_pcm_bytes = 0;
            compress->songs_pcm_decoded_bytes = 0;
            compress->songs_pcm_bytes = 0;
            compress->played_pcm_bytes = 0;
            compress->is_start = 0;
            compress->is_end = 0;
            if(compress->resampler) {
                release_resampler(compress->resampler);
                compress->resampler = NULL;
            }
            stream_clean(&compress->pcm_stream);
            if(compress->cache_pcm_stream.ring) {
                ringbuffer_free(compress->cache_pcm_stream.ring);
                compress->cache_pcm_stream.ring = NULL;
            }
            compress_write_data_notify(compress);
            compress_partial_drain_notify( compress,1);
            compress_drain_wait_interrupt(compress);
            compress_wait_for_stream_avail_notify(compress,1);
            compress_drain_notify(compress);
            pthread_mutex_lock(&compress->pcm_mutex);
            if(compress->pcm){
                pcm_close(compress->pcm);
                compress->pcm = NULL;
            }
            pthread_mutex_unlock(&compress->pcm_mutex);
            if(cmd->is_sync) {
                cmd->ret = ret;
                sem_post(&cmd->sync_sem);
            }
            else {
                free(cmd);
            }
            ret = 0;
            break;
        case COMPERSS_CMD_START:
            compress->loop_state = STATE_START;
            compress->drain_ok = 0;
            compress->partial_drain_ok = 0;
            compress->partial_drain_interrupt= 0;
            compress->partial_wait_interrupt = 0;
            compress->no_src_data = 0;
            compress->bytes_decoded = 0;
            compress->is_start = 1;
            compress->pcm_bytes_left = 0;
            compress->pcm_bytes_offset = 0;
            compress->prev_samples = 0;
            compress->is_end = 0;
            compress->gapless_state.next_song_pcm_bytes = 0;

            if(compress->mMP3DecHandle) {
                MP3_ARM_DEC_InitDecoder(compress->mMP3DecHandle);
            }
            pthread_mutex_lock(&compress->pcm_mutex);
            if(!compress->pcm) {
                compress->pcm= pcm_open(compress->card, compress->device, PCM_OUT| PCM_MMAP |PCM_NOIRQ |PCM_MONOTONIC, &compress->pcm_cfg);
                compress->prev_samples = 0;
                compress->pcm_bytes_write_to_dev = 0;
                compress->dev_start = 0;
                if(pcm_is_ready(compress->pcm)) {
                    ret = 0;
                    ALOGE("peter: pcm_open ok");
                }
                else {
                    ALOGE(" COMPERSS_CMD_START pcm open failed");
                    pcm_close(compress->pcm);
                    compress->pcm = NULL;
                }
                compress->pcm_time_ms =(uint32_t)((uint64_t)compress->pcm_cfg.period_count*compress->pcm_cfg.period_size* 1000)/compress->pcm_cfg.rate;
                ALOGI("peter:compress->pcm_time_ms is %d",compress->pcm_time_ms);
            }
            pthread_mutex_unlock(&compress->pcm_mutex);

            fade_init(&compress->fade_resume,compress->pcm_cfg.rate*100/1000);
            fade_init(&compress->fade_start,compress->pcm_cfg.rate*100/1000);
            fade_enable(&compress->fade_start);
            fade_disable(&compress->fade_resume);

            stream_clean(&compress->pcm_stream);
            compress->play_cache_pcm_first = 0;

            if(cmd->is_sync) {
                cmd->ret = ret;
                sem_post(&cmd->sync_sem);
            }
            else {
                free(cmd);
            }
            ret = 0;
            break;
        case COMPERSS_CMD_PAUSE:
            compress->loop_state = STATE_PAUSE;
            compress->is_start = 0;
            uint32_t cur_samples = 0;
            uint32_t cur_consume_bytes = 0;
            if(compress->pcm_time_ms >= LARGE_PCM_BUFFER_TIME) {
                if(compress->position_ctl) {
                    cur_samples = mixer_ctl_get_value(compress->position_ctl, 0);
                }
                cur_consume_bytes = (cur_samples - compress->prev_samples)*(compress->pcm_cfg.channels<<1);
                ALOGI("cur_samples %d,prev_samples %lu, total bytes:%d,data bytes: %d,cur_samples1:%d,",cur_samples,compress->prev_samples,stream_total_bytes(&compress->pcm_stream),stream_data_bytes(&compress->pcm_stream),(stream_total_bytes(&compress->pcm_stream) - stream_data_bytes(&compress->pcm_stream))/4);
                ret = stream_comsume(&compress->pcm_stream,cur_consume_bytes);
                if(ret) {
                    ALOGE("stream_comsume error ret : %d",ret);
                }
                compress->played_pcm_bytes += cur_consume_bytes;

                ALOGI("cur_samples %u,prev_samples %lu, total bytes:%d,data bytes: %d,cur_samples1:%d,",cur_samples,compress->prev_samples,stream_total_bytes(&compress->pcm_stream),stream_data_bytes(&compress->pcm_stream),(stream_total_bytes(&compress->pcm_stream) - stream_data_bytes(&compress->pcm_stream))/4);
            }
            else {
                compress->played_pcm_bytes = compress->bytes_decoded_after_src - stream_data_bytes(&compress->pcm_stream);
                ALOGI("peter: pause: compress->played_pcm_bytes:%lu,pcm_data_bytes:%d,data_after_src:%lu",compress->played_pcm_bytes,stream_data_bytes(&compress->pcm_stream),compress->bytes_decoded_after_src);
            }
            ALOGI("compress->pcm_time_ms:%d",compress->pcm_time_ms);
            if(compress->cache_pcm_stream.ring) {
                ringbuffer_free(compress->cache_pcm_stream.ring);
                compress->cache_pcm_stream.ring = NULL;
            }
            stream_copy(&compress->cache_pcm_stream,&compress->pcm_stream);
            pthread_mutex_lock(&compress->pcm_mutex);
            if(compress->pcm){
                pcm_close(compress->pcm);
                compress->pcm = NULL;
            }
            pthread_mutex_unlock(&compress->pcm_mutex);
            compress->pcm_bytes_left = 0;
            compress->pcm_bytes_offset = 0;
            compress->prev_samples = 0;

            if(cmd->is_sync) {
                cmd->ret = 0;
                sem_post(&cmd->sync_sem);
            }
            else {
                free(cmd);
            }
            ret = -1;
            break;
        case COMPERSS_CMD_RESUME:
            compress->loop_state = STATE_RESUME;
            compress->is_start = 1;
            compress->is_end = 0;
            pthread_mutex_lock(&compress->pcm_mutex);
            g_prev_samples = 0;
            if(!compress->pcm) {
                compress->pcm= pcm_open(compress->card, compress->device, PCM_OUT| PCM_MMAP |PCM_NOIRQ |PCM_MONOTONIC, &compress->pcm_cfg);
                if(!pcm_is_ready(compress->pcm)) {
                    ALOGE("peter: pcm_open error:%s",pcm_get_error(compress->pcm));
                    pcm_close(compress->pcm);
                    compress->pcm = NULL;
                }
                else {
                    compress->pcm_bytes_write_to_dev = 0;
                    compress->dev_start = 0;
                    ret = 0;
                    ALOGE("peter: pcm_open ok");
                }
                compress->pcm_time_ms =(uint32_t)((uint64_t)compress->pcm_cfg.period_count*compress->pcm_cfg.period_size* 1000)/compress->pcm_cfg.rate;
                ALOGI("peter:compress->pcm_time_ms is %d,pcm_stream_bytes:%d,data_after_src:%lu,played_bytes:%lu,bytes_decoded:%d",compress->pcm_time_ms,stream_data_bytes(&compress->pcm_stream),
                    compress->bytes_decoded_after_src,compress->played_pcm_bytes,compress->bytes_decoded);
                compress->played_pcm_bytes = compress->bytes_decoded_after_src - stream_data_bytes(&compress->pcm_stream);
            }
            pthread_mutex_unlock(&compress->pcm_mutex);

            fade_init(&compress->fade_resume,compress->pcm_cfg.rate*100/1000);
            fade_init(&compress->fade_start,compress->pcm_cfg.rate*100/1000);
            fade_disable(&compress->fade_start);
            fade_enable(&compress->fade_resume);

            if(compress->position_ctl && (compress->pcm_time_ms >= LARGE_PCM_BUFFER_TIME)) {
                cur_samples = mixer_ctl_get_value(compress->position_ctl, 0);
            }

            if(compress->cache_pcm_stream.ring) {
                ringbuffer_free(compress->cache_pcm_stream.ring);
                compress->cache_pcm_stream.ring = NULL;
            }
            stream_copy(&compress->cache_pcm_stream,&compress->pcm_stream);
            ALOGI("prev_samples %lu, total bytes:%d,data bytes: %d,cur_samples1:%d,pcm_cached:%lu",compress->prev_samples,stream_total_bytes(&compress->pcm_stream),stream_data_bytes(&compress->pcm_stream),
                (stream_total_bytes(&compress->pcm_stream) - stream_data_bytes(&compress->pcm_stream))/4,
            compress->bytes_decoded_after_src-compress->played_pcm_bytes);
            compress->prev_samples = 0;
            if(stream_data_bytes(&compress->pcm_stream)) {
                compress->play_cache_pcm_first = 1;
            }
            if(cmd->is_sync){
                cmd->ret = 0;
                sem_post(&cmd->sync_sem);
            }
            else {
                free(cmd);
            }
            ret = 0;
            break;
        case COMPERSS_CMD_CHANGE_DEV:
        {
            struct dev_set_param  *param = cmd->param;
            pthread_mutex_lock(&compress->position_mutex);
            compress->card = param->card;
            compress->device = param->device;
            compress->mixer = param->mixerctl;
            compress->gapless_state.next_song_pcm_bytes = 0;

            if(param->config) {
                memcpy(&compress->pcm_cfg, param->config, sizeof(struct pcm_config));
                if(compress->pcm_cfg.start_threshold) {
                    compress->start_threshold = (compress->pcm_cfg.start_threshold<<2);
                }
                else {
                    compress->start_threshold = (compress->pcm_cfg.period_count * compress->pcm_cfg.period_size/2)<<2;
                }
            }
            if(!param->config->avail_min) {
                compress->pcm_cfg.avail_min = param->config->period_size;
            }
            compress->noirq_frames_per_msec = param->config->rate / 1000;

            if(compress->pcm_stream.ring) {
                ringbuffer_free(compress->pcm_stream.ring);
                compress->pcm_stream.ring = NULL;
            }

            if(compress->cache_pcm_stream.ring) {
                ringbuffer_free(compress->cache_pcm_stream.ring);
                compress->cache_pcm_stream.ring = NULL;
            }
            compress->pcm_stream.ring = ringbuffer_init(param->config->period_count*param->config->period_size*4 + (1152*10*4));
            if(compress->pcm_stream.ring == NULL) {
                ALOGE("error:COMPERSS_CMD_CHANGE_DEV (compress->pcm_stream.ring == NULL)");
            }
            const char *mixer_ctl_name = "PCM_TOTAL_DEEPBUF";
            compress->position_ctl = mixer_get_ctl_by_name(compress->mixer, mixer_ctl_name);
            if (!compress->position_ctl) {
                ALOGE("%s: Could not get ctl for mixer cmd - %s",
                __func__, mixer_ctl_name);
            }
            pthread_mutex_lock(&compress->pcm_mutex);
            if(compress->pcm){
                ALOGE("COMPERSS_CMD_CHANGE_DEV  pcm close in");
                pcm_close(compress->pcm);
                compress->pcm = NULL;
                ALOGE("COMPERSS_CMD_CHANGE_DEV  pcm close out");
            }
            pthread_mutex_unlock(&compress->pcm_mutex);

            if(compress->resample_buffer) {
                free(compress->resample_buffer);
                compress->resample_buffer = NULL;
            }
            if(compress->resampler) {
                release_resampler(compress->resampler);
                compress->resampler = NULL;
            }

            if(cmd->is_sync) {
                cmd->ret = 0;
                sem_post(&cmd->sync_sem);
            }
            else {
                if(cmd->param) {
                    free(cmd->param);
                }
                free(cmd);
            }
            pthread_mutex_unlock(&compress->position_mutex);
            ret = 0;
        }
            break;
        case COMPRESS_CMD_DRAIN:
            compress->loop_state = STATE_DRAIN;
            if(cmd->is_sync) {
                cmd->ret = 0;
                sem_post(&cmd->sync_sem);
            }
            else {
            free(cmd);
            }
            ret = 0;
            break;
        case COMPRESS_CMD_PARTIAL_DRAIN:
            compress->loop_state = STATE_PARTIAL_DRAIN;
            if(cmd->is_sync) {
                cmd->ret = 0;
                sem_post(&cmd->sync_sem);
            }
            else {
                free(cmd);
            }
            ret = 0;
            break;
        case COMPERSS_CMD_NEXTTRACK:
        {
            struct nexttrack_param *param = (struct nexttrack_param *)cmd->param;
            if(param) {
                compress->next_stream_id = param->stream_id;
            }
            if(cmd->is_sync) {
                cmd->ret = 0;
                sem_post(&cmd->sync_sem);
            }
            else {
                free(cmd);
            }
            if(!compress->is_start){
                ret = -1;
            }
            break;
        }
        case COMPERSS_CMD_CLOSE:
            compress->loop_state = STATE_CLOSE;
            if(cmd->is_sync) {
                cmd->ret = 0;
                sem_post(&cmd->sync_sem);
            }
            else {
                free(cmd);
            }
            ret = -1;
        break;
        case COMPERSS_CMD_SEND_DATA:
            if(cmd->is_sync) {
                cmd->ret = 0;
                sem_post(&cmd->sync_sem);
            }
            else {
                free(cmd);
            }
            if(!compress->is_start){
                ret = -1;
            }
            break;
        default:
            if(cmd->is_sync) {
                cmd->ret = 0;
                sem_post(&cmd->sync_sem);
            }
            else {
                free(cmd);
            }
            if(!compress->is_start){
                ret = -1;
            }
            break;

        }
    }
    return ret;
}


static void *thread_func(void * param)
{
    int ret = 0;
    ALOGI(" thread_func in");
    struct compress *compress  = param;
    struct compress_cmd  *cmd;
    struct listnode *item;
    pid_t tid;

    setpriority(PRIO_PROCESS, 0, ANDROID_PRIORITY_AUDIO);
    set_sched_policy(0, SP_FOREGROUND);
    prctl(PR_SET_NAME, (unsigned long)"Audio Offload decoder Thread", 0, 0, 0);
    if(!compress) {
            return NULL;
    }
#ifdef SET_OFFLOAD_AFFINITY
    tid = gettid();
    ret = set_audio_affinity(tid);
    if(ret < 0){
        ALOGE("set playback affinity failed for tid:%d, error:%d", tid, ret);
    } else {
        ALOGI("set playback affinity successfully for tid:%d, error:%d", tid, ret);
    }
#endif

    clock_gettime(CLOCK_MONOTONIC, &compress->ts);
    while(!((compress->state == STATE_EXIT )
            &&(compress->loop_state == STATE_CLOSE))) {
        cmd = NULL;
        pthread_mutex_lock(&compress->cmd_mutex);
        if (list_empty(&compress->cmd_list)) {
            if((compress->state == STATE_OPEN)
                || (compress->state == STATE_PAUSE)
                ||(compress->state == STATE_STOP)){
                ALOGI("%s: Waiting for signal ", __func__);
                ret = pthread_cond_wait(&compress->cmd_cond, &compress->cmd_mutex);
               // compress_wait_event(compress,&compress->cmd_mutex);
            }
            else if((compress->state == STATE_START)
                        ||(compress->state == STATE_RESUME)){
                if(compress->pcm_bytes_left || compress->no_src_data) {
                    if(stream_data_bytes(&compress->pcm_stream)) {
                        if(compress->dev_start) {
                            long wait_time = compress_get_wait_time(compress);
                            clock_gettime(CLOCK_MONOTONIC, &compress->ts);
                            compress->ts.tv_nsec += wait_time;
                            if(compress->ts.tv_nsec >1000000000) {
                                compress->ts.tv_nsec -= 1000000000;
                                compress->ts.tv_sec += 1;
                            }
                            //ALOGE("wait in:%ld,",wait_time);
                            if(wait_time) {
#ifndef ANDROID4X
                                ret = pthread_cond_timedwait(&compress->cmd_cond, &compress->cmd_mutex, &compress->ts);
#else
                                ret = pthread_cond_timedwait_monotonic(&compress->cmd_cond, &compress->cmd_mutex, &compress->ts);
#endif
                                //ALOGE("wait out:%ld",ret);
                            }
                            compress_pcm_state_check(compress);
                        }
                    }
                    else {
                        clock_gettime(CLOCK_MONOTONIC, &compress->ts);
                        compress->ts.tv_sec += 1;
                        ALOGE("wait in: no src data,%d, wait for 1 second",compress->no_src_data);
#ifndef ANDROID4X
                        ret = pthread_cond_timedwait(&compress->cmd_cond, &compress->cmd_mutex,  &compress->ts);
#else
                        ret = pthread_cond_timedwait_monotonic(&compress->cmd_cond, &compress->cmd_mutex,  &compress->ts);
#endif
                        //ret = pthread_cond_wait(&compress->cmd_cond, &compress->cmd_mutex);
                        ALOGE("wait out:%d",ret);
                    }
                }
                else if(compress->no_src_data) {
                }
            }
#if 0
          ALOGE("peter: ret : %d,compress->state %d,compress->pcm_bytes_left :%d,compress->loop_state:%d,,no_src_data:%d,partial_w:%d,drain_w:%d",
            ret,compress->state,compress->pcm_bytes_left,compress->loop_state,compress->no_src_data,
            compress->partial_drain_waiting,compress->drain_waiting);
#endif
        }

        if (!list_empty(&compress->cmd_list)) {
            /* get the command from the list, then process the command */
            item = list_head(&compress->cmd_list);
            cmd = node_to_item(item, struct compress_cmd, node);
            list_remove(item);
        }
        pthread_mutex_unlock(&compress->cmd_mutex);

        if(cmd) {
            ret = compress_cmd_process(compress, cmd);
            if(ret) {
                continue;
            }
        }

        if(compress->is_start)
        {
            pthread_mutex_lock(&compress->position_mutex);
            ret = audio_process(compress);
            pthread_mutex_unlock(&compress->position_mutex);
            if(ret == -ERROR_NO_DATA) {
                compress_partial_drain_notify(compress,0);
            }
            else if(ret == -ERROR_NO_PCM_DATA) {
                compress_drain_notify(compress);
            }
            else {
                compress_partial_drain_clear(compress);
            }
        }
    }
    ALOGI(" thread_func exit 1");
    while (!list_empty(&compress->cmd_list)) {
        struct compress_cmd  *cmd;
        struct listnode *item;
        /* get the command from the list, then process the command */
        item = list_head(&compress->cmd_list);
        cmd = node_to_item(item, struct compress_cmd, node);
        list_remove(item);
        if(cmd->is_sync) {
            cmd->ret = 0;
            sem_post(&cmd->sync_sem);
        }
        else {
            if(cmd->param) {
                free(cmd->param);
            }
            free(cmd);
        }
    }
    ALOGI(" thread_func exit 2");
    return NULL;
}

static int compress_send_cmd(struct compress *compress, COMPRESS_CMD_T command,void * parameter, uint32_t is_sync)
{
    int ret = 0;
    struct compress_cmd *cmd = (struct compress_cmd *)calloc(1, sizeof(struct compress_cmd));

    ALOGI("%s, cmd:%d",
        __func__, command);
    /* add this command to list, then send signal to offload thread to process the command list */
    cmd->cmd = command;
    cmd->is_sync = is_sync;
    cmd->param = parameter;
    sem_init(&cmd->sync_sem, 0, 0);
    pthread_mutex_lock(&compress->cmd_mutex);
    list_add_tail(&compress->cmd_list, &cmd->node);
    compress_signal(compress);
    pthread_mutex_unlock(&compress->cmd_mutex);
    if(is_sync) {
        if((command == COMPERSS_CMD_STOP)
            ||(command == COMPERSS_CMD_CLOSE)) {
           // stream_break(&compress->stream);
        }
        sem_wait(&cmd->sync_sem);
        ret = cmd->ret;
        if(cmd->param) {
            free(cmd->param);
        }
        free(cmd);
    }
    return ret;
}

int32_t compress_set_gain(struct compress *compress,long left_vol, long right_vol)
{
    if(!compress) {
        return -1;
    }
    ALOGI("%s in:left_vol:%ld,right_vol:%ld",__FUNCTION__,left_vol,right_vol);
    if(left_vol <= AUDIO_OFFLOAD_PLAYBACK_VOLUME_MAX){
        compress->left_vol = left_vol;
    }

     if(right_vol <= AUDIO_OFFLOAD_PLAYBACK_VOLUME_MAX) {
        compress->right_vol = right_vol;
    }
    return 0;
}


int32_t compress_set_dev(struct compress *compress,unsigned int card, unsigned int device,
        struct pcm_config  *config,void * mixerctl)
{
    int ret = 0;
    int need_reset = 0;
    if(!compress) {
        ALOGE("%s error: compres is null",__FUNCTION__);
        return -1;
    }
    ALOGI("%s in",__FUNCTION__);
    pthread_mutex_lock(&compress->lock);
    if (!is_compress_ready(compress)) {
        pthread_mutex_unlock(&compress->lock);
        return -1;
    }
    if(mixerctl == NULL) {
        ALOGE("%s mixer is NULL",__FUNCTION__);
        pthread_mutex_unlock(&compress->lock);
        return -1;
    }
    if((compress->card !=  (int)card)
            || (compress->device != (int)device)) {
        struct dev_set_param  *param = NULL;
        param = calloc(1, sizeof(struct dev_set_param));
        if(!param) {
            ALOGE("error mallock %s",__FUNCTION__);
            goto ERROR;
        }
        param->card = card;
        param->device = device;
        param->config = config;
        param->mixerctl = mixerctl;
        ret = compress_send_cmd(compress, COMPERSS_CMD_CHANGE_DEV,param,1);
    }
    else {
        ALOGI("dev config is same");
    }

ERROR:
    pthread_mutex_unlock(&compress->lock);
    ALOGE("%s out",__FUNCTION__);
    return 0;
}





struct compress *compress_open(unsigned int card, unsigned int device,
        unsigned int flags, struct compr_config *config)
{
    struct compress *compress;
    int ret = 0;
    ALOGI("compress_open in");

    if (!config) {
        ALOGE("%s error: config is null",__FUNCTION__);
        return NULL;
    }
    if ((config->fragment_size == 0) || (config->fragments == 0)) {
        ALOGE("%s param is error",__FUNCTION__);
        return NULL;
    }

    compress = calloc(1, sizeof(struct compress));
    if (!compress) {
        ALOGE("%s param is compress NULL",__FUNCTION__);
        return NULL;
    }

    memset(compress,0, sizeof(struct compress));

    pthread_mutex_init(&compress->lock, NULL);
    pthread_cond_init(&compress->cond, NULL);
    pthread_mutex_init(&compress->gapless_state_mutex, NULL);
    pthread_mutex_init(&compress->cond_mutex,NULL);
    pthread_cond_init(&compress->drain_cond, NULL);
    pthread_mutex_init(&compress->drain_mutex,NULL);
    pthread_mutex_init(&compress->pcm_mutex,NULL);
    pthread_mutex_init(&compress->position_mutex,NULL);
    pthread_cond_init(&compress->partial_drain_cond, NULL);
    pthread_mutex_init(&compress->partial_drain_mutex,NULL);

    pthread_condattr_t attr;
    pthread_condattr_init(&attr);
#ifndef ANDROID4X
    pthread_condattr_setclock(&attr, CLOCK_MONOTONIC);
#endif
    pthread_cond_init(&compress->cmd_cond, &attr);
    pthread_mutex_init(&compress->cmd_mutex,NULL);

    pthread_condattr_init(&attr);
#ifndef ANDROID4X
    pthread_condattr_setclock(&attr, CLOCK_MONOTONIC);
#endif
    pthread_cond_init(&compress->partial_wait_cond, &attr);
    pthread_mutex_init(&compress->partial_wait_mutex,NULL);

    list_init(&compress->cmd_list);
    compress->config = calloc(1, sizeof(struct compr_config));
    if (!compress->config)
        goto error_fail;
    memcpy(compress->config, config, sizeof(struct compr_config));
    compress->flags = flags;
    if (!((flags & COMPRESS_OUT) || (flags & COMPRESS_IN))) {
        ALOGE("%s can't deduce device direction from given flags",__FUNCTION__);
        goto error_fail;
    }
    ret = MP3_ARM_DEC_Construct(&compress->mMP3DecHandle);
    if(ret) {
        ALOGE("MP3_ARM_DEC_Construct failed");
        goto error_fail;
    }
    compress->stream.ring = ringbuffer_init(config->fragments * config->fragment_size);
    compress->state = STATE_OPEN;
    pthread_attr_t threadattr;
    pthread_attr_init(&threadattr);
    pthread_attr_setdetachstate(&threadattr, PTHREAD_CREATE_JOINABLE);
    ret = pthread_create(&compress->thread, &threadattr,
    thread_func, (void *)compress);
    if (ret) {
        ALOGE("bt sco : duplicate thread create fail, code is %d", ret);
        goto error_fail;
    }
    pthread_attr_destroy(&threadattr);

    compress->aud_fd = -1;//open(SPRD_AUD_DRIVER,O_RDWR);
    if(compress->aud_fd <= 0) {
       // ALOGE("bt sco : compress->aud_fd %s open fail, code is %d", SPRD_AUD_DRIVER,ret);
      //  goto error_fail;
    }
    compress->ready = 1;
    compress->cur_stream_id =1;
    compress->client_stream_id=1;
    compress->use_dsp_gapless_mode = 1;
    ALOGI("compress_open out");
    g_prev_samples = 0;
    return compress;

error_fail:
    if(compress->config) {
        free(compress->config);
    }
    if(compress->pcm) {
        pcm_close(compress->pcm);
    }
    if(compress->aud_fd >0) {
        close(compress->aud_fd);
    }
    free(compress);
    return NULL;
}

int compress_write(struct compress *compress, const void *buf, unsigned int size)
{
    int total = 0, ret;
    uint32_t bytes_written = 0;
    if(!compress) {
        ALOGE("%s error: compres is null",__FUNCTION__);
        return -1;
    }
    //ALOGE("compress_write in");
    pthread_mutex_lock(&compress->lock);
    if (!(compress->flags & COMPRESS_IN)) {
        pthread_mutex_unlock(&compress->lock);
        return -1;
    }
    if (!is_compress_ready(compress)) {
        ALOGE("compress_write out total 1 :%d",total);
        pthread_mutex_unlock(&compress->lock);
        return -1;
    }
    ret = stream_write(&compress->stream,(uint8_t *) buf, size, (uint32_t *)&total);
    if(total) {
    // out_dump_doing3(buf,total);
        ret = compress_send_cmd(compress, COMPERSS_CMD_SEND_DATA,NULL,0);
    }
    pthread_mutex_unlock(&compress->lock);
    //ALOGE("compress_write out total:%d",total);
    return total;
}

int compress_start(struct compress *compress)
{
    int ret = 0;
    pthread_attr_t attr;
    if(!compress) {
        ALOGE("%s error: compres is null",__FUNCTION__);
        return -1;
    }
    pthread_mutex_lock(&compress->lock);
    if (!is_compress_ready(compress)) {
        pthread_mutex_unlock(&compress->lock);
        return -1;
    }
    if(compress->state == STATE_START) {
        pthread_mutex_unlock(&compress->lock);
        return 0;
    }
    compress->state = STATE_START;
    compress->running = 1;
    ret = compress_send_cmd(compress, COMPERSS_CMD_START,NULL,1);

    compress->start_time = getCurrentTimeUs();
    pthread_mutex_unlock(&compress->lock);
    return 0;
}

int compress_pause(struct compress *compress)
{
    int ret = 0;
    if(!compress) {
        ALOGE("%s error: compres is null",__FUNCTION__);
        return -1;
    }
    ALOGI("%s in",__FUNCTION__);
    pthread_mutex_lock(&compress->lock);
    if (!is_compress_running(compress)) {
        pthread_mutex_unlock(&compress->lock);
        return 0;
    }
    compress->state = STATE_PAUSE;
    ret = compress_send_cmd(compress, COMPERSS_CMD_PAUSE,NULL,1);

    pthread_mutex_unlock(&compress->lock);
    ALOGI("%s out",__FUNCTION__);
    return 0;
}

int compress_resume(struct compress *compress)
{
    int ret = 0;
    if(!compress) {
        ALOGE("%s error: compres is null",__FUNCTION__);
        return -1;
    }
    ALOGI("%s in",__FUNCTION__);
    pthread_mutex_lock(&compress->lock);
    if(compress->state != STATE_PAUSE) {
        pthread_mutex_unlock(&compress->lock);
        return 0;
    }
    compress->state = STATE_RESUME;
    if (!is_compress_running(compress)) {
        pthread_mutex_unlock(&compress->lock);
        return 0;
    }
   ret = compress_send_cmd(compress, COMPERSS_CMD_RESUME,NULL,1);

    pthread_mutex_unlock(&compress->lock);
    ALOGI("%s out",__FUNCTION__);
    return 0;
}

int compress_resume_async(struct compress *compress,int sync)
{
    int ret = 0;
    if(!compress) {
        ALOGE("%s error: compres is null",__FUNCTION__);
        return -1;
    }
    ALOGI("%s in",__FUNCTION__);
    pthread_mutex_lock(&compress->lock);
    compress->state = STATE_RESUME;
    if (!is_compress_running(compress)) {
        pthread_mutex_unlock(&compress->lock);
        return 0;
    }
    ret = compress_send_cmd(compress, COMPERSS_CMD_RESUME,NULL,0);

    pthread_mutex_unlock(&compress->lock);
    ALOGI("%s out",__FUNCTION__);
    return 0;
}



int compress_drain(struct compress *compress)
{
    int ret = 0;
    if(!compress) {
        ALOGE("%s error: compres is null",__FUNCTION__);
        return -1;
    }
    ALOGI("%s in",__FUNCTION__);
    pthread_mutex_lock(&compress->lock);
    if (!is_compress_running(compress)) {
        pthread_mutex_unlock(&compress->lock);
        return -1;
    }
    ret = compress_send_cmd(compress, COMPRESS_CMD_DRAIN,NULL,1);
    pthread_mutex_unlock(&compress->lock);
    ALOGI("%s out half",__FUNCTION__);
    pthread_mutex_lock(&compress->drain_mutex);
    if(!compress->drain_ok){
        compress->drain_waiting = 1;
        ALOGI("%s in 1",__FUNCTION__);
        pthread_cond_wait(&compress->drain_cond, &compress->drain_mutex);
        ALOGI("%s out 1",__FUNCTION__);
        compress->drain_waiting = 0;
    }
    compress->drain_ok = 0;
    pthread_mutex_unlock(&compress->drain_mutex);
    ALOGI("%s out",__FUNCTION__);
    return 0;
}

int compress_partial_drain(struct compress *compress)
{
    int ret = 0;
    long wait_time = 0;
    if(!compress) {
        ALOGE("%s error: compres is null",__FUNCTION__);
        return -1;
    }
    ALOGI("%s in",__FUNCTION__);
    pthread_mutex_lock(&compress->lock);
    if (!is_compress_running(compress)){
        pthread_mutex_unlock(&compress->lock);
        return 0;
    }
    if(!compress->use_dsp_gapless_mode) {
        pthread_mutex_unlock(&compress->lock);
        ALOGE("ignore trigger next track %s out",__FUNCTION__);
        return compress_drain(compress);
    }
    ret = compress_send_cmd(compress, COMPRESS_CMD_PARTIAL_DRAIN,NULL,1);
    pthread_mutex_lock(&compress->gapless_state_mutex);
    compress->gapless_state.partial_drain_waiting = 1;
    pthread_mutex_unlock(&compress->gapless_state_mutex);
    pthread_mutex_unlock(&compress->lock);
    ALOGI("compress_partial_drain_wait in");
    ret = compress_partial_drain_wait(compress);
    if(ret) {
        ALOGE("compress_partial_drain_wait error");
        pthread_mutex_lock(&compress->gapless_state_mutex);
        compress->gapless_state.partial_drain_waiting = 0;
        pthread_mutex_unlock(&compress->gapless_state_mutex);
        return 0;
    }
    wait_time = compress_get_buffer_time(compress);
    wait_time = wait_time > PARTIAL_DRAIN_ACK_EARLY_BY_MSEC ?
    wait_time - PARTIAL_DRAIN_ACK_EARLY_BY_MSEC : 0;
    ALOGI("compress_partial_drain_wait out and start delay :%ld",wait_time);
    pthread_mutex_lock(&compress->gapless_state_mutex);
    compress->gapless_state.gapless_transition = 1;
    compress->gapless_state.partial_drain_waiting = 0;
    compress->gapless_state.gapless_transition_delaying=1;
    pthread_mutex_unlock(&compress->gapless_state_mutex);
    ret = compress_partial_drain_delay(compress,wait_time);
    if(ret) {
        ALOGE("compress_partial_drain_wait being break");
        pthread_mutex_lock(&compress->gapless_state_mutex);
        compress->gapless_state.gapless_transition = 0;
        compress->gapless_state.gapless_transition_delaying=0;
        pthread_mutex_unlock(&compress->gapless_state_mutex);
        return 0;
    }
    ALOGI("compress_partial_drain_delay delay out ok");
    pthread_mutex_lock(&compress->lock);
    pthread_mutex_lock(&compress->gapless_state_mutex);
    compress->client_stream_id = NEXT_STREAM_ID(compress->client_stream_id);
    compress->gapless_state.gapless_transition_delaying=0;
    pthread_mutex_unlock(&compress->gapless_state_mutex);
    pthread_mutex_unlock(&compress->lock);
    ALOGI("%s out,compress->client_stream_id:%d",__FUNCTION__,compress->client_stream_id);
    return 0;
}


int compress_get_tstamp(struct compress *compress,
        unsigned long *samples, unsigned int *sampling_rate)
{
    uint32_t avail = 0;
    uint32_t total = 0;
    int ret = 0;

    if(!compress) {
        ALOGE("%s error: compres is null",__FUNCTION__);
        return -1;
    }
    //ALOGI(" compress_get_tstamp in :%l, %d",*samples,*sampling_rate);
    pthread_mutex_lock(&compress->lock);
    if (!is_compress_ready(compress)){
        pthread_mutex_unlock(&compress->lock);
        return -1;
    }

    pthread_mutex_lock(&compress->position_mutex);
    pthread_mutex_lock(&compress->pcm_mutex);
    if(compress->is_end) {
        ALOGE("%s info: compres is end",__FUNCTION__);
        //pthread_mutex_unlock(&compress->pcm_mutex);
        //pthread_mutex_unlock(&compress->position_mutex);
        //pthread_mutex_unlock(&compress->lock);
        //return -1;
    }
    if(compress->pcm) {
        avail= pcm_avail_update(compress->pcm);
        avail = pcm_frames_to_bytes(compress->pcm, avail);
        total = pcm_get_buffer_size(compress->pcm);
        total = pcm_frames_to_bytes(compress->pcm, total);

        pthread_mutex_lock(&compress->gapless_state_mutex);
        if(compress->gapless_state.gapless_transition && (!compress->gapless_state.gapless_transition_delaying)) {
            if(compress->samplerate == compress->pcm_cfg.rate) {
                if((total - avail) > compress->gapless_state.next_song_pcm_bytes){
                    *samples = pcm_bytes_to_frames(compress->pcm,compress->songs_pcm_decoded_bytes- ((total - avail)-compress->gapless_state.next_song_pcm_bytes));
                }
                else {
                    *samples = pcm_bytes_to_frames(compress->pcm,compress->songs_pcm_decoded_bytes);
                }
            }
            else {
            *samples =pcm_bytes_to_frames(compress->pcm,compress->songs_pcm_decoded_bytes) - (pcm_bytes_to_frames(compress->pcm,(total - avail-compress->gapless_state.next_song_pcm_bytes))* compress->samplerate)/compress->pcm_cfg.rate;
            // *samples = ((uint64_t)(stream_total_bytes(stream)+ compress->base_position )* compress->samplerate +(compress->pcm_cfg.rate>>1))/compress->pcm_cfg.rate;//(uint32_t)((uint64_t)(cur_time  -  compress->start_time) * compress->samplerate*2/1000);
            }
        }
        else {
            if(compress->samplerate == compress->pcm_cfg.rate) {
                *samples = pcm_bytes_to_frames(compress->pcm,compress->bytes_decoded- stream_data_bytes(&compress->cache_pcm_stream) -(compress->pcm_bytes_left + total - avail));
            }
            else {
                *samples =pcm_bytes_to_frames(compress->pcm,compress->bytes_decoded) -(pcm_bytes_to_frames(compress->pcm,stream_data_bytes(&compress->cache_pcm_stream)+(compress->pcm_bytes_left + total - avail))* compress->samplerate)/compress->pcm_cfg.rate;
            // *samples = ((uint64_t)(stream_total_bytes(stream)+ compress->base_position )* compress->samplerate +(compress->pcm_cfg.rate>>1))/compress->pcm_cfg.rate;//(uint32_t)((uint64_t)(cur_time  -  compress->start_time) * compress->samplerate*2/1000);
            }
        }
        pthread_mutex_unlock(&compress->gapless_state_mutex);
        //ALOGI("peter_time: *samples: %lu, bytes_decoded:%lu,pcm_bytes_left:%d,total:%d,avail: %d,pcm:%d,songs_pcm_bytes:%lu,bytes_decoded_after_src:%lu,samplerate:%d,,pcm_cfg.rate:%d,gaplesstransition:%d,next_song_pcm_bytes:%d,partial_transition_delaying:%d,cache_stream:%d",*samples,compress->bytes_decoded,compress->pcm_bytes_left,total,avail,(uint32_t)compress->pcm,compress->songs_pcm_decoded_bytes,compress->bytes_decoded,compress->samplerate,compress->pcm_cfg.rate,
            //compress->gapless_state.gapless_transition,compress->gapless_state.next_song_pcm_bytes,compress->gapless_state.gapless_transition_delaying,stream_data_bytes(&compress->cache_pcm_stream));
    }
    else {
        *samples =(compress->bytes_decoded)/(compress->pcm_cfg.channels<<1)- ((stream_data_bytes(&compress->pcm_stream)/(compress->pcm_cfg.channels<<1))* compress->samplerate)/compress->pcm_cfg.rate;
        //ALOGI("peter_time_no_pcm: *samples: %lu, bytes_decoded:%lu,pcm_bytes_left:%d,total:%d,avail: %d,pcm:%d,songs_pcm_bytes:%lu,bytes_decoded_after_src:%lu,samplerate:%d,,pcm_cfg.rate:%d,gaplesstransition:%d,next_song_pcm_bytes:%d,partial_transition_delaying:%d,cache_stream:%d,stream pcm:%d",*samples,compress->bytes_decoded,compress->pcm_bytes_left,total,avail,(uint32_t)compress->pcm,compress->songs_pcm_decoded_bytes,compress->bytes_decoded,compress->samplerate,compress->pcm_cfg.rate,
            //compress->gapless_state.gapless_transition,compress->gapless_state.next_song_pcm_bytes,compress->gapless_state.gapless_transition_delaying,stream_data_bytes(&compress->cache_pcm_stream),stream_data_bytes(&compress->pcm_stream);
    }
    pthread_mutex_unlock(&compress->pcm_mutex);
    pthread_mutex_unlock(&compress->position_mutex);

    *sampling_rate = compress->samplerate;
    if(compress->samplerate == 0) {
        *sampling_rate = 44100;
    }
    pthread_mutex_unlock(&compress->lock);
    //ALOGI("peter: compress_get_tstamp_out:%zu, %d",(size_t)*samples,*sampling_rate);
    if(g_prev_samples > *samples) {
        ALOGI("peter_time_error: *samples: %lu, g_prev_samples:%lu,bytes_decoded:%lu,pcm_bytes_left:%d,total:%d,avail: %d,pcm:%d,songs_pcm_bytes:%lu,bytes_decoded_after_src:%lu,samplerate:%d,,pcm_cfg.rate:%d,gaplesstransition:%d,next_song_pcm_bytes:%d,partial_transition_delaying:%d",*samples,g_prev_samples,compress->bytes_decoded,compress->pcm_bytes_left,total,avail,(uint32_t)compress->pcm,compress->songs_pcm_decoded_bytes,compress->bytes_decoded,compress->samplerate,compress->pcm_cfg.rate,
        compress->gapless_state.gapless_transition,compress->gapless_state.next_song_pcm_bytes,compress->gapless_state.gapless_transition_delaying);
    }
    g_prev_samples = *samples;
    return 0;
}

int compress_next_track(struct compress *compress)
{
    int stream_id = 0;
    int ret = 0;
    int32_t stream_index;
    struct  nexttrack_param  *param = NULL;

    if(!compress) {
        ALOGE("%s error: compres is null",__FUNCTION__);
        return -1;
    }
    ALOGE("%s in",__FUNCTION__);
    pthread_mutex_lock(&compress->lock);
    if (!is_compress_running(compress)) {
        pthread_mutex_unlock(&compress->lock);
        return -1;
    }
    if(!compress->use_dsp_gapless_mode) {
        pthread_mutex_unlock(&compress->lock);
        ALOGE("ignore trigger next track %s out",__FUNCTION__);
        return 0;
    }
    ALOGE("%s in,stream_id:%d,song_count:%d",__FUNCTION__,stream_id,compress->gapless_state.songs_count);
    stream_id = NEXT_STREAM_ID(compress->client_stream_id);
    stream_index = STREAM_ARRAY_INDEX(stream_id);
    if (stream_index >= MAX_NUMBER_OF_STREAMS ||stream_index < 0) {
        ALOGE("%s: Invalid stream index: %d", __func__,
        stream_index);
        pthread_mutex_unlock(&compress->lock);
        return -1;
    }
    pthread_mutex_lock(&compress->gapless_state_mutex);
    if (compress->gapless_state.stream_opened[stream_index]) {
        if (compress->gapless_state.gapless_transition) {
            ALOGE("compress_wait_for_stream_avail wait in:stream_id:%d",stream_id);
            ret = compress_wait_for_stream_avail(compress,&compress->gapless_state_mutex);
            if(ret) {
                ALOGE("compress_wait_for_stream_avail interrupted\n");
                pthread_mutex_unlock(&compress->gapless_state_mutex);
                pthread_mutex_unlock(&compress->lock);
                return 0;
            }
            ALOGE("compress_wait_for_stream_avail wait out ok:stream_id:%d,stream_opened:%d",stream_id,compress->gapless_state.stream_opened[stream_index]);
        } else {
        /*
        * If session is already opened break out if
        * the state is not gapless transition. This
        * is when seek happens after the last buffer
        * is sent to the driver. Next track would be
        * called again after last buffer is sent.
        */
        ALOGE("next session is in opened state\n");
        pthread_mutex_unlock(&compress->gapless_state_mutex);
        pthread_mutex_unlock(&compress->lock);
        return 0;
        }
    }

    param = calloc(1, sizeof(struct nexttrack_param));
    if(!param) {
        ALOGE("error mallock %s",__FUNCTION__);
        pthread_mutex_unlock(&compress->gapless_state_mutex);
        pthread_mutex_unlock(&compress->lock);
        return 0;
    }
    param->stream_id = stream_id;
    compress->gapless_state.songs_count++;
    compress->gapless_state.stream_opened[stream_index] = 1;
    compress->gapless_state.set_next_stream_id = true;
    pthread_mutex_unlock(&compress->gapless_state_mutex);
    ALOGE("send COMPERSS_CMD_NEXTTRACK in:stream1_open:%d,stream2_open:%d",compress->gapless_state.stream_opened[0],compress->gapless_state.stream_opened[1]);
    compress_send_cmd(compress, COMPERSS_CMD_NEXTTRACK,param,1);
    pthread_mutex_unlock(&compress->lock);
    ALOGE("%s out,stream_id:%d,song_count:%d",__FUNCTION__,stream_id,compress->gapless_state.songs_count);
    return 0;
}

int compress_wait(struct compress *compress, int timeout_ms)
{
     ALOGI("%s in,timeout_ms:%d",__FUNCTION__,timeout_ms);

    if(!compress) {
        ALOGE("%s error: compres is null",__FUNCTION__);
        return -1;
    }
    pthread_mutex_lock(&compress->cond_mutex);
    if(!compress->buffer_ready) {
        ALOGI("%s in 1",__FUNCTION__);
        pthread_cond_wait(&compress->cond, &compress->cond_mutex);
        ALOGI("%s out 1",__FUNCTION__);
    }
    compress->buffer_ready = 0;
    pthread_mutex_unlock(&compress->cond_mutex);
    ALOGI("%s out",__FUNCTION__);
    //sem_wait(&compress->sem_wait);
    return 0;
}

int compress_stop(struct compress *compress)
{
    if(!compress) {
        ALOGE("%s error: compres is null",__FUNCTION__);
        return -1;
    }
    g_prev_samples = 0;
    ALOGI("compress_stop in");
    pthread_mutex_lock(&compress->lock);
    if (!is_compress_running(compress)){
        pthread_mutex_unlock(&compress->lock);
        return 0;
    }
    compress->running = 0;
    compress->state = STATE_STOP ;
    compress_send_cmd(compress, COMPERSS_CMD_STOP,NULL,1);
    pthread_mutex_unlock(&compress->lock);
    ALOGI("compress_stop out");
    return 0;
}

void compress_nonblock(struct compress *compress, int nonblock)
{

}

void compress_close(struct compress *compress)
{
    int ret = 0;
    if(!compress) {
        ALOGE("%s error: compres is null",__FUNCTION__);
        return ;
    }
    ALOGE("compress_close in");
    do{
        ret = pthread_mutex_trylock(&compress->lock);
        if(ret) {
            ALOGE("compress_close but is busy");
            pthread_mutex_lock(&compress->cond_mutex);
            compress->buffer_ready = 1;
            pthread_cond_signal(&compress->cond);
            pthread_mutex_unlock(&compress->cond_mutex);
            usleep(20000);
        }
    }while(ret);

    compress->running = 0;
    compress->ready = 0;

    compress->state = STATE_EXIT;
    compress_send_cmd(compress, COMPERSS_CMD_CLOSE,NULL,1);

    if(compress->thread) {
        pthread_join(compress->thread, (void **) NULL);
    }
    if(compress->mMP3DecHandle) {
        MP3_ARM_DEC_Deconstruct(&compress->mMP3DecHandle);
        compress->mMP3DecHandle = NULL;
    }

    if(compress->pcm_stream.ring) {
        ringbuffer_free(compress->pcm_stream.ring);
        compress->pcm_stream.ring = NULL;
    }

    if(compress->stream.ring) {
        ringbuffer_free(compress->stream.ring);
        compress->stream.ring = NULL;
    }

    if(compress->cache_pcm_stream.ring) {
        ringbuffer_free(compress->cache_pcm_stream.ring);
        compress->cache_pcm_stream.ring = NULL;
    }

    if(compress->pcm){
        pcm_close(compress->pcm);
        compress->pcm = NULL;
    }

    if(compress->aud_fd >0) {
        close(compress->aud_fd);
    }

    if(compress->resample_buffer) {
        free(compress->resample_buffer);
    }

    if(compress->resampler) {
        release_resampler(compress->resampler);
    }

    pthread_mutex_destroy(&compress->lock);

    pthread_cond_destroy(&compress->cond);
    pthread_mutex_destroy(&compress->cond_mutex);

    pthread_cond_destroy(&compress->cmd_cond);
    pthread_mutex_destroy(&compress->cmd_mutex);
    pthread_mutex_destroy(&compress->gapless_state_mutex);

    pthread_mutex_destroy(&compress->pcm_mutex);

    pthread_mutex_destroy(&compress->position_mutex);
    pthread_cond_destroy(&compress->drain_cond);
    pthread_mutex_destroy(&compress->drain_mutex);

    pthread_cond_destroy(&compress->partial_drain_cond);
    pthread_mutex_destroy(&compress->partial_drain_mutex);

    pthread_cond_destroy(&compress->partial_wait_cond);
    pthread_mutex_destroy(&compress->partial_wait_mutex);

    free(compress->config);
    free(compress);
    ALOGE("compress_close out");
}

void register_offload_pcmdump(struct compress *compress,void *fun){
    compress->dumppcmfun=fun;
}




