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
	{
	  .operate = 0x02, .addr = 0x00, .val = 0x00, .mask = 0xFF,
	},
	{
	  .operate = 0x02, .addr = 0x02, .val = 0x21, .mask = 0xFF,
	},
	{
	  .operate = 0x02, .addr = 0x4E, .val = 0x20, .mask = 0xFF,
	},
	{
	  .operate = 0x02, .addr = 0xDB, .val = 0x14, .mask = 0xFF,
	},
	{
	  .operate = 0x02, .addr = 0xF4, .val = 0xC0, .mask = 0xFF,
	},
	{
	  .operate = 0x02, .addr = 0xFA, .val = 0x01, .mask = 0xFF,
	},

};

static struct iic_unit eCmdEnableArray[] = {
  {
    .operate = 0x02, .addr = 0x00, .val = 0x02, .mask = 0x02,
  },
  {
    .operate = 0x04, .addr = 0x00, .val = 0x00, .mask = 0x6E,
  },
};

static struct iic_unit eCmdDisableArray[] = {
  {
    .operate = 0x02, .addr = 0x00, .val = 0x00, .mask = 0x02,
  },
};

static struct iic_unit eCmdGetRawDataArray[] = {
  /* Low Bit */
  {
    .operate = 0x01, .addr = 0x14, .val = 0x00, .mask = 0xFF,
  },
  /* High Bit */
  {
    .operate = 0x01, .addr = 0x13, .val = 0x00, .mask = 0xFF,
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
};

static struct iic_unit eCmdSetModeArray[] = {
};

static struct iic_unit eCmdSetSettingArray[] = {
};

static struct iic_unit eCmdCheckIdArray[] = {
  {
    .operate = 0x03, .addr = 0x3E, .val = 0x5B, .mask = 0xFF,
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
    /*8bit bit0 = 0*/
    .Slave_Addr     = 0xAE,
    .Chip_Id        = 0x5B,
    .Resolution     = 0.3f,
    .Range          = 0,
    .Postion        = 0,
    .Vendor         = 1,
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

static light_cali_t eLightCaliArray[] = {
  {
    .start = 0, .end = 50, .a = 1.0f, .b = 0.0f,
  },
  {
	.start = 50, .end = 100, .a = 1.0f, .b = 0.0f,
  },
  {
	.start = 100, .end = 300, .a = 1.0f, .b = 0.0f,
  },
  {
	.start = 300, .end = 1000, .a = 1.0f, .b = 0.0f,
  },
  {
	.start = 1000, .end = 5000, .a = 1.0f, .b = 0.0f,
  },
  {
	.start = 5000, .end = (uint16_t)-1, .a = 1.0f, .b = 0.0f,
  },
};


void SensorHubOpcodeRegisterLight_V2000(void) {
  SensorHubOpcodeRegister("light_V2000", sensorInfoConfigArray, sSensorOpcode, (uint8_t*)eLightCaliArray, sizeof(eLightCaliArray));
  return;
}
