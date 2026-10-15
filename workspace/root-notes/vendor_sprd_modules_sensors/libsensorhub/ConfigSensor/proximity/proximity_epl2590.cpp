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
    .operate = 0x02, .addr = 0x11, .val = 0x02, .mask = 0xFF,
  },
  {
    .operate = 0x02, .addr = 0xFD, .val = 0x8E, .mask = 0xFF,
  },
  {
    .operate = 0x02, .addr = 0xFE, .val = 0x22, .mask = 0xFF,
  },
  {
    .operate = 0x02, .addr = 0xFE, .val = 0x02, .mask = 0xFF,
  },
  {
    .operate = 0x02, .addr = 0xFD, .val = 0x00, .mask = 0xFF,
  },
  {
    .operate = 0x02, .addr = 0x00, .val = 0x00, .mask = 0xFF,
  },
};

static struct iic_unit eCmdEnableArray[] = {
  {
    .operate = 0x02, .addr = 0x11, .val = 0x02, .mask = 0xFF,
  },
  {
    .operate = 0x02, .addr = 0x00, .val = 0x01, .mask = 0xFD,
  },
};

static struct iic_unit eCmdDisableArray[] = {

  {
    .operate = 0x02, .addr = 0x00, .val = 0x00, .mask = 0xFD,
  },
  {
    .operate = 0x02, .addr = 0x11, .val = 0x01, .mask = 0xFF,
  },
};

static struct iic_unit eCmdGetRawDataArray[] = {
  {
    .operate = 0x01, .addr = 0x1E, .val = 0x00, .mask = 0xFF,
  },
  {
    .operate = 0x01, .addr = 0x1F, .val = 0x00, .mask = 0xFF,
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
    .operate = 0x01, .addr = 0x20, .val = 0x00, .mask = 0xFF,
  },
};

static struct iic_unit eCmdScanSlaveArray[] = {
};

static struct iic_unit eCmdGetPstatusArray[] = {
};

static struct iic_unit eCmdLockDataArray[] = {
};

static struct iic_unit eCmdUnlockDataArray[] = {
};

static struct iic_unit eCmdSetThreadholdArray[] = {
  {
    .operate = 0x02, .addr = 0x0C, .val = 0x00, .mask = 0xFF,
  },
  {
    .operate = 0x02, .addr = 0x0D, .val = 0x00, .mask = 0xFF,
  },
  {
    .operate = 0x02, .addr = 0x0E, .val = 0x00, .mask = 0xFF,
  },
  {
    .operate = 0x02, .addr = 0x0F, .val = 0x00, .mask = 0xFF,
  },
};

static struct iic_unit eCmdGetIrqFlagArray[] = {
  {
    .operate = 0x1, .addr = 0x06, .val = 0x00, .mask = 0x0C,
  },
};

static sensor_info_t sensorInfoConfigArray[] = {
  {
    .Interface      = 0,
    .Interface_Freq = 400,
    .ISR_Mode       = 0,
    .GPIO_ISR_Num   = 0,
    .Slave_Addr     = 0x92,
    .Chip_Id        = 0x88,
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
  {
    .eCmd = CMD_GET_PSTATUS,
    .nIIcUnits = sizeof(eCmdGetPstatusArray) / sizeof(iic_unit_t),
    .pIIcUnits = eCmdGetPstatusArray,
  },
  {
    .eCmd = CMD_LOCK_DATA,
    .nIIcUnits = sizeof(eCmdLockDataArray) / sizeof(iic_unit_t),
    .pIIcUnits = eCmdLockDataArray,
  },
  {
    .eCmd = CMD_UNLOCK_DATA,
    .nIIcUnits = sizeof(eCmdUnlockDataArray) / sizeof(iic_unit_t),
    .pIIcUnits = eCmdUnlockDataArray,
  },
  {
    .eCmd = CMD_SET_THREADHOLD,
    .nIIcUnits = sizeof(eCmdSetThreadholdArray) / sizeof(iic_unit_t),
    .pIIcUnits = eCmdSetThreadholdArray,
  },
  {
    .eCmd = CMD_GET_IRQFLAG,
    .nIIcUnits = sizeof(eCmdGetIrqFlagArray) / sizeof(iic_unit_t),
    .pIIcUnits = eCmdGetIrqFlagArray,
  },
};

static prox_cali_t eProxCaliArray[] = {
  {
    .collect_num = 7,
    .ps_threshold_high = 250,
    .ps_threshold_low = 200,
    .dyna_cali = 1000,
    .dyna_cali_add = 20,
    .noise_threshold = 350,
    .noise_high_add = 60,
    .noise_low_add = 10,
    .ps_threshold_high_def = 250,
    .ps_threshold_low_def = 200,
    .dyna_cali_threahold = 65,
    .dyna_cali_reduce = 5,
    .value_threshold = 350,
    .noise_reference = 0x7FFF,
  },
};

void SensorHubOpcodeRegisterProximity_epl2590(void) {
  SensorHubOpcodeRegister("proximity_epl2590", sensorInfoConfigArray, sSensorOpcode, (uint8_t*)eProxCaliArray, sizeof(eProxCaliArray));
  return;
}
