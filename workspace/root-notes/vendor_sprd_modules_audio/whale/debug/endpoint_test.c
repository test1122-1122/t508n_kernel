#define LOG_TAG "audio_hw_bbat"
#include "endpoint_test.h"
#include <audio_utils/resampler.h>
#include <sys/select.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdlib.h>
#include <pthread.h>
#include <stdint.h>

#include "ring_buffer.h"

#include <errno.h>
#include <pthread.h>
#include <stdint.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/mman.h>
#include <semaphore.h>

#ifdef AUDIOHAL_V4
#include <log/log.h>
#else
#include <cutils/log.h>
#endif
#include <cutils/str_parms.h>
#include <cutils/properties.h>

#include <hardware/hardware.h>
#include <system/audio.h>
#include <hardware/audio.h>

#include <expat.h>

#include <tinyalsa/asoundlib.h>
#include "audio_hw.h"
#include "smartamp.h"
#include <audio_utils/primitives.h>

extern int32_t mono2stereo(int16_t *out, int16_t * in, uint32_t in_samples);
extern unsigned int producer_proc(struct ring_buffer *ring_buf,unsigned char * buf,unsigned int size);
extern int agdsp_send_msg_test(void * arg,UNUSED_ATTR struct str_parms * params,int opt,UNUSED_ATTR char *val);

#define MIN_BUFFER_TIMS_MS  10
#define DEFAULT_FRAME_SIZE 320
#define AUDIO_EXT_CONTROL_PIPE_MAX_BUFFER_SIZE  1024
#ifdef AUDIOHAL_V4
#define MMI_DEFAULT_PCM_FILE  "/data/vendor/local/media/aploopback.pcm"
#else
#define MMI_DEFAULT_PCM_FILE  "/data/local/media/aploopback.pcm"
#endif
#define TEST_BUFFER_COUNT 16
long getCurrentTimeUs();
#define TEST_DEFAULT_SAMPLE_RATE 48000

static const unsigned short s_48000_pcmdata_mono[] = {
    0x0001,0x10b5,0x2122,0x30fb,0x4000,0x4deb,0x5a82,0x658b,
    0x6eda,0x7640,0x7ba3,0x7ee7,0x7ffe,0x7ee7,0x7ba2,0x7642,
    0x6ed9,0x658b,0x5a81,0x4deb,0x4000,0x30fc,0x2121,0x10b5,
    0x0000,0xef4b,0xdede,0xcf04,0xc000,0xb215,0xa57f,0x9a74,
    0x9127,0x89bf,0x845e,0x811a,0x8001,0x8119,0x845e,0x89bf,
    0x9126,0x9a75,0xa57e,0xb215,0xc000,0xcf04,0xdedf,0xef4b,
};

audio_format_t pcmformat_to_audioformat(enum pcm_format format){
    switch(format){
        case PCM_FORMAT_S16_LE:
            return AUDIO_FORMAT_PCM_16_BIT;
            break;
        case PCM_FORMAT_S32_LE:
            return AUDIO_FORMAT_PCM_32_BIT;
            break;
        case PCM_FORMAT_S24_LE:
            return AUDIO_FORMAT_PCM_8_24_BIT;
            break;
        case PCM_FORMAT_S24_3LE:
            return AUDIO_FORMAT_PCM_24_BIT_PACKED;
            break;
        default:
            break;
    }
    return AUDIO_FORMAT_PCM_16_BIT;
}

static int32_t stereo2mono(int16_t *out, int16_t * in, uint32_t in_samples) {
    int i = 0;
    int out_samples =  in_samples >> 1;
    for(i = 0 ; i< out_samples ; i++) {
        out[i] =(in[2*i+1] + in[2*i]) /2;
    }
    return out_samples;
}

static int endpoint_calculation_ring_buffer_size(int size){
    int i=30;
    while(i){
        if((size & (1<<i))!=0)
            break;
        else
            i--;
    }

    if(i<=0)
        return 0;
    else
        return 1<<(i+1);
}


static int deinit_resampler(struct endpoint_resampler_ctl *resampler_ctl)
{
    LOG_I("deinit_resampler:%p",resampler_ctl->resampler);
    if (resampler_ctl->resampler) {
        release_resampler(resampler_ctl->resampler);
        resampler_ctl->resampler = NULL;
    }

    if(resampler_ctl->resampler_buffer!=NULL){
        free(resampler_ctl->resampler_buffer);
        resampler_ctl->resampler_buffer=NULL;
        resampler_ctl->resampler_buffer_size=0;
    }

    if(resampler_ctl->conversion_buffer!=NULL){
        free(resampler_ctl->conversion_buffer);
        resampler_ctl->conversion_buffer=NULL;
        resampler_ctl->conversion_buffer_size=0;
    }
    return 0;
}

static int init_resampler(struct endpoint_resampler_ctl *resampler_ctl)
{
    resampler_ctl->resampler = NULL;
    resampler_ctl->resampler_buffer=NULL;
    resampler_ctl->resampler_buffer_size=0;
    resampler_ctl->conversion_buffer=NULL;
    resampler_ctl->conversion_buffer_size=0;
    return 0;
}

static int pcm_buffer_format_change(
    void *input_buffer ,
    int input_buffer_size,
    struct endpoint_resampler_ctl *resampler_ctl){

    int frame_size=4;

    if ( resampler_ctl->channels!=resampler_ctl->request_channles){
            int num_read_buff_bytes=0;

            num_read_buff_bytes=input_buffer_size*resampler_ctl->request_channles/resampler_ctl->channels;
            if (num_read_buff_bytes != resampler_ctl->conversion_buffer_size) {
                if (num_read_buff_bytes > resampler_ctl->conversion_buffer_size) {
                    resampler_ctl->conversion_buffer_size=num_read_buff_bytes;
                    LOG_I("pcm_buffer_format_change LINE:%d %p %d input_buffer_size:%d",__LINE__,
                        resampler_ctl->conversion_buffer,resampler_ctl->conversion_buffer_size,input_buffer_size);
                    resampler_ctl->conversion_buffer = realloc(resampler_ctl->conversion_buffer, resampler_ctl->conversion_buffer_size);
                }
            }

            if((resampler_ctl->request_channles == 1) && (resampler_ctl->channels == 2)) {
                stereo2mono((int16_t *)resampler_ctl->conversion_buffer,input_buffer,input_buffer_size/2);
            }else if((resampler_ctl->request_channles == 2) && (resampler_ctl->channels == 1)) {
                mono2stereo((int16_t *)resampler_ctl->conversion_buffer,input_buffer,input_buffer_size/2);
            }
    }

    if ( resampler_ctl->rate!=resampler_ctl->request_rate){
        int resampler_inputbuffer_size=0;
        void *resampler_inputbuffer=NULL;
        int new_resampler_buffer_size=0;
        int in_frames=0;
        int out_frames=0;

        if((resampler_ctl->resampler_buffer==NULL)&&(NULL==resampler_ctl->resampler)){
            int ret=0;
            ret = create_resampler(resampler_ctl->rate,
                                   resampler_ctl->request_rate,
                                   resampler_ctl->request_channles,
                                   RESAMPLER_QUALITY_DEFAULT,
                                   NULL, &resampler_ctl->resampler);
            if (ret != 0) {
                ret = -EINVAL;
                LOG_E("%s line:%d create_resampler Failed",__func__,__LINE__);
                return ret;
            }
        }

        if(resampler_ctl->conversion_buffer!=NULL){
            resampler_inputbuffer=resampler_ctl->conversion_buffer;
            resampler_inputbuffer_size=resampler_ctl->conversion_buffer_size;
        }else{
            resampler_inputbuffer=input_buffer;
            resampler_inputbuffer_size=input_buffer_size;
        }
        new_resampler_buffer_size=2*resampler_inputbuffer_size*resampler_ctl->request_rate/resampler_ctl->rate;

        if (new_resampler_buffer_size > resampler_ctl->resampler_buffer_size) {
            resampler_ctl->resampler_buffer_size=new_resampler_buffer_size;
            LOG_I("pcm_buffer_format_change LINE:%d %p %d",__LINE__,
                resampler_ctl->resampler_buffer,resampler_ctl->resampler_buffer_size);
            resampler_ctl->resampler_buffer = realloc(resampler_ctl->resampler_buffer, resampler_ctl->resampler_buffer_size);
        }

        in_frames=resampler_inputbuffer_size/frame_size;
        out_frames=resampler_ctl->resampler_buffer_size/frame_size;
        LOG_I("pcm_buffer_format_change LINE:%d in_frames:%d out_frames:%d",
        __LINE__,in_frames,out_frames);
        resampler_ctl->resampler->resample_from_input(resampler_ctl->resampler,
                (int16_t *)resampler_inputbuffer,(size_t *)&in_frames,
                (int16_t *) resampler_ctl->resampler_buffer,(size_t *)&out_frames);
        LOG_I("pcm_buffer_format_change LINE:%d in_frames:%d out_frames:%d",
        __LINE__,in_frames,out_frames);
        return out_frames*frame_size;
    }else{
        if(resampler_ctl->resampler_buffer!=NULL){
            free(resampler_ctl->resampler_buffer);
            resampler_ctl->resampler_buffer=NULL;
        }
        resampler_ctl->resampler_buffer_size=0;
   }
    return resampler_ctl->conversion_buffer_size;
}

