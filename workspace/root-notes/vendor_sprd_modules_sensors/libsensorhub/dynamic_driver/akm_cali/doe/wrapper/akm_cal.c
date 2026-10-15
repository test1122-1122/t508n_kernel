#include "akm_cal.h"
#include "timer.h"
//#include "OffsetIO.h"

#define WRAPPER_GAUSS2UT        1
//#define QCM_MAG_QFMT    1
#define CONVERT_TO_MS   1

extern int akm_init_addr;
extern float MAG_OFFSET[3];
extern int   ACCURACY;

int akm_cal_init(void)
{
    float offset[3] = {0.0f};
    unsigned char pdc[27] = {0};
    int accuracy = 0;

    if (akm_init_addr == 0) {
        AKMD_Init(MAG_OFFSET, ACCURACY, pdc);
        akm_init_addr = 1;
    }

    akm_cal_start();

    return 0;
}

void akm_cal_deinit(void)
{
    akm_cal_stop();

    AKMD_Release();

    return;
}

int akm_cal_start(void)
{
    int ret = 0;

    ret = AKMD_Start();

    return ret;
}

int akm_cal_stop(void)
{
    int ret = 0;

    ret = AKMD_Stop();

    return ret;
}

//void akm_calibrate(float data[], INPUT_TYPE input_type, unsigned int ts)
void akm_calibrate(
    const  int32_t input_data[],
    const  INPUT_TYPE input_type,
    float  output_mag[3],
    float  output_offset[3],
    int8_t *output_acc
) {
    unsigned int mag_valid = 0;
    unsigned int acc_valid = 0;
    unsigned int gyro_valid = 0;
    static int16_t pre_magtimestamp;
    int16_t cur_timestamp = 0;
    int16_t tmp_dur = 0;
    int16_t st2 = 0;
    int8_t accuracy = 0;
    float acc_x = 0, acc_y = 0, acc_z = 0;
    int32_t mag_x = 0, mag_y = 0, mag_z = 0;
    float gyro_x = 0, gyro_y = 0, gyro_z = 0;
    float sensor_data[3];
    float mag_offset[3];
    int16_t ret = 0;

    if (akm_init_addr == 0) {
        akm_cal_init();
        return; 
    }

    if (input_data == NULL) {
        return;
    }

    cur_timestamp = (int16_t)(GetCurrentTime() * CONVERT_TO_MS);

    switch (input_type) {
        case INPUT_TYPE_ACCEL:
            acc_x = input_data[0];
            acc_y = input_data[1];
            acc_z = input_data[2];
            acc_valid = 1;
            break;
        case INPUT_TYPE_GYRO:
            gyro_x = input_data[0];
            gyro_y = input_data[1];
            gyro_z = input_data[2];
            tmp_dur = (int16_t)((cur_timestamp - pre_magtimestamp) * CONVERT_TO_MS);
            gyro_valid = 1;
            break;
        case INPUT_TYPE_MAG:
            mag_x = input_data[0] * WRAPPER_GAUSS2UT;
            mag_y = input_data[1] * WRAPPER_GAUSS2UT;
            mag_z = input_data[2] * WRAPPER_GAUSS2UT;
            st2= (int16_t)input_data[3];
            mag_valid = 1;
            tmp_dur = (int16_t)((cur_timestamp - pre_magtimestamp) * CONVERT_TO_MS);
            break;
        default:
            return;
    }
//printf("ccl-mx=%d,my=%d,mz=%d\n",mag_x,mag_y,mag_z);
    AKMD_SetData(mag_x, mag_y, mag_z, st2, tmp_dur, mag_valid,
            gyro_x, gyro_y, gyro_z, tmp_dur, gyro_valid,
            acc_x, acc_y, acc_z, tmp_dur, acc_valid);

    if (input_type == INPUT_TYPE_MAG ||
        input_type == INPUT_TYPE_GYRO) {
        ret = AKMD_Calibrate(
                &sensor_data[0],
                &sensor_data[1],
                &sensor_data[2],
                &mag_offset[0],
                &mag_offset[1],
                &mag_offset[2],
                &accuracy);
    }

    if ((ret == AKMD_SUCCESS) &&
        (mag_valid) &&
        (output_mag != NULL) &&
        (output_offset != NULL) &&
        (output_acc != NULL)) {
        output_mag[0] = sensor_data[0];
        output_mag[1] = sensor_data[1];
        output_mag[2] = sensor_data[2];
        output_offset[0] = mag_offset[0];
        output_offset[1] = mag_offset[1];
        output_offset[2] = mag_offset[2];
        *output_acc = accuracy;

        MAG_OFFSET[0] = mag_offset[0];
        MAG_OFFSET[1] = mag_offset[1];
        MAG_OFFSET[2] = mag_offset[2];
        ACCURACY = accuracy;
    }

    if (input_type == INPUT_TYPE_MAG) {
        pre_magtimestamp = cur_timestamp;
    }

    return;
}

#ifdef AKMD_ENABLE_ORIENTATION
void akm_get_orientation(float *ori_data, const int16_t len, int8_t *status)
{
    AKMD_GetDirection(ori_data, len, status);
}
#endif

#ifdef AKMD_ENABLE_PG
void akm_get_pg_gyro(float *pg_gyro, const int16_t len, int8_t *status)
{
    AKMD_PG_Gyro(pg_gyro, len, status);
}

void akm_get_pg_rv(float *rv_data, const int16_t len)
{
    AKMD_PG_RotationVector(rv_data, len);
}

void akm_get_pg_ga(float *ga_data, const int16_t len)
{
    AKMD_PG_Gravity(ga_data, len);
}

void akm_get_pg_la(float *la_data, const int16_t len)
{
    AKMD_PG_LA(la_data, len);
}
#endif
