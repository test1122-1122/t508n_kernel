#include "LibAPIs.h"

size_t LibAPIs_GetBufferSize(void)
{
    return 0;
}

#ifndef AKMD_DISABLE_DOEPLUS
void LibAPIs_SetPrmsPointer(AKSCPRMS *prms, size_t prms_sz)
{
    prms->m_doep_var = (AKSC_DOEPVAR *)(((char *)prms) + prms_sz);

    return;
}
#else
void LibAPIs_SetPrmsPointer(AKSCPRMS* UNUSED(prms), size_t UNUSED(prms_sz))
{
    return;
}
#endif

void LibAPIs_Init(AKSCPRMS *prms)
{
    // Initialize HDOE parameters
    AKSC_InitHDOEProcPrmsS3(
            &prms->m_hdoev,
            1,
            &prms->m_ho,
            prms->m_hdst
            );

    return;
}

void LibAPIs_SetLevel(AKSCPRMS *prms, const AKSC_HDST hdst)
{
    // Set a HDOE level as "HDST_UNSOLVED"
    AKSC_SetHDOELevel(
            &prms->m_hdoev,
            &prms->m_ho,
            hdst,
            1
            );

    return;
}

int16 LibAPIs_ExeCalib(AKSCPRMS *prms)
{
    int16 hdSucc = 0;

#ifndef AKMD_DISABLE_DOEPLUS
    int16 doep_ret;
    int i;
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

    AKMDEBUG(DBG_LEVEL1,"DOEP: %7d, %7d, %7d ",
            prms->m_hdata[0].u.x,
            prms->m_hdata[0].u.y,
            prms->m_hdata[0].u.z);
    AKMDEBUG(DBG_LEVEL1,"|%7d, %7d, %7d \n",
            prms->m_hdata_plus[0].u.x,
            prms->m_hdata_plus[0].u.y,
            prms->m_hdata_plus[0].u.z);

    //Calculate Magnetic sensor's offset by DOE
    hdSucc = AKSC_HDOEProcessS3(
            prms->m_licenser,
            prms->m_licensee,
            prms->m_key,
            &prms->m_hdoev,
            prms->m_hdata_plus,
            prms->m_hn,
            &prms->m_ho,
            &prms->m_hdst
            );
#else
    hdSucc = AKSC_HDOEProcessS3(
            prms->m_licenser,
            prms->m_licensee,
            prms->m_key,
            &prms->m_hdoev,
            prms->m_hdata,
            prms->m_hn,
            &prms->m_ho,
            &prms->m_hdst
            );
#endif

    return hdSucc;
}
