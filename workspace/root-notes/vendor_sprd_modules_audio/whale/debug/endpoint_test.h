#ifndef ENDPINT_TEST_H
#define ENDPINT_TEST_H
#include <pthread.h>
#include "ring_buffer.h"
#include <semaphore.h>
#include <cutils/str_parms.h>
#include "audio_debug.h"

enum aud_endpoint_test_type{
    ENDPOINT_TEST_IDLE =0,
    INPUT_ENDPOINT_TEST,
    OUTPUT_ENDPOINT_TEST,
    INPUT_OUTPUT_ENDPOINT_LOOP_TEST,
};

struct test_endpoint_param{
    int pcm_rate;
    int pcm_channels;
    int fd;
    void *data;
    unsigned int data_size;
    int type;
    int delay;
    int test_step;
    int loopcunt;
    int durationMs;
};

struct endpoint_resampler_ctl{
    struct resampler_itfe *resampler;
    char *resampler_buffer;
    int resampler_buffer_size;
    char *conversion_buffer;
    int conversion_buffer_size;
    int rate;
    int channels;
    int request_rate;
    int request_channles;
};

struct audio_test_point{
    pthread_t thread;
    bool is_exit;
    pthread_mutex_t lock;
    void *stream;
    struct ring_buffer *ring_buf;
    struct test_endpoint_param param;
    struct endpoint_resampler_ctl resampler_ctl;
    int period_size;
    int npi_readsize;
    void *testctl;
    void *buffer;
};

struct endpoint_test_control{
    struct audio_test_point *source;
    struct audio_test_point *sink;
    int loopcunt;
    int durationMs;

#ifdef SPRD_AUDIO_HIDL_CLIENT
    char *hidl_buffer;
    int hidl_buffer_size;
    int written;
    void *hidl_inputstream;
    void *hild_outputstream;
#endif
  };

struct endpoint_control{
    void *primary_ouputstream;
    struct endpoint_test_control ouput_test;
    struct endpoint_test_control input_test;
    struct endpoint_test_control loop_test;
};

int audio_endpoint_test(void * dev,struct str_parms *parms,UNUSED_ATTR int opt,UNUSED_ATTR char * val);
int audio_endpoint_test_init(void * dev,void *out_stream);
#ifdef SPRD_AUDIO_HIDL_CLIENT
int hidl_stream_read(void *dev, void *buffer,int size);
int malloc_endpoint_outputtest_buffer(struct endpoint_control *ctl,int size);
int write_endpoint_outputtest_buffer(struct endpoint_control *ctl, const void* buffer,
    size_t bytes);
void set_hidl_read_size(void *dev,int size);
#endif
#define AUDIO_EXT_CONTROL_PIPE_MAX_BUFFER_SIZE  1024
#endif
