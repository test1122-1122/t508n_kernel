/*
 * Copyright (C) 2012 The Android Open Source Project
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

#ifndef INCLUDE_SENSORS_SENSORTYPEPRIVATE_H_
#define INCLUDE_SENSORS_SENSORTYPEPRIVATE_H_

/*
 * DONOT Modify this file,
 *  Match with platform/frameworks/base/core/java/android/hardware/Sensor.java
 * */
#define SPRD_SENSOR_TYPE_BASE   0x10000

#define SENSOR_TYPE_SPRD_SHAKE                    (SPRD_SENSOR_TYPE_BASE + 1)
#define SENSOR_TYPE_SPRD_POCKET_MODE              (SPRD_SENSOR_TYPE_BASE + 2)
#define SENSOR_TYPE_SPRD_TAP                      (SPRD_SENSOR_TYPE_BASE + 3)
#define SENSOR_TYPE_SPRD_FACE_UP_DOWN             (SPRD_SENSOR_TYPE_BASE + 4)
#define SENSOR_TYPE_SPRD_FACE_DUMMY               (SPRD_SENSOR_TYPE_BASE + 5)
#define SENSOR_TYPE_SPRD_TILT                     (SPRD_SENSOR_TYPE_BASE + 6)
#define SENSOR_TYPE_SPRD_WAKE_UP                  (SPRD_SENSOR_TYPE_BASE + 7)
#define SENSOR_TYPE_SPRD_GLANCE                   (SPRD_SENSOR_TYPE_BASE + 8)
#define SENSOR_TYPE_SPRD_PICKUP                   (SPRD_SENSOR_TYPE_BASE + 9)
#define SENSOR_TYPE_SPRD_FLIP                     (SPRD_SENSOR_TYPE_BASE + 10)
#define SENSOR_TYPE_SPRD_TWIST                    (SPRD_SENSOR_TYPE_BASE + 11)
#define SENSOR_TYPE_SPRD_HAND_UP                  (SPRD_SENSOR_TYPE_BASE + 12)
#define SENSOR_TYPE_SPRD_HAND_DOWN                (SPRD_SENSOR_TYPE_BASE + 13)
#define SENSOR_TYPE_SPRD_REAR_CAMERA              (SPRD_SENSOR_TYPE_BASE + 14)
#define SENSOR_TYPE_SPRD_FRONT_CAMERA             (SPRD_SENSOR_TYPE_BASE + 15)
#define SENSOR_TYPE_SPRD_SCREEN_ON                (SPRD_SENSOR_TYPE_BASE + 16)
#define SENSOR_TYPE_SPRD_FALL                     (SPRD_SENSOR_TYPE_BASE + 17)
#define SENSOR_TYPE_SPRD_PRIVATE_SENSOR_A         (SPRD_SENSOR_TYPE_BASE + 18)
#define SENSOR_TYPE_SPRD_CONTEXT_AWARENESS        (SPRD_SENSOR_TYPE_BASE + 19)
#define SENSOR_TYPE_SPRD_STATIC_DETECTOR          (SPRD_SENSOR_TYPE_BASE + 20)
#define SENSOR_TYPE_SPRD_AIR_MOUSE                (SPRD_SENSOR_TYPE_BASE + 21)
#define SENSOR_TYPE_SPRD_WATCH_HANDUPDOWN         (SPRD_SENSOR_TYPE_BASE + 22)
#define SENSOR_TYPE_SPRD_AUTO_PICK_UP             (SPRD_SENSOR_TYPE_BASE + 23)
#define SENSOR_TYPE_SPRD_HAND_DETECTOR            (SPRD_SENSOR_TYPE_BASE + 24)
#define SENSOR_TYPE_SPRD_PDR                      (SPRD_SENSOR_TYPE_BASE + 25)
#define SENSOR_TYPE_SPRD_AIR_RECOGNITION          (SPRD_SENSOR_TYPE_BASE + 26)
#define SENSOR_TYPE_SPRD_SLEEPING                 (SPRD_SENSOR_TYPE_BASE + 27)
#define SENSOR_TYPE_SPRD_AUDIO                    (SPRD_SENSOR_TYPE_BASE + 28)
#define SENSOR_TYPE_SPRD_AUDIO_RECOGNITION        (SPRD_SENSOR_TYPE_BASE + 29)
#define SENSOR_TYPE_SPRD_VIRTUAL_GYRO             (SPRD_SENSOR_TYPE_BASE + 30)
#define SENSOR_TYPE_SPRD_LOCALIZATION             (SPRD_SENSOR_TYPE_BASE + 31)
#define SENSOR_TYPE_SPRD_WATCH_TWIST              (SPRD_SENSOR_TYPE_BASE + 32)
#define SENSOR_TYPE_SPRD_WATCH_GESTURE            (SPRD_SENSOR_TYPE_BASE + 33)
#define SENSOR_TYPE_SPRD_CUSTOMALGO1              (SPRD_SENSOR_TYPE_BASE + 34)
#define SENSOR_TYPE_SPRD_CUSTOMALGO2              (SPRD_SENSOR_TYPE_BASE + 35)
#define SENSOR_TYPE_SPRD_STEP_NOTIFIER            (SPRD_SENSOR_TYPE_BASE + 36)
#define SENSOR_TYPE_SPRD_CONTEXT_AWARENESS_CUSTOM (SPRD_SENSOR_TYPE_BASE + 37)
#define SENSOR_TYPE_SPRD_UNWEAR_NOTIFICATION      (SPRD_SENSOR_TYPE_BASE + 38)
#define SENSOR_TYPE_SPRD_PRIVATE_ANSWERING_CALL   (SPRD_SENSOR_TYPE_BASE + 39)
#define SENSOR_TYPE_SPRD_CUSTOMIZED_PEDOMETER     (SPRD_SENSOR_TYPE_BASE + 40)
#define SENSOR_TYPE_SPRD_TOUCH                    (SPRD_SENSOR_TYPE_BASE + 41)
#define SENSOR_TYPE_SPRD_PDR_MOVEMENT             (SPRD_SENSOR_TYPE_BASE + 42)
#define SENSOR_TYPE_SPRD_HALL                     (SPRD_SENSOR_TYPE_BASE + 43)
#define SENSOR_TYPE_SPRD_DIGITALHALL              (SPRD_SENSOR_TYPE_BASE + 44)
#define SENSOR_TYPE_SPRD_FREE_FALL                (SPRD_SENSOR_TYPE_BASE + 45)
#define SENSOR_TYPE_SPRD_MEDIA_FLIP               (SPRD_SENSOR_TYPE_BASE + 46)

