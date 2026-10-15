#define LOG_TAG "AudioClient"

#include <media/audiohal/DevicesFactoryHalInterface.h>
#include <media/audiohal/DeviceHalInterface.h>
#include <media/audiohal/StreamHalInterface.h>

#include <android/hardware/audio/2.0/IDevicesFactory.h>
#include <android/hardware/audio/4.0/IDevicesFactory.h>
#include <android/hardware/audio/5.0/IDevicesFactory.h>

#include <system/audio.h>

#include <utils/Log.h>
#include <cutils/properties.h>

#define AUDIO_HARDWARE_MODULE_ID_PRIMARY "primary"

    namespace android {

    class DevicesFactoryHalInterface;
    class DeviceHalInterface;
    class StreamOutHalInterface;
    class StreamInHalInterface;

    const char *streamName="SprdAudioNpiStream";
    sp<DevicesFactoryHalInterface> mDevicesFactoryHal;
    sp<DeviceHalInterface> primarydev;
    sp<StreamOutHalInterface> outStream;
    sp<StreamInHalInterface> inStream;
    audio_io_handle_t handleOut;
    audio_io_handle_t handleIn;
    bool audiocliient_init_status=false;
    bool audiocliient_output_init_status=false;
    bool audiocliient_input_init_status=false;

    struct audio_config out_config;
    struct audio_config in_config;

    size_t mOutputBufferSize=0;
    size_t mInputBufferSize=0;

extern "C" {
    void audioclient_init(void){

       int rc=-1;

        if(audiocliient_init_status==true){
            ALOGI("audioclient_init aready init");
            return;
        }
       mDevicesFactoryHal = DevicesFactoryHalInterface::create();
       if(mDevicesFactoryHal){
          rc = mDevicesFactoryHal->openDevice(AUDIO_HARDWARE_MODULE_ID_PRIMARY, &primarydev);
           if (rc) {
               ALOGW("openDevice() error %d loading module %s", rc, AUDIO_HARDWARE_MODULE_ID_PRIMARY);
               audiocliient_init_status=false;
           }else{
               ALOGI("audioclient_init success");
               audiocliient_init_status=true;
               primarydev->setParameters(String8("AudioNpiInit=1"));
           }
       }else{
            ALOGI("audioclient_init failed DevicesFactoryHalInterface");
            audiocliient_init_status=false;
       }
    }

    void audioclient_deinit(void){
        ALOGE("audioclient_deinit");
        audiocliient_init_status=false;
        audiocliient_output_init_status=false;
        audiocliient_input_init_status=false;
        outStream=NULL;
        inStream=NULL;
        primarydev=NULL;
        mDevicesFactoryHal=NULL;
    }

    bool audioclient_check(void){
        return true;
    }

    int audioclient_setParameters(const char *str){
        
        String8  kvPairs=String8(str);

        if(false==audiocliient_init_status){
            audioclient_init();
            if(false==audiocliient_init_status){
                ALOGW("audioclient_setParameters Failed, audiocliient_init_status error");
                return -1;
            }
        }

        if(audioclient_check()==false){
            return -1;
        }
        return primarydev->setParameters(kvPairs);
    }

    int audioclient_getParameters(const char *str,char *rsp){
        String8 kvPairs=String8(str);
        String8 values;
        if(false==audiocliient_init_status){
            ALOGW("audioclient_getParameters Failed, audiocliient_init_status error");
            return -1;
        }

        if(audioclient_check()==false){
            return -1;
        }

        primarydev->getParameters(kvPairs,&values);
        memcpy(rsp,values.c_str(),strlen(values.c_str()));
        return 0;
    }

    int StreamOutClose(void)
    {
        if(false==audiocliient_init_status){
            ALOGW("StreamOutClose Failed, audiocliient_init_status error");
        }

        if(false==audiocliient_output_init_status){
            ALOGW("StreamOutClose Failed, audiocliient_output_init_status error");
        }

        audiocliient_output_init_status=false;
        outStream=NULL;
        return 0;
    }

    int StreamOutstandby(void)
    {
        if(false==audiocliient_init_status){
            ALOGW("StreamOutstandby Failed, audiocliient_init_status error");
            return -1;
        }

        if(false==audiocliient_output_init_status){
            ALOGW("StreamOutstandby Failed, audiocliient_output_init_status error");
            return -2;
        }

        if(audioclient_check()==false){
            return -1;
        }

        return outStream->standby();
    }

    int StreamOutsetParameters(const char *str){
        String8  kvPairs=String8(str);
        if(false==audiocliient_init_status){
            ALOGW("StreamOutsetParameters Failed, audiocliient_init_status error");
            return -1;
        }

        if(false==audiocliient_output_init_status){
            ALOGW("StreamOutsetParameters Failed, audiocliient_output_init_status error");
            return -2;
        }

        if(audioclient_check()==false){
            return -1;
        }
        ALOGI("StreamOutsetParameters[%s] [%s]",str,kvPairs.c_str());
        return outStream->setParameters(kvPairs);
    }

    ssize_t StreamOutwrite(const void *buffer, size_t numBytes)
    {
        size_t bytesWritten;

        if(false==audiocliient_init_status){
            ALOGW("StreamOutwrite Failed, audiocliient_init_status error");
            return -1;
        }

        if(false==audiocliient_output_init_status){
            ALOGW("StreamOutwrite Failed, audiocliient_output_init_status error");
            return -2;
        }

        if(audioclient_check()==false){
            return -1;
        }

        status_t result = outStream->write(buffer, numBytes, &bytesWritten);
        return result == OK ? bytesWritten : result;
    }

    int StreamInstandby(void)
    {
        if(false==audiocliient_init_status){
            ALOGW("StreamInstandby Failed, audiocliient_init_status error");
            return -1;
        }

        if(false==audiocliient_input_init_status){
            ALOGW("StreamInstandby Failed, audiocliient_input_init_status error");
            return -2;
        }

        if(audioclient_check()==false){
            return -1;
        }

        return inStream->standby();
    }

    int StreamInClose(void)
      {
          if(false==audiocliient_init_status){
              ALOGW("StreamInClose Failed, audiocliient_init_status error");
          }

          if(false==audiocliient_input_init_status){
              ALOGW("StreamInClose Failed, audiocliient_input_init_status error");
          }
          audiocliient_input_init_status=false;
          inStream=NULL;
          return 0;
      }

      int openStreamOut(
          int devices,
          int sample_rate,
          int channels)
      {
          if(false==audiocliient_init_status){
              audioclient_init();
              if(false==audiocliient_init_status){
                  ALOGW("openStreamOut Failed, audiocliient_init_status error");
                  return -1;
              }
          }

          if(true==audiocliient_output_init_status){
              ALOGI("openStreamOut aready opened");
              return 0;
          }

          out_config.sample_rate=44100;
          if(channels==2){
              out_config.channel_mask=AUDIO_CHANNEL_OUT_STEREO;
          }else{
              out_config.channel_mask=AUDIO_CHANNEL_OUT_MONO;
          }

          if(audioclient_check()==false){
              return -1;
          }

          out_config.format=AUDIO_FORMAT_PCM_16_BIT;
          out_config.sample_rate=sample_rate;
          int status = primarydev->openOutputStream(
                  handleOut,
                  (audio_devices_t)devices,
                  AUDIO_OUTPUT_FLAG_DIRECT_PCM,
                  &out_config,
                  streamName,
                  &outStream);
          ALOGI("AudioStreamOut::open(), HAL returned "
                  " stream %p, sampleRate %d, Format %#x, "
                  "channelMask %#x, status %d",
                  outStream.get(),
                  in_config.sample_rate,
                  in_config.format,
                  in_config.channel_mask,
                  status);
          if (status == NO_ERROR) {
              audiocliient_output_init_status=true;
              StreamOutsetParameters("hidlstream=1");
              status = outStream->getBufferSize(&mOutputBufferSize);
              ALOGI("openStreamOut success");
          }
          return status;
      }

      int StreamInsetParameters(const char *str){
          String8  kvPairs=String8(str);
          if(false==audiocliient_init_status){
              ALOGW("StreamInsetParameters Failed, audiocliient_init_status error");
              return -1;
          }

          if(false==audiocliient_input_init_status){
              ALOGW("StreamInsetParameters Failed, audiocliient_input_init_status error");
              return -2;
          }

          if(audioclient_check()==false){
              return -1;
          }
          ALOGI("StreamInsetParameters[%s] [%s]",str,kvPairs.c_str());
          return inStream->setParameters(kvPairs);
      }

      int StreamIngetParameters(const char *str,char *rsp){
          String8 kvPairs=String8(str);
          String8 values;
          if(false==audiocliient_init_status){
              ALOGW("StreamIngetParameters Failed, audiocliient_init_status error");
              return -1;
          }

          if(false==audiocliient_input_init_status){
              ALOGW("StreamIngetParameters Failed, audiocliient_input_init_status error");
              return -2;
          }

          if(audioclient_check()==false){
              return -1;
          }

          inStream->getParameters(kvPairs,&values);
          memcpy(rsp,values.c_str(),strlen(values.c_str()));
          return 0;
      }

      ssize_t StreamInread( void *buffer, size_t numBytes)
      {
          size_t read;

          ALOGI("StreamInread  enter:%zd buffer:%p",numBytes,buffer);
          if(false==audiocliient_init_status){
              ALOGW("StreamInread Failed, audiocliient_input_init_status error");
              return -1;
          }

          if(false==audiocliient_input_init_status){
              ALOGW("StreamInread Failed, audiocliient_input_init_status error");
              return -2;
          }

          if(audioclient_check()==false){
              return -1;
          }

          status_t result = inStream->read(buffer, numBytes, &read);
          ALOGI("StreamInread  ret:%d read:%zd",result,read);
          return result == OK ? read : result;
      }

    int openStreamIn(
            int devices,
            int sample_rate,
            int channels)
    {
        status_t status;

        if(false==audiocliient_init_status){
            audioclient_init();
            if(false==audiocliient_init_status){
                ALOGW("openStreamOut Failed, audiocliient_init_status error");
                return -1;
            }
        }

        if(true==audiocliient_input_init_status){
            ALOGI("openStreamIn aready opened");
            return -2;
        }

        in_config.sample_rate=sample_rate;
        if(channels==2){
            in_config.channel_mask=AUDIO_CHANNEL_IN_STEREO;
        }else{
            in_config.channel_mask=AUDIO_CHANNEL_IN_MONO;
        }

        if(audioclient_check()==false){
            return -1;
        }

        in_config.format=AUDIO_FORMAT_PCM_16_BIT;
        status = primarydev->openInputStream(
                handleIn, (audio_devices_t)devices, &in_config, AUDIO_INPUT_FLAG_NONE, streamName, AUDIO_SOURCE_DEFAULT, 
                AUDIO_DEVICE_NONE,NULL,&inStream);
        ALOGI("openInput_l() openInputStream returned input %p, devices %#x, SamplingRate %d"
               ", Format %#x, Channels %#x, flags %#x, status %d addr %s",
                inStream.get(),
                devices,
                in_config.sample_rate,
                in_config.format,
                in_config.channel_mask,
                AUDIO_INPUT_FLAG_NONE,
                status, streamName);
        if (status == NO_ERROR) {
            audiocliient_input_init_status=true;
            StreamInsetParameters("hidlstream=1");
            status=inStream->getBufferSize(&mInputBufferSize);
            ALOGI("openStreamIn success");
        }
        return status;
    }

}
}
