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

#include <stdlib.h>
#include <sensors/SensorLocalLogdef.h>

#ifndef CONFIGSENSOR_SENSORHUBOPCODEEXTRATOR_H_
#define CONFIGSENSOR_SENSORHUBOPCODEEXTRATOR_H_

typedef enum {
  READ_OPERATION = 1,
  WRITE_OPERATION,
  CHECK_OPERATION,
  DELAY_OPERATION,
  GPIO_CONTROL,
} reg_operation_e;

typedef enum {
  AKM = 1,
  ST,
  VTC,
} mag_vendor;

typedef enum {
  ACC_FUSION = 0x01,
  ACC_GYRO_MAG_FUSION = 0x02,
  ACC_MAG_FUSION = 0x04,
  ACC_GYRO_FUSION = 0x08,
  VENDOR_ORIENTATION = 0x10,
  VENDOR_GRAVITY = 0x20,
  VENDOR_LINEAR_ACC = 0x40,
  VENDOR_ROTATION_VECTOR = 0x80,
} fusion_switch;

#pragma pack(1)
typedef struct {
  uint8_t Interface;
  uint16_t Interface_Freq;
  uint8_t ISR_Mode;
  uint8_t GPIO_ISR_Num;
  uint8_t Slave_Addr;
  uint8_t Chip_Id;
  float Resolution;
  uint16_t Range;
  uint8_t Postion;
  uint8_t Vendor;
} sensor_info_t;
#pragma pack()
/*
 *
 * */
typedef struct iic_unit {
  /* read/write/msleep */
  uint8_t operate; /* 0xFF means delay */
  uint8_t addr;
  uint8_t val;
  uint8_t mask;
} iic_unit_t;

typedef enum {
  CMD_HWINIT,
  CMD_ENABLE,
  CMD_DISABLE,
  CMD_GET_BYPASS,
  CMD_GET_FIFO,
  CMD_GET_STATUS,
  CMD_SET_STATUS,
  CMD_SET_SELFTEST,
  CMD_SET_OFFSET,
  CMD_SET_RATE,
  CMD_SET_MODE,
  CMD_SET_SETTING,
  CMD_CHECK_ID,
  CMD_SCANSALVE,
#if 0
  eCmdOpen,
  eCmdClose,
  eCmdGetRawData,
  eCmdSetDelay,
  eCmdSetRange,
  eCmdEnable,
  eCmdDisable,


  eCmdInit,
  eCmdReadId, /* Init Cmd serial maybe before cmd read id, if sensor id depand on initial */
  eCmdReset,
  eCmdEnable,
  eCmdDisable,
  eCmdSetDelay,
  eCmdBatch,
  eCmdFlush,
#endif
  eCmdTotal,
} opcode_t;

#pragma pack(1)

typedef struct prox_cali_tag {
  uint8_t collect_num;
  uint16_t ps_threshold_high;
  uint16_t ps_threshold_low;
  uint16_t dyna_cali;
  uint16_t dyna_cali_add;
  uint16_t noise_threshold;
  uint16_t noise_high_add;
  uint16_t noise_low_add;
  uint16_t ps_threshold_high_def;
  uint16_t ps_threshold_low_def;
  uint16_t dyna_cali_threahold;
  uint16_t dyna_cali_reduce;
  uint16_t value_threshold;
  uint16_t noise_reference;
} prox_cali_t;

typedef struct light_cali_tag {
  uint16_t start;
  uint16_t end;
  float a;
  float b;
} light_cali_t;

#pragma pack()


typedef struct opcode_unit {
  opcode_t eCmd;
  uint32_t nIIcUnits;
  iic_unit_t *pIIcUnits;
} opcode_unit_t;

/*
 * struct head of SensorHub firmware
 * */

// #pragma pack(8)

typedef struct fwshub_unit {
  opcode_t eCmd;
  uint32_t nShift;
  uint32_t nUnits;
} fwshub_unit_t;

#define SHUB_SENSOR_NAME "sprd-shub"
#define SHUB_SENSOR_NAME_LENGTH 10

#define FIXED_SYSFS_PATCH_LEN_MAX  128

typedef struct fwshub_head {
  char sensorName[SHUB_SENSOR_NAME_LENGTH];
  int sensorType;
  fwshub_unit_t indexOpcode[eCmdTotal];
} fwshub_head_t;

typedef struct fwiic_attr {
  uint8_t salveWriteAddr;
  uint8_t frequency;
  uint8_t IIC_Port;
} fwiic_attr_t;

typedef struct fwworkmode {
  uint8_t readMode;
  uint8_t numberIrq;
  uint8_t triggle;
} fwworkmode_t;

/*
 * Function declaration
 * */
extern void SensorHubOpcodeRegister(const char *name, sensor_info_t *sensorInfo,
                                    opcode_unit *sensorOpcode, uint8_t *cali, int cali_len);
