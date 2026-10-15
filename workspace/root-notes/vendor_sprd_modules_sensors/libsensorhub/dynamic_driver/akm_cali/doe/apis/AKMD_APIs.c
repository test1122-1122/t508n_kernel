/******************************************************************************
 *
 *  $Id: //source/qcom/qct/core/sensors/dsps/apps/common/main/latest/sam/algo/mag_cal/src/AKM8963/AKMD_APIs.c#2 $
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
#include "AKMD_APIs.h"

#include "ParameterIO.h"
#include "SensorsData.h"
#include "LibAPIs.h"
#include "Measure.h"
#ifdef AKMD_DOEaG
#include "../libSmartCompass/Private_AKHDOEaG.h"
#endif
// 0: NOT initialized,  1:Initialized
extern int16 g_init;
// 0: Initial value.
// The maximum value will be specified when AKMD_Init() is called.
extern int16 g_form;
extern uint8 g_prms[2136];

extern unsigned int mag_debug_data[10];


/******************************************************************************/
/*    STATIC Function                                                           */
/******************************************************************************/
/*! Get current formation.
 @return Form factor number.
 */
int16 getFormation(void)
{
    return g_form;
}

/******************************************************************************/
/*! Set license key for SmartCompass cetification.
\return AKMD_SUCCESS on success. AKMD_ERROR if an error occured.
\param[in] regs Register values which are obtained from AKM device.
\param[out] prms The member variable of AKSCPRMS structure (m_key, m_licenser and
 m_licensee) will be filled with license information.
\param[in] licensee Magic phrase.
*/
static void SetLicenseKey(
    const    int16    regs[4],
            AKSCPRMS    *prms,
    const    char    *licensee
)
{
    // Set keywords for SmartCompassLibrary certification
    prms->m_key[0] = AKSC_GetVersion_Device();
    prms->m_key[1] = regs[0];
    prms->m_key[2] = regs[1];
    prms->m_key[3] = regs[2];
    prms->m_key[4] = regs[3];
    strncpy((char*)prms->m_licenser, CSPEC_CI_LICENSER, AKSC_CI_MAX_CHARSIZE);
    prms->m_licenser[AKSC_CI_MAX_CHARSIZE] = '\0';
    strncpy((char*)prms->m_licensee, licensee, AKSC_CI_MAX_CHARSIZE);
    prms->m_licensee[AKSC_CI_MAX_CHARSIZE] = '\0';
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wsign-conversion"
/*! Get size of AKSCPRMS
 */
static size_t GetSizeOfAKSCPRMS(void)
{
    size_t sz;
    sz = sizeof(AKSCPRMS);
    if ((sz & 0x3) != 0) {
        sz = (sz & (~0x3)) + 4;
    }
    return sz;
}
#pragma GCC diagnostic pop

/*! Get size of AKSCPRMS
 */
static size_t AKMD_getSizeOfBuffer(void)
{
    size_t sz;

    sz = GetSizeOfAKSCPRMS();
    sz += LibAPIs_GetBufferSize();

#ifndef AKMD_DISABLE_DOEPLUS
    sz += (size_t)((uint16)AKSC_GetSizeDOEPVar()*sizeof(int32));
#endif
    return sz;
}

/******************************************************************************/
/*    PUBLIC Function                                                           */
/******************************************************************************/
/*! Initialize AKSCPRMS structure. This function must be called before
application uses any APIs in this file.  If AKSCPRMS are already initialized,
this function discards all existing data.  When APIs are not used anymore,
AKMD_Release function must be called at the end.  When this function succeeds,
form is set to 0.
\return AKMD_SUCCESS on success. AKMD_ERROR if an error occured.
\param[in] maxFormNum The maximum number of form of this device. This
number should be from 1 to CSPEC_NUM_FORMATION.
\param[in] regs Specify pointer to a uint8_t array.  The array should be filled
with the values which are acquired from AK8963 device.
*/
int16_t AKMD_Init(float offset[],
        int accuracy,
        uint8 pdc_prms[]
)
{
    int16 regs[4];
    int16 maxFormNum = CSPEC_NUM_FORMATION;
    // Parameters for calculation.
    AKSCPRMS *prms = (AKSCPRMS *)g_prms;
    int i;

    AKMDEBUG(DBG_LEVEL3, "AKSCPRMS init begin.\n");

    // Clear allocated memory at first.
    AKMD_Release();

#ifdef AKMD_VALUE_CHECK
    // Check the range of arguments
    if((maxFormNum <= 0)||(CSPEC_NUM_FORMATION < maxFormNum)){
        AKMDEBUG(DBG_LEVEL0, "AKSCPRMS init failed.\n");
        return AKMD_ERROR;
    }
#endif

	mag_debug_data[0] = sizeof(g_prms);
	mag_debug_data[1] = AKMD_getSizeOfBuffer();
    // Clear all data
    if (mag_debug_data[0] < mag_debug_data[1]) {
        AKMERROR;
        AKMDEBUG(DBG_LEVEL0, "AKSCPRMS buffer size if not enough.\n");
        return AKMD_ERROR;
    }
    memset(prms, 0, AKMD_getSizeOfBuffer());

    /* Set parameter pointer. */
    LibAPIs_SetPrmsPointer(prms, GetSizeOfAKSCPRMS());

    regs[0] = AKMD_WIA;
    regs[1] = AKMD_ASA;
    regs[2] = AKMD_ASA;
    regs[3] = AKMD_ASA;

    // Copy current value
    prms->m_maxFormNum = maxFormNum;

    // Initialize AKSCPRMS structure.
    InitAKSCPRMS(prms);

    // Additional settings
    prms->m_asa.u.x = regs[1];
    prms->m_asa.u.y = regs[2];
    prms->m_asa.u.z = regs[3];

    // Initialize the certification key.
    SetLicenseKey(regs, prms, CSPEC_CI_LICENSEE);

    for(i=0; i<CSPEC_NUM_FORMATION; i++){
        prms->HSUC_HDST[i] = accuracy;
        prms->HSUC_HO[i].u.x = (int16)(offset[0] / CONVERT_TO_ANDROID);
        prms->HSUC_HO[i].u.y = (int16)(offset[1] / CONVERT_TO_ANDROID);
        prms->HSUC_HO[i].u.z = (int16)(offset[2] / CONVERT_TO_ANDROID);
        prms->HFLUCV_HREF[i].u.x = prms->HSUC_HO[i].u.x;
        prms->HFLUCV_HREF[i].u.y = prms->HSUC_HO[i].u.y;
        prms->HFLUCV_HREF[i].u.z = prms->HSUC_HO[i].u.z;
        prms->HSUC_HBASE[i].u.x = 0;
        prms->HSUC_HBASE[i].u.y = 0;
        prms->HSUC_HBASE[i].u.z = 0;
    }
    // Save parameters after init.
    SaveParameters(prms);

    //SetPDC(prms, pdc_prms);

#ifdef AKMD_ENABLE_PG
#ifdef AKMD_PG_FLOAT_POINTS
    prms->PG_filter = CSPEC_PG_FILTER;
#endif
#endif

    // Set flag
    g_init = AKMD_PRMS_INIT;

    AKMDEBUG(DBG_LEVEL3, "AKSCPRMS init done\n");

    return AKMD_SUCCESS;
}

/******************************************************************************/
/*! Release allocated memory. This function must be called at the end of using
APIs.
\return None
*/
void AKMD_Release(void)
{
    AKSCPRMS *prms = (AKSCPRMS *)g_prms;
    if(g_init != AKMD_PRMS_NOT_INIT){
        // Save prameters
        AKMD_Stop();

        // Clear all data
        memset(prms, 0, AKMD_getSizeOfBuffer());

        // Put invalid number for safety.
        prms->m_maxFormNum = 0;

        // Set flag
        g_init = AKMD_PRMS_NOT_INIT;
    }
}

/******************************************************************************/
/*! Load parameters from a file and initialize SmartCompass library. This
function must be called when a sequential measurement starts.
\return AKMD_SUCCESS on success. AKMD_ERROR if an error occured.
\param[in] path The path to a setting file to be read out. The path name should
be terminated with NULL.
*/
int16_t  AKMD_Start(void)
{
    AKSCPRMS *prms = (AKSCPRMS *)g_prms;

#ifdef AKMD_VALUE_CHECK
    if(g_init == AKMD_PRMS_NOT_INIT){
        //ALOGE("AKMD_Start: PRMS are not initialized.");
        return AKMD_ERROR;
    }
#endif

    // Load parameters
    LoadParameters(prms);
    prms->m_mag_patno = PAT1;//layout value
    // Init SmartCompass library functions.
    if(Init_Measure(prms) != AKRET_PROC_SUCCEED){
        //ALOGE("AKMD_Start: Start error.");
        return AKMD_ERROR;
    }

    AKMDEBUG(DBG_LEVEL0, "AK%d. Library Version=%d.%d.%d.%d\n",
            AKSC_GetVersion_Device(),
            AKSC_GetVersion_Major(),
            AKSC_GetVersion_Minor(),
            AKSC_GetVersion_Revision(),
            AKSC_GetVersion_DateCode());
    AKMDEBUG(DBG_LEVEL0, "LICENSEE = %s\n", CSPEC_CI_LICENSEE);

    return AKMD_SUCCESS;
}

/******************************************************************************/
/*! Save parameters to a file. This function must be called when a sequential
measurement ends.
\return AKMD_SUCCESS on success. AKMD_ERROR if an error occured.
\param[in] path The path to a setting file to be written. The path name should
be terminated with NULL.
*/
int16_t AKMD_Stop(void)
{
    AKSCPRMS *prms = (AKSCPRMS *)g_prms;

#ifdef AKMD_VALUE_CHECK
    if(g_init == AKMD_PRMS_NOT_INIT){
        //ALOGE("AKMD_Start: PRMS are not initialized.");
        return AKMD_ERROR;
    }
#endif

    // Save parameters
    if(SaveParameters(prms) == 0){
        //ALOGE("AKMD_Stop: Setting file cannot be written.");
        return AKMD_ERROR;
    }

    return AKMD_SUCCESS;
}

/******************************************************************************/
/*! It is used to calculate offset from the sensor value. This function convert
from X,Y,Z data format to SmartCompass "bData" format.
\return AKMD_SUCCESS on success. AKMD_ERROR if an error occurred.
\param[in] ihx: X-axis value Gauss in Q16 format
\param[in] ihy: Y-axis value Gauss
\param[in] ihz: Z-axis value Gauss
\param[in] ihtstmp: timestamp of magnetometer data, in nano second unit
\param[in] ihvalid: flag indicating if mag data is valid (0 = invalid, 1 = valid)
\param[in] igx: gyro X-axis value, rad/sec in Q16 format
\param[in] igy: gyro Y-axis value, rad/sec in Q16 format
\param[in] igz: gyro Z-axis value, rad/sec in Q16 format
\param[in] igtstmp: timestamp of gyro data, in nano second unit
\param[in] igvalid: flag indicating if gyro data is valid (0 = invalid, 1 = valid)
\param[in] igworking: flag indicating if gyro is working (0 = not working, 1 = working)
\param[in] st2: Specify the ST2 register value.
\param[in] freq: Measurement interval in Hz.
\param[out] ox: X-axis value of offset-subtracted magnetic field, uT unit
\param[out] oy: Y-axis value of offset-subtracted magnetic field, uT unit
\param[out] oz: Z-axis value of offset-subtracted magnetic field, uT unit
\param[out] oxbias: X-axis value of offset magnetic field, uT unit
\param[out] oybias: Y-axis value of offset magnetic field, uT unit
\param[out] ozbias: Z-axis value of offset magnetic field, uT unit
\param[out] accuracy: accuracy of offset
*/
int16_t  AKMD_SetData(
    const   int32_t ihx,
    const   int32_t ihy,
    const   int32_t ihz,
    const   int16_t st2,
    const   int16_t ihdt,
    const   unsigned int ihvalid,
    const   float igx,
    const   float igy,
    const   float igz,
    const   int16_t igdt,
    const   unsigned int igvalid,
    const   float iax,
    const   float iay,
    const   float iaz,
    const   int16_t UNUSED(iadt),
    const   unsigned int iavalid
)
{
    AKSCPRMS *prms = (AKSCPRMS *)g_prms;

#ifdef AKMD_VALUE_CHECK
    if (g_init == AKMD_PRMS_NOT_INIT) {
         AKMDEBUG(DBG_LEVEL0, "AKMD_SaveMag: PRMS are not initialized.");
         return AKMD_ERROR;
    }
    if ((ihx < AKMD_MAG_MINVAL) || (AKMD_MAG_MAXVAL < ihx)) {
         AKMDEBUG(DBG_LEVEL0, "AKMD_SaveMag: Invalid ix value %f.", ihx);
         return AKMD_ERROR;
    }
    if ((ihy < AKMD_MAG_MINVAL) || (AKMD_MAG_MAXVAL < ihy)) {
         AKMDEBUG(DBG_LEVEL0, "AKMD_SaveMag: Invalid iy value %f.", ihy);
         return AKMD_ERROR;
    }
    if ((ihz < AKMD_MAG_MINVAL) || (AKMD_MAG_MAXVAL < ihz)) {
        AKMDEBUG(DBG_LEVEL0, "AKMD_SaveMag: Invalid iz value %f.", ihz);
        return AKMD_ERROR;
    }
#endif
#ifdef AKMD_DOEaG
    // set gyro data
    if (igvalid == 1) {
        AKM_SetGyroData(prms, igx, igy, igz, igdt);
    } else {
        prms->m_gdt_ag = -1;
    }
#endif
    // set mag data
    if (ihvalid == 1) {
        AKM_SetMagData(prms, ihx, ihy, ihz, st2, ihdt);
    } else {
        prms->m_hdt_ag = -1;
    }

    if (iavalid == 1) {
        AKM_SetAccData(prms, iax, iay, iaz);
    }

    return AKMD_SUCCESS;
}

int16_t AKMD_Calibrate(
        float *ox,
        float *oy,
        float *oz,
        float* oxbias,
        float* oybias,
        float* ozbias,
        int8_t* accuracy
)
{
    int16_t ret = AKMD_SUCCESS;

    AKSCPRMS *prms = (AKSCPRMS *)g_prms;
    int8_t    totalHDST;

    // calculate Magnetic offset.
    if ((prms->m_magIniUpdateFlag == 1)&&((prms->m_hdt_ag > 0)||(prms->m_gdt_ag > 0))) {
        ret = GetMagneticVectorOffset(prms);
        // Check the return value
        if ((ret != AKRET_PROC_SUCCEED) && (ret != AKRET_FORMATION_CHANGED)) {
            AKMDEBUG(DBG_LEVEL2, "GetMagneticVectorOffset has failed %d.\n", ret);
        }
    }

#ifndef AKMD_DISABLE_DOEPLUS
    /* Adjust magnetic reliability based on the level of each algorithm */
    if ((prms->m_hdst == 3) && (prms->m_doep_lv <= 2)){
        totalHDST = 2;
    } else if ((prms->m_hdst == 2) && (prms->m_doep_lv <= 1)){
        totalHDST = 1;
    } else {
        totalHDST = prms->m_hdst;
    }
    AKMDEBUG(DBG_LEVEL1, "DOE level: %d, %d, %d\n",
        prms->m_hdst, prms->m_doep_lv, totalHDST);
#else
    totalHDST = prms->m_hdst;
#endif

    // Convert from SmartCompass to Android
    *ox = ((float)prms->m_calib.u.x * CONVERT_TO_ANDROID);
    *oy = ((float)prms->m_calib.u.y * CONVERT_TO_ANDROID);
    *oz = ((float)prms->m_calib.u.z * CONVERT_TO_ANDROID);

    //Bias offset is in uTesla
    *oxbias = ((float)prms->m_bias.u.x * CONVERT_TO_ANDROID);
    *oybias = ((float)prms->m_bias.u.y * CONVERT_TO_ANDROID);
    *ozbias = ((float)prms->m_bias.u.z * CONVERT_TO_ANDROID);

    *accuracy = totalHDST;

#ifdef AKMD_ENABLE_ORIENTATION
    /* Calculate direction */
    if (CalcDirection(prms) != AKRET_PROC_SUCCEED) {
        AKMDEBUG(DBG_LEVEL4, "CalcDirection failed.");
    }
#endif

#ifdef AKMD_ENABLE_PG
    /* Calculate angular rate */
    if (CalcAngularRate(prms) != AKRET_PROC_SUCCEED) {
        AKMDEBUG(DBG_LEVEL4, "CalcAngularRate failed.");
    }
#endif

    return ret;
}

#ifdef AKMD_ENABLE_ORIENTATION
void AKMD_GetDirection(
   float ori_buf[],
   int16_t buf_size,
   int8_t *status
) {
    AKSCPRMS *prms = (AKSCPRMS *)g_prms;
    GetDirection(prms, ori_buf, buf_size, status);

    return;
}
#endif

#ifdef AKMD_ENABLE_PG
void AKMD_PG_Gyro(
    float gyro_buf[],
    const int16_t buf_size,
    int8_t *status
) {
    AKSCPRMS *prms = (AKSCPRMS *)g_prms;
    Get_PG_GyroData(prms, gyro_buf, buf_size, status);

    return;
}

void AKMD_PG_RotationVector(
    float rv_buf[],
    const int16_t buf_size
) {
    AKSCPRMS *prms = (AKSCPRMS *)g_prms;
    Get_PG_RotationVector(prms, rv_buf, buf_size);

    return;
}

void AKMD_PG_Gravity(
    float gravity_buf[],
    const int16_t buf_size
) {
    AKSCPRMS *prms = (AKSCPRMS *)g_prms;
    Get_PG_Gravity(prms, gravity_buf, buf_size);

    return;
}

void AKMD_PG_LA(
    float la_buf[],
    const int16_t buf_size
) {
    AKSCPRMS *prms = (AKSCPRMS *)g_prms;
    Get_PG_LA(prms, la_buf, buf_size);

    return;
}
#endif

/******************************************************************************/
/*! It resets HDOE
\return None
*/
void  AKMD_ResetDOE(void)
{
    AKSCPRMS *prms = (AKSCPRMS *)g_prms;
    LibAPIs_SetLevel(prms, AKSC_HDST_UNSOLVED);
    prms->m_hdst = AKSC_HDST_UNSOLVED;
}

/******************************************************************************/
/*! It is used to change the calibration values in according to the current
form.
\return AKMD_SUCCESS on success. AKMD_ERROR if an error occured.
\param[in] form The number of forum to be used from next API
call. This number should be larger than 0 and less than maxFormNum which is
specified in AKMD_Init function.
*/
int16_t  AKMD_ChangeFormation(int16_t form)
{
    AKSCPRMS *prms = (AKSCPRMS *)g_prms;

#ifdef AKMD_VALUE_CHECK
    if(g_init == AKMD_PRMS_NOT_INIT){
        return AKMD_ERROR;
    }
    if((form < 0) || (prms->m_maxFormNum <= form)){
        return AKMD_ERROR;
    }
#endif
    g_form = form;

    return AKMD_SUCCESS;
}

