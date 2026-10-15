#ifndef _OVERLAY_TYPES_H_
#define _OVERLAY_TYPES_H_

#include <stdint.h>

struct dynamic_driver {
	/* mag sensor extern call */
	int (*mag_init)(int32_t sampleTime_ms, float *mag_offset);
	int (*mag_update)(double *mag, double *acc, double *gyro, float *cali_mag,
			  int *mag_accuracy, unsigned long long currTime, uint32_t odr);
	int (*mag_close)(float *mag_offset);
	int (*mag_update_cfg)(int *arg);
	/* prox sensor extern call */
	int (*prox_enable)(int mode, void *pdat, int len);
	int (*prox_update_cfg)(int *data);
	int (*prox_get_data)(int *value, float *raw_data);
	int (*prox_disable)(void);
	/* light sensor extern call */
	int (*light_enable)(int mode, void *pdat, int len);
	int (*light_update_cfg)(int *data);
	int (*light_get_data)(int *value, float *raw_data);
	int (*light_disable)(void);
	/* pressure sensor extern call */
	int (*pressure_enable)(int mode, void *pdat, int len);
	int (*pressure_update_cfg)(int *data);
	int (*pressure_get_data)(int *value, float *raw_data);
	int (*pressure_disable)(void);
	/* acc sensor extern call */
	int (*acc_enable)(int mode, void *pdat, int len);
	int (*acc_update_cfg)(int *data);
	int (*acc_get_data)(int *value, float *raw_data);
	int (*acc_disable)(void);
	/* gyro sensor extern call */
	int (*gyro_enable)(int mode, void *pdat, int len);
	int (*gyro_update_cfg)(int *data);
	int (*gyro_get_data)(int *value, float *raw_data);
	int (*gyro_disable)(void);

	int (*extern_main)(void);
	int (*get_vendor_fusion)(float *cali_acc, float *cali_gyro, float *cali_mag, uint32_t timestamp);
};

#endif
