#ifndef _SPRD_AUDIO_PLATFORM_DATA_H
#define _SPRD_AUDIO_PLATFORM_DATA_H
#include <system/audio.h>
#include <expat.h>
#include "audio_xml_utils.h"
#ifdef __cplusplus
extern "C" {
#endif

struct audio_platform_data {
    void *dev;
    uint32_t microphones_count;
    struct audio_microphone_characteristic_t microphones[AUDIO_MICROPHONE_MAX_COUNT];
};

void parse_microphone_characteristic(param_group_t group,void *platform);
bool platform_set_microphone_characteristic(void *platform,
                                            struct audio_microphone_characteristic_t mic) ;
int platform_get_microphones(void *platform,
                             struct audio_microphone_characteristic_t *mic_array,
                             size_t *mic_count) ;
int platform_get_active_microphones(void *platform, audio_devices_t device, unsigned int channels,
                                    int source __unused,
                                    struct audio_microphone_characteristic_t *mic_array,
                                    size_t *mic_count) ;
#ifdef __cplusplus
}
#endif
#endif