static int sink_to_write(struct audio_test_point *sink,void *buffer, int size){
    struct tiny_stream_out * stream_out=(struct tiny_stream_out *)(sink->stream);
    int mframeCount=0;
    int read_size=0;
    audio_format_t mdst_format=AUDIO_FORMAT_PCM_16_BIT;

    if(stream_out!=NULL){
        mdst_format=pcmformat_to_audioformat(stream_out->config->format);
        mframeCount=size/audio_bytes_per_sample(mdst_format);
        read_size=mframeCount*audio_bytes_per_sample(AUDIO_FORMAT_PCM_16_BIT);
    }else{
        read_size=size;
    }
    int num_read=0;
    stream_out=(struct tiny_stream_out *)(sink->stream);
    // ring_buffer data format is AUDIO_FORMAT_PCM_16_BIT
    num_read=ring_buffer_get(sink->ring_buf, (void *)buffer, read_size);
    if(num_read > 0){
        int ret=-1;
        if(NULL!=stream_out){
            if((mdst_format==AUDIO_FORMAT_PCM_8_24_BIT)
                &&(NULL!=sink->buffer)){
                mframeCount=num_read/audio_bytes_per_sample(AUDIO_FORMAT_PCM_16_BIT);
                memcpy_to_q8_23_from_i16(sink->buffer,buffer,mframeCount);
                ret = out_write_test(&stream_out->stream,
                sink->buffer,mframeCount*audio_bytes_per_sample(mdst_format));
            }else{
                ret = out_write_test(&stream_out->stream,
                    buffer,num_read);
            }
        }else{
            usleep(MIN_BUFFER_TIMS_MS*1000);
        }

        if(sink->is_exit==false)
        {
            int available_size=ring_buffer_len(sink->ring_buf);
            if(available_size<read_size){
                usleep(MIN_BUFFER_TIMS_MS*1000);
            }
        }
    }else if(sink->is_exit==false){
        usleep(MIN_BUFFER_TIMS_MS*1000);
    }
    return num_read;
}

static int source_in_read(struct audio_test_point *source,struct test_endpoint_param *param, void *buffer, int size){
    struct tiny_stream_in * stream_in=NULL;
    stream_in=(struct tiny_stream_in *)(source->stream);

    int num_read=0;
    int read_size=0;
    read_size=size*source->resampler_ctl.channels*source->resampler_ctl.rate/
        (source->resampler_ctl.request_channles*source->resampler_ctl.request_rate);

    read_size/=4;
    read_size*=4;

    if(param->fd>0){
        num_read=read(param->fd,buffer,read_size);
        if((num_read<read_size)||(num_read<0)){
            lseek(param->fd,0,SEEK_SET);
            if(num_read<0){
                LOG_E("audio_source_thread read:%d failed size:%d",param->fd,read_size);
                return 0;
            }
        }

        if((source->resampler_ctl.channels!=source->resampler_ctl.request_channles)
            ||(source->resampler_ctl.rate!=source->resampler_ctl.request_rate)){
            num_read=pcm_buffer_format_change(buffer,num_read,&source->resampler_ctl);
            if(source->resampler_ctl.resampler_buffer!=NULL){
                producer_proc(source->ring_buf, (unsigned char *)source->resampler_ctl.resampler_buffer, (unsigned int)num_read);
            }else{
                producer_proc(source->ring_buf, (unsigned char *)source->resampler_ctl.conversion_buffer, (unsigned int)num_read);
            }
        }else{
            producer_proc(source->ring_buf, (unsigned char *)buffer, (unsigned int)num_read);
        }
    }else if ((param->data!=NULL) &&(param->data_size>0)){
        int data_count=read_size/param->data_size;
        int i=0;
        read_size=0;
        if(data_count<=0){
            LOG_E("%s line:%d data_count:%d read_size:%d size:%d"
                ,__func__,__LINE__,data_count,read_size,size);
            return 0;
        }
        for(i=0;i<data_count;i++){
            memcpy((char *)buffer+i*param->data_size,param->data,param->data_size);
            read_size+=param->data_size;
        }
        if((source->resampler_ctl.channels!=source->resampler_ctl.request_channles)
            ||(source->resampler_ctl.rate!=source->resampler_ctl.request_rate)){
            num_read=pcm_buffer_format_change(buffer,read_size,&source->resampler_ctl);
            if(source->resampler_ctl.resampler_buffer!=NULL){
                producer_proc(source->ring_buf, (unsigned char *)source->resampler_ctl.resampler_buffer, (unsigned int)num_read);
            }else{
                producer_proc(source->ring_buf, (unsigned char *)source->resampler_ctl.conversion_buffer, (unsigned int)num_read);
            }
        }else{
            producer_proc(source->ring_buf, (unsigned char *)buffer, (unsigned int)read_size);
        }
    }else if(source->stream!=NULL) {
        num_read=stream_in->stream.read(source->stream,buffer,size);
        LOG_D("audio_source_thread read:0x%x req:0x%x",num_read,size);
        if (num_read){
            producer_proc(source->ring_buf, (unsigned char *)buffer, (unsigned int)num_read);
        }else{
            LOG_W("audio_source_thread: no data read num_read:%d size:%d",num_read,size);
            usleep(MIN_BUFFER_TIMS_MS*1000);
        }
    }else{
        LOG_E("%s %d",__func__,__LINE__);
        usleep(50*MIN_BUFFER_TIMS_MS*1000);
    }
    return num_read;
}

