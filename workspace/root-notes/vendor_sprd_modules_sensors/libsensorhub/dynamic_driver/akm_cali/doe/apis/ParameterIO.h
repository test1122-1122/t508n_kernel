/******************************************************************************
 *
 *  $Id: $
 *
 * -- Copyright Notice --
 *
 * Copyright (c) 2004 Asahi Kasei Microdevices Corporation, Japan
 * All Rights Reserved.
 *
 * This software program is the proprietary program of Asahi Kasei Microdevices
 * Corporation("AKM") licensed to authorized Licensee under the respective
 * agreement between the Licensee and AKM only for use with AKM's electronic
 * compass IC.
 *
 * THIS SOFTWARE IS PROVIDED TO YOU "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABLITY, FITNESS FOR A PARTICULAR PURPOSE AND NON INFRINGEMENT OF
 * THIRD PARTY RIGHTS, AND WE SHALL NOT BE LIABLE FOR ANY LOSSES AND DAMAGES
 * WHICH MAY OCCUR THROUGH USE OF THIS SOFTWARE.
 *
 * -- End Asahi Kasei Microdevices Copyright Notice --
 *
 ******************************************************************************/
#ifndef __PARAMETERIO_H__
#define __PARAMETERIO_H__

#include "AKCompass.h"

// A parameter structure.
typedef struct _AKSCSAVEPRMS{
    int16vec        HSUC_HO[CSPEC_NUM_FORMATION];
    int16vec        HFLUCV_HREF[CSPEC_NUM_FORMATION];
    AKSC_HDST       HSUC_HDST[CSPEC_NUM_FORMATION];
    int32vec        HSUC_HBASE[CSPEC_NUM_FORMATION];
} AKSCSAVEPRMS;

int16 LoadParameters(AKSCPRMS* prms);
int16 SaveParameters(const AKSCPRMS* prms);
int16 SetPDC(AKSCPRMS* prms, uint8 pdc_prms[]);
void InitAKSCPRMS(AKSCPRMS *prms);
void SetDefaultPRMS(AKSCPRMS *prms);

#endif /* __PARAMETERIO_H__ */
