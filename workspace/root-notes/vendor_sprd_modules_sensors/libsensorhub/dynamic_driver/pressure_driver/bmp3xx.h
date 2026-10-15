
#include <stdint.h>

typedef signed char s8;
typedef unsigned char u8;

typedef signed short s16;
typedef unsigned short u16;

typedef signed int s32;
typedef unsigned int u32;

typedef signed long s64;
typedef unsigned long u64;


struct calib_param
{
    u16 par_T1;
    u16 par_T2;
    s8 par_T3;
    s16 par_P1;
    s16 par_P2;
    s8 par_P3;
    s8 par_P4;
    u16 par_P5;
    u16 par_P6;
    s8 par_P7;
    s8 par_P8;
    s16 par_P9;
    s8 par_P10;
    s8 par_P11;

    double quantized_T1;
    double quantized_T2;
    double quantized_T3;
    double quantized_P1;
    double quantized_P2;
    double quantized_P3;
    double quantized_P4;
    double quantized_P5;
    double quantized_P6;
    double quantized_P7;
    double quantized_P8;
    double quantized_P9;
    double quantized_P10;
    double quantized_P11;

    double t_fine;

};
int bmp3xx_pressure_get_data(int *value, float *raw_data);
int bmp3xx_pressure_enable(int mode, void *pdat, int len);
