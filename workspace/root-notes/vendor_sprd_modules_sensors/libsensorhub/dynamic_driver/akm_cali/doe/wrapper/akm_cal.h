
#include "AKCompass.h"
#include "AKMD_APIs.h"
#include "Extension.h"
#include "ParameterIO.h"


typedef enum {
    INPUT_TYPE_ACCEL = 0,
    INPUT_TYPE_MAG,
    INPUT_TYPE_GYRO
} INPUT_TYPE;

int akm_cal_init(void);
void akm_cal_deinit(void);
int akm_cal_start(void);
int akm_cal_stop(void);
//void akm_calibrate(float data[], INPUT_TYPE input_type, unsigned int ts);
void akm_calibrate(
    const  int32_t input_data[],
    const  INPUT_TYPE input_type,
    float  output_mag[3],
    float  output_offset[3],
    int8_t *output_acc
);

void akm_get_orientation(float *ori_data, const int16_t len, int8_t *status);
void akm_get_pg_gyro(float *pg_gyro, const int16_t len, int8_t *status);
void akm_get_pg_rv(float *rv_data, const int16_t len);
void akm_get_pg_ga(float *ga_data, const int16_t len);
void akm_get_pg_la(float *la_data, const int16_t len);

/*void AKM_Open( void);
void AKM_Close( void);
int convert_magnetic(float mx, float my, float mz);
int convert_gyroscope(float grx, float gry, float grz);
int get_orientation(float accx, float accy, float accz);*/
