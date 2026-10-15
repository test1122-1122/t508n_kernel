#include "dynamic_api_akm.h"

#include "dynamic_driver.h"
#include "sensorhub_interface.h"

#include <stdio.h>
#include <string.h>

extern uint32_t __bss_start;
extern uint32_t __bss_end;

static unsigned int mag_library_zi_Limit = &__bss_end;
static unsigned int mag_library_zi_Base = &__bss_start;

static int InitMain(void)
{
	memset(mag_library_zi_Base, 0, mag_library_zi_Limit - mag_library_zi_Base);

	return 0;
}

static int PATCH_TABLE_AKM_Init(int32_t sampleTime_ms, float *mag_offset)
{

	OVERLAY_Akm_MagCaliInit(sampleTime_ms);

	return 1;

}

static int PATCH_TABLE_AKM_Update(double *mag_raw_data, double *acc, double *gyro, float *cali_mag, int *mag_accuracy, unsigned long long currTime, uint32_t odr)
{
	OVERLAY_Akm_SetMagData(mag_raw_data, acc, gyro, cali_mag, mag_accuracy, currTime, odr);

	return 0;

}

static int PATCH_TABLE_AKM_Close(float *mag_offset)
{
	OVERLAY_Akm_CaliClose();

	return 0;
}

static int PATCH_TABLE_AKM_UpdateCfg(int *arg)
{
	int ret = 0;
	ret = OVERLAY_Akm_UpdateCfg(arg);

	return ret;
}

struct dynamic_driver dynamic_driver_interface __attribute__((section("sensor_driver_interface")))= {
	PATCH_TABLE_AKM_Init,
	PATCH_TABLE_AKM_Update,
	PATCH_TABLE_AKM_Close,
	PATCH_TABLE_AKM_UpdateCfg,

	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,

	InitMain,
	NULL,
};