static void endpoint_thread_exit(struct audio_test_point *point){
    deinit_resampler(&point->resampler_ctl);
    if(point->param.fd>0){
        close(point->param.fd);
        point->param.fd=-1;
    }

    if(point->param.data!=NULL){
        free(point->param.data);
        point->param.data=NULL;
    }
}

static void *audio_source_thread(void *args){
    struct audio_test_point *source=(struct audio_test_point *)args;
    struct test_endpoint_param *param=(struct test_endpoint_param *)&source->param;
    struct endpoint_test_control *ctl=(struct endpoint_test_control *)source->testctl;
    struct tiny_stream_in * stream_in=NULL;

    int ret=0;
    int loopcunt=ctl->loopcunt;
    int durationMs = ctl->durationMs;
    long stoptime=(ctl->durationMs*1000)+getCurrentTimeUs();
    int mode=0;

    if(loopcunt>0){
        mode=1;  //loop mode
    }else if(durationMs>0){
        mode=2;  // timeout mode
    }

    char *buffer= (char *)malloc(source->period_size);
    if (NULL==buffer) {
        LOG_E("Unable to allocate %d bytes\n", source->period_size);
        return NULL;
    }

    pthread_mutex_lock(&(source->lock));

    LOG_I("audio_source_thread param:%d %d %p period_size:%d",
        param->fd,param->data_size,source->stream,source->period_size);

    while(source->is_exit==false){
        pthread_mutex_unlock(&(source->lock));
        ret=source_in_read(source,&source->param, buffer, source->period_size);
        if((mode==1)&&(ret< 0)&&(source->param.fd>0)){
            if(loopcunt>0){
                loopcunt--;
            }
            if(loopcunt<=0){
                LOG_I("audio_source_thread start stop test with loop end");
                source->is_exit=true;
            }
        }else if((mode==2)&&(stoptime< getCurrentTimeUs())){
            LOG_I("audio_source_thread start stop test with timeout");
            source->is_exit=true;
        }
        pthread_mutex_lock(&(source->lock));
    }

    if(NULL!=buffer){
        free(buffer);
    }

    if(source->stream!=NULL){
        stream_in = (struct tiny_stream_in *)source->stream;
        stream_in->stream.common.standby(source->stream);
    }
    endpoint_thread_exit(source);
    pthread_mutex_unlock(&(source->lock));

    if(mode!=0){
        struct audio_test_point *sink=(struct audio_test_point *)ctl->sink;
        pthread_mutex_lock(&(sink->lock));
        sink->is_exit=true;
        LOG_I("audio_source_thread stop sink thread");
        pthread_mutex_unlock(&(sink->lock));
    }

    LOG_I("audio_source_thread exit");
    return args;
}

static void *audio_sink_thread(void *args){
    struct audio_test_point *sink=(struct audio_test_point *)args;
    struct tiny_stream_out * stream_out=NULL;
    audio_format_t mformat=PCM_FORMAT_INVALID;
    int mframeCount=0;
    int read_size=0;
    char *buffer=NULL;
    stream_out=(struct tiny_stream_out *)(sink->stream);

    if(NULL!=stream_out){
        mformat=pcmformat_to_audioformat(stream_out->config->format);
        mframeCount=sink->period_size/(audio_bytes_per_sample(mformat)*stream_out->config->channels);

        read_size=mframeCount*stream_out->config->channels*audio_bytes_per_sample(AUDIO_FORMAT_PCM_24_BIT_PACKED);
    }else{
        read_size=sink->period_size;
    }
    buffer= (char *)malloc(read_size);
    if (NULL==buffer) {
        LOG_E("Unable to allocate %d bytes\n",read_size);
        return NULL;
    }

    if(read_size>sink->period_size){
        sink->buffer= (char *)malloc(read_size);
    }else{
        sink->buffer= (char *)malloc(sink->period_size);
    }
    if (NULL==buffer) {
        LOG_E("Unable to allocate %d bytes\n",read_size);
        return NULL;
    }


    LOG_I("audio_sink_thread stream:%p period_size:%d read_size%d"
        , sink->stream,sink->period_size,read_size);
    LOG_I("audio_sink_thread ring_buf:%p sink:%p",sink->ring_buf,sink);

    pthread_mutex_lock(&(sink->lock));
    while(sink->is_exit==false){
        pthread_mutex_unlock(&(sink->lock));
        if(sink->npi_readsize){
            usleep(MIN_BUFFER_TIMS_MS*1000);
        }else{
            sink_to_write(sink,buffer,read_size);
        }
        pthread_mutex_lock(&(sink->lock));
    }

    if(NULL!=buffer){
        free(buffer);
        buffer=NULL;
    }

    if(sink->stream!=NULL){
        stream_out = (struct tiny_stream_out * )sink->stream;
        stream_out->stream.common.standby(sink->stream);
    }

    if(sink->buffer!=NULL){
        free(sink->buffer);
        sink->buffer=NULL;
    }

    endpoint_thread_exit(sink);
    pthread_mutex_unlock(&(sink->lock));
    LOG_I("audio_sink_thread exit");
    return args;
}

static void open_input_endpoint(void * dev,
    struct audio_test_point *input,
    struct test_endpoint_param *param){

    struct tiny_audio_device * adev=(struct tiny_audio_device *)dev;
    struct audio_config config;
    struct audio_stream_in *stream_in=NULL;

    config.sample_rate=param->pcm_rate;
    if(param->pcm_channels==2){
        config.channel_mask=AUDIO_CHANNEL_IN_STEREO;
    }else{
        config.channel_mask=AUDIO_CHANNEL_IN_MONO;
    }
    config.format=AUDIO_FORMAT_PCM_16_BIT;

    if(config.sample_rate==0){
        config.sample_rate=TEST_DEFAULT_SAMPLE_RATE;
    }

    LOG_I("open_input_endpoint rate:%d stream:%p in_devices:0x%x"
        ,config.sample_rate,input->stream,adev->dev_ctl->debug_in_devices);
    if(NULL==input->stream){
        adev_open_input_stream(dev,0,adev->dev_ctl->debug_in_devices,&config,&stream_in,AUDIO_INPUT_FLAG_NONE,NULL,AUDIO_SOURCE_VOICE_RECOGNITION);
        input->stream=stream_in;
    }
    if(NULL!=input->stream){
        stream_in->common.standby(&stream_in->common);
    }else{
        LOG_E("%s line:%d",__func__,__LINE__);
    }
}

