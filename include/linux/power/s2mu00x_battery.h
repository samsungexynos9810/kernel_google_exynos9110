/*
 * s2mu00x_battery.h
 *
 * Copyright (C) 2017 Samsung Electronics, Inc.
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

#ifndef __S2MU00X_BATTERY_H
#define __S2MU00X_BATTERY_H

#include <linux/module.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/err.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/device.h>
#include <linux/power_supply.h>

ssize_t s2mu00x_bat_show_attrs(struct device *dev,
		struct device_attribute *attr, char *buf);

ssize_t s2mu00x_bat_store_attrs(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count);

#define BATTERY_ATTR(_name)                     \
{                                   \
	    .attr = {.name = #_name, .mode = 0664}, \
	    .show = s2mu00x_bat_show_attrs,                 \
	    .store = s2mu00x_bat_store_attrs,                   \
}

enum {
	BATT_READ_RAW_SOC = 0,
	TEST_MODE,
};
#endif /* __S2MU00X_BATTERY_H */
