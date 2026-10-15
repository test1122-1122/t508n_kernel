#define LOG_TAG "audio_hw_platform"

#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <log/log.h>
#include <expat.h>
#include "audio_xml_utils.h"

#include "tinyxml.h"

#include "audio_platform.h"

 void parse_microphone_characteristic(param_group_t group,void *platform) {
    struct audio_microphone_characteristic_t microphone;
    TiXmlElement * ele_tmp;
    const char *attr_str;
    ALOGI("parse_microphone_characteristic");

    ele_tmp=(TiXmlElement *)XML_get_first_sub_group(group);
    while(ele_tmp!=NULL){
        attr_str = ele_tmp->Attribute("device_id");
        if (NULL!=attr_str) {
            if (strlen(attr_str) > AUDIO_MICROPHONE_ID_MAX_LEN) {
                ALOGE("%s: device_id %s is too long", __func__, attr_str);
                goto done;
            }
            strcpy(microphone.device_id, attr_str);
            microphone.device = strtoul(attr_str,NULL,0);
        }else{
            ALOGE("%s: device_id not found", __func__);
        }

        attr_str = ele_tmp->Attribute("address");
        if (NULL!=attr_str) {
            if (strlen(attr_str) > AUDIO_DEVICE_MAX_ADDRESS_LEN) {
                ALOGE("%s, address %s is too long", __func__, attr_str);
                goto done;
            }
            strcpy(microphone.address, attr_str);
            if (strlen(microphone.address) == 0) {
                // If the address is empty, populate the address according to device type.
                if (microphone.device == AUDIO_DEVICE_IN_BUILTIN_MIC) {
                    strcpy(microphone.address, AUDIO_BOTTOM_MICROPHONE_ADDRESS);
                } else if (microphone.device == AUDIO_DEVICE_IN_BACK_MIC) {
                    strcpy(microphone.address, AUDIO_BACK_MICROPHONE_ADDRESS);
                }
            }
        }else{
            ALOGE("%s: address not found", __func__);
        }

        attr_str = ele_tmp->Attribute("location");
        if (NULL!=attr_str) {
            microphone.location = (audio_microphone_location_t)atoi(attr_str);
        }else{
            ALOGE("%s: location not found", __func__);
        }

        attr_str = ele_tmp->Attribute("group");
        if (NULL!=attr_str) {
            microphone.group = atoi(attr_str);
        }else{
            ALOGE("%s: group not found", __func__);
        }

        attr_str = ele_tmp->Attribute("index_in_the_group");
        if (NULL!=attr_str) {
            microphone.index_in_the_group = atoi(attr_str);
        }else{
            ALOGE("%s: index_in_the_group not found", __func__);
        }

        attr_str = ele_tmp->Attribute("directionality");
        if (NULL!=attr_str) {
            microphone.directionality = (audio_microphone_directionality_t)atoi(attr_str);
        }else{
            ALOGE("%s: directionality not found", __func__);
        }

        attr_str = ele_tmp->Attribute("num_frequency_responses");
        if (NULL!=attr_str) {
            microphone.num_frequency_responses = atoi(attr_str);

            if (microphone.num_frequency_responses > AUDIO_MICROPHONE_MAX_FREQUENCY_RESPONSES) {
                ALOGE("%s: num_frequency_responses is too large", __func__);
                goto done;
            }
        }else{
            ALOGE("%s: num_frequency_responses not found", __func__);
        }

        if (microphone.num_frequency_responses > 0) {
            attr_str = ele_tmp->Attribute("frequencies");
            if (NULL==attr_str) {
                ALOGE("%s: frequencies not found", __func__);
                goto done;
            }
            char *token = strtok((char *)attr_str, ",");
            uint32_t num_frequencies = 0;
            while (token) {
                microphone.frequency_responses[0][num_frequencies++] = atof(token);
                if (num_frequencies > AUDIO_MICROPHONE_MAX_FREQUENCY_RESPONSES) {
                    ALOGE("%s: num %u of frequency is too large", __func__, num_frequencies);
                    goto done;
                }
                token = strtok(NULL, ",");
            }

            attr_str = ele_tmp->Attribute("responses");
            if (NULL==attr_str) {
                ALOGE("%s: responses not found", __func__);
                goto done;
            }
            token = strtok((char *)attr_str, ",");
            uint32_t num_responses = 0;
            while (token) {
                microphone.frequency_responses[1][num_responses++] = atof(token);
                if (num_responses > AUDIO_MICROPHONE_MAX_FREQUENCY_RESPONSES) {
                    ALOGE("%s: num %u of response is too large", __func__, num_responses);
                    goto done;
                }
                token = strtok(NULL, ",");
            }

            if (num_frequencies != num_responses
                    || num_frequencies != microphone.num_frequency_responses) {
                ALOGE("%s: num of frequency and response not match: %u, %u, %u",
                      __func__, num_frequencies, num_responses, microphone.num_frequency_responses);
                goto done;
            }
        }

        microphone.sensitivity = AUDIO_MICROPHONE_SENSITIVITY_UNKNOWN;

        microphone.max_spl = AUDIO_MICROPHONE_SPL_UNKNOWN;
        microphone.min_spl = AUDIO_MICROPHONE_SPL_UNKNOWN;

        microphone.orientation.x = 0.0f;
        microphone.orientation.y = 0.0f;
        microphone.orientation.z = 0.0f;

        microphone.geometric_location.x = AUDIO_MICROPHONE_COORDINATE_UNKNOWN;
        microphone.geometric_location.y = AUDIO_MICROPHONE_COORDINATE_UNKNOWN;
        microphone.geometric_location.z = AUDIO_MICROPHONE_COORDINATE_UNKNOWN;


        platform_set_microphone_characteristic(platform, microphone);

        ele_tmp = (TiXmlElement *)XML_get_next_sibling_group(ele_tmp);

    }
done:
    return;
}


