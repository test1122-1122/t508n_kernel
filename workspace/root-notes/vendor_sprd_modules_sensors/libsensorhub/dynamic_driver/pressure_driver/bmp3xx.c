#include "sensorhub_interface.h"
#include "bmp3xx.h"

#include <stdio.h>
#include <string.h>

static const u8 BMP3XX_CALIB_REG_START = 0x31;
static const u8 BMP3XX_RAW_DATA_START = 0x04;
struct calib_param bmp3xx_calib_param_t;


struct i2c_dev_info bmp3xx_i2c_info = {
    .interface_num = 0,
    .slave_addr = 0xEC,
    .reg_addr_len = 1
    };

static double bmp3_pow(double base, uint8_t power)
{
     double pow_output = 1;

     while (power != 0) {
        pow_output = base * pow_output;
        power--;
    }
    return pow_output;
}

static int bmp3xx_get_calib_param()
{
    int com_rslt = -1;
    uint8_t ret = FAIL;
    double temp_var = 0.0;
    uint8_t a_data_u8[21] = {0};
    ret = i2c_read(&bmp3xx_i2c_info, &BMP3XX_CALIB_REG_START, a_data_u8, 21);
    if (ret <= 0)
    {
        LOG_PRINTF("bmp3xx get calib param fail\n");
        return FAIL;
    }
    bmp3xx_calib_param_t.par_T1 = (u16)((a_data_u8[1] << 8) | a_data_u8[0]);
    temp_var = 0.00390625f;
    bmp3xx_calib_param_t.quantized_T1 = bmp3xx_calib_param_t.par_T1 / temp_var;
    bmp3xx_calib_param_t.par_T2 = (u16)((a_data_u8[3] << 8) | a_data_u8[2]);
    temp_var = 1073741824.0f;
    bmp3xx_calib_param_t.quantized_T2 = bmp3xx_calib_param_t.par_T2 / temp_var;
    bmp3xx_calib_param_t.par_T3 = (s8)(a_data_u8[4]);
    temp_var = 281474976710656.0f;
    bmp3xx_calib_param_t.quantized_T3 = bmp3xx_calib_param_t.par_T3 / temp_var;
    bmp3xx_calib_param_t.par_P1 = (s16)((a_data_u8[6] << 8) | a_data_u8[5]);
    temp_var = 1048576.0f;
    bmp3xx_calib_param_t.quantized_P1 = (bmp3xx_calib_param_t.par_P1 - 16384) / temp_var;
    bmp3xx_calib_param_t.par_P2 = (s16)((a_data_u8[8] << 8) | a_data_u8[7]);
    temp_var = 536870912.0f;
    bmp3xx_calib_param_t.quantized_P2 = (bmp3xx_calib_param_t.par_P2 - 16384) / temp_var;
    bmp3xx_calib_param_t.par_P3 = (s8)(a_data_u8[9]);
    temp_var = 4294967296.0f;
    bmp3xx_calib_param_t.quantized_P3 = bmp3xx_calib_param_t.par_P3 / temp_var;
    bmp3xx_calib_param_t.par_P4 = (s8)(a_data_u8[10]);
    temp_var = 137438953472.0f;
    bmp3xx_calib_param_t.quantized_P4 = bmp3xx_calib_param_t.par_P4 / temp_var;
    bmp3xx_calib_param_t.par_P5 = (u16)((a_data_u8[12] << 8) | a_data_u8[11]);
    temp_var = 0.125f;
    bmp3xx_calib_param_t.quantized_P5 = bmp3xx_calib_param_t.par_P5 / temp_var;
    bmp3xx_calib_param_t.par_P6 = (u16)((a_data_u8[14] << 8) | a_data_u8[13]);
    temp_var = 64.0f;
    bmp3xx_calib_param_t.quantized_P6 = bmp3xx_calib_param_t.par_P6 / temp_var;
    bmp3xx_calib_param_t.par_P7 = (s8)(a_data_u8[15]);
    temp_var = 256.0f;
    bmp3xx_calib_param_t.quantized_P7 = bmp3xx_calib_param_t.par_P7 / temp_var;
    bmp3xx_calib_param_t.par_P8 = (s8)(a_data_u8[16]);
    temp_var = 32768.0f;
    bmp3xx_calib_param_t.quantized_P8 = bmp3xx_calib_param_t.par_P8 / temp_var;
    bmp3xx_calib_param_t.par_P9 = (s16)((a_data_u8[18] << 8) | a_data_u8[17]);
    temp_var = 281474976710656.0f;
    bmp3xx_calib_param_t.quantized_P9 = bmp3xx_calib_param_t.par_P9 / temp_var;
    bmp3xx_calib_param_t.par_P10 = (s8)(a_data_u8[19]);
    temp_var = 281474976710656.0f;
    bmp3xx_calib_param_t.quantized_P10 = bmp3xx_calib_param_t.par_P10 / temp_var;
    bmp3xx_calib_param_t.par_P11 = (s8)(a_data_u8[20]);
    temp_var = 36893488147419103232.0f;
    bmp3xx_calib_param_t.quantized_P11 = bmp3xx_calib_param_t.par_P11 / temp_var;
    return com_rslt;
}

