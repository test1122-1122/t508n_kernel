#include <stdio.h>
#include <string.h>
#include "sensorhub_interface.h"


typedef unsigned int uint32;
typedef signed int int32;

typedef unsigned long long uint64;
typedef long long int64;

typedef unsigned char uint8;
typedef signed char int8;

typedef unsigned short uint16;
typedef signed short int16;


#define SC7A2X_ERR_I2C			(-1)

#define SC7A2X_AXIS_X			0
#define SC7A2X_AXIS_Y			1
#define SC7A2X_AXIS_Z			2
#define SC7A2X_AXES_NUM			3
#define SC7A2X_DATA_LEN			6


#define SC7A2X_REG_DATA_X_L		0x28
#define SC7A2X_REG_DATA_X_H		0x29
#define SC7A2X_REG_DATA_Y_L		0x2A
#define SC7A2X_REG_DATA_Y_H		0x2B
#define SC7A2X_REG_DATA_Z_L		0x2C
#define SC7A2X_REG_DATA_Z_H		0x2D

#define SC7A2X_REG_CTRL1		0x20
#define SC7A2X_REG_CTRL2		0x21
#define SC7A2X_REG_CTRL3		0x22
#define SC7A2X_REG_CTRL4		0x23
#define SC7A2X_REG_CTRL5		0x24
#define SC7A2X_REG_CTRL6		0x25

#define SC7A2X_GRAVITY_2G		1024
#define SC7A2X_GRAVITY_4G		512
#define SC7A2X_GRAVITY_8G		256

#define SC7A2X_GRAVITY_STD		9.807   //standart unit
#define SC7A2X_RESOLUTION_REVERSE	26.1

#define SILAN_SC7A20_FILTER		0
#ifdef SILAN_SC7A20_FILTER

typedef struct FilterChannelTag{
	int16 sample_l;
	int16 sample_h;

	int16 flag_l;
	int16 flag_h;

} FilterChannel;

typedef struct Silan_core_channel_s{
	int16 filter_param_l;
	int16 filter_param_h;
	int16 filter_threhold;
	FilterChannel  sl_channel[3];
} Silan_core_channel;

#endif	/*__SILAN_SC7A20_FILTER__*/

#ifdef SILAN_SC7A20_FILTER
	Silan_core_channel	core_channel;
#endif

#ifdef SILAN_SC7A20_FILTER
static int16 filter_average(int16 preAve, int16 sample, int16 Filter_num, int16* flag)
{
	if (*flag == 0)
	{
		preAve = sample;
		*flag = 1;
	}

	return preAve + (sample - preAve) / Filter_num;
}

static int16 silan_filter_process(FilterChannel* fac, int16 sample)
{
	if(fac == NULL)
	{
		return 0;
	}

	fac->sample_l = filter_average(fac->sample_l, sample, core_channel.filter_param_l, &fac->flag_l);
	fac->sample_h = filter_average(fac->sample_h, sample, core_channel.filter_param_h, &fac->flag_h);

	if (abs(fac->sample_l - fac->sample_h) > core_channel.filter_threhold)
	{
		fac->sample_h = fac->sample_l;
	}

	return fac->sample_h;
}
#endif /* ! SILAN_SC7A20_FILTER */

uint32 sc7a2x_first_load_fac = 0;

int sc7a2x_acc_get_data(int *value, float *raw_data)
{
	uint8 addr = (SC7A2X_REG_DATA_X_L | 0x80);
	uint8 data[SC7A2X_DATA_LEN] = {0};
	int16 xyz_value[SC7A2X_AXES_NUM] = {0, 0, 0};
	int err = 0;

	if(sc7a2x_first_load_fac != 1234567)
	{
#ifdef SILAN_SC7A20_FILTER
	/* configure default filter param */
		core_channel.filter_param_l  = 2;
		core_channel.filter_param_h  = 8;
		core_channel.filter_threhold = 12;   //8G scale: 12; 4G scale: 25; 2G scale: 50

		{
			int j = 0;
			for(j = 0; j < 3; j++)
			{
				core_channel.sl_channel[j].sample_l = 0;
				core_channel.sl_channel[j].sample_h = 0;
				core_channel.sl_channel[j].flag_l 	= 0;
				core_channel.sl_channel[j].flag_h 	= 0;
			}
		}
#endif
		sc7a2x_first_load_fac = 1234567;
	}

	if (!raw_data)
	{
		shub_extern_call->debug_trace("sc7a2x raw_data is NULL\n");
		return SC7A2X_ERR_I2C;
	}

	xyz_value[SC7A2X_AXIS_X] = ((int16)(SC7A2X_RESOLUTION_REVERSE*(double)raw_data[SC7A2X_AXIS_X]));
	xyz_value[SC7A2X_AXIS_Y] = ((int16)(SC7A2X_RESOLUTION_REVERSE*(double)raw_data[SC7A2X_AXIS_Y]));
	xyz_value[SC7A2X_AXIS_Z] = ((int16)(SC7A2X_RESOLUTION_REVERSE*(double)raw_data[SC7A2X_AXIS_Z]));

	data[0] = (xyz_value[SC7A2X_AXIS_X] & 0xF) << 4;
	data[1] = xyz_value[SC7A2X_AXIS_X] >> 4;
	data[2] = (xyz_value[SC7A2X_AXIS_Y] & 0xF) << 4;
	data[3] = xyz_value[SC7A2X_AXIS_Y] >> 4;
	data[4] = (xyz_value[SC7A2X_AXIS_Z] & 0xF) << 4;
	data[5] = xyz_value[SC7A2X_AXIS_Z] >> 4;

	xyz_value[SC7A2X_AXIS_X] = ((int16)(data[0] | (data[1] << 8))) >> 4;
	xyz_value[SC7A2X_AXIS_Y] = ((int16)(data[2] | (data[3] << 8))) >> 4;
	xyz_value[SC7A2X_AXIS_Z] = ((int16)(data[4] | (data[5] << 8))) >> 4;

#ifdef SILAN_SC7A20_FILTER
	xyz_value[SC7A2X_AXIS_X] = silan_filter_process(&core_channel.sl_channel[0], xyz_value[SC7A2X_AXIS_X]);
	xyz_value[SC7A2X_AXIS_Y] = silan_filter_process(&core_channel.sl_channel[1], xyz_value[SC7A2X_AXIS_Y]);
	xyz_value[SC7A2X_AXIS_Z] = silan_filter_process(&core_channel.sl_channel[2], xyz_value[SC7A2X_AXIS_Z]);
#endif

	raw_data[SC7A2X_AXIS_X] =  (float)xyz_value[SC7A2X_AXIS_X] * SC7A2X_GRAVITY_STD / SC7A2X_GRAVITY_8G;
	raw_data[SC7A2X_AXIS_Y] =  (float)xyz_value[SC7A2X_AXIS_Y] * SC7A2X_GRAVITY_STD / SC7A2X_GRAVITY_8G;
	raw_data[SC7A2X_AXIS_Z] =  (float)xyz_value[SC7A2X_AXIS_Z] * SC7A2X_GRAVITY_STD / SC7A2X_GRAVITY_8G;

	return 0;
}

int InitMain(void)
{
	shub_extern_call->debug_trace("InitMain");
	return 0;
}
