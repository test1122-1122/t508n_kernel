/*
* Copyright (C) 2010 The Android Open Source Project
* Copyright (C) 2012-2015, The Linux Foundation. All rights reserved.
*
* Not a Contribution, Apache license notifications and license are retained
* for attribution purposes only.
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

#ifndef __HW_AAUDIO_H__
#define __HW_AAUDIO_H__
#include <hardware/audio.h>

int out_start(const struct audio_stream_out* stream);
int out_stop(const struct audio_stream_out* stream);
int out_create_mmap_buffer(const struct audio_stream_out *stream,
                                  int32_t min_size_frames,
                                  struct audio_mmap_buffer_info *info);
int out_get_mmap_position(const struct audio_stream_out *stream,
                                  struct audio_mmap_position *position);

int in_start(const struct audio_stream_in* stream);
int in_stop(const struct audio_stream_in* stream);
int in_create_mmap_buffer(const struct audio_stream_in *stream,
                                  int32_t min_size_frames,
                                  struct audio_mmap_buffer_info *info);
int in_get_mmap_position(const struct audio_stream_in *stream,
                                  struct audio_mmap_position *position);

#endif // __HW_AAUDIO_H_

