#ifndef _DYNAMIC_API_AKM_H_
#define _DYNAMIC_API_AKM_H_

#include <stdint.h>

int OVERLAY_Akm_MagCaliInit(int32_t sampleTime_ms);
int OVERLAY_Akm_SetMagData(double *mag_raw_data, double *acc, double *gyro, float *cali_mag, int *mag_accuracy, unsigned long long currTime, uint32_t odr);
int OVERLAY_Akm_CaliClose(void);
int OVERLAY_Akm_UpdateCfg(int *arg);

#endif