static void  init_test_bufffer(struct audio_test_point *point,struct test_endpoint_param *param,bool is_sink){
    int delay_size=0;

    if(NULL==point->stream){
        point->period_size=0;
        point->ring_buf=NULL;
        return;
    }else{
        if(true==is_sink){
            struct tiny_stream_out * stream_out=(struct tiny_stream_out *)point->stream;
            struct audio_stream_out * audio_stream=NULL;
            audio_stream=(struct audio_stream_out *)(&stream_out->stream);
            point->period_size=audio_stream->common.get_buffer_size(&audio_stream->common);
            LOG_I("%s line:%d %d",__func__,__LINE__,point->period_size);
            delay_size=param->delay*stream_out->config->rate*stream_out->config->channels*2/1000;
        }else{
            struct tiny_stream_in * stream_in=(struct tiny_stream_in *)point->stream;
            struct audio_stream_in *audio_stream=NULL;
            audio_stream=(struct audio_stream_in *)(&stream_in->stream);
            point->period_size=audio_stream->common.get_buffer_size(&audio_stream->common);
            LOG_I("%s line:%d %d",__func__,__LINE__,point->period_size);
            delay_size=param->delay*stream_in->config->rate*stream_in->config->channels*2/1000;
        }
    }

    {
        int ring_buffer_size=point->period_size*TEST_BUFFER_COUNT;
        if((unsigned int)ring_buffer_size<param->data_size){
            ring_buffer_size=param->data_size*TEST_BUFFER_COUNT;
            LOG_W("init_test_bufffer data_size:%d period_size:%d",param->data_size,point->period_size);
        }
        ring_buffer_size=endpoint_calculation_ring_buffer_size(ring_buffer_size);
        point->ring_buf=ring_buffer_init(ring_buffer_size,(delay_size/DEFAULT_FRAME_SIZE+1)*DEFAULT_FRAME_SIZE);
        if(NULL==point->ring_buf){
            point->period_size=0;
        }
        LOG_I("%s line:%d %d",__func__,__LINE__,point->period_size);
    }
}

static void dump_point(struct audio_test_point *point){
    if(point==NULL){
        LOG_E("%s line:%d point:%p",__func__,__LINE__,point);
        return;
    }

    LOG_I("point:%p is_exit:%d stream:%p period_size:%d",
        point,point->is_exit,point->stream,point->period_size);

    if(point->ring_buf!=NULL){
        LOG_I("point:%p ring_buf:%p ring_buf size:%d in:%d out:%d",
            point,point->ring_buf,point->ring_buf->size,
            point->ring_buf->in,
            point->ring_buf->out);
    }

    LOG_I("point:%p param: pcm_rate:%d pcm_channels:%d fd:%d data:%p data_size:%d type:%d delay:%d test_step:%d",
        point,
        point->param.pcm_rate,
        point->param.pcm_channels,
        point->param.fd,
        point->param.data,
        point->param.data_size,
        point->param.type,
        point->param.delay,
        point->param.test_step
    );
}

static int free_endpoint(struct audio_test_point *point){

    if(point==NULL){
        LOG_E("%s line:%d point:%p",__func__,__LINE__,point);
        return -1;
    }

    point->is_exit=true;
    usleep(MIN_BUFFER_TIMS_MS*1000);

    if(point->ring_buf!=NULL){
        ring_buffer_free(point->ring_buf);
       point->ring_buf=NULL;
    }
    free(point);
    return 0;
}

static struct audio_test_point *create_source(void *dev,struct test_endpoint_param *param){

    struct audio_test_point *source=NULL;

    source=malloc(sizeof(struct audio_test_point));
    if(NULL==source){
         LOG_E("create_source malloc  failed");
         return NULL;
    }

    memset(source,0,sizeof(struct audio_test_point));
    source->stream=NULL;
    source->param.type=param->type;
    source->param.data=NULL;
    source->param.data_size=0;
    source->param.fd=-1;
    init_resampler(&source->resampler_ctl);

    if (pthread_mutex_init(&(source->lock), NULL) != 0) {
        LOG_E("Failed pthread_mutex_init loop->in.lock,errno:%u,%s",
              errno, strerror(errno));
        return NULL;
    }

    source->is_exit=false;

    if(OUTPUT_ENDPOINT_TEST==param->type){
        LOG_I("create_source data:%p data_size:%d",param->data,param->data_size);
    }else{
        open_input_endpoint(dev,source,param);
         if(INPUT_ENDPOINT_TEST==param->type){
             init_test_bufffer(source,param,false);
         }
    }
    LOG_I("create_source fd:%d",source->param.fd);
    return source;
}

struct audio_test_point *create_sink(void *dev,void * stream,
    struct test_endpoint_param *param){
    struct tiny_audio_device * adev=(struct tiny_audio_device *)dev;

    struct tiny_stream_out * stream_out=NULL;

    struct audio_test_point *sink=NULL;
    sink=malloc(sizeof(struct audio_test_point));
    if(NULL==sink){
        return NULL;
    }
    memset(sink,0,sizeof(struct audio_test_point));
    sink->param.data=NULL;
    sink->param.data_size=0;
    sink->param.fd=-1;
    sink->param.type=param->type;
    sink->resampler_ctl.resampler=NULL;
    init_resampler(&sink->resampler_ctl);
    stream_out=(struct tiny_stream_out *)(stream);
    sink->stream=stream;

    if(NULL!=stream_out){
        if(false==stream_out->standby){
            LOG_E("%s line:%d %p",__func__,__LINE__,stream_out);
            goto error;
        }
        stream_out->devices=adev->dev_ctl->debug_out_devices;
    }

    if (pthread_mutex_init(&(sink->lock), NULL) != 0) {
        LOG_E("Failed pthread_mutex_init loop->in.lock,errno:%u,%s",
              errno, strerror(errno));
        goto error;

    }

    if(INPUT_ENDPOINT_TEST!=param->type){
        init_test_bufffer(sink,param,true);
        sink->param.fd=param->fd;
    }
    LOG_I("create_sink fd:%d",sink->param.fd);
    LOG_I("%s line:%d %d stream:%p",__func__,__LINE__,sink->period_size,sink->stream);

    sink->is_exit=false;
    return sink;

error:

    if(sink!=NULL){
        free(sink);
        sink=NULL;
    }
    return sink;
}

static int source_to_sink(struct audio_test_point *source,
    struct audio_test_point *sink,struct test_endpoint_param *param){
    struct tiny_stream_in * stream_in=NULL;
    struct tiny_stream_out * stream_out=NULL;
    stream_in=(struct tiny_stream_in *)(source->stream);
    stream_out=(struct tiny_stream_out *)(sink->stream);

    LOG_I("source_to_sink:%p %p param type:%d fd:%d"
        ,source,sink,param->type,param->fd);
    switch(param->type){
        case INPUT_ENDPOINT_TEST:
                stream_in->requested_channels=param->pcm_channels;
                stream_in->requested_rate=param->pcm_rate;
                sink->period_size=source->period_size;
                sink->ring_buf=source->ring_buf;
                sink->param.fd=param->fd;
            break;
        case OUTPUT_ENDPOINT_TEST:
                source->resampler_ctl.request_channles=stream_out->config->channels;
                source->resampler_ctl.request_rate=stream_out->config->rate;
                source->resampler_ctl.channels=param->pcm_channels;
                source->resampler_ctl.rate=param->pcm_rate;

                if((param->data_size!=0)&&(param->data!=NULL)){
                    source->param.data=param->data;
                    source->param.data_size=param->data_size;
                    param->data=NULL;
                    param->data_size=0;
                }
                LOG_I("%s line:%d sink %d",__func__,__LINE__,sink->period_size);
                LOG_I("%s line:%d source %d",__func__,__LINE__,source->period_size);

                source->param.fd=param->fd;
                source->period_size=sink->period_size*source->resampler_ctl.channels*source->resampler_ctl.rate/
                        (source->resampler_ctl.request_channles*source->resampler_ctl.request_rate);
                if(NULL!=source->param.data){
                    source->period_size=((source->period_size/source->param.data_size)+1)*source->param.data_size;
                }
                LOG_I("%s line:%d source %d",__func__,__LINE__,source->period_size);
                source->ring_buf=sink->ring_buf;
                if(source->stream!=NULL){
                    source->stream=NULL;
                }
            break;
        case INPUT_OUTPUT_ENDPOINT_LOOP_TEST:
                stream_in->requested_channels=stream_out->config->channels;
                stream_in->requested_rate=stream_out->config->rate;
                source->period_size=sink->period_size;
                source->ring_buf=sink->ring_buf;
                source->param.fd=-1;
            break;
        default:
            stream_in->requested_channels=0;
            stream_in->requested_rate=0;
            source->period_size=0;
            sink->period_size=0;
            LOG_E("source_to_sink unknow test case");
            break;
    }

    LOG_I("source_to_sink: fd:%d source:%d",sink->param.fd,source->param.fd);
    dump_point(sink);
    dump_point(source);

    if(pthread_create(&sink->thread, NULL, audio_sink_thread, (void *)sink)) {
        LOG_E("audio_sink_thread creating tx thread failed !!!!");
        return -2;
    }

    if(pthread_create(&source->thread, NULL, audio_source_thread, (void *)source)){
        LOG_E("audio_source_thread creating rx thread failed !!!!");
        return -3;
    }
    return 0;
}

