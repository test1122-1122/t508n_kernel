#ifndef _SENSORHUB_INTF_H_
#define _SENSORHUB_INTF_H_

#include <stdint.h>
#include <stdbool.h>

typedef enum {
	FAIL = -1,
	SUCCESS = 0,
	NO_DATA = 1,
} RETURN_MSG;

struct i2c_dev_info {
	/* to specify which i2c to use, i2c0: interface_num = 0, i2c1: interface_num = 1, ... */
	uint32_t interface_num;
	/* i2c slave device's write address */
	uint8_t slave_addr;
	/* 1: for reg address is 1 byte i2c , 2: for 2 bytes reg address */
	uint32_t reg_addr_len;
};

struct shub_sys_interface {
	/***** init_param ---- deprecated *****/
	uint32_t (*init_param)(int32_t *param, int size);
	/***** debug_trace ---- log print interface *****/
	uint32_t (*debug_trace)(const char *x_format, ...);
	/***** extern_i2c_read ---- deprecated *****/
	uint32_t (*extern_i2c_read)(uint32_t handle, uint8_t *reg_addr, uint8_t *buffer, uint32_t bytes);
	/***** extern_i2c_write ---- deprecated *****/
	uint32_t (*extern_i2c_write)(uint32_t handle, uint8_t *reg_addr, uint8_t *buffer, uint32_t bytes);
	/* msleep ---- set cpu to sleep interface, time unit is ms */
	void (*msleep)(uint32_t ms);
	/***** extern_i2c_open ---- deprecated *****/
	int32_t (*extern_i2c_open)(void *i2c_dev);
	/***** extern_i2c_close ---- deprecated *****/
	int32_t (*extern_i2c_close)(uint32_t handle);
	/* extern_gpio_config ---- config gpio voltage level interface */
	void (*extern_gpio_config)(uint32_t gpio_num, bool value);
	/* sensor_i2c_read ---- i2c read interface */
	int32_t (*sensor_i2c_read)(struct i2c_dev_info *i2c_info, uint8_t *reg_addr, uint8_t *buffer, uint32_t bytes);
	/* sensor_i2c_write ---- i2c write interface */
	int32_t (*sensor_i2c_write)(struct i2c_dev_info *i2c_info, uint8_t *reg_addr, uint8_t *buffer, uint32_t bytes);
};

/**
 * i2c_read - execute single or multi bytes i2c read operation
 * @i2c_info: handle to i2c slave device
 * @reg_addr: register address to be read or the start register address to be read at multi registers read
 *            if i2c slave device' register address is 16 bit, reg_addr is the start address of the register.
 * @reg_value: byte array for data returned by slave
 * @regs_num: specify the amounts of register to be read
 *
 * Return Value
 * On success, the number of bytes read is returned.
 * On error, a negative value is returned.
 */
int32_t i2c_read(struct i2c_dev_info *i2c_info, uint8_t *reg_addr, uint8_t *reg_value, uint32_t regs_num);
/**
 * i2c_write - execute single or multi bytes i2c write operation
 * @i2c_info: handle to i2c slave device
 * @reg_addr: register address to be written or the start register address to be written at multi registers write
 * @reg_value: byte array for data to be written to slave device
 * @regs_num: specify the amounts of register to be written
 *
 * Return Value
 * On success, the number of bytes has been written is returned.
 * On error, a negative value is returned.
 */
int32_t i2c_write(struct i2c_dev_info *i2c_info, uint8_t *reg_addr, uint8_t *reg_value, uint32_t regs_num);
/**
 * gpio_set - set gpio voltage level
 * @gpio_num: the gpio number to be set.
 * @value: voltage level to be set. 1 for high level, 0 for low.
 */
void gpio_set(uint32_t gpio_num, bool value);
/**
 * msleep
 * @ms: Time in milliseconds to sleep for
 */
void msleep(uint32_t ms);

#ifndef LOG_PRINTF
extern const volatile struct shub_sys_interface * const shub_extern_call;
/**
 * LOG_PRINTF - format and print data
 * for log print, just use as printf in C std lib.
 */
#define LOG_PRINTF(fmt, args...) shub_extern_call->debug_trace(fmt, ##args)
#endif

#endif //_SENSORHUB_INTF_H_
