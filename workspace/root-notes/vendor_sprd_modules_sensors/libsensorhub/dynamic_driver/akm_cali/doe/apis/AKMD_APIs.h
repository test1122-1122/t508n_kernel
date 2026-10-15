/******************************************************************************
 *
 *  $Id: //source/qcom/qct/core/sensors/dsps/apps/common/main/latest/sam/algo/mag_cal/src/AKM8963/AKMD_APIs.h#1 $
 *
 * -- Copyright Notice --
 *
 * Copyright (c) 2004 - 2011 Asahi Kasei Microdevices Corporation, Japan
 * All Rights Reserved.
 *
 * This software program is proprietary program of Asahi Kasei Microdevices
 * Corporation("AKM") licensed to authorized Licensee under Software License
 * Agreement (SLA) executed between the Licensee and AKM.
 *
 * Use of the software by unauthorized third party, or use of the software
 * beyond the scope of the SLA is strictly prohibited.
 *
 * -- End Asahi Kasei Microdevices Copyright Notice --
 *
 ******************************************************************************/
#pragma once

#include <stdint.h> /* int64_t */

#include "AKCommon.h"
#include "AKCompass.h"

//#define AKMD_VALUE_CHECK
//#define AKMD_TEST_OUTPUT
#define AKMD_ST2_16BIT    0x10
#define AKMD_ST2_14BIT    0x00

#if defined(__cplusplus)
extern "C" {
#endif

int16_t AKMD_Init(float offset[],
        int accuracy,
        uint8 pdc_prms[]
);

void AKMD_Release(void);

int16_t AKMD_Start(void);

int16_t AKMD_Stop(void);

int16_t AKMD_SetData(
    const    int32_t ihx,
    const    int32_t ihy,
    const    int32_t ihz,
    const    int16_t st2,
    const    int16_t ihdt,
    const    unsigned int ihvalid,
    const    float igx,
    const    float igy,
    const    float igz,
    const    int16_t igdt,
    const    unsigned int igvalid,
    const    float iax,
    const    float iay,
    const    float iaz,
    const    int16_t iadt,
    const    unsigned int iavalid
);

int16_t AKMD_Calibrate(
        float *ox,
        float *oy,
        float *oz,
        float* oxbias,
        float* oybias,
        float* ozbias,
        int8_t* accuracy
);

#ifdef AKMD_ENABLE_ORIENTATION
void AKMD_GetDirection(
    float ori_buf[],
    const int16_t buf_size,
    int8_t *status
);
#endif

#ifdef AKMD_ENABLE_PG
void AKMD_PG_Gyro(
    float gyro_buf[],
    const int16_t buf_size,
    int8_t *status
);

void AKMD_PG_RotationVector(
    float rv_buf[],
    const int16_t buf_size
);

void AKMD_PG_Gravity(
    float gravity_buf[],
    const int16_t buf_size
);

void AKMD_PG_LA(
    float la_buf[],
    const int16_t buf_size
);
#endif

void AKMD_ResetDOE(void);

int16_t AKMD_ChangeFormation(int16_t form);

#if defined(__cplusplus)
}
#endif