#ifdef SPRD_AUDIO_HIDL_CLIENT
void set_hidl_read_size(void *dev,int size){
    struct tiny_audio_device * adev=(struct tiny_audio_device *)dev;
    struct audio_test_point *sink=adev->test_ctl.input_test.sink;
    pthread_mutex_lock(&(sink->lock));
    if(sink!=NULL){
        sink->npi_readsize=size;
        LOG_I("set_hidl_read_size npi_readsize:%d",sink->npi_readsize);
    }else{
        LOG_E("set_hidl_read_size failed");
    }
    pthread_mutex_unlock(&(sink->lock));
}

int hidl_stream_read(void *dev, void *buffer,int size){
    struct tiny_audio_device * adev=(struct tiny_audio_device *)dev;
    struct audio_test_point *sink=adev->test_ctl.input_test.sink;
    char *buffer_tmp=buffer;
    int ret=-1;
    int total_read=size;
    int read_count=10;

    if(sink->npi_readsize>0){
        if(sink->npi_readsize<size){
            size=sink->npi_readsize;
        }

        if(sink==NULL){
            LOG_W("hidl_stream_read Failed sink is null");
            return 0;
        }

        do{
            ret=ring_buffer_get(sink->ring_buf, (void *)buffer_tmp, total_read);
            LOG_I("hidl_stream_read ring_buffer_get:%d %d",total_read,ret);
            total_read-=ret;
            buffer_tmp+=ret;
            if(total_read){
                usleep(10*1000);
                LOG_I("hidl_stream_readtotal_read:%d < size:%d",total_read,size);
                if(ret<=0){
                    read_count--;
                    if(read_count<=0){
                        break;
                    }
                }else{
                    read_count=10;
                }
            }
        }while(total_read);
        sink->npi_readsize-=size;
    }

    return size-total_read;
}
#endif

static int read_record_data(struct audio_test_point *sink,int size){
    char *buffer=NULL;
    char *buffer_tmp=NULL;
    struct test_endpoint_param *param=&sink->param;
    int ret=0;
    int default_fd=-1;
    int write_fd=-1;

    if((NULL==sink)||(size<=0)){
        LOG_E("%s line:%d sink:%p size:%d",__func__,__LINE__,sink,size);
        return -1;
    }

    buffer=malloc(size);
    if(NULL==buffer){
        LOG_E("%s line:%d malloc %d bytes failed",__func__,__LINE__,size);
        return -1;
    }

    pthread_mutex_lock(&sink->lock);
    buffer_tmp=buffer;
    sink->npi_readsize=size;
    do{
        ret=ring_buffer_get(sink->ring_buf, (void *)buffer_tmp, sink->npi_readsize);
        sink->npi_readsize-=ret;
        buffer_tmp+=ret;
        usleep(10*1000);
        LOG_I("read_record_data total_read:%d < size:%d",sink->npi_readsize,size);
    }while(sink->npi_readsize);
    pthread_mutex_unlock(&sink->lock);

    if(param->fd<=0){
        LOG_I("read_record_data start open:%s",MMI_DEFAULT_PCM_FILE);
        default_fd=open(MMI_DEFAULT_PCM_FILE, O_WRONLY);
        if(default_fd< 0) {
            LOG_E("read_record_data open:%s failed",MMI_DEFAULT_PCM_FILE);
            free(buffer);
            return -1;
        }
        write_fd=default_fd;
    }else{
        LOG_I("%s line:%d",__func__,__LINE__);
        write_fd=param->fd;
    }

    {
        char *data_tmp=buffer;
        int to_write=size;

        while(to_write) {
            ret = write(write_fd,data_tmp,to_write);
            if(ret <= 0) {
                usleep(10000);
                continue;
            }
            if(ret < to_write) {
                usleep(10000);
            }
            to_write -= ret;
            data_tmp += ret;
        }
    }
    free(buffer);

    if(default_fd>= 0) {
        close(default_fd);
        default_fd=-1;
    }

    if(param->fd>= 0) {
        close(param->fd);
        param->fd=-1;
    }

    return ret;
}

static int stop_endpoint_test(struct endpoint_test_control *ctl){
    struct audio_test_point *source=ctl->source;
    struct audio_test_point *sink=ctl->sink;
    void *res;

    if((source==NULL)||(sink==NULL)){
        LOG_E("%s line:%d source:%p sink:%p",__func__,__LINE__,source,sink);
        return -1;
    }

    /* wait source thread exit */
    pthread_mutex_lock(&source->lock);
    if(source->is_exit==true){
        LOG_E("%s line:%d source not running",__func__,__LINE__);
        pthread_mutex_unlock(&source->lock);
    }else{
        source->is_exit=true;
        pthread_mutex_unlock(&source->lock);
        pthread_join(source->thread,  &res);
        LOG_I("endpoint_test_stop %d",__LINE__);
    }

    /* wait sink thread exit */
    pthread_mutex_lock(&sink->lock);
    if(sink->is_exit==true){
        LOG_E("%s line:%d sink not running",__func__,__LINE__);
        pthread_mutex_unlock(&sink->lock);
    }else{
        sink->is_exit=true;
        pthread_mutex_unlock(&sink->lock);
        pthread_join(sink->thread,  &res);
        LOG_I("endpoint_test_stop %d",__LINE__);
    }

    /* free test buffer */
    pthread_mutex_lock(&sink->lock);
    if(sink->ring_buf!=NULL){
        ring_buffer_free(sink->ring_buf);
        sink->ring_buf=NULL;
    }
    pthread_mutex_unlock(&sink->lock);

    pthread_mutex_destroy(&(source->lock));
    pthread_mutex_destroy(&(sink->lock));

    free(ctl->sink);
    free(ctl->source);
    ctl->sink=NULL;
    ctl->source=NULL;

#ifdef SPRD_AUDIO_HIDL_CLIENT
    ctl->hidl_inputstream=NULL;
    ctl->hild_outputstream=NULL;
#endif

    LOG_I("endpoint_test_stop exit");
    return 0;
}

