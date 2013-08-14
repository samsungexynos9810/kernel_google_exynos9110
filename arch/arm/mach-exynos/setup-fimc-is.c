/* linux/arch/arm/mach-exynos/setup-fimc-is.c
 *
 * Copyright (c) 2011 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com/
 *
 * FIMC-IS gpio and clock configuration
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/gpio.h>
#include <linux/clk.h>
#include <linux/err.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/regulator/consumer.h>
#include <linux/delay.h>
#include <mach/regs-gpio.h>
#include <mach/map.h>
#include <mach/regs-clock.h>
#include <plat/clock.h>
#include <plat/gpio-cfg.h>
#include <plat/map-s5p.h>
#include <plat/cpu.h>
#include <media/exynos_fimc_is.h>
#ifdef CONFIG_OF
#include <linux/of_gpio.h>
#endif
#if defined(CONFIG_SOC_EXYNOS5430)
#include <mach/regs-clock-exynos5430.h>
#endif

struct platform_device; /* don't need the contents */

#if defined(CONFIG_ARCH_EXYNOS5)
/*------------------------------------------------------*/
/*		Exynos5 series - FIMC-IS		*/
/*------------------------------------------------------*/
int exynos5_fimc_is_cfg_gpio(struct platform_device *pdev,
				int channel, bool flag_on)
{
	pr_debug("%s\n", __func__);

	return 0;
}

int exynos5_fimc_is_print_cfg(struct platform_device *pdev, u32 channel)
{
	pr_debug("%s\n", __func__);

	return 0;
}

int exynos5430_fimc_is_cfg_clk(struct platform_device *pdev)
{
	pr_debug("%s\n", __func__);

	return 0;
}

int exynos5430_fimc_is_clk_on(struct platform_device *pdev)
{
	pr_debug("%s\n", __func__);

	return 0;
}

int exynos5430_fimc_is_clk_off(struct platform_device *pdev)
{
	pr_debug("%s\n", __func__);

	return 0;
}

int exynos5430_fimc_is_sensor_clk_on(struct platform_device *pdev, u32 source)
{
	pr_debug("%s\n", __func__);

	return 0;
}

int exynos5430_fimc_is_sensor_clk_off(struct platform_device *pdev, u32 source)
{
	pr_debug("%s\n", __func__);

	return 0;
}

/* sequence is important, don't change order */
int exynos5_fimc_is_sensor_power_on(struct platform_device *pdev, int sensor_id)
{
	pr_debug("%s\n", __func__);

	return 0;
}

/* sequence is important, don't change order */
int exynos5_fimc_is_sensor_power_off(struct platform_device *pdev, int sensor_id)
{
	pr_debug("%s\n", __func__);

	return 0;
}
#endif
