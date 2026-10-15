/******************************************************************************
 *
 * $Id: //source/qcom/qct/core/sensors/dsps/apps/common/main/latest/sam/algo/mag_cal/src/AKM8963/CustomerSpec.h#1 $
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
#ifndef AKMD_INC_CUSTOMERSPEC_H
#define AKMD_INC_CUSTOMERSPEC_H

/*******************************************************************************
 User defines parameters.
 ******************************************************************************/

// Certification information
#define CSPEC_CI_LICENSER	"ASAHIKASEI"
#if defined(__LP64__)
#define CSPEC_CI_LICENSEE	"WT_11_AG_16_64"
#else
#define CSPEC_CI_LICENSEE	"SPRD_11_V3_NOS"
#endif

#define AKMD_AK09911
#define AKMD_DOE
#define AKMD_DISABLE_DOEPLUS
//#define AKMD_ENABLE_ORIENTATION
//#define AKMD_ENABLE_PG
//#define AKMD_PG_FLOAT_POINTS

#define CSPEC_PG_FILTER     0 // 0 - 7

#define CSPEC_SETTING_FILE	"/data/misc/akmd_set.txt"
#define CSPEC_PDC_FILE		"/system/vendor/etc/pdc.txt"

// Parameters for Average
// Variable that indicates how many gyroscope data are averaged.
// gNave must be 1, 2, 4, 8, 16, 32, 64.
#define CSPEC_GNAVE             64

// Parameters for Average
// The number of magnetic data to be averaged.
// CSPEC_HNAVE must be 1, 2, 4, 8, 16 or 32.
#define CSPEC_HNAVE		8

// Parameters for Direction Calculation
#define CSPEC_DVEC_X		0
#define CSPEC_DVEC_Y		0
#define CSPEC_DVEC_Z		0

//
#define CSPEC_HLAYOUT00		1
#define CSPEC_HLAYOUT01		0
#define CSPEC_HLAYOUT02		0
#define CSPEC_HLAYOUT10		0
#define CSPEC_HLAYOUT11		1
#define CSPEC_HLAYOUT12		0
#define CSPEC_HLAYOUT20		0
#define CSPEC_HLAYOUT21		0
#define CSPEC_HLAYOUT22		1

#define CSPEC_GLAYOUT00		1
#define CSPEC_GLAYOUT01		0
#define CSPEC_GLAYOUT02		0
#define CSPEC_GLAYOUT10		0
#define CSPEC_GLAYOUT11		1
#define CSPEC_GLAYOUT12		0
#define CSPEC_GLAYOUT20		0
#define CSPEC_GLAYOUT21		0
#define CSPEC_GLAYOUT22		1

// The number of formation
#define CSPEC_NUM_FORMATION		1

/* the counter of Suspend */
#define CSPEC_CNTSUSPEND_SNG	8

// Parameters for FST
//  1 : USE SPI
//  0 : NOT USE SPI(I2C)
#define CSPEC_SPI_USE			0

//Parameters for Temperature sensor (specific function of AK09912)
//	0x00 : disable
//	0x80 : enable
#define CSPEC_TEMPERATURE		0x80

//Parameters for Noise Suppression Filter (specific function of AK09912)
//	0x00 : disable
//	0x20 : Low
//	0x40 : Middle
//	0x60 : High
// #define CSPEC_NSF				0x40

#ifdef AKMD_DOEaG
#define CSPEC_DOEAG_MODE        0 // 0->DOEaG, 1->DOE
// Since recommendation is 1600 ideally, but we need to consider timer jitter 10%.
#define CSPEC_DOEAG_OEDT		1440
#endif

#endif //AKMD_INC_CUSTOMERSPEC_H