static double bmp3xx_compaensate_T(u32 uncomp_temperture)
{
    double partial_data1;
    double partial_data2;

    partial_data1 = (double)(uncomp_temperture - bmp3xx_calib_param_t.quantized_T1);
    partial_data2 = (double)(partial_data1 * bmp3xx_calib_param_t.quantized_T2);
    bmp3xx_calib_param_t.t_fine = partial_data2 + (partial_data1 * partial_data1) * bmp3xx_calib_param_t.quantized_T3;
    return bmp3xx_calib_param_t.t_fine;
}

static double bmp3xx_cmpensate_P(u32 uncomp_pressure)
{
    double pressure;
    double partial_data1;
    double partial_data2;
    double partial_data3;
    double partial_data4;
    double partial_out1;
    double partial_out2;

    partial_data1 = bmp3xx_calib_param_t.quantized_P6 * bmp3xx_calib_param_t.t_fine;
    partial_data2 = bmp3xx_calib_param_t.quantized_P7 * bmp3_pow(bmp3xx_calib_param_t.t_fine, 2);
    partial_data3 = bmp3xx_calib_param_t.quantized_P8 * bmp3_pow(bmp3xx_calib_param_t.t_fine, 3);
    partial_out1 = bmp3xx_calib_param_t.quantized_P5 + partial_data1 + partial_data2 + partial_data3;

    partial_data1 = bmp3xx_calib_param_t.quantized_P2 * bmp3xx_calib_param_t.t_fine;
    partial_data2 = bmp3xx_calib_param_t.quantized_P3 * bmp3_pow(bmp3xx_calib_param_t.t_fine, 2);
    partial_data3 = bmp3xx_calib_param_t.quantized_P4 * bmp3_pow(bmp3xx_calib_param_t.t_fine, 3);
    partial_out2 = uncomp_pressure * (bmp3xx_calib_param_t.quantized_P1 + partial_data1 + partial_data2 + partial_data3);

    partial_data1 = bmp3_pow((double)uncomp_pressure, 2);
    partial_data2 = bmp3xx_calib_param_t.quantized_P9 + bmp3xx_calib_param_t.quantized_P10 * bmp3xx_calib_param_t.t_fine;
    partial_data3 = partial_data1 * partial_data2;
    partial_data4 = partial_data3 + bmp3_pow((double)uncomp_pressure, 3) * bmp3xx_calib_param_t.quantized_P11;
    pressure = partial_out1 + partial_out2 + partial_data4;
    return pressure;
}

int bmp3xx_pressure_enable(int mode, void *pdat, int len)
{
    int ret = SUCCESS;
    bmp3xx_get_calib_param();
    return ret;
}

int bmp3xx_pressure_get_data(int *value, float *raw_data)
{
    int ret = FAIL;
    u8 a_data_u8[6] = {0};
    u32 uncomp_temperture_data = 0;
    u32 uncomp_pressure_data = 0;
    double actual_temperture_data = 0.0;
    u32 actual_pressure_data = 0;
    float actual_pressure_hpa = 0.0;
    ret = i2c_read(&bmp3xx_i2c_info, &BMP3XX_RAW_DATA_START, a_data_u8, 6);
    if (ret <= 0)
    {
        LOG_PRINTF("bmp3xx data reg read failed\n");
    }
    uncomp_pressure_data = (a_data_u8[2] << 16) | (a_data_u8[1] << 8) | (a_data_u8[0]);
    uncomp_temperture_data = (a_data_u8[5] << 16) | (a_data_u8[4] << 8) | (a_data_u8[3]);
    actual_temperture_data = bmp3xx_compaensate_T(uncomp_temperture_data);
    actual_pressure_data = (u32)bmp3xx_cmpensate_P(uncomp_pressure_data);
    actual_pressure_hpa = actual_pressure_data / 100.f;
    *raw_data = actual_pressure_hpa;
    return SUCCESS;
}