#define SENSOR_STRING_TYPE_SPRD_SHAKE "com.spreadtrum.shake"
#define SENSOR_STRING_TYPE_SPRD_POCKET_MODE "com.spreadtrum.pocket_mode"
#define SENSOR_STRING_TYPE_SPRD_TAP                      "com.spreadtrum.tap"
#define SENSOR_STRING_TYPE_SPRD_FACE_UP_DOWN              "com.spreadtrum.face_up_down"
#define SENSOR_STRING_TYPE_SPRD_FACE_DUMMY               "com.spreadtrum.face_dummy"
#define SENSOR_STRING_TYPE_SPRD_TILT                     "com.spreadtrum.tilt"
#define SENSOR_STRING_TYPE_SPRD_WAKE_UP                 "com.spreadtrum.wake_up"
#define SENSOR_STRING_TYPE_SPRD_GLANCE                   "com.spreadtrum.glance"
#define SENSOR_STRING_TYPE_SPRD_PICKUP                   "com.spreadtrum.pickup"
#define SENSOR_STRING_TYPE_SPRD_FLIP                     "com.spreadtrum.flip"
#define SENSOR_STRING_TYPE_SPRD_TWIST                   "com.spreadtrum.twist"
#define SENSOR_STRING_TYPE_SPRD_HAND_UP                  "com.spreadtrum.hand_up"
#define SENSOR_STRING_TYPE_SPRD_HAND_DOWN                "com.spreadtrum.hand_down"
#define SENSOR_STRING_TYPE_SPRD_REAR_CAMERA              "com.spreadtrum.rear_camera"
#define SENSOR_STRING_TYPE_SPRD_FRONT_CAMERA             "com.spreadtrum.front_camera"
#define SENSOR_STRING_TYPE_SPRD_SCREEN_ON               "com.spreadtrum.screen_on"
#define SENSOR_STRING_TYPE_SPRD_FALL                    "com.spreadtrum.fall"
#define SENSOR_STRING_TYPE_SPRD_CONTEXT_AWARENESS         "com.spreadtrum.context_awareness"
#define SENSOR_STRING_TYPE_SPRD_MEDIA_FLIP               "com.spreadtrum.media_flip"
#endif /* INCLUDE_SENSORS_SENSORTYPEPRIVATE_H_ */
