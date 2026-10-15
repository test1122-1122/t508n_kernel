
/******************************************************************************
 *
 *  $Id: Measure.c 1071 2013-08-30 06:58:03Z yamada.rj $
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
#include "Measure.h"

/*!
 This function check formation status
 @return The index of formation.
 */
static int16 checkForm(void)
{
#if 0
    if (g_form != NULL) {
        if (g_form->check != NULL) {
            return g_form->check();
        }
    }
#endif
    // If function is not set, return default value.
    return 0;
}

#if 0
/*!
 This function registers the callback function.
 @param[in]
 */
void RegisterFormClass(FORM_CLASS* pt)
{
    g_form = pt;
}
#endif

/*!
 Set initial values for SmartCompass library.
 @return If parameters are read successfully, the return value is
 #AKRET_PROC_SUCCEED. Otherwise the return value is #AKRET_PROC_FAIL. No
 error code is reserved to show which operation has failed.
 @param[in,out] prms A pointer to a #AKSCPRMS structure.
 */
int16 Init_Measure(AKSCPRMS *prms)
{
#if 0
    // Reset device.
    if (AKD_Reset() != AKD_SUCCESS) {
        AKMERROR;
        return AKRET_PROC_FAIL;
    }
#endif
    prms->m_form = checkForm();

    // Restore the value when succeeding in estimating of HOffset.
    prms->m_ho   = prms->HSUC_HO[prms->m_form];
    prms->m_ho32.u.x = (int32_akm)prms->HSUC_HO[prms->m_form].u.x;
    prms->m_ho32.u.y = (int32_akm)prms->HSUC_HO[prms->m_form].u.y;
    prms->m_ho32.u.z = (int32_akm)prms->HSUC_HO[prms->m_form].u.z;

    prms->m_hdst = prms->HSUC_HDST[prms->m_form];
    prms->m_hbase = prms->HSUC_HBASE[prms->m_form];

    // Initialize the decompose parameters
    AKSC_InitDecomp(prms->m_hdata);

#ifdef AKMD_DOEaG
    prms->m_mode = CSPEC_DOEAG_MODE;
    prms->m_oedt = CSPEC_DOEAG_OEDT;
#endif

#ifndef AKMD_DISABLE_DOEPLUS
    // Initialize DOEPlus parameters
    AKSC_InitDOEPlus(prms->m_doep_var);
    prms->m_doep_lv = AKSC_LoadDOEPlus(
                        prms->DOEP_PRMS[prms->m_form],
                        prms->m_doep_var);
    AKSC_InitDecomp(prms->m_hdata_plus);
#endif

    // Initialize core parameters
    LibAPIs_Init(prms);

    AKSC_InitHFlucCheck(
            &(prms->m_hflucv),
            &(prms->HFLUCV_HREF[prms->m_form]),
            HFLUCV_TH
            );

#ifdef AKMD_ENABLE_PG
    AKSC_InitPseudoGyro(
            &prms->m_pgcond,
            &prms->m_pgvar
            );

    PG_InitConfig(prms);
#endif // #ifdef AKMD_ENABLE_PG

    // Reset counter
    prms->m_cntSuspend = 0;
    prms->m_callcnt = 0;

    return AKRET_PROC_SUCCEED;
}

/*!
 SmartCompass main calculation routine. This function will be processed
 when INT pin event is occurred.
 @retval AKRET_
 @param[in] bData An array of register values which holds,
 ST1, HXL, HXH, HYL, HYH, HZL, HZH and ST2 value respectively.
 @param[in,out] prms A pointer to a #AKSCPRMS structure.
 @param[in] curForm The index of hardware position which represents the
 index when this function is called.
 @param[in] hDecimator HDOE will execute once while this function is called
 this number of times.
 */
