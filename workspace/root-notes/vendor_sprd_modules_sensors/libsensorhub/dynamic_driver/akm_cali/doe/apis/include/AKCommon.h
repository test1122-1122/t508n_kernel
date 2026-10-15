/******************************************************************************
 *
 * $Id: //source/qcom/qct/core/sensors/dsps/apps/common/main/latest/sam/algo/mag_cal/src/AKM8963/AKCommon.h#1 $
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
#ifndef AKMD_INC_AKCOMMON_H
#define AKMD_INC_AKCOMMON_H

#ifdef WIN32
#include "Android.h"
#else

#include <stdio.h>     //frpintf
#include <stdlib.h>    //atoi
#include <string.h>    //memset
//#include <unistd.h>
#include <stdarg.h>    //va_list
#include <errno.h>     //errno
//#include <sys/stat.h>  //chmod (in FileIO)
//#include <utils/Log.h> //LOGV
#endif

#include "Extension.h"
#include "CustomerSpec.h"


/*** Constant definition ******************************************************/
#if defined(AKMD_AK8975)
#define AKMD_BDATA_SIZE     8
#define AKMD_WIA            0x48
#define AKMD_ASA            0x80
#define AKMD_ST2            0x00
#define CONVERT_TO_AKM      (0.3f)    /* From android to AKM */
#elif defined(AKMD_AK8963)
#define AKMD_BDATA_SIZE     8
#define AKMD_WIA            0x48
#define AKMD_ASA            0x80
#define AKMD_ST2            0x10
#define CONVERT_TO_AKM      (0.15f)    /* From android to AKM */
#elif defined(AKMD_AK09912)
#define AKMD_BDATA_SIZE     9
#define AKMD_WIA            0x448
#define AKMD_ASA            0x80
#define AKMD_ST2            0x00
#define CONVERT_TO_AKM      (0.15f)    /* From android to AKM */
#elif defined(AKMD_AK09911)
#define AKMD_BDATA_SIZE     9
#define AKMD_WIA            0x548
#define AKMD_ASA            0x80
#define AKMD_ST2            0x00
#define CONVERT_TO_AKM      (0.6f)  /* From android to AKM (100 / 65536 / 0.6) */
#elif defined(AKMD_AK09916)
#define AKMD_BDATA_SIZE		10
#define AKMD_WIA			0x948
#define AKMD_ASA			0x80
#define AKMD_ST2			0x00
#define CONVERT_TO_AKM      (15*65536/100)  /* From android to AKM (100 / 65536 / 0.15) */
#endif

#define CONVERT_Q6(x)       ((int32_t)((x) * 64))

#define AKM_GYRO_CONVERT    (16 * RAD2DEG)
#define CONVERT_TO_ANDROID  (0.06f)

#if defined (AKMD_AK09911)||(AKMD_AK09912)
#define AKMD_ST2_POS 8
#else
#define AKMD_ST2_POS 7
#endif

/*** Constant definition ******************************************************/
#define AKRET_PROC_SUCCEED      0x00    /*!< The process has been successfully done. */
#define AKRET_FORMATION_CHANGED 0x01    /*!< The formation is changed */
#define AKRET_DATA_READERROR    0x02    /*!< Data read error occurred. */
#define AKRET_DATA_OVERFLOW     0x04    /*!< Data overflow occurred. */
#define AKRET_OFFSET_OVERFLOW   0x08    /*!< Offset values overflow. */
#define AKRET_HBASE_CHANGED     0x10    /*!< hbase was changed. */
#define AKRET_HFLUC_OCCURRED    0x20    /*!< A magnetic field fluctuation occurred. */
#define AKRET_VNORM_ERROR       0x40    /*!< AKSC_VNorm error. */
#define AKRET_PROC_FAIL         0x80    /*!< The process failes. */

#define AKMD_MAG_MIN_INTERVAL   10000000    /*!< Minimum magnetometer interval */
#define AKMD_LOOP_MARGIN        3000000    /*!< Minimum sleep time */
#define AKMD_SETTING_INTERVAL   500000000    /*!< Setting event interval */

// 1/64
#define DISP_CONV_Q6F(val)      ((val)*0.015625f)

// format
#define AKM_GYR_FMT             16.f

#define AKM_CONVERT_G           (1/9.80665f)
#define AKM_ACC_FMT             720.0f

/* deg x (pi/180.0) */
#define DEG2RAD                 (AKSC_PI / 180.f)
#define RAD2DEG                 (1/DEG2RAD)
#define AKSC2SI(x)              ((AKSC_FLOAT)(((x) * 9.80665f) / 720.0))

#define GAUSS2UT                (100.f)

#define AKMAPI_SUCCESS          1

#define AKMD_PRMS_INIT          1
#define AKMD_PRMS_NOT_INIT      0

