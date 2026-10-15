/*
 * Copyright (C) 2008 The Android Open Source Project
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

#include <fcntl.h>
#include <errno.h>
#include <math.h>
#include <poll.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/select.h>
#include <cutils/log.h>

#include "GyroSensor.h"

#define GYRO_INPUT_NAME "gyroscope"
#define SYSFS_PATH_CLASS_INPUT "/sys/class/input/input"
#define SYSFS_NODE_ENABLE "gyro_enable"
#define SYSFS_NODE_DELAY "gyro_delay"
#define SYSFS_NODE_CHIP_INFO "chip_info"

struct SensorInfo {
  const char *sensor_name;
  sensor_t *sensor_list;
};

static char chip_info[20];
static sensor_t *gyro_sensor_list;

/*****************************************************************************/
static const char *bmi160 = "bmi160";

static struct sensor_t sensor_list_bmi160[] = {
  {
    .name = "BMI160 3-axis Gyroscope",
    .vendor = "Bosch Sensortec",
    .version = 1,
    .handle = SENSORS_GYROSCOPE_HANDLE,
    .type = SENSOR_TYPE_GYROSCOPE,
    .maxRange = 34.906585039886591538473815369772, //2000*PI/180, PI=3.14159265
    .resolution = 0.00106526443603169529841533860381,
    .power = 3.6f,
    .minDelay = 5000, // us
    .fifoReservedEventCount = 0,
    .fifoMaxEventCount = 0,
    .stringType = SENSOR_STRING_TYPE_GYROSCOPE,
    .requiredPermission = "",
    .maxDelay = 200000, //us
    .flags = SENSOR_FLAG_CONTINUOUS_MODE,
    .reserved = {}
  }
};

static struct sensor_t sSensorList_NULL[] = {
  {
    "NULL Sensor",
    "Null",
    0,
    SENSORS_GYROSCOPE_HANDLE,
    SENSOR_TYPE_GYROSCOPE,
    0,
    0,
    0,
    0, // us fastest is 100Hz
    0,
    0,
    SENSOR_STRING_TYPE_GYROSCOPE,
    "",
    0,//unused
    SENSOR_FLAG_CONTINUOUS_MODE,
    {}
  }
};

struct SensorInfo gyro_sensors[] = {
  {bmi160, sensor_list_bmi160},
  {"", sSensorList_NULL},
};

static int read_sysfs(char *filename, char *buf, int size) {
  int fd;
  int count = 0;

  if (filename == NULL || buf == NULL || size <= 0)
    return 0;

  fd = open(filename, O_RDONLY);

  if (fd > 0) {
    count = read(fd, buf, size);
    close(fd);
  } else {
    ALOGE("GyroSensor: read sysfs file %s error, %s\n", filename, strerror(errno));
    return 0;
  }

  return count;
}

static void getGyroChipInfo(char *input_sys_path) {
  int size = 0;
  char sys_chip_info[PATH_MAX];
  sprintf(sys_chip_info, "%s/%s",
          input_sys_path, SYSFS_NODE_CHIP_INFO);
  ALOGD("GyroSensor: sys_chip_info=%s, input_sysfs_path=%s\n",
        sys_chip_info, input_sys_path);
  memset(chip_info, 0, sizeof(chip_info));
  size = read_sysfs(sys_chip_info, chip_info, sizeof(chip_info));
  ALOGD("GyroSensor: size=%d\n", size);

  if (0 == size) {
    ALOGE("GyroSensor: %s:chip_info failed!\n", __FUNCTION__);
  } else {
    /*  add string termination symbol  */
    chip_info[size] = '\0';
    ALOGD("GyroSensor: %s ,%s\n", __FUNCTION__, chip_info);
  }
}

static sensor_t *getGyroSensorList(char *chipinfo) {
  short int sensor_info_count = 0;
  sensor_info_count = sizeof(gyro_sensors) / sizeof(SensorInfo);
  ALOGD("GyroSensor: sensorInfoCount=%d\n", sensor_info_count);

  if (chipinfo == NULL || sensor_info_count == 1) {
    ALOGD("GyroSensor: there is no gyro sensor!");
    return gyro_sensors[sensor_info_count - 1].sensor_list;
  }

  for (int i = 0; i < sensor_info_count - 1; i++) {
    if (!strcmp(chipinfo, gyro_sensors[i].sensor_name)) {
      return gyro_sensors[i].sensor_list;
    }
  }

  return gyro_sensors[sensor_info_count - 1].sensor_list;
}

GyroSensor::GyroSensor()
  : SensorBase(NULL, GYRO_INPUT_NAME),
    mEnabled(0),
    mDelay(-1),
    mLayout(0),
    mInputReader(32),
    mHasPendingEvent(false),
    input_sysfs_path_len(0),
    mSensorCoordinate() {
  mPendingEvent.version = sizeof(sensors_event_t);
  mPendingEvent.sensor = ID_G;
  mPendingEvent.type = SENSOR_TYPE_GYROSCOPE;
  memset(mPendingEvent.data, 0, sizeof(mPendingEvent.data));
  int InputNum = getInputNum(GYRO_INPUT_NAME);
  ALOGE_IF(InputNum < 0, "couldn't find '%s' input device InputNum=%d",
           SYSFS_PATH_CLASS_INPUT, InputNum);
  sprintf(input_sysfs_path, "%s%d",
          SYSFS_PATH_CLASS_INPUT, InputNum);
  setInitialState();
  getGyroChipInfo(input_sysfs_path);
  gyro_sensor_list = getGyroSensorList(chip_info);
}

GyroSensor::~GyroSensor() {
  if (mEnabled) {
    setEnable(0, 0);
  }
}