static int start_endpoint_test(void * dev,struct endpoint_test_control *ctl,
    void *stream,struct test_endpoint_param *param){
    struct tiny_audio_device * adev=(struct tiny_audio_device *)dev;

    if((ctl->sink!=NULL)||(ctl->source!=NULL)){
        LOG_E("%s line:%d source:%p sink:%p",__func__,__LINE__,ctl->source,ctl->sink);
        return -1;
    }


#ifdef SPRD_AUDIO_HIDL_CLIENT
    ctl->written=0;

     if(ctl->hild_outputstream!=NULL){
         struct tiny_stream_out * stream_out=(struct tiny_stream_out *)ctl->hild_outputstream;
         if(stream_out->devices){
            adev->out_devices=stream_out->devices;
            LOG_I("start_endpoint_test line:%d device:0x%x",__LINE__,adev->out_devices);
        }
     }
#endif

    ctl->sink=create_sink(dev,stream,param);
    if(ctl->sink==NULL){
        goto error;
    }

#ifdef SPRD_AUDIO_HIDL_CLIENT
     if(ctl->hidl_inputstream!=NULL){
         struct tiny_stream_in * stream_in=(struct tiny_stream_in *)ctl->hidl_inputstream;
         LOG_I("start_endpoint_test line:%d device:0x%x",__LINE__,adev->in_devices);
            if(stream_in->devices){
            LOG_I("start_endpoint_test line:%d device:0x%x",__LINE__,adev->in_devices);
            adev->in_devices=stream_in->devices;
            LOG_I("start_endpoint_test line:%d device:0x%x",__LINE__,adev->in_devices);
        }
     }
#endif

    ctl->source=create_source(dev,param);
    if(ctl->source==NULL){
        goto error;
    }

    ctl->source->testctl=ctl;
    ctl->sink->testctl=ctl;
    ctl->durationMs=-1;
    ctl->loopcunt=-1;

    if(param->loopcunt>0){
        ctl->loopcunt=param->loopcunt;
    }else if(param->durationMs>0){
        ctl->durationMs=param->durationMs;
    }

    source_to_sink(ctl->source,ctl->sink,param);
    return 0;

error:

    if(ctl->source!=NULL){
        free_endpoint(ctl->source);
    }

    if(ctl->sink!=NULL){
        free_endpoint(ctl->sink);
    }
    return -1;
}

int endpoint_test_process(void * dev,struct endpoint_control *ctl,
    struct test_endpoint_param *param){

    switch(param->type){
        case OUTPUT_ENDPOINT_TEST:
            if(param->test_step==0){
                LOG_I("%s line:%d",__func__,__LINE__);
                stop_endpoint_test(&ctl->ouput_test);
                set_out_testmode(dev,AUDIO_HW_APP_PRIMARY,false);
              }else{
                LOG_I("%s line:%d",__func__,__LINE__);
                set_out_testmode(dev,AUDIO_HW_APP_PRIMARY,true);
                start_endpoint_test(dev,&ctl->ouput_test,ctl->primary_ouputstream,param);
            }
            break;

        case INPUT_ENDPOINT_TEST:
            if(param->test_step==0){
                LOG_I("%s line:%d",__func__,__LINE__);
                stop_endpoint_test(&ctl->input_test);
            }else if(param->test_step==1){
                start_endpoint_test(dev,&ctl->input_test,NULL,param);
                LOG_I("%s line:%d sink:%p",__func__,__LINE__,ctl->input_test.sink);
            }else{
                LOG_I("%s line:%d sink:%p",__func__,__LINE__,ctl->input_test.sink);
                read_record_data(ctl->input_test.sink , param->data_size);
            }
            break;

        case INPUT_OUTPUT_ENDPOINT_LOOP_TEST:
            if(param->test_step==0){
                LOG_I("%s line:%d",__func__,__LINE__);
                stop_endpoint_test(&ctl->loop_test);
                set_out_testmode(dev,AUDIO_HW_APP_PRIMARY,false);
              }else{
                LOG_I("%s line:%d",__func__,__LINE__);
                set_out_testmode(dev,AUDIO_HW_APP_PRIMARY,true);
                start_endpoint_test(dev,&ctl->loop_test,ctl->primary_ouputstream,param);
            }
            break;

        default:
            LOG_I("%s line:%d",__func__,__LINE__);
            break;
    }
    return 0;
}

#ifdef SPRD_AUDIO_HIDL_CLIENT
int malloc_endpoint_outputtest_buffer(struct endpoint_control *ctl,int size){

        if(NULL!=ctl->ouput_test.hidl_buffer){
            LOG_E("%s line:%d",__func__,__LINE__);
            return -1;
        }

        ctl->ouput_test.hidl_buffer=malloc(size);
        if(NULL==ctl->ouput_test.hidl_buffer){
            LOG_E("%s line:%d",__func__,__LINE__);
            return -2;
        }
        ctl->ouput_test.hidl_buffer_size=size;
        return ctl->ouput_test.hidl_buffer_size;
}

int write_endpoint_outputtest_buffer(struct endpoint_control *ctl, const void* buffer,
    size_t bytes){
    size_t copybytes=0;
    int sprdhidl_writesize=ctl->ouput_test.hidl_buffer_size;

        if(NULL==ctl->ouput_test.hidl_buffer){
            LOG_E("%s line:%d",__func__,__LINE__);
            return -1;
        }

        if(bytes>=(size_t)(ctl->ouput_test.hidl_buffer_size-ctl->ouput_test.written)){
            copybytes=sprdhidl_writesize-ctl->ouput_test.written;
        }else{
            copybytes=bytes;
        }
        memcpy(ctl->ouput_test.hidl_buffer+ctl->ouput_test.written,buffer,copybytes);
        ctl->ouput_test.written+=copybytes;

        return copybytes;
}
#endif