#define AKMD_MAG_MINVAL        -3219128
#define AKMD_MAG_MAXVAL        3219128

/*** Constant definition ******************************************************/
#undef LOG_TAG
#define LOG_TAG "AKMD2"

#define DBG_LEVEL0    0    // Critical
#define DBG_LEVEL1    1    // Notice
#define DBG_LEVEL2    2    // Information
#define DBG_LEVEL3    3    // Debug
#define DBG_LEVEL4    4    // Verbose

#ifndef DBG_LEVEL
#define DBG_LEVEL    DBG_LEVEL2
#endif

#define DATA_AREA01    0x0001
#define DATA_AREA02    0x0002
#define DATA_AREA03    0x0004
#define DATA_AREA04    0x0008
#define DATA_AREA05    0x0010
#define DATA_AREA06    0x0020
#define DATA_AREA07    0x0040
#define DATA_AREA08    0x0080
#define DATA_AREA09    0x0100
#define DATA_AREA10    0x0200
#define DATA_AREA11    0x0400
#define DATA_AREA12    0x0800
#define DATA_AREA13    0x1000
#define DATA_AREA14    0x2000
#define DATA_AREA15    0x4000
#define DATA_AREA16    0x8000


/* Debug area definition */
#define AKMDATA_BDATA       DATA_AREA01    /*<! AK8963's BDATA */
#define AKMDATA_AVEC        DATA_AREA02    /*<! Acceleration data */
#define AKMDATA_EXECTIME    DATA_AREA03    /*<! Time of each loop cycle */
#define AKMDATA_EXECFLAG    DATA_AREA04    /*<! Execution flags */
#define AKMDATA_MAGDRV      DATA_AREA05    /*<! AK8963 driver's data */
#define AKMDATA_ACCDRV      DATA_AREA06    /*<! Acceleration driver's data */
#define AKMDATA_GETINTERVAL DATA_AREA07    /*<! Interval */
#define AKMDATA_D6D         DATA_AREA08 /*<! Direction6D */

#ifndef ENABLE_AKMDEBUG
#define ENABLE_AKMDEBUG        1    /* Eanble debug output when it is 1. */
#endif

#ifndef OUTPUT_STDOUT
#define OUTPUT_STDOUT        0    /* Output to stdout when it is 1. */
#endif

#ifdef __GNUC__
#  define UNUSED(x) UNUSED_ ## x __attribute__((__unused__))
#else
#  define UNUSED(x) UNUSED_ ## x
#endif

#ifdef __GNUC__
#  define UNUSED_FUNCTION(x) __attribute__((__unused__)) UNUSED_ ## x
#else
#  define UNUSED_FUNCTION(x) UNUSED_ ## x
#endif

/***** Debug output ******************************************/
#if ENABLE_AKMDEBUG
#if OUTPUT_STDOUT
#define AKMDEBUG(level, format, ...) \
    (((level) <= DBG_LEVEL) \
      ? (fprintf(stdout, (format), ##__VA_ARGS__)) \
      : ((void)0))
#else
#define AKMDEBUG(level, format, ...) \
    //ALOGD_IF(((level) <= DBG_LEVEL), (format), ##__VA_ARGS__)
    //ALOGD_IF(((level) <= DBG_LEVEL), ("%s:%d,"##format), __FUNCTION__, __LINE__, ##__VA_ARGS__)
#endif
#else
#define AKMDEBUG(level, format, ...)
#endif

/***** Dbg Area Output ***************************************/
#if ENABLE_AKMDEBUG
#define AKMDATA(flag, format, ...)  \
    ((((int)flag) & g_dbgzone) \
      ? (fprintf(stdout, (format), ##__VA_ARGS__)) \
      : ((void)0))
#else
#define AKMDATA(flag, format, ...)
#endif

/***** Data output *******************************************/
#if OUTPUT_STDOUT
#define AKMDUMP(format, ...) \
    fprintf(stderr, (format), ##__VA_ARGS__)
#else
#define AKMDUMP(format, ...) \
    ALOGD((format), ##__VA_ARGS__)
#endif

/***** Log output ********************************************/  
#ifdef AKM_LOG_ENABLE
#define AKM_LOG(format, ...)    ALOGD((format), ##__VA_ARGS__)
#else
#define AKM_LOG(format, ...)
#endif

/***** Error output *******************************************/
#if OUTPUT_STDOUT
#define AKMERROR \
      fprintf(stderr, "%s:%d Error.\n", __FUNCTION__, __LINE__)
#else
#define AKMERROR \
      //ALOGE("%s:%d Error.", __FUNCTION__, __LINE__)
#endif

/*** Type declaration *********************************************************/

/*** Prototype of function ****************************************************/

#endif //AKMD_INC_AKCOMMON_H