int16 GetMagneticVectorDecomp(
    const int16 bData[],
    AKSCPRMS    *prms,
    const int16 curForm)
{
    const int16vec hrefZero = {{0, 0, 0}};
    int16       temperature, dor, derr, hofl, cb, dc;
    int32vec    preHbase;
    int16       overflow;
    int16       aksc_ret;
    int16       ret;
#ifndef AKMD_DISABLE_DOEPLUS
    int16       i;
    int16       doep_ret;
#endif

    temperature = 0;
    dor = 0;
    derr = 0;
    hofl = 0;
    cb = 0;
    dc = 0;

    preHbase = prms->m_hbase;
    overflow = 0;
    ret = AKRET_PROC_SUCCEED;

    // Subtract the formation suspend counter
    if (prms->m_cntSuspend > 0) {
        prms->m_cntSuspend--;

        // Check the suspend counter
        if (prms->m_cntSuspend == 0) {
            // Restore the value when succeeding in estimating of HOffset.
            prms->m_ho   = prms->HSUC_HO[prms->m_form];
            prms->m_ho32.u.x = (int32_akm)prms->HSUC_HO[prms->m_form].u.x;
            prms->m_ho32.u.y = (int32_akm)prms->HSUC_HO[prms->m_form].u.y;
            prms->m_ho32.u.z = (int32_akm)prms->HSUC_HO[prms->m_form].u.z;

            prms->m_hdst = prms->HSUC_HDST[prms->m_form];
            prms->m_hbase = prms->HSUC_HBASE[prms->m_form];

            // Initialize the decompose parameters
            AKSC_InitDecomp(prms->m_hdata);

#ifndef AKMD_DISABLE_DOEPLUS
            // Initialize DOEPlus parameters
            AKSC_InitDOEPlus(prms->m_doep_var);
            prms->m_doep_lv = AKSC_LoadDOEPlus(
                  prms->DOEP_PRMS[prms->m_form],
                  prms->m_doep_var);
            AKSC_InitDecomp(prms->m_hdata_plus);
#endif
            // Initialize core parameters parameters
            LibAPIs_Init(prms);

            // Initialize HFlucCheck parameters
            AKSC_InitHFlucCheck(
                    &(prms->m_hflucv),
                    &(prms->HFLUCV_HREF[prms->m_form]),
                    HFLUCV_TH
                );
        }
    }

    // Decompose one block data into each Magnetic sensor's data
    aksc_ret = AKSC_DecompS3(
            AKSC_GetVersion_Device(),
            bData,
            prms->m_hnave,
            &prms->m_asa,
            prms->m_pdcptr,
            prms->m_hdata,
            &prms->m_hbase,
            &prms->m_hn,
            &prms->m_have,
            &temperature,
            &dor,
            &derr,
            &hofl,
            &cb,
            &dc
            );
    /*AKMDEBUG(DBG_LEVEL3, "ST1,ST2=%d,%d", bData[0], bData[AKMD_ST2_POS]);
    AKMDEBUG(DBG_LEVEL3, "HXH&HXL,HYH&HYL,HZH&HZL=%d,%d,%d",
            (int16)(((uint16)bData[2])<<8|bData[1]),
            (int16)(((uint16)bData[4])<<8|bData[3]),
            (int16)(((uint16)bData[6])<<8|bData[5]));
    AKMDEBUG(DBG_LEVEL3, "hdata[0].u.x, hdata[0].u.y, hdata[0].u.z=%d,%d,%d",
            prms->m_hdata[0].u.x, prms->m_hdata[0].u.y, prms->m_hdata[0].u.z);
    AKMDEBUG(DBG_LEVEL3, "asax,asay,asaz=%d,%d,%d", prms->m_asa.u.x, prms->m_asa.u.y, prms->m_asa.u.z);*/

    /*AKMDEBUG(DBG_LEVEL3, "%s: ST1, HXH&HXL, HYH&HYL, HZH&HZL, ST2,"
            " hdata[0].u.x, hdata[0].u.y, hdata[0].u.z,"
            " asax, asay, asaz ="
            " %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d\n",
            __FUNCTION__,
            bData[0],
            (int16)(((uint16)bData[2])<<8|bData[1]),
            (int16)(((uint16)bData[4])<<8|bData[3]),
            (int16)(((uint16)bData[6])<<8|bData[5]), bData[AKMD_ST2_POS],
            prms->m_hdata[0].u.x, prms->m_hdata[0].u.y, prms->m_hdata[0].u.z,
            prms->m_asa.u.x, prms->m_asa.u.y, prms->m_asa.u.z);*/
	//printf("akscret=%d\r\n",aksc_ret);
				     /*  printf(
                 "ST1,HX,Y,Z,ST2,"
                 "hdx,y,z,"
                 "asax,y,z,"
								 "derr="
                 " %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d ,%d\r\n",
                  bData[0],
            (int16)(((uint16)bData[2])<<8|bData[1]),
            (int16)(((uint16)bData[4])<<8|bData[3]),
            (int16)(((uint16)bData[6])<<8|bData[5]), bData[8],
            prms->m_hdata[0].u.x, prms->m_hdata[0].u.y, prms->m_hdata[0].u.z,
            prms->m_asa.u.x, prms->m_asa.u.y, prms->m_asa.u.z,derr);*/
    if (aksc_ret == 0) {
        AKMERROR;
        AKMDEBUG(DBG_LEVEL1, "AKSC_DecompS3 failed\n");
        AKMDEBUG(DBG_LEVEL3,
          "AKSC_DecompS3 failed.\n"
          "  ST1=0x%02X, ST2=0x%02X\n"
          "  XYZ(HEX)=%02X,%02X,%02X,%02X,%02X,%02X\n"
          "  asa(dec)=%d,%d,%d\n"
          "  pdc(addr)=0x%p\n"
          "  hbase(dec)=%ld,%ld,%ld\n",
          bData[0], bData[AKMD_ST2_POS],
          bData[1], bData[2], bData[3], bData[4], bData[5], bData[6],
          prms->m_asa.u.x, prms->m_asa.u.y, prms->m_asa.u.z,
          prms->m_pdcptr,
          prms->m_hbase.u.x, prms->m_hbase.u.y, prms->m_hbase.u.z);
			       /*printf(
                 "AKSC_DecompS3 failed.\n"
                 "AKSC_GetVersion_Device = %d  ST1=0x%02X\n"
                 "  XYZ(HEX)=%d,%d,%d,%d,%d,%d,b7=%d,ST2= %d,b9=%d\n"
                 "  asa(dec)=%d,%d,%d\n"
                 "  pdc(addr)=0x%p\n"
                 "  hbase(dec)=%d,%d,%d\r\n",
                 AKSC_GetVersion_Device(),bData[0], 
                 bData[1], bData[2], bData[3], bData[4], bData[5], bData[6], bData[7],bData[8],bData[9],
                 prms->m_asa.u.x, prms->m_asa.u.y, prms->m_asa.u.z,
                 prms->m_pdcptr,
                 prms->m_hbase.u.x, prms->m_hbase.u.y, prms->m_hbase.u.z);*/
        return AKRET_PROC_FAIL;
    }

    // Check the formation change
    if (prms->m_form != curForm) {
        prms->m_form = curForm;
        prms->m_cntSuspend = CSPEC_CNTSUSPEND_SNG;
        ret |= AKRET_FORMATION_CHANGED;
        return ret;
    }

    // Check derr
    if (derr == 1) {
        ret |= AKRET_DATA_READERROR;
        return ret;
    }

    // Check hofl
    if (hofl == 1) {
        if (prms->m_cntSuspend <= 0) {
            // Set a HDOEEx level as "HDST_UNSOLVED"
            LibAPIs_SetLevel(prms, AKSC_HDST_UNSOLVED);
            prms->m_hdst = AKSC_HDST_UNSOLVED;
        }
        ret |= AKRET_DATA_OVERFLOW;
        return ret;
    }

    // Check hbase
    if (cb == 1) {
        // Translate HOffset
        AKSC_TransByHbase(
                &(preHbase),
                &(prms->m_hbase),
                &(prms->m_ho),
                &(prms->m_ho32),
                &overflow
            );
        if (overflow == 1) {
            ret |= AKRET_OFFSET_OVERFLOW;
        }

        // Set hflucv.href to 0
        AKSC_InitHFlucCheck(
                &(prms->m_hflucv),
                &(hrefZero),
                HFLUCV_TH
            );

        if (prms->m_cntSuspend <= 0) {
            // Set a HDOEEx level as "HDST_UNSOLVED"
            LibAPIs_SetLevel(prms, AKSC_HDST_UNSOLVED);
            prms->m_hdst = AKSC_HDST_UNSOLVED;
        }

        ret |= AKRET_HBASE_CHANGED;
        return ret;
    }

    return AKRET_PROC_SUCCEED;
}