int GyroSensor::setInitialState() {
  int clockid = CLOCK_BOOTTIME;

  if (!ioctl(data_fd, EVIOCSCLOCKID, &clockid)) {
    ALOGD("GyroSensor: set EVIOCSCLOCKID = %d\n", clockid);
  } else {
    ALOGE("GyroSensor: set EVIOCSCLOCKID failed \n");
  }

  return 0;
}

bool GyroSensor::hasPendingEvents() const {
  return mHasPendingEvent;
}

int GyroSensor::setEnable(int32_t handle, int enabled) {
  int err = 0;
  char writeBuffer[8];
  int writeBytes;
  char sysfsEnable[PATH_MAX];

  /* handle check */
  if (handle != ID_G) {
    ALOGE("GyroSensor: Invalid handle (%d)", handle);
    return -EINVAL;
  }

  ALOGD("GyroSensor: handle=%d,enabled=%d\n", handle, enabled);
  sprintf(sysfsEnable, "%s/%s",
          input_sysfs_path, SYSFS_NODE_ENABLE);

  if (enabled) {
    setInitialState();
    writeBytes = sprintf(writeBuffer, "%d\n", 1) + 1;
    if (mEnabled != 1) {
      err = write_sys_attribute(sysfsEnable, writeBuffer, writeBytes);
      mEnabled = 1;
    } else {
      ALOGD("GyroSensor: Already enabled\n");
    }
  } else {
    writeBytes = sprintf(writeBuffer, "%d\n", 0) + 1;
    if (mEnabled != 0) {
      err = write_sys_attribute(sysfsEnable, writeBuffer, writeBytes);
      mEnabled = 0;
    }
  }

  ALOGD("GyroSensor: mEnabled = %d\n", mEnabled);
  return err;
}

int GyroSensor::setDelay(int32_t handle, int64_t delay_ns) {
  int err = 0;
  int ms;
  char sysfsEnable[PATH_MAX];
  char writeBuffer[8];

  /* handle check */
  if (handle != ID_G) {
    ALOGE("GyroSensor: Invalid handle (%d)", handle);
    return -EINVAL;
  }

  ALOGE("GyroSensor: mDelay(%lld) delay_ns(%lld)", mDelay, delay_ns);

  if (mDelay != delay_ns) {
    ms = delay_ns / 1000000;
    ALOGE("GyroSensor: ms(%d) ", ms);
    int writeBytes = sprintf(writeBuffer, "%d\n", ms) + 1;
    sprintf(sysfsEnable, "%s/%s",
            input_sysfs_path, SYSFS_NODE_DELAY);
    err = write_sys_attribute(sysfsEnable, writeBuffer, writeBytes);

    if (err != 0) {
      ALOGE("GyroSensor:setDelay fail (%s)", strerror(errno));
      return -errno;
    }

    mDelay = delay_ns;
  }

  return err;
}

int64_t GyroSensor::getDelay(int32_t handle) {
  return (handle == ID_G) ? mDelay : 0;
}

int GyroSensor::getEnable(int32_t handle) {
  return (handle == ID_G) ? mEnabled : 0;
}

int GyroSensor::readEvents(sensors_event_t *data, int count) {
  if (count < 1) return -EINVAL;

  if (mHasPendingEvent) {
    mHasPendingEvent = false;
    mPendingEvent.timestamp = getTimestamp();
    *data = mPendingEvent;
    return mEnabled ? 1 : 0;
  }

  ssize_t n = mInputReader.fill(data_fd);

  if (n < 0) return n;

  int numEventReceived = 0;
  input_event const *event;
  static float gyro_latest_x;
  static float gyro_latest_y;
  static float gyro_latest_z;

  while (count && mInputReader.readEvent(&event)) {
    int type = event->type;

    if (type == EV_ABS) {
      float value = event->value;

      if (event->code == ABS_RX) {
        gyro_latest_x = gyro_sensor_list->resolution * value;
      } else if (event->code == ABS_RY) {
        gyro_latest_y = gyro_sensor_list->resolution * value;
      } else if (event->code == ABS_RZ) {
        gyro_latest_z = gyro_sensor_list->resolution * value;
      }
    } else if (type == EV_SYN) {
      mPendingEvent.timestamp = timevalToNano(event->time);

      if (mEnabled) {
        mPendingEvent.gyro.x = gyro_latest_x;
        mPendingEvent.gyro.y = gyro_latest_y;
        mPendingEvent.gyro.z = gyro_latest_z;
        mSensorCoordinate.coordinate_data_convert(mPendingEvent.gyro.v,
            INSTALL_DIR);
        *data++ = mPendingEvent;
        count--;
        numEventReceived++;
      }
    } else {
      ALOGE("GyroSensor: unknown event (type=%d, code=%d)", type, event->code);
    }

    mInputReader.next();
  }

  return numEventReceived;
}

int GyroSensor::getInputNum(const char *InputName) {
  int num = -1;
  char filename[64];
  int err;
  char ev_name[64];
  int fd;

  for (int i = 0; i < 100; i++) {
    sprintf(filename, "/dev/input/event%d", i);
    fd = open(filename, O_RDONLY);

    if (fd >= 0) {
      err = ioctl(fd, EVIOCGNAME(sizeof(ev_name) - 1), &ev_name);
      ALOGD("get input device ev_name: %s", ev_name);

      if (err < 1)
        ev_name[0] = '\0';

      if (0 == strcmp(ev_name, InputName)) {
        num = i;
        close(fd);
        break;
      }

      close(fd);
    } else
      ALOGE("GyroSensor: open %s failedi fd=%d\n", filename, fd);
  }

  return num;
}

int GyroSensor::populateSensorList(struct sensor_t *list) {
  memcpy(list, gyro_sensor_list, sizeof(struct sensor_t) * numSensors);
  return numSensors;
}