int audio_endpoint_test(void * dev,struct str_parms *parms,UNUSED_ATTR int opt,UNUSED_ATTR char * val){
    char value[AUDIO_EXT_CONTROL_PIPE_MAX_BUFFER_SIZE];
    int ret = -1;

    struct tiny_audio_device * adev=(struct tiny_audio_device *)dev;
    struct test_endpoint_param *test_param=malloc(sizeof(struct test_endpoint_param));
    if(NULL==test_param){
        LOG_E("%s line:%d",__func__,__LINE__);
        return -1;
    }
    memset(test_param,0,sizeof(struct test_endpoint_param));
    test_param->type=-1;
    test_param->data=NULL;
    test_param->data_size=0;
    test_param->fd=-1;

    ret = str_parms_get_str(parms,"endpoint_test", value, sizeof(value));
    if(ret >= 0){
        test_param->type= strtoul(value,NULL,0);
    }

    ret = str_parms_get_str(parms,"endpoint_teststep", value, sizeof(value));
    if(ret >= 0){
        test_param->test_step= strtoul(value,NULL,0);
        if(0==test_param->test_step){
            goto out;
        }
    }

    ret = str_parms_get_str(parms,"endpoint_delay", value, sizeof(value));
    if(ret >= 0){
        test_param->delay= strtoul(value,NULL,0);
    }

    ret = str_parms_get_str(parms,"durationMs", value, sizeof(value));
    if(ret >= 0){
        test_param->durationMs= strtoul(value,NULL,0);
    }

    ret = str_parms_get_str(parms,"loopcunt", value, sizeof(value));
    if(ret >= 0){
        test_param->loopcunt= strtoul(value,NULL,0);
    }

    ret = str_parms_get_str(parms,"pcm_channels", value, sizeof(value));
    if(ret >= 0){
        test_param->pcm_channels= strtoul(value,NULL,0);
        LOG_D("%s %d pcm_channels:%d",__func__,__LINE__,test_param->pcm_channels);
    }

    ret = str_parms_get_str(parms,"pcm_rate", value, sizeof(value));
    if(ret >= 0){
        test_param->pcm_rate = strtoul(value,NULL,0);
        LOG_D("%s %d pcm_rate:%d",__func__,__LINE__,test_param->pcm_rate);
    }

    ret = str_parms_get_str(parms,"pcm_datasize", value, sizeof(value));
    if(ret >= 0){
        test_param->data_size= strtoul(value,NULL,0);

        if(test_param->data_size>0){
            test_param->data=(char *)malloc(test_param->data_size);
            if(NULL == test_param->data){
                LOG_E("audio_endpoint_test malloc %d bytes failed",test_param->data_size);
                test_param->data_size=0;
            }
        }
    }

    ret = str_parms_get_str(parms,"pcm_data", value, sizeof(value));
    if(ret >= 0){
        if(NULL==test_param->data){
            LOG_E("pcm_data ERR");
        }else{
            unsigned int size =(unsigned int)string_to_hex(test_param->data,value,test_param->data_size);
            if(test_param->data_size!=size){
                LOG_E("pcm_data data_size ERR:%d %d",size,test_param->data_size);
            }
        }
    }else{
        ret = str_parms_get_str(parms,"pcm_file", value, sizeof(value));
        if(ret >= 0){
            if(test_param->type==OUTPUT_ENDPOINT_TEST){
                test_param->fd=open(value, O_RDONLY);
                LOG_I("%s line:%d fd:%d pcm file:%s",
                    __func__,__LINE__,test_param->fd,value);
                if(test_param->fd<0){
                    LOG_W("audio_endpoint_test open pcm file %s failed errno=%d errr msg:%s",
                        value,errno,strerror(errno));
                }
            }else if(test_param->type==INPUT_ENDPOINT_TEST){
                test_param->fd=open(value, O_WRONLY | O_CREAT |O_TRUNC ,
                S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
                LOG_I("%s line:%d fd:%d",__func__,__LINE__,test_param->fd);
                if(test_param->fd<0){
                    LOG_W("audio_endpoint_test create pcm file %s failed errno=%d errr msg:%s",
                        value,errno,strerror(errno));
                }
            }
        }
    }

    if(test_param->type==OUTPUT_ENDPOINT_TEST){
#ifdef SPRD_AUDIO_HIDL_CLIENT
        if(adev->test_ctl.ouput_test.hild_outputstream!=NULL){
            struct tiny_stream_out * hidlstream_out=NULL;
            hidlstream_out=(struct tiny_stream_out *)(adev->test_ctl.ouput_test.hild_outputstream);
            test_param->pcm_channels=hidlstream_out->config->channels;
            test_param->pcm_rate=hidlstream_out->config->rate;
            test_param->data_size= adev->test_ctl.ouput_test.hidl_buffer_size;

            if((test_param->data_size>0)&&(adev->test_ctl.ouput_test.hidl_buffer!=NULL)){
                test_param->data=(char *)malloc(test_param->data_size);
                if(NULL == test_param->data){
                    LOG_E("audio_endpoint_test malloc %d bytes failed",test_param->data_size);
                    test_param->data_size=0;
                }else{
                    memcpy(test_param->data,adev->test_ctl.ouput_test.hidl_buffer,test_param->data_size);
                }
                free(adev->test_ctl.ouput_test.hidl_buffer);
                adev->test_ctl.ouput_test.hidl_buffer=NULL;
                adev->test_ctl.ouput_test.hidl_buffer_size=0;
            }
        }
#endif
        if(test_param->pcm_rate==0){
            test_param->pcm_rate=TEST_DEFAULT_SAMPLE_RATE;
        }

        if(test_param->pcm_channels==0){
            test_param->pcm_channels=2;
        }

        if(((test_param->data==NULL)||(test_param->data_size==0))
            &&( test_param->fd<0)){
                if(test_param->data!=NULL){
                    free(test_param->data);
                }
                test_param->data_size=sizeof(s_48000_pcmdata_mono);
                test_param->data=(char *)malloc(test_param->data_size);
                if(NULL == test_param->data){
                    LOG_E("audio_endpoint_test malloc failed");
                    test_param->data_size=0;
                }

                LOG_I("use default pcm data for playback:%p size:%d",test_param->data,test_param->data_size);
                memcpy(test_param->data,&s_48000_pcmdata_mono,sizeof(s_48000_pcmdata_mono));
                test_param->pcm_channels=1;
                test_param->pcm_rate=48000;
        }
    }else if(test_param->type==INPUT_ENDPOINT_TEST){

#ifdef SPRD_AUDIO_HIDL_CLIENT
        if(adev->test_ctl.input_test.hidl_inputstream!=NULL){
            struct tiny_stream_in * hidlstream_in=NULL;
            hidlstream_in=(struct tiny_stream_in *)((struct tiny_stream_out *)(adev->test_ctl.input_test.hidl_inputstream));
            test_param->pcm_channels=hidlstream_in->requested_channels;
            test_param->pcm_rate=hidlstream_in->requested_rate;
        }
#endif

        if(test_param->pcm_rate==0){
            test_param->pcm_rate=16000;
        }

        if(test_param->pcm_channels==0){
            test_param->pcm_channels=1;
        }
    }

out:
    LOG_I("test_param file:%d pcm_rate:%d pcm_channels:%d data_size:%d",
        test_param->fd,test_param->pcm_rate,test_param->pcm_channels,test_param->data_size);
    ret= endpoint_test_process(dev,&adev->test_ctl,test_param);

    if(test_param->data!=NULL){
        free(test_param->data);
        test_param->data=NULL;
    }

    free(test_param);
    return ret;
}

UNUSED_ATTR static int audio_endpoint_play_file(void *dev,const char *pcm_file){
    struct tiny_audio_device * adev=(struct tiny_audio_device *)dev;
    struct test_endpoint_param *test_param=malloc(sizeof(struct test_endpoint_param));
    int ret=0;
    if(NULL==test_param){
        LOG_E("%s line:%d",__func__,__LINE__);
        return -1;
    }

    memset(test_param,0,sizeof(struct test_endpoint_param));
    test_param->type=OUTPUT_ENDPOINT_TEST;
    test_param->test_step=1;

    test_param->data=NULL;
    test_param->data_size=0;
    test_param->pcm_channels=2;
    test_param->pcm_rate=TEST_DEFAULT_SAMPLE_RATE;

    test_param->fd=open(pcm_file, O_RDONLY);
    if(test_param->fd<0){
        LOG_W("audio_endpoint_play_file open pcm file %s failed errno=%d errr msg:%s",
            pcm_file,errno,strerror(errno));
        free(test_param);
        return -2;
    }

    ret= endpoint_test_process(dev,&adev->test_ctl,test_param);
    free(test_param);
    return ret;
}

static int audio_endpoint_play_mute(void *dev){
    struct tiny_audio_device * adev=(struct tiny_audio_device *)dev;
    struct test_endpoint_param *test_param=malloc(sizeof(struct test_endpoint_param));
    int ret=0;
    if(NULL==test_param){
        LOG_E("%s line:%d",__func__,__LINE__);
        return -1;
    }

    memset(test_param,0,sizeof(struct test_endpoint_param));
    test_param->type=OUTPUT_ENDPOINT_TEST;
    test_param->test_step=1;

    test_param->data=malloc(320);
    if(test_param->data==NULL){
        LOG_E("%s line:%d Failed",__func__,__LINE__);
        free(test_param);
        return -2;
    }
    memset(test_param->data,0,320);
    test_param->data_size=320;
    test_param->pcm_channels=2;
    test_param->pcm_rate=TEST_DEFAULT_SAMPLE_RATE;
    test_param->fd=-1;

    ret=endpoint_test_process(dev,&adev->test_ctl,test_param);

    if(test_param->data!=NULL){
        LOG_I("audio_endpoint_play_mute Free param data");
        free(test_param->data);
        test_param->data=NULL;
    }
    free(test_param);
    return ret;
}

static int audio_endpoint_stop_playback(void *dev){
    int ret=0;
    struct tiny_audio_device * adev=(struct tiny_audio_device *)dev;
    struct test_endpoint_param *test_param=malloc(sizeof(struct test_endpoint_param));
    if(NULL==test_param){
        LOG_E("%s line:%d",__func__,__LINE__);
        return -1;
    }

    memset(test_param,0,sizeof(struct test_endpoint_param));
    test_param->type=OUTPUT_ENDPOINT_TEST;
    test_param->test_step=0;

    test_param->data=NULL;
    test_param->data_size=0;
    test_param->pcm_channels=2;
    test_param->pcm_rate=TEST_DEFAULT_SAMPLE_RATE;
    ret= endpoint_test_process(dev,&adev->test_ctl,test_param);
    free(test_param);
    return ret;
}

static int apply_smartamp_cali_param(void *dev,struct smart_amp_cali_param *cali_out){
    struct vbc_fw_header *header=NULL;
    int fd=-1;
    int ret=0;
    struct tiny_audio_device * adev=(struct tiny_audio_device *)dev;
    void *smart_param=NULL;
    int smart_param_size=0;

    smart_param_size=read_smart_firmware(&smart_param);
    if((smart_param_size<=0)||(NULL==smart_param)){
        LOG_W("audio_endpoint_smartamp_cali Failed with invalid SmartAmp param");
        return -1;
    }

    header=(struct vbc_fw_header *)smart_param;

    {
        int i=0;
        char *data_point=NULL;

        for(i=0;i<header->num_mode;i++){
            data_point=(char *)smart_param+sizeof(struct vbc_fw_header)+i*header->len;
            *(int16 *)(data_point+RE_T_CALI)=cali_out->Re_T_cali_out;
            *(int16 *)(data_point+POSSTFILTER_FLAG)=cali_out->postfilter_flag;
        }
    }

    fd=open(get_audioparam_filename(&adev->audio_param,SND_AUDIO_PARAM_SMARTAMP_PROFILE,AUDIO_PARAM_DATA_BIN), O_RDWR | O_CREAT,
                          S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
    if(fd<0){
    LOG_E("open:%s failed Error:%s\n"
        ,get_audioparam_filename(&adev->audio_param,SND_AUDIO_PARAM_SMARTAMP_PROFILE,AUDIO_PARAM_DATA_BIN),
        strerror(errno));
    }else{
         ret=write(fd, smart_param, smart_param_size);
        if(ret!=smart_param_size){
            LOG_E("%s write failed ret:%d smart_param_size:%d",__func__,ret,smart_param_size);
        }
        close(fd);
        fd=-1;
    }

    free_smart_firmware_buffer(smart_param);
    upload_audio_profile_param_firmware(&adev->audio_param,SND_AUDIO_PARAM_SMARTAMP_PROFILE);

    return ret;
}


int audio_endpoint_smartamp_cali(void *dev){
    int ret = -1;
    int read_count=100;
    struct tiny_audio_device * adev=(struct tiny_audio_device *)dev;
    struct smart_amp_cali_param cali_out;
    int cali_time=5000-500;
    int timeout_ms=0;
    unsigned int t1=getCurrentTimeMs();
    int time_diff=0;
    adev->dev_ctl->debug_out_devices=AUDIO_DEVICE_OUT_SPEAKER;

    LOG_I("%s set start playback",__func__);
    set_smartamp_calibrating(&adev->dev_ctl->smartamp_ctl,true);
    ret=audio_endpoint_play_mute(dev);

    cali_out.Re_T_cali_out=0;
    cali_out.postfilter_flag=0;

    usleep(1000*1000);
    enable_smartamp_pt(adev->dev_ctl->agdsp_ctl,true);

    usleep(1500*1000);
    time_diff=getCurrentTimeMs()-t1;
    timeout_ms=cali_time-time_diff;
    dump_all_audio_reg(-1,(1<<ADEV_DUMP_CODEC_REG)|(1<<ADEV_DUMP_VBC_REG));

    LOG_I("%s start cali",__func__);
    while((read_count--)&&(timeout_ms>0)){
        LOG_I("%s read iv count:%d timeout_ms:%d",__func__,read_count,timeout_ms);
        if(true==get_smartamp_cali_iv(adev->dev_ctl->agdsp_ctl,&adev->dev_ctl->smartamp_ctl,timeout_ms)){
            if(true==smartamp_cali_process(&adev->dev_ctl->smartamp_ctl,&cali_out)){
                write_smartamp_cali_param(&cali_out);
                apply_smartamp_cali_param(dev,&cali_out);
                break;
            }
        }
        time_diff=getCurrentTimeMs()-t1;
        timeout_ms=cali_time-time_diff;

        if(timeout_ms<=10){
            read_count=0;
            LOG_W("audio_endpoint_smartamp_cali time out");
        }else{
            usleep(40*1000);
        }
    }
    LOG_I("%s stop playback",__func__);
    enable_smartamp_pt(adev->dev_ctl->agdsp_ctl,false);
    audio_endpoint_stop_playback(adev);
    clear_smartamp_param_state(&adev->dev_ctl->param_res);
    set_smartamp_calibrating(&adev->dev_ctl->smartamp_ctl,false);
    return ret;
}

int audio_endpoint_test_init(void * dev,void *out_stream){
    struct tiny_audio_device * adev=(struct tiny_audio_device *)dev;
    struct endpoint_control *test_ctl=(struct endpoint_control *)&adev->test_ctl;

    memset(test_ctl,0,sizeof(struct endpoint_control));

    test_ctl->primary_ouputstream=out_stream;
    test_ctl->ouput_test.source=NULL;
    test_ctl->ouput_test.sink=NULL;
#ifdef SPRD_AUDIO_HIDL_CLIENT
    test_ctl->ouput_test.hidl_inputstream=NULL;
    test_ctl->ouput_test.hild_outputstream=NULL;
    test_ctl->ouput_test.hidl_buffer=NULL;
#endif
    test_ctl->input_test.source=NULL;
    test_ctl->input_test.sink=NULL;
#ifdef SPRD_AUDIO_HIDL_CLIENT
    test_ctl->input_test.hidl_inputstream=NULL;
    test_ctl->input_test.hild_outputstream=NULL;
    test_ctl->input_test.hidl_buffer=NULL;
#endif
    test_ctl->loop_test.source=NULL;
    test_ctl->loop_test.sink=NULL;
#ifdef SPRD_AUDIO_HIDL_CLIENT
    test_ctl->loop_test.hidl_inputstream=NULL;
    test_ctl->loop_test.hild_outputstream=NULL;
    test_ctl->loop_test.hidl_buffer=NULL;
#endif
    LOG_I("%s %d %p",__func__,__LINE__, test_ctl->primary_ouputstream);
    return 0;
}