int16 GetMagneticVectorOffset(AKSCPRMS *prms)
{
    int16    hdSucc, hfluc, aksc_ret;
    int16vec hvec;
    int16    ret = AKRET_PROC_SUCCEED;
#ifndef AKMD_DISABLE_DOEPLUS
    int      i;
    int16    doep_ret;
#endif
    if (prms->m_cntSuspend <= 0) {
        // Detect a fluctuation of magnetic field.
        hfluc = AKSC_HFlucCheck(&(prms->m_hflucv), &(prms->m_hdata[0]));
        if (hfluc == 1) {
            // Set a HDOEEx level as "HDST_UNSOLVED"
            LibAPIs_SetLevel(prms, AKSC_HDST_UNSOLVED);
            prms->m_hdst = AKSC_HDST_UNSOLVED;
            AKMDEBUG(DBG_LEVEL1, "%s:%d, hfluc return AKRET_HFLUC_OCCURRED\n", __FUNCTION__, __LINE__);

            ret |= AKRET_HFLUC_OCCURRED;
            return ret;
        } else {
#ifndef AKMD_DISABLE_DOEPLUS
                // Compensate Magnetic Distortion by DOEPlus
                doep_ret = AKSC_DOEPlus(&prms->m_hdata[0], prms->m_doep_var, &prms->m_doep_lv);

                // Save DOEPlus parameters
                if ((doep_ret == 1) && (prms->m_doep_lv == 3)) {
                    AKSC_SaveDOEPlus(prms->m_doep_var, prms->DOEP_PRMS[prms->m_form]);
                }

                // Calculate compensated vector for DOE
                for(i = 0; i < prms->m_hn; i++) {
                    AKSC_DOEPlus_DistCompen(&prms->m_hdata[i], prms->m_doep_var, &prms->m_hdata_plus[i]);
                }

                AKMDEBUG(DBG_LEVEL4,"DOEP: %7d, %7d, %7d ",
                        prms->m_hdata[0].u.x,
                        prms->m_hdata[0].u.y,
                        prms->m_hdata[0].u.z);
                AKMDEBUG(DBG_LEVEL4,"|%7d, %7d, %7d \n",
                        prms->m_hdata_plus[0].u.x,
                        prms->m_hdata_plus[0].u.y,
                        prms->m_hdata_plus[0].u.z);

                //Calculate Magnetic sensor's offset by DOEEx
                hdSucc = LibAPIs_ExeCalib(prms);
#else
                //Calculate Magnetic sensor's offset by DOEEx
                hdSucc = LibAPIs_ExeCalib(prms);
#endif
                if (hdSucc == AKSC_CERTIFICATION_DENIED) {
                    AKMERROR;
                    AKMDEBUG(DBG_LEVEL0,"AKSC_CERTIFICATION_DENIED\n");
                    return AKRET_PROC_FAIL;
                }
                //    if (hdSucc > 0) {
                prms->HSUC_HO[prms->m_form] = prms->m_ho;
                prms->m_ho32.u.x = (int32_akm)prms->m_ho.u.x;
                prms->m_ho32.u.y = (int32_akm)prms->m_ho.u.y;
                prms->m_ho32.u.z = (int32_akm)prms->m_ho.u.z;

                prms->HSUC_HDST[prms->m_form] = prms->m_hdst;
                prms->HFLUCV_HREF[prms->m_form] = prms->m_hflucv.href;
                prms->HSUC_HBASE[prms->m_form] = prms->m_hbase;
                //        }
            }
    }

#ifndef AKMD_DISABLE_DOEPLUS
    // Calculate compensated vector
    AKSC_DOEPlus_DistCompen(&prms->m_have, prms->m_doep_var, &prms->m_have);
#endif

    // Subtract offset and normalize magnetic field vector.
    aksc_ret = AKSC_VNorm(
            &prms->m_have,
            &prms->m_ho,
            &prms->m_hs,
            AKSC_HSENSE_TARGET,
            &hvec
            );
    if (aksc_ret == 0) {
        AKMERROR;
        AKMDEBUG(DBG_LEVEL0,
                "AKSC_VNorm failed.\n"
                "  have=%6d,%6d,%6d  ho=%6d,%6d,%6d  hs=%6d,%6d,%6d\n",
                prms->m_have.u.x, prms->m_have.u.y, prms->m_have.u.z,
                prms->m_ho.u.x, prms->m_ho.u.y, prms->m_ho.u.z,
                prms->m_hs.u.x, prms->m_hs.u.y, prms->m_hs.u.z);
        ret |= AKRET_VNORM_ERROR;
        return ret;
    } else {
        //ALOGE("AKSC_VNorm success\n");
        AKMDEBUG(DBG_LEVEL4,
                "AKSC_VNorm success.\n"
                "  have=%6d,%6d,%6d  ho=%6d,%6d,%6d  hs=%6d,%6d,%6d gdata=%6d,%6d,%6d hdt_ag=%6d gdt_ag=%6d\n",
                prms->m_have.u.x, prms->m_have.u.y, prms->m_have.u.z,
                prms->m_ho.u.x, prms->m_ho.u.y, prms->m_ho.u.z,
                prms->m_hs.u.x, prms->m_hs.u.y, prms->m_hs.u.z,
                prms->m_gvec.u.x, prms->m_gvec.u.y, prms->m_gvec.u.z,
                prms->m_hdt_ag, prms->m_gdt_ag
                );
    }

    // hvec is updated only when VNorm function is succeeded.
    prms->m_hvec = hvec;

    // Bias of Uncalibrated Magnetic Field
    prms->m_bias.u.x = (int32_akm)(prms->m_ho.u.x) + prms->m_hbase.u.x;
    prms->m_bias.u.y = (int32_akm)(prms->m_ho.u.y) + prms->m_hbase.u.y;
    prms->m_bias.u.z = (int32_akm)(prms->m_ho.u.z) + prms->m_hbase.u.z;

#if 0
    //Convert layout from sensor to Android by using PAT number.
    // Magnetometer
    ConvertCoordinate(prms->m_hlayout, &prms->m_hvec);
    // Bias of Uncalibrated Magnetic Field
    ConvertCoordinate32(prms->m_hlayout, &prms->m_bias);
#endif

    // Magnetic Field
    prms->m_calib.u.x = prms->m_hvec.u.x;
    prms->m_calib.u.y = prms->m_hvec.u.y;
    prms->m_calib.u.z = prms->m_hvec.u.z;

    // Uncalibrated Magnetic Field
    prms->m_uncalib.u.x = (int32_akm)(prms->m_calib.u.x) + prms->m_bias.u.x;
    prms->m_uncalib.u.y = (int32_akm)(prms->m_calib.u.y) + prms->m_bias.u.y;
    prms->m_uncalib.u.z = (int32_akm)(prms->m_calib.u.z) + prms->m_bias.u.z;

    AKMDEBUG(DBG_LEVEL4,
            "mag(dec)=%6d,%6d,%6d\n"
            "maguc(dec),bias(dec)=%7ld,%7ld,%7ld,%7ld,%7ld,%7ld\n",
            prms->m_calib.u.x, prms->m_calib.u.y, prms->m_calib.u.z,
            prms->m_uncalib.u.x, prms->m_uncalib.u.y, prms->m_uncalib.u.z,
            prms->m_bias.u.x, prms->m_bias.u.y, prms->m_bias.u.z);

    return AKRET_PROC_SUCCEED;
}

