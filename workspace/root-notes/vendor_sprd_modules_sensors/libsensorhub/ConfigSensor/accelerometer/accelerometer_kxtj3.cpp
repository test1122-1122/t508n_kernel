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

#include "SensorHubOpCodeExtrator.h"

static struct iic_unit eCmdInitArray[] = {
  //Delay 20ms
  {
    .operate = 0x04, .addr = 0x00, .val = 0x00, .mask = 20,
  },

  //Soft reset
  {
    .operate = 0x02, .addr = 0x1D, .val = 0x80, .mask = 0xFF,
  },

  //Delay 20ms
  {
    .operate = 0x04, .addr = 0x00, .val = 0x00, .mask = 20,
  },

  //Write 0x10 to CTRL1, set to 8G and power off in default
  {
    .operate = 0x02, .addr = 0x1B, .val = 0x10, .mask = 0xFF,
  },

};

static struct iic_unit eCmdEnableArray[] = {
  //Write 0xD0 to CTRL1
  {
    .operate = 0x02, .addr = 0x1B, .val = 0xD0, .mask = 0xFF,
  },
  //Delay 20ms
  {
    .operate = 0x04, .addr = 0x00, .val = 0x00, .mask = 20,
  },
};

static struct iic_unit eCmdDisableArray[] = {
  //Write 0x10 to CTRL1
  {
    .operate = 0x02, .addr = 0x1B, .val = 0x10, .mask = 0xFF,
  },
};

static struct iic_unit eCmdGetRawDataArray[] = {
  {
    .operate = 0x01, .addr = 0x06, .val = 0x00, .mask = 0xF0,
  },
  {
    .operate = 0x01, .addr = 0x07, .val = 0x00, .mask = 0xFF,
  },
  {
    .operate = 0x01, .addr = 0x08, .val = 0x00, .mask = 0xF0,
  },
  {
    .operate = 0x01, .addr = 0x09, .val = 0x00, .mask = 0xFF,
  },
  {
    .operate = 0x01, .addr = 0x0A, .val = 0x00, .mask = 0xF0,
  },
  {
    .operate = 0x01, .addr = 0x0B, .val = 0x00, .mask = 0xFF,
  },
};

static struct iic_unit eCmdGetFifoDataArray[] = {
};

static struct iic_unit eCmdGetStatusArray[] = {
};

static struct iic_unit eCmdSetStatusArray[] = {
};

static struct iic_unit eCmdSetSelftestArray[] = {
};

static struct iic_unit eCmdSetOffsetArray[] = {
};

static struct iic_unit eCmdSetRateArray[] = {
  //100Hz
  {
    .operate = 0x02, .addr = 0x21, .val = 0x03, .mask = 0xFF,
  },

  //50Hz
  {
    .operate = 0x02, .addr = 0x21, .val = 0x02, .mask = 0xFF,
  },

  //25Hz
  {
    .operate = 0x02, .addr = 0x21, .val = 0x01, .mask = 0xFF,
  },

  //6.25Hz
  {
    .operate = 0x02, .addr = 0x21, .val = 0x0B, .mask = 0xFF,
  },

  //delay and discard first event
  {
    .operate = 0x04, .addr = 0x00, .val = 0x01, .mask = 0xc8,
  },
};

static struct iic_unit eCmdSetModeArray[] = {
};

static struct iic_unit eCmdSetSettingArray[] = {
};

/*Check if reg 0x0F, value=0x35 */
static struct iic_unit eCmdCheckIdArray[] = {
  {
    .operate = 0x03, .addr = 0x0F, .val = 0x35, .mask = 0xFF,
  },
};

static struct iic_unit eCmdScanSlaveArray[] = {
};

static sensor_info_t sensorInfoConfigArray[] = {
  {
    .Interface      = 0,
    .Interface_Freq = 400,
    .ISR_Mode       = 0,
    .GPIO_ISR_Num   = 0,
    .Slave_Addr     = 0x1C,
    .Chip_Id        = 0x35,
    .Resolution     = 0.038281f,
    .Range          = 8,
    .Postion        = 7,
    .Vendor         = 0,
  },
};

static struct opcode_unit sSensorOpcode[eCmdTotal] = {
  {
    .eCmd = CMD_HWINIT,
    .nIIcUnits = sizeof(eCmdInitArray) / sizeof(iic_unit_t),
    .pIIcUnits = eCmdInitArray,
  },
  {
    .eCmd = CMD_ENABLE,
    .nIIcUnits = sizeof(eCmdEnableArray) / sizeof(iic_unit_t),
    .pIIcUnits = eCmdEnableArray,
  },
  {
    .eCmd = CMD_DISABLE,
    .nIIcUnits = sizeof(eCmdDisableArray) / sizeof(iic_unit_t),
    .pIIcUnits = eCmdDisableArray,
  },
  {
    .eCmd = CMD_GET_BYPASS,
    .nIIcUnits = sizeof(eCmdGetRawDataArray) / sizeof(iic_unit_t),
    .pIIcUnits = eCmdGetRawDataArray,
  },
  {
    .eCmd = CMD_GET_FIFO,
    .nIIcUnits = sizeof(eCmdGetFifoDataArray) / sizeof(iic_unit_t),
    .pIIcUnits = eCmdGetFifoDataArray,
  },
  {
    .eCmd = CMD_GET_STATUS,
    .nIIcUnits = sizeof(eCmdGetStatusArray) / sizeof(iic_unit_t),
    .pIIcUnits = eCmdGetStatusArray,
  },
  {
    .eCmd = CMD_SET_STATUS,
    .nIIcUnits = sizeof(eCmdSetStatusArray) / sizeof(iic_unit_t),
    .pIIcUnits = eCmdSetStatusArray,
  },
  {
    .eCmd = CMD_SET_SELFTEST,
    .nIIcUnits = sizeof(eCmdSetSelftestArray) / sizeof(iic_unit_t),
    .pIIcUnits = eCmdSetSelftestArray,
  },
  {
    .eCmd = CMD_SET_OFFSET,
    .nIIcUnits = sizeof(eCmdSetOffsetArray) / sizeof(iic_unit_t),
    .pIIcUnits = eCmdSetOffsetArray,
  },
  {
    .eCmd = CMD_SET_RATE,
    .nIIcUnits = sizeof(eCmdSetRateArray) / sizeof(iic_unit_t),
    .pIIcUnits = eCmdSetRateArray,
  },
  {
    .eCmd = CMD_SET_MODE,
    .nIIcUnits = sizeof(eCmdSetModeArray) / sizeof(iic_unit_t),
    .pIIcUnits = eCmdSetModeArray,
  },
  {
    .eCmd = CMD_SET_SETTING,
    .nIIcUnits = sizeof(eCmdSetSettingArray) / sizeof(iic_unit_t),
    .pIIcUnits = eCmdSetSettingArray,
  },
  {
    .eCmd = CMD_CHECK_ID,
    .nIIcUnits = sizeof(eCmdCheckIdArray) / sizeof(iic_unit_t),
    .pIIcUnits = eCmdCheckIdArray,
  },
  {
    .eCmd = CMD_SCANSALVE,
    .nIIcUnits = sizeof(eCmdScanSlaveArray) / sizeof(iic_unit_t),
    .pIIcUnits = eCmdScanSlaveArray,
  },
};

static uint8_t eAccCaliDataArray[] = {
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
};

void SensorHubOpcodeRegisterAccelerometer_kxtj3(void) {
  SensorHubOpcodeRegister("accelerometer_kxtj3", sensorInfoConfigArray, sSensorOpcode, eAccCaliDataArray, sizeof(eAccCaliDataArray));
  return;
}
