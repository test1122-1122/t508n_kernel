#include "sensorhub_interface.h"
#include "platform_config.h"

const volatile struct shub_sys_interface * const shub_extern_call =
	(volatile struct shub_sys_interface *)(SENSORHUB_INTERFACE_ADDR);

//#define LOG_PRINTF(fmt, args...) shub_extern_call->debug_trace(fmt, ##args)

int32_t i2c_read(struct i2c_dev_info *i2c_info, uint8_t *reg_addr, uint8_t *reg_value, uint32_t regs_num)
{
	return shub_extern_call->sensor_i2c_read(i2c_info, reg_addr, reg_value, regs_num);
}

int32_t i2c_write(struct i2c_dev_info *i2c_info, uint8_t *reg_addr, uint8_t *reg_value, uint32_t regs_num)
{
	return shub_extern_call->sensor_i2c_write(i2c_info, reg_addr, reg_value, regs_num);
}

void gpio_set(uint32_t gpio_num, bool value)
{
	shub_extern_call->extern_gpio_config(gpio_num, value);
}

void msleep(uint32_t ms)
{
	shub_extern_call->msleep(ms);
}
