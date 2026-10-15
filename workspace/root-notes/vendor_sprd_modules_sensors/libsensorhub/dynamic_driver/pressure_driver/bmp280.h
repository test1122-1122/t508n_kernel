
#include <stdio.h>

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
    s16 par_T2;
    s16 par_T3;
    u16 par_P1;
    s16 par_P2;
    s16 par_P3;
    s16 par_P4;
    s16 par_P5;
    s16 par_P6;
    s16 par_P7;
    s16 par_P8;
    s16 par_P9;
    s16 par_P10;
    s16 par_P11;
    s32 t_fine;

};
int bmp280_pressure_get_data(int *value, float *raw_data);
int bmp280_pressure_enable(int mode, void *pdat, int len);