AKSC_HDST GetHDST(AKSCPRMS* prms)
{
    AKSC_HDST totalHDST = AKSC_HDST_UNSOLVED;

#ifndef AKMD_DISABLE_DOEPLUS
    /* Adjust magnetic reliability based on the level of each algorithm */
    if ((prms->m_hdst == 3) && (prms->m_doep_lv <= 2)){
        totalHDST = 2;
    } else if ((prms->m_hdst == 2) && (prms->m_doep_lv <= 1)){
        totalHDST = 1;
    } else {
        totalHDST = prms->m_hdst;
    }
#else
    totalHDST = prms->m_hdst;
#endif

    return totalHDST;
}

#ifdef AKMD_ENABLE_PG
void PG_InitConfig(AKSCPRMS* prms)
{
    int16  m_ocoef   = 103;
    uint16 m_th_rdif = 32766;
    uint16 m_th_rmax = 32766;
    uint16 m_th_rmin = 0;

    prms->m_pgcond.fmode = 1;
    prms->m_pgcond.ihave = 24;
    prms->m_pgcond.iaave = 24;

    switch(prms->PG_filter){
        case 0:
            prms->m_pgcond.ihave = 24;
            prms->m_pgcond.iaave = 24;
            m_ocoef = 103;//0.1;
            break;
        case 1:
            prms->m_pgcond.ihave = 24;
            prms->m_pgcond.iaave = 24;
            m_ocoef = 205;//0.2
            break;
        case 2:
            prms->m_pgcond.ihave = 24;
            prms->m_pgcond.iaave = 24;
            m_ocoef = 307;//0.3
            break;
        case 3:
            prms->m_pgcond.ihave = 32;
            prms->m_pgcond.iaave = 32;
            m_ocoef = 205;//0.2;
            break;
        case 4:
            prms->m_pgcond.ihave = 32;
            prms->m_pgcond.iaave = 32;
            m_ocoef = 307;//0.3;
            break;
        case 5:
            prms->m_pgcond.ihave = 12;
            prms->m_pgcond.iaave = 12;
            m_ocoef = 307;//0.3;
            break;
        case 6:
            prms->m_pgcond.ihave = 12;
            prms->m_pgcond.iaave = 12;
            m_ocoef = 205;//0.2;
            break;
        case 7:
            prms->m_pgcond.ihave = 12;
            prms->m_pgcond.iaave = 12;
            m_ocoef = 103;//0.1;
            break;
        default:
            break;
    }

#ifdef AKMD_PG_FLOAT_POINTS
    prms->m_pgcond.ocoef   = (AKSC_FLOAT)m_ocoef/1024;//90;//103;
    prms->m_pgcond.th_rdif = (int16)(m_th_rdif/16);
    prms->m_pgcond.th_rmax = (int16)(m_th_rmax/16);
    prms->m_pgcond.th_rmin = (int16)(m_th_rmin/16);
#else
    prms->m_pgcond.ocoef   = m_ocoef;//90;//103;
    prms->m_pgcond.th_rdif = m_th_rdif;
    prms->m_pgcond.th_rmax = m_th_rmax;
    prms->m_pgcond.th_rmin = m_th_rmin;
#endif
}
#endif
