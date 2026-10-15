#include "patch_table_sensor_sc7a20.h"

#include "dynamic_driver.h"
#include "sensorhub_interface.h"

#include <stdio.h>
#include <string.h>

extern uint32_t __bss_start;
extern uint32_t __bss_end;

static unsigned int mag_library_zi_Limit = &__bss_end;
static unsigned int mag_library_zi_Base = &__bss_start;

struct dynamic_driver dynamic_driver_interface __attribute__((section("sensor_driver_interface"))) = {
	NULL,
	NULL,
	NULL,
	NULL,

	NULL,
	NULL,
	NULL,
	NULL,

	NULL,
	NULL,
	NULL,
	NULL,

	NULL,
	NULL,
	NULL,
	NULL,

	NULL,
	NULL,
	sc7a2x_acc_get_data,
	NULL,

	NULL,
	NULL,
	NULL,
	NULL,

	InitMain,

	NULL,
};

