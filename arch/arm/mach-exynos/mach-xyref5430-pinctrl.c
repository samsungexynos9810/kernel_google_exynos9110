/*
 * SAMSUNG XYREF5430 pinctrl configuration based on exynos5430.
 *
 * Copyright (c) 2012 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/of_platform.h>
#include <linux/of_fdt.h>
#include <linux/memblock.h>
#include <linux/io.h>
#include <linux/clocksource.h>
#include <linux/pinctrl/machine.h>

#include <asm/mach/arch.h>
#include <mach/pinctrl-samsung.h>

static unsigned long pin_default_config[] = {
	PINCFG_PACK(PINCFG_TYPE_FUNC, 0x3),
	PINCFG_PACK(PINCFG_TYPE_DRV, 0x3),
};

static unsigned long pin_sleep_config[] = {
	PINCFG_PACK(PINCFG_TYPE_FUNC, 0x1),
	PINCFG_PACK(PINCFG_TYPE_DRV, 0x1),
};

static const struct pinctrl_map xyref5430_pinctrl_map[] = {
	PIN_MAP_CONFIGS_PIN_HOG("14cc0000.pinctrl", PINCTRL_STATE_DEFAULT,
				      "gpd2-6", pin_default_config),
	PIN_MAP_CONFIGS_PIN_HOG("14cc0000.pinctrl", PINCTRL_STATE_SLEEP,
				      "gpd2-6", pin_sleep_config),
};

static int __init xyref5430_pinctrl_init(void)
{
	return pinctrl_register_mappings(xyref5430_pinctrl_map,
					 ARRAY_SIZE(xyref5430_pinctrl_map));
}
postcore_initcall(xyref5430_pinctrl_init);
