#include "SensorsData.h"

void AKM_SetMagData(
    AKSCPRMS    *prms,
    const int32_t mx,
    const int32_t my,
    const int32_t mz,
    const int16_t st2,
    const int16_t mdt
){
    int16 ret;
    int16 regx, regy, regz;
   // AKSC_FVEC magut;
    int16    bData[AKMD_BDATA_SIZE];

    prms->m_hdt_ag = -1;

    //magut.u.x = (AKSC_FLOAT)mx;
    //magut.u.y = (AKSC_FLOAT)my;
    //magut.u.z = (AKSC_FLOAT)mz;
		//printf("mx=%f,mgx=%f\r\n",mx,magut.u.x);
     //uT to AKM reg
    regx = (int16)(mx / CONVERT_TO_AKM);
    regy = (int16)(my / CONVERT_TO_AKM);
    regz = (int16)(mz / CONVERT_TO_AKM);

   // regx = (int16)(mx /(15*65536/100) );
    //regy = (int16)(my /(15*65536/100) );
   // regz = (int16)(mz /(15*65536/100) );

    // Inverse decomp, i.e. compose
    bData[0] = (int16)(1);
    bData[1] = (int16)(regx & 0xFF);
    bData[2] = (int16)((regx >> 8) & 0xFF);
    bData[3] = (int16)(regy & 0xFF);
    bData[4] = (int16)((regy >> 8) & 0xFF);
    bData[5] = (int16)(regz & 0xFF);
    bData[6] = (int16)((regz >> 8) & 0xFF);

    /*bData[1] = (int16)akm09916_8rawMag[0];
    bData[2] = (int16)akm09916_8rawMag[1];
    bData[3] = (int16)akm09916_8rawMag[2];
    bData[4] = (int16)akm09916_8rawMag[3];
    bData[5] = (int16)akm09916_8rawMag[4];
    bData[6] = (int16)akm09916_8rawMag[5];*/
    //if (AKMD_ST2_POS < AKMD_BDATA_SIZE) {
    //    bData[AKMD_ST2_POS] = (int16)(AKMD_ST2);
   // }
#if defined(AKMD_AK8975) | defined(AKMD_AK8963)
	bData[7] = (int16)(AKMD_ST2);
#elif defined(AKMD_AK09912) | defined(AKMD_AK09911)
	bData[7] = (int16)(0x80);
	bData[8] = (int16)(AKMD_ST2);
#elif defined(AKMD_AK09916)
	bData[7] = (int16)(0x80);
	bData[8] = (int16)(st2);
	//bData[8] = (int16)akm09916_8rawMag[7];
	//printf("f,d,i=%d,%d,%d\r\n",sizeof(float),sizeof(double),sizeof(int16_t));
	//printf("crc=%d\r\n",((bData[8] >> 4) & 0x07));
	//printf("b1=%d,%d\r\n",(int16)akm09916_8rawMag[0],(int16)(regx & 0xFF));
#endif

#if defined(AKMD_AK09916)
	//ret = set_akm_axis(prms->m_mag_patno);
	//if (ret != AKMD_SUCCESS)  {
            //ALOGE("%s(%d): set_akm_axis has failed (%d).\n", __FUNCTION__, __LINE__, ret);
        //}
				//printf("ret1=%d\r\n",ret);
	//ret = axisInfo2pat(akm_axis_order,akm_axis_sign,&bData[9]);
	//if (ret != AKMD_SUCCESS)  {
           // ALOGE("%s(%d): axisInfo2pat has failed (%d).\n", __FUNCTION__, __LINE__, bData[9]);
        //}
								//printf("bdata9=%d\r\n",bData[9]);
#endif

    ret = GetMagneticVectorDecomp(bData, prms, 0);
    // Check the return value
    if ((ret != AKRET_PROC_SUCCEED) && (ret != AKRET_FORMATION_CHANGED)) {
        AKMDEBUG(DBG_LEVEL0, "GetMagneticVectorDecomp has failed:%d\n", ret);
			//decomperr++;
			//printf("Decomp err:%d\r\n",ret);

    } else {
			//decomp++;
        if (prms->m_magIniUpdateFlag ==1){
            prms->m_hdt_ag = 1600;//(int16)(mdt * 16);
        } else {
            prms->m_magIniUpdateFlag=1;
        }
    }

#ifdef AKMD_ENABLE_PG
	prms->m_pgdt = (int16)(mdt * 16);
#endif
}

#ifdef AKMD_DOEaG
// grx, gry, grz: [rad]
// currTime
void AKM_SetGyroData(
	AKSCPRMS	*prms,
	const float grx,
	const float gry,
	const float grz,
	const int16_t gdt
){
	int16 i;
	AKSC_FLOAT cvt;

	prms->m_gdt_ag = -1;
    prms->m_hdt_ag = -1;

	/* from radian to AKM format dps*16 */
	cvt = RAD2DEG * AKM_GYR_FMT;

	if(((AKSC_FLOAT)grx*cvt < -32768) || (32767 < ((AKSC_FLOAT)grx*cvt))){
        AKMDEBUG(DBG_LEVEL0, "Gyrodata is overflow when convert to AKM format \n");
		return;
	}else{
		prms->m_gvec.v[0] = (int16)((AKSC_FLOAT)grx*cvt);
	}
	if(((AKSC_FLOAT)gry*cvt < -32768) || (32767 < ((AKSC_FLOAT)gry*cvt))){
        AKMDEBUG(DBG_LEVEL0, "Gyrodata is overflow when convert to AKM format \n");
		return;
	}else{
		prms->m_gvec.v[1] = (int16)((AKSC_FLOAT)gry*cvt);
	}
	if(((AKSC_FLOAT)grz*cvt < -32768) || (32767 < ((AKSC_FLOAT)grz*cvt))){
        AKMDEBUG(DBG_LEVEL0, "Gyrodata is overflow when convert to AKM format \n");
		return;
	}else{
		prms->m_gvec.v[2] = (int16)((AKSC_FLOAT)grz*cvt);
	}
	/* buffering */
	for (i = CSPEC_GNAVE - 1; i >= 1; i--) {
		prms->m_gdata[i] = prms->m_gdata[i - 1];
	}
	prms->m_gdata[0] = prms->m_gvec;
	AKSC_GetGyroOffsetAuto(prms->m_gdata, CSPEC_GNAVE, &prms->m_GO);

	prms->m_gvec.u.x = (int16)(prms->m_gvec.u.x - prms->m_GO.u.x);
	prms->m_gvec.u.y = (int16)(prms->m_gvec.u.y - prms->m_GO.u.y);
	prms->m_gvec.u.z = (int16)(prms->m_gvec.u.z - prms->m_GO.u.z);

	if(prms->m_magIniUpdateFlag==1){
        prms->m_gdt_ag = (int16)(gdt * 16);
		AKMDEBUG(DBG_LEVEL3, "gyr(dec)=%d,%d,%d",
				 prms->m_gvec.u.x, prms->m_gvec.u.y, prms->m_gvec.u.z);
	}
}
#endif
// [m/s^2]
void AKM_SetAccData(
        AKSCPRMS	*prms,
        const float ix,
        const float iy,
        const float iz
)
{
	float cvt;

	cvt = AKM_CONVERT_G * AKM_ACC_FMT;

    prms->m_avec.u.x = (int16)(ix * cvt);
    prms->m_avec.u.y = (int16)(iy * cvt);
    prms->m_avec.u.z = (int16)(iz * cvt);
}
