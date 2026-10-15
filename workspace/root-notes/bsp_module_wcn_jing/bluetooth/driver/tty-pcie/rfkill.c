/*
 * Copyright (C) 2015 Spreadtrum Communications Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/rfkill.h>
#include <linux/gpio.h>
#include <linux/ioport.h>
#include <linux/clk.h>
#include <linux/of_gpio.h>
#include <misc/marlin_platform.h>

#include <misc/wcn_bus.h>

#include "lpm.h"

enum current_block_state {
	block_state_unknown = -1,
	block_state_blocked,
	block_state_unblocked
};

static struct rfkill *bt_rfk;
static const char bt_name[] = "bluetooth";
static enum current_block_state is_blocked = block_state_unknown;
int set_power_ret = 0;
DEFINE_MUTEX(set_power_mutex);

static int bluetooth_set_power(void *data, bool blocked)
{
	mutex_lock(&set_power_mutex);

	if (blocked && is_blocked == block_state_blocked)
		goto done;

	pr_info("%s: start_block=%d\n", __func__, blocked);
	if (!blocked) {
		host_wakeup_bt();
		set_power_ret = 0;
	} else {
		host_suspend_bt();
		set_power_ret = 0;
	}
	pr_info("%s: end_block: %d, ret: %d\n", __func__, blocked, set_power_ret);
	msleep(100);
	is_blocked = blocked ? block_state_blocked : block_state_unblocked;

done:
	mutex_unlock(&set_power_mutex);
	return set_power_ret;
}

static struct rfkill_ops rfkill_bluetooth_ops = {
	.set_block = bluetooth_set_power,
};

int rfkill_bluetooth_init(struct platform_device *pdev)
{
	int rc = 0;

	pr_info("-->%s\n", __func__);
	bt_rfk = rfkill_alloc(bt_name, &pdev->dev, RFKILL_TYPE_BLUETOOTH,
			&rfkill_bluetooth_ops, NULL);
	if (!bt_rfk) {
		rc = -ENOMEM;
		goto err_rfkill_alloc;
	}
	/* userspace cannot take exclusive control */
	rfkill_init_sw_state(bt_rfk, true);
	rc = rfkill_register(bt_rfk);
	if (rc)
		goto err_rfkill_reg;

	pr_info("start up\n");
	start_marlin(MARLIN_BLUETOOTH);

	pr_info("<--%s\n", __func__);

	return 0;

err_rfkill_reg:
	rfkill_destroy(bt_rfk);
err_rfkill_alloc:
	return rc;
}

int rfkill_bluetooth_remove(struct platform_device *dev)
{
	pr_info("-->%s\n", __func__);
	start_marlin(MARLIN_BLUETOOTH);
	rfkill_unregister(bt_rfk);
	rfkill_destroy(bt_rfk);
	pr_info("<--%s\n", __func__);
	return 0;
}


