/******************************************************************************
 *
 * $Id: //source/qcom/qct/core/sensors/dsps/apps/common/main/latest/sam/algo/mag_cal/src/AKM8963/AKCompass.h#1 $
 *
 * -- Copyright Notice --
 *
 * Copyright (c) 2004 Asahi Kasei Microdevices Corporation, Japan
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
#ifndef AKMD_INC_AKCOMPASS_H
#define AKMD_INC_AKCOMPASS_H

#include "AKCommon.h"
#include "CustomerSpec.h"

//**************************************
#include "AKCertification.h"
#include "AKConfigure.h"
#include "AKDecomp.h"
#include "AKMDevice.h"
//#include "../libSmartCompass/include/AKMDeviceF.h"
#include "AKDirection6D.h"
//#include "../libSmartCompass/include/AKDirection9D.h"
#include "AKHDOE.h"
#ifdef AKMD_DOEEx
#include "AKHDOEEX.h"
#endif
#ifdef AKMD_DOEaG
#include "AKHDOEaG.h"
#endif
#include "AKHFlucCheck.h"
#include "AKMDevice.h"
//#include "../libSmartCompass/include/AKMDeviceF.h"
//#include "../libSmartCompass/include/AKManualCal.h"
//#include "../libSmartCompass/include/AKPseudoGyro.h"
#include "AKVersion.h"
#ifndef AKMD_DISABLE_DOEPLUS
#include "AKDOEPlus.h"
#endif

/*** Constant definition ******************************************************/
#define AKMD_ERROR		-1
#define AKMD_SUCCESS	 0

#define	THETAFILTER_SCALE	4128
#define	HFLUCV_TH			2500
#define PDC_SIZE			27

/*** Type declaration *********************************************************/
typedef enum _AKMD_PATNO {
    PAT_INVALID = 0,
    PAT1,
    PAT2,
    PAT3,
    PAT4,
    PAT5,
    PAT6,
    PAT7,
    PAT8
} AKMD_PATNO;
/*! A parameter structure which is needed for HDOE and Direction calculation. */
typedef struct _AKSCPRMS {
	// Variables for magnetic sensor.
	int16vec	m_ho;
	int16vec	HSUC_HO[CSPEC_NUM_FORMATION];
	int32vec	m_ho32;
	int16vec	m_hs;
	int16vec	HFLUCV_HREF[CSPEC_NUM_FORMATION];
	AKSC_HFLUCVAR	m_hflucv;

    // Variable for uncalibrated magnetic field.
    int32vec      m_uncalib;
    int32vec      m_bias;

    // Variable for magnetic field.
    int16vec      m_calib;

    // Variables for DecompS3 .
    int16         m_hn;     // Number of acquired data
    int16vec      m_hdata[AKSC_HDATA_SIZE];
	int16		  m_hnave;
	int16vec	  m_dvec;
    int16vec      m_hvec;   // Averaged value(offset compensated)
    int16vec      m_have;   // Averaged value
    int16vec      m_asa;
    uint8         m_pdc[PDC_SIZE];
    uint8         *m_pdcptr;


#ifndef AKMD_DISABLE_DOEPLUS
	// Variables for DOEPlus.
	AKSC_DOEPVAR	*m_doep_var;
	int16	        m_doep_lv;
	AKSC_FLOAT      DOEP_PRMS[CSPEC_NUM_FORMATION][AKSC_DOEP_SIZE];
	int16vec        m_hdata_plus[AKSC_HDATA_SIZE];
#endif

#ifdef AKMD_DOEEx
	// Variables for DOEEx.
	AKSC_DOEEXVAR	*m_doeex_var;
#endif

	// Variables for HDOE.
	AKSC_HDOEVAR	m_hdoev;
	AKSC_HDST		m_hdst;
	AKSC_HDST		HSUC_HDST[CSPEC_NUM_FORMATION];
#ifdef AKMD_DOEaG
    // Variables for HDOEaG.
    AKSC_DOEAGVAR *m_doeag_var;
    int16         m_mode;
	int16		  m_oedt;
#endif
    int16         m_hdt_ag;
    int16         m_gdt_ag;
	int16		  m_magIniUpdateFlag;

	// Variables for formation change
	int16		m_form;
	int16		m_cntSuspend;

	// Certification
	uint8		m_licenser[AKSC_CI_MAX_CHARSIZE+1];	//end with '\0'
	uint8		m_licensee[AKSC_CI_MAX_CHARSIZE+1];	//end with '\0'
	int16		m_key[AKSC_CI_MAX_KEYSIZE];

#ifdef AKMD_DOEaG
    // Variables for gyroscope sensor.
    int16vec      m_GO;
    int16vec      m_gvec;
    int16vec      m_gdata[AKSC_GDATA_SIZE];
#endif
    // Layout information
    AKMD_PATNO    m_mag_patno;
    I16MATRIX     m_hlayout;
    I16MATRIX     m_alayout;
    I16MATRIX     m_glayout;

    // Variables for measurement interval
    int16         m_hdt;
    int16         m_adt;
    int16         m_gdt;

	// base
	int32vec	m_hbase;
	int32vec	HSUC_HBASE[CSPEC_NUM_FORMATION];

    // For Qualcomm
	int16			m_maxFormNum;

	// Special variables for SAM
	int16		m_curForm;

    // Variables for Direction.
    int16		m_d6dRet;
    int16		m_theta;
    int16		m_delta;
    int16		m_hr;
    int16		m_hrhoriz;
    int16		m_ar;
    int16		m_phi180;
    int16		m_phi90;
    int16		m_eta180;
    int16		m_eta90;
    I16MATRIX	m_mat;
    I16QUAT		m_quat;

	// Variables for acceleration sensor.
	int16vec	m_avec;

	// Variables for decimation.
	int16		m_callcnt;

#ifdef AKMD_ENABLE_PG
	// PseudoGyro
#ifndef AKMD_PG_FLOAT_POINTS
	int16			m_pgRet;
	int16			m_pgdt;
	AKPG_COND		m_pgcond;
	AKPG_VAR		m_pgvar;
	int32vec		m_pgout;
	I16QUAT			m_pgquat;
	int16vec		m_pgGravity;
	int16vec		m_pgLinAcc;
	int16			PG_filter;
#else
	int16			m_pgRet;
	int16			m_pgdt;
	AKPG_COND		m_pgcond;
	AKPG_VAR		m_pgvar;
	AKSC_FVEC		m_adataPG;
	AKSC_FVEC		m_hdataPG;
	AKSC_FVEC       m_pgout;
	AKSC_FVEC       m_pgGravity;
	AKSC_FVEC       m_pgLinAcc;
	AKSC_FQUAT		m_pgquat;
	int16			PG_filter;
#endif
#endif
} AKSCPRMS;


/*** Global variables *********************************************************/

/*** Prototype of function ****************************************************/

#endif //AKMD_INC_AKCOMPASS_H

