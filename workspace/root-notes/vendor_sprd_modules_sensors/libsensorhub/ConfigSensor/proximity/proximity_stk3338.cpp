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
	  .operate = 0x02, .addr = 0x01, .val = 0x30, .mask = 0xFF,
	},
	{
	  .operate = 0x02, .addr = 0x03, .val = 0x40, .mask = 0xFF,
	},
	{
	  .operate = 0x02, .addr = 0x04, .val = 0x00, .mask = 0xFF,
	},
	{
	  .operate = 0x02, .addr = 0x05, .val = 0x20, .mask = 0xFF,
	},
	{
	  .operate = 0x02, .addr = 0x4F, .val = 0x43, .mask = 0xFF,
	},
	{
	  .operate = 0x02, .addr = 0xA0, .val = 0x10, .mask = 0xFF,
	},
	{
	  .operate = 0x02, .addr = 0xA1, .val = 0x0F, .mask = 0xFF,
	},
	{
	  .operate = 0x02, .addr = 0xAA, .val = 0x40, .mask = 0xFF,
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
    .operate = 0x02, .addr = 0x00, .val = 0x05, .mask = 0x05,
  },
  {
    .operate = 0x02, .addr = 0x05, .val = 0x20, .mask = 0xFF,
  },
  {
    .operate = 0x04, .addr = 0x00, .val = 0x00, .mask = 0x6E,
  },
};

static struct iic_unit eCmdDisableArray[] = {
  {
    .operate = 0x02, .addr = 0x00, .val = 0x00, .mask = 0x05,
  },
};

static struct iic_unit eCmdGetRawDataArray[] = {
  /* Low Bit */
  {
    .operate = 0x01, .addr = 0x12, .val = 0x00, .mask = 0xFF,
  },
  /* High Bit */
  {
    .operate = 0x01, .addr = 0x11, .val = 0x00, .mask = 0xFF,
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
    .operate = 0x03, .addr = 0x3E, .val = 0x50, .mask = 0xF0,
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
    .Slave_Addr     = 0xCE,
    .Chip_Id        = 0x58,
    .Resolution     = 1.0f,
    .Range          = 0,
    .Postion        = 0,
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
static prox_cali_t eProxCaliArray[] = {
  {
    .collect_num = 7,
    .ps_threshold_high = 1300,
    .ps_threshold_low = 800,
    .dyna_cali = 6000,
    .dyna_cali_add = 50,
    .noise_threshold = 1600,
    .noise_high_add = 800,
    .noise_low_add = 300,
    .ps_threshold_high_def = 1300,
    .ps_threshold_low_def = 800,
    .dyna_cali_threahold = 300,
    .dyna_cali_reduce = 600,
    .value_threshold = 1500,
    .noise_reference = 1500,
  },
};

void SensorHubOpcodeRegisterProximity_stk3338(void) {
  SensorHubOpcodeRegister("proximity_stk3338", sensorInfoConfigArray, sSensorOpcode, (uint8_t*)eProxCaliArray, sizeof(eProxCaliArray));
  return;
}
