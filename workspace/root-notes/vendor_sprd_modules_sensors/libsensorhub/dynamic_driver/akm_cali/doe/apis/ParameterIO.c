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
#include "ParameterIO.h"
#include "AKMD_APIs.h"
#include "CustomerSpec.h"
#include "AKCompass.h"

//#define SIZE_PARAMETER_STORE ((2+6+6+12)*CSPEC_NUM_FORMATION)
extern AKSCSAVEPRMS    offset_prms;

int16 LoadParameters(
    AKSCPRMS *prms    //A pointer to AKPRMS structure. Loaded parameters stored in this structure.
)
{
    int16    i;

    for(i=0; i<CSPEC_NUM_FORMATION; i++){
        prms->HSUC_HDST[i] = offset_prms.HSUC_HDST[i];
        prms->HSUC_HO[i].u.x = offset_prms.HSUC_HO[i].u.x;
        prms->HSUC_HO[i].u.y = offset_prms.HSUC_HO[i].u.y;
        prms->HSUC_HO[i].u.z = offset_prms.HSUC_HO[i].u.z;
        prms->HFLUCV_HREF[i].u.x = offset_prms.HFLUCV_HREF[i].u.x;
        prms->HFLUCV_HREF[i].u.y = offset_prms.HFLUCV_HREF[i].u.y;
        prms->HFLUCV_HREF[i].u.z = offset_prms.HFLUCV_HREF[i].u.z;
        prms->HSUC_HBASE[i].u.x = offset_prms.HSUC_HBASE[i].u.x;
        prms->HSUC_HBASE[i].u.y = offset_prms.HSUC_HBASE[i].u.y;
        prms->HSUC_HBASE[i].u.z = offset_prms.HSUC_HBASE[i].u.z;
    }

    return AKMD_SUCCESS;
}

int16 SaveParameters(
    const AKSCPRMS *prms ///[in] Pointer to parameters of AKPRMS
)
{
    int16 i;

    for(i=0; i<CSPEC_NUM_FORMATION; i++){
        offset_prms.HSUC_HDST[i] = prms->HSUC_HDST[i] ;
        offset_prms.HSUC_HO[i].u.x = prms->HSUC_HO[i].u.x ;
        offset_prms.HSUC_HO[i].u.y = prms->HSUC_HO[i].u.y;
        offset_prms.HSUC_HO[i].u.z = prms->HSUC_HO[i].u.z;
        offset_prms.HFLUCV_HREF[i].u.x = prms->HFLUCV_HREF[i].u.x;
        offset_prms.HFLUCV_HREF[i].u.y = prms->HFLUCV_HREF[i].u.y ;
        offset_prms.HFLUCV_HREF[i].u.z = prms->HFLUCV_HREF[i].u.z ;
        offset_prms.HSUC_HBASE[i].u.x = prms->HSUC_HBASE[i].u.x ;
        offset_prms.HSUC_HBASE[i].u.y = prms->HSUC_HBASE[i].u.y ;
        offset_prms.HSUC_HBASE[i].u.z = prms->HSUC_HBASE[i].u.z ;
    }

    return AKMD_SUCCESS;
}

//int16_t LoadPDC(unsigned char pdc_table[], AKSCPRMS* prms)
int16 SetPDC(AKSCPRMS *prms, uint8 pdc_prms[])
{
    int i;

    for (i=0; i<PDC_SIZE; i++) {
        prms->m_pdc[i] = pdc_prms[i];
    }
    // Set parameter's pointer.
    prms->m_pdcptr = prms->m_pdc;

    return 0;
}

/*!
 Initialize #AKSCPRMS structure. At first, 0 is set to all parameters.
 After that, some parameters, which should not be 0, are set to specific
 value. Some of initial values can be customized by editing the file
 \c "CustomerSpec.h".
 @param[out] prms A pointer to #AKSCPRMS structure.
 */
void InitAKSCPRMS(AKSCPRMS *prms)
{
    // Sensitivity
    prms->m_hs.u.x = AKSC_HSENSE_TARGET;
    prms->m_hs.u.y = AKSC_HSENSE_TARGET;
    prms->m_hs.u.z = AKSC_HSENSE_TARGET;

    // HDOE
    prms->m_hdst = AKSC_HDST_UNSOLVED;

    // (m_hdata is initialized with AKSC_InitDecomp)
    prms->m_hnave = CSPEC_HNAVE;
    prms->m_dvec.u.x = CSPEC_DVEC_X;
    prms->m_dvec.u.y = CSPEC_DVEC_Y;
    prms->m_dvec.u.z = CSPEC_DVEC_Z;

    prms->m_hlayout.m[0][0] = 0;
    prms->m_hlayout.m[0][1] = 1;
    prms->m_hlayout.m[0][2] = 0;
    prms->m_hlayout.m[1][0] = -1;
    prms->m_hlayout.m[1][1] = 0;
    prms->m_hlayout.m[1][2] = 0;
    prms->m_hlayout.m[2][0] = 0;
    prms->m_hlayout.m[2][1] = 0;
    prms->m_hlayout.m[2][2] = 1;

    prms->m_alayout.m[0][0] = 0;
    prms->m_alayout.m[0][1] = -1;
    prms->m_alayout.m[0][2] = 0;
    prms->m_alayout.m[1][0] = 1;
    prms->m_alayout.m[1][1] = 0;
    prms->m_alayout.m[1][2] = 0;
    prms->m_alayout.m[2][0] = 0;
    prms->m_alayout.m[2][1] = 0;
    prms->m_alayout.m[2][2] = -1;

    prms->m_glayout.m[0][0] = 0;
    prms->m_glayout.m[0][1] = 1;
    prms->m_glayout.m[0][2] = 0;
    prms->m_glayout.m[1][0] = -1;
    prms->m_glayout.m[1][1] = 0;
    prms->m_glayout.m[1][2] = 0;
    prms->m_glayout.m[2][0] = 0;
    prms->m_glayout.m[2][1] = 0;
    prms->m_glayout.m[2][2] = 1;
}

/*!
 Fill #AKSCPRMS structure with default value.
 @param[out] prms A pointer to #AKSCPRMS structure.
 */
void SetDefaultPRMS(AKSCPRMS *prms)
{
    int16 i;
#ifndef AKMD_DISABLE_DOEPLUS
    int16 j;
#endif
    // Set parameter to HDST, HO, HREF
    for (i = 0; i < CSPEC_NUM_FORMATION; i++) {
        prms->HSUC_HDST[i] = AKSC_HDST_UNSOLVED;
        prms->HSUC_HO[i].u.x = 0;
        prms->HSUC_HO[i].u.y = 0;
        prms->HSUC_HO[i].u.z = 0;
        prms->HFLUCV_HREF[i].u.x = 0;
        prms->HFLUCV_HREF[i].u.y = 0;
        prms->HFLUCV_HREF[i].u.z = 0;
        prms->HSUC_HBASE[i].u.x = 0;
        prms->HSUC_HBASE[i].u.y = 0;
        prms->HSUC_HBASE[i].u.z = 0;
#ifndef AKMD_DISABLE_DOEPLUS
        for (j = 0; j < AKSC_DOEP_SIZE; j++) {
            prms->DOEP_PRMS[i][j] = (AKSC_FLOAT)(0.);
        }
#endif
    }
}