extern void SensorHubOpcodeRegisterAccelerometer_mc3416p();
extern void SensorHubOpcodeRegisterAccelerometer_kxtj3();
extern void SensorHubOpcodeRegisterAccelerometer_kxtj3_ud710_20c11();
extern void SensorHubOpcodeRegisterAccelerometer_bma233();
extern void SensorHubOpcodeRegisterAccelerometer_bma253();
extern void SensorHubOpcodeRegisterAccelerometer_bmi160();
extern void SensorHubOpcodeRegisterAccelerometer_bmi160_1();
extern void SensorHubOpcodeRegisterAccelerometer_bmi160_9861s3();
extern void SensorHubOpcodeRegisterAccelerometer_bmi160_ud710_20c11();
extern void SensorHubOpcodeRegisterAccelerometer_icm20600();
extern void SensorHubOpcodeRegisterAccelerometer_icm20600_l3_1c10();
extern void SensorHubOpcodeRegisterAccelerometer_icm20608();
extern void SensorHubOpcodeRegisterAccelerometer_icm40607();
extern void SensorHubOpcodeRegisterAccelerometer_lsm6ds3();
extern void SensorHubOpcodeRegisterAccelerometer_lsm6ds3tr();
extern void SensorHubOpcodeRegisterAccelerometer_lsm6dsl();
extern void SensorHubOpcodeRegisterAccelerometer_lsm6dsl_ud710();
extern void SensorHubOpcodeRegisterAccelerometer_mxc4005();
extern void SensorHubOpcodeRegisterAccelerometer_qma6981();
extern void SensorHubOpcodeRegisterAccelerometer_sc7a20();
extern void SensorHubOpcodeRegisterAccelerometer_sc7a20_ud710_2c11();
extern void SensorHubOpcodeRegisterAccelerometer_stk8ba53();

extern void SensorHubOpcodeRegisterGyroscope_bmi160();
extern void SensorHubOpcodeRegisterGyroscope_bmi160_1();
extern void SensorHubOpcodeRegisterGyroscope_bmi160_9861s3();
extern void SensorHubOpcodeRegisterGyroscope_icm20600();
extern void SensorHubOpcodeRegisterGyroscope_icm20600_l3_1c10();
extern void SensorHubOpcodeRegisterGyroscope_icm20608();
extern void SensorHubOpcodeRegisterGyroscope_icm40607();
extern void SensorHubOpcodeRegisterGyroscope_lsm6ds3();
extern void SensorHubOpcodeRegisterGyroscope_lsm6ds3tr();
extern void SensorHubOpcodeRegisterGyroscope_lsm6dsl();
extern void SensorHubOpcodeRegisterGyroscope_lsm6dsl_ud710();
extern void SensorHubOpcodeRegisterGyroscope_null();

extern void SensorHubOpcodeRegisterMagnetic_af8133i();
extern void SensorHubOpcodeRegisterMagnetic_af9133();
extern void SensorHubOpcodeRegisterMagnetic_akm09911c();
extern void SensorHubOpcodeRegisterMagnetic_akm09916();
extern void SensorHubOpcodeRegisterMagnetic_akm09918();
extern void SensorHubOpcodeRegisterMagnetic_akm09918_ud710();
extern void SensorHubOpcodeRegisterMagnetic_lis3mdl();
extern void SensorHubOpcodeRegisterMagnetic_mmc3630();
extern void SensorHubOpcodeRegisterMagnetic_mmc3630_9861s3();
extern void SensorHubOpcodeRegisterMagnetic_mmc35160();
extern void SensorHubOpcodeRegisterMagnetic_qmc6983mm();
extern void SensorHubOpcodeRegisterMagnetic_mmc5603();

extern void SensorHubOpcodeRegisterProximity_stk3332();
extern void SensorHubOpcodeRegisterProximity_stk3338();
extern void SensorHubOpcodeRegisterProximity_ap3426();
extern void SensorHubOpcodeRegisterProximity_epl2590();
extern void SensorHubOpcodeRegisterProximity_ltr553als();
extern void SensorHubOpcodeRegisterProximity_ltr553iwhale2();
extern void SensorHubOpcodeRegisterProximity_pa12200002();
extern void SensorHubOpcodeRegisterProximity_rpr0521();
extern void SensorHubOpcodeRegisterProximity_ltr568als();
extern void SensorHubOpcodeRegisterProximity_ltr578als();
extern void SensorHubOpcodeRegisterProximity_stk3338v();
extern void SensorHubOpcodeRegisterProximity_stk3a5x();

extern void SensorHubOpcodeRegisterLight_ltr568als();
extern void SensorHubOpcodeRegisterLight_ltr578als();
extern void SensorHubOpcodeRegisterLight_stk3332();
extern void SensorHubOpcodeRegisterLight_ap3426();
extern void SensorHubOpcodeRegisterLight_epl2590();
extern void SensorHubOpcodeRegisterLight_ltr553als();
extern void SensorHubOpcodeRegisterLight_pa12200002();
extern void SensorHubOpcodeRegisterLight_rpr0521();
extern void SensorHubOpcodeRegisterLight_V2000();
extern void SensorHubOpcodeRegisterLight_stk3338v();
extern void SensorHubOpcodeRegisterLight_stk3a5x();

extern void SensorHubOpcodeRegisterPressure_lps25hb();
extern void SensorHubOpcodeRegisterPressure_bmp280();

extern void SensorHubCalibrationRegisterAllSensor(void);

#endif  // CONFIGSENSOR_SENSORHUBOPCODEEXTRATOR_H_