bool platform_set_microphone_characteristic(void *platform,
                                            struct audio_microphone_characteristic_t mic) {
    struct audio_platform_data *my_data = (struct audio_platform_data *)platform;
    if (my_data->microphones_count >= AUDIO_MICROPHONE_MAX_COUNT) {
        ALOGE("mic number is more than maximum number");
        return false;
    }
    for (size_t ch = 0; ch < AUDIO_CHANNEL_COUNT_MAX; ch++) {
        mic.channel_mapping[ch] = AUDIO_MICROPHONE_CHANNEL_MAPPING_UNUSED;
    }
    my_data->microphones[my_data->microphones_count++] = mic;
    return true;
}

 int platform_get_microphones(void *platform,
                             struct audio_microphone_characteristic_t *mic_array,
                             size_t *mic_count) {
    struct audio_platform_data *my_data = (struct audio_platform_data *)platform;
    if (mic_count == NULL) {
        return -EINVAL;
    }
    if (mic_array == NULL) {
        return -EINVAL;
    }

    if (*mic_count == 0) {
        *mic_count = my_data->microphones_count;
        return 0;
    }

    size_t max_mic_count = *mic_count;
    size_t actual_mic_count = 0;
    for (size_t i = 0; i < max_mic_count && i < my_data->microphones_count; i++) {
        mic_array[i] = my_data->microphones[i];
        actual_mic_count++;
    }
    *mic_count = actual_mic_count;
    return 0;
}

 int platform_get_active_microphones(void *platform, audio_devices_t device, unsigned int channels,
                                    int source __unused,
                                    struct audio_microphone_characteristic_t *mic_array,
                                    size_t *mic_count) {
    struct audio_platform_data *my_data = (struct audio_platform_data *)platform;
    if (mic_count == NULL) {
        return -EINVAL;
    }
    if (mic_array == NULL) {
        return -EINVAL;
    }

    if (*mic_count == 0) {
        *mic_count = my_data->microphones_count;
        return 0;
    }

    size_t max_mic_count = *mic_count;
    size_t actual_mic_count = 0;
    for (size_t i = 0; i < max_mic_count && i < my_data->microphones_count; i++) {
        // TODO: get actual microphone and channel mapping type.
        if ((my_data->microphones[i].device & device) == device) {
            mic_array[actual_mic_count] = my_data->microphones[i];
            for (size_t ch = 0; ch < channels; ch++) {
                mic_array[actual_mic_count].channel_mapping[ch] =
                        AUDIO_MICROPHONE_CHANNEL_MAPPING_DIRECT;
            }
            actual_mic_count++;
        }
    }
    *mic_count = actual_mic_count;
    return 0;
}
