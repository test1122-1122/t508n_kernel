/*
 * Copyright (C) 2016 The Android Open Source Project
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

#define LOG_TAG "AudioHalTool"
//#define LOG_NDEBUG 0

#include <hardware/audio.h>
#include <system/audio.h>
#include <log/log.h>

static int load_audio_interface(const char *if_name, audio_hw_device_t **dev)
{
    const hw_module_t *mod;
    int rc;

    rc = hw_get_module_by_class(AUDIO_HARDWARE_MODULE_ID, if_name, &mod);
    if (rc) {
        ALOGE("%s couldn't load audio hw module %s.%s (%s)", __func__,
                AUDIO_HARDWARE_MODULE_ID, if_name, strerror(-rc));
        goto out;
    }
    rc = audio_hw_device_open(mod, dev);
    if (rc) {
        ALOGE("%s couldn't open audio hw device in %s.%s (%s)", __func__,
                AUDIO_HARDWARE_MODULE_ID, if_name, strerror(-rc));
        goto out;
    }
    if ((*dev)->common.version < AUDIO_DEVICE_API_VERSION_MIN) {
        ALOGE("%s wrong audio hw device version %04x", __func__, (*dev)->common.version);
        rc = -1;
        audio_hw_device_close(*dev);
        goto out;
    }
    return 0;

out:
    *dev = NULL;
    return rc;
}

int running = 1;

void sigint_handler(int sig __unused)
{
    running = 0;
}


int main(int argc, char *argv[]){
    audio_hw_device_t *dev=NULL;
    struct audio_stream_out *stream_out=NULL;
    struct audio_stream_in *stream_in=NULL;

    audio_io_handle_t handle=0;
    struct audio_config config;
    int ret=0;
    FILE *file=NULL;
    int num_read=0;
    int size=0;
    char *buffer=NULL;
    int mode=0;
    int bytes_read=0;

    config.sample_rate=44100;
    config.channel_mask=AUDIO_CHANNEL_OUT_STEREO;
    config.format=AUDIO_FORMAT_PCM_16_BIT;

    ret=load_audio_interface(AUDIO_HARDWARE_MODULE_ID_PRIMARY, &dev);
    if(ret!=0){
        printf("load primary failed\n");
        return 1;
    }

    ALOGI("argv[2]:%s",argv[2]);

   if (strcmp(argv[1], "play") == 0){
        mode=1;
        file = fopen(argv[2], "rb");
        if (NULL==file) {
            fprintf(stderr, "Unable to open file '%s'error:%s\n", argv[2],strerror(errno));
            return 1;
        }
   } if(strcmp(argv[1], "usbplay") == 0){
        mode=1;
        file = fopen(argv[2], "rb");
        if (NULL==file) {
            fprintf(stderr, "Unable to open file '%s'error:%s\n", argv[2],strerror(errno));
            return 1;
        }
        dev->set_parameters(dev,"connect=67108864");
        dev->set_parameters(dev,"card=4;connect=67108864;device=0");
   }else if (strcmp(argv[1], "record") == 0){
        mode=2;
        file = fopen(argv[2], "wb");
        if (NULL==file) {
            fprintf(stderr, "Unable to open file '%s error:%s'\n"
                , argv[2],strerror(errno));
            return 1;
        }
   }
    running=1;
    ALOGI("mode:%d argc:%d",mode,argc);
    signal(SIGINT, sigint_handler);
    signal(SIGHUP, sigint_handler);
    signal(SIGTERM, sigint_handler);
    if(mode==1){
        ret= dev->open_output_stream(dev,handle,AUDIO_DEVICE_OUT_SPEAKER,AUDIO_OUTPUT_FLAG_PRIMARY,&config,&stream_out,"");
        if(NULL!=stream_out){
            size=stream_out->common.get_buffer_size(&stream_out->common);
            buffer=(char *)malloc(size);
            if(NULL!=buffer){
                 printf("playback start\n");
                do {
                    num_read = fread(buffer, 1, size, file);
                    if (num_read > 0) {
                        stream_out->write(stream_out, buffer, num_read);
                    }
                } while (running&&num_read > 0);
                printf("playback end\n");
               stream_out->common.standby(&stream_out->common);
               free(buffer);
            }
        }
    }

    if(mode==2){
        config.channel_mask=AUDIO_CHANNEL_IN_STEREO;

        ret= dev->open_input_stream(dev,handle,AUDIO_DEVICE_IN_BUILTIN_MIC,&config,&stream_in,AUDIO_INPUT_FLAG_NONE,
            "",AUDIO_SOURCE_DEFAULT);
        if(NULL!=stream_in){
            size=stream_in->common.get_buffer_size(&stream_in->common);
            buffer=(char *)malloc(size);
            if(NULL!=buffer){
                 printf("record start\n");
                 while (running && stream_in->read(stream_in, buffer, size)) {
                     if (fwrite(buffer, 1, size, file) != size) {
                         fprintf(stderr,"Error running sample\n");
                         break;
                     }
                     bytes_read += size;
                 }
                printf("record end\n");
               stream_in->common.standby(&stream_in->common);
               free(buffer);
            }
        }
    }

    if(mode==0){
        printf("running  start\n");
        while(running){
            sleep(10);
        }
        printf("running  end\n");
    }

    if(NULL==dev){
        audio_hw_device_close(dev);
    }    
    return ret;
}
