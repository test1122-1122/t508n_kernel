#include "sensorhub_interface.h"
#include "bmp280.h"

#include <stdio.h>
#include <string.h>

static u8 BMP280_CALIB_REG_START = 0x88;
static u8 BMP280_RAW_DATA_START = 0xF7;
struct calib_param bmp280_calib_param_t;



struct i2c_dev_info bmp280_i2c_info = {
    .interface_num = 0,
    .slave_addr = 0xEC,
    .reg_addr_len = 1
    };


static int bmp280_get_calib_param()
{
    int com_rslt = -1;
    uint8_t ret = FAIL;
    uint8_t a_data_u8[24] = {0};
    ret = i2c_read(&bmp280_i2c_info, &BMP280_CALIB_REG_START, a_data_u8, 24);
    if (ret <= 0)
    {
        LOG_PRINTF("bmp280 get calib param fail\n");
        return FAIL;
    }
    bmp280_calib_param_t.par_T1 = (u16)((a_data_u8[1] << 8) | a_data_u8[0]);
    bmp280_calib_param_t.par_T2 = (s16)((a_data_u8[3] << 8) | a_data_u8[2]);
    bmp280_calib_param_t.par_T3 = (s16)((a_data_u8[5] << 8) | a_data_u8[4]);
    bmp280_calib_param_t.par_P1 = (u16)((a_data_u8[7] << 8) | a_data_u8[6]);
    bmp280_calib_param_t.par_P2 = (s16)((a_data_u8[9] << 8) | a_data_u8[8]);
    bmp280_calib_param_t.par_P3 = (s16)((a_data_u8[11] << 8) | a_data_u8[10]);
    bmp280_calib_param_t.par_P4 = (s16)((a_data_u8[13] << 8) | a_data_u8[12]);
    bmp280_calib_param_t.par_P5 = (s16)((a_data_u8[15] << 8) | a_data_u8[14]);
    bmp280_calib_param_t.par_P6 = (s16)((a_data_u8[17] << 8) | a_data_u8[16]);
    bmp280_calib_param_t.par_P7 = (s16)((a_data_u8[19] << 8) | a_data_u8[18]);
    bmp280_calib_param_t.par_P8 = (s16)((a_data_u8[21] << 8) | a_data_u8[20]);
    bmp280_calib_param_t.par_P9 = (s16)((a_data_u8[23] << 8) | a_data_u8[22]);
    return com_rslt;
}

static s32 bmp280_compensate_T(s32 uncomp_temperture)
{
    s32 partial_data1;
    s32 partial_data2;
    s32 temperture;

    partial_data1 = ((((uncomp_temperture >>3) - ((s32)bmp280_calib_param_t.par_T1 << 1))) * ((s32)bmp280_calib_param_t.par_T2)) >>11;
    partial_data2 = (((((uncomp_temperture >>4) - ((s32)bmp280_calib_param_t.par_T1)) * ((uncomp_temperture >>4) - ((s32)bmp280_calib_param_t.par_T1))) >>12) *
                    ((s32)bmp280_calib_param_t.par_T3)) >>14;
    bmp280_calib_param_t.t_fine = partial_data1 + partial_data2;
    temperture = (bmp280_calib_param_t.t_fine * 5 + 128) >>8;
    return temperture;
}

static u32 bmp280_cmpensate_P(u32 uncomp_pressure)
{
    u32 pressure;
    s32 partial_data1;
    s32 partial_data2;

    partial_data1 = (((s32)bmp280_calib_param_t.t_fine) >> 1) - (s32)64000;
    partial_data2 = (((partial_data1 >> 2) * (partial_data1 >> 2)) >> 11) * ((s32)bmp280_calib_param_t.par_P6);
    partial_data2 = partial_data2 + ((partial_data1 *((s32)bmp280_calib_param_t.par_P5)) << 1);
    partial_data2 = (partial_data2 >> 2) + (((s32)bmp280_calib_param_t.par_P4) << 16);
    partial_data1 = (((bmp280_calib_param_t.par_P3 * (((partial_data1 >> 2) * (partial_data1 >> 2)) >> 13)) >> 3) +
                    ((((s32)bmp280_calib_param_t.par_P2) * partial_data1) >> 2)) >> 18;
    partial_data1 = ((((32768 + partial_data1)) * ((s32) bmp280_calib_param_t.par_P1)) >> 15);
    pressure = (((u32)(((s32)1048576) - uncomp_pressure) - (partial_data2 >>12))) *3125;

    if (pressure < 0x80000000)
            if (partial_data1 != 0)
                pressure = (pressure << 1) / ((u32)partial_data1) ;
            else
                return 0;
    else
            if (partial_data1 != 0)
                pressure = (pressure / (u32)partial_data1) * 2;
            else
                return 0;
    partial_data1 = (((s32)bmp280_calib_param_t.par_P9) * ((s32)(((pressure >> 3) * (pressure >>3)) >>13))) >>12;
    partial_data2 = (((s32)(pressure >> 2)) * ((s32)bmp280_calib_param_t.par_P8)) >>13;
    pressure = (u32)((s32)pressure + ((partial_data1 + partial_data2 +bmp280_calib_param_t.par_P7) >> 4));

    return pressure;
}

int bmp280_pressure_enable(int mode, void *pdat, int len)
{
    int ret = SUCCESS;
    bmp280_get_calib_param();
    return ret;
}

int bmp280_pressure_get_data(int *value, float *raw_data)
{
    int ret = FAIL;
    u8 a_data_u8[6] = {0};
    u32 uncomp_temperture_data = 0;
    u32 uncomp_pressure_data = 0;
    u32 actual_temperture_data = 0.0;
    u32 actual_pressure_data = 0;
    ret = i2c_read(&bmp280_i2c_info, &BMP280_RAW_DATA_START, a_data_u8, 6);
    if (ret <= 0)
    {
        LOG_PRINTF("bmp280 data reg read failed\n");
    }
    uncomp_pressure_data = (s32)(((u32)a_data_u8[0] << 12) | ((u32)a_data_u8[1] << 4) | ((u32)a_data_u8[2] >> 4));
    uncomp_temperture_data = (s32)(((u32)a_data_u8[3] << 12) | ((u32)a_data_u8[4] << 4) | ((u32)a_data_u8[5] >> 4));
    actual_temperture_data = bmp280_compensate_T(uncomp_temperture_data);
    actual_pressure_data = bmp280_cmpensate_P(uncomp_pressure_data);
    *raw_data = actual_pressure_data / 100.0f;
    return SUCCESS;
}
