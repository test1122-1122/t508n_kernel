#include "dynamic_api_akm.h"
#include "akm_cal.h"

#include <stdio.h>

static int32_t input_data[3];
static int8_t i = 0, init_flag = 0;
static float output_offset[3];
static float cali_data[3] = {0.0f, 0.0f, 0.0f};

int akm_init_addr = 0;
float MAG_OFFSET[3];
int   ACCURACY = 0;

// 0: NOT initialized,  1:Initialized
int16_t g_init = 0;
// 0: Initial value.
// The maximum value will be specified when AKMD_Init() is called.
int16_t g_form = 0;
uint8_t g_prms[1136];

AKSCSAVEPRMS    offset_prms;

unsigned int mag_debug_data[2];

int OVERLAY_Akm_MagCaliInit(int32_t sampleTime_ms)
{
	if (init_flag != 1) {
		g_init = 0;
		g_form = 0;
		akm_init_addr = 0;
		ACCURACY = 0;
		init_flag = 1;
	}

	akm_cal_init();

	return 0;
}

int OVERLAY_Akm_SetMagData(double *mag_raw_data, double *acc, double *gyro, float *cali_mag, int *mag_accuracy, unsigned long long currTime, uint32_t odr)
{
	input_data[0] = (int32_t)(*mag_raw_data);
	input_data[1] = (int32_t)(*(mag_raw_data + 1));
	input_data[2] = (int32_t)(*(mag_raw_data + 2));

	akm_calibrate(input_data, INPUT_TYPE_MAG, &(cali_data[0]), output_offset, &i);

	*cali_mag = cali_data[0];
	*(cali_mag + 1) = cali_data[1];
	*(cali_mag + 2) = cali_data[2];

	*mag_accuracy = (int)i;

	return 0;
}

int OVERLAY_Akm_UpdateCfg(int *arg)
{
	int ret = 0;

	return ret;
}

int OVERLAY_Akm_CaliClose(void)
{
	akm_cal_stop();
	return 0;
}

