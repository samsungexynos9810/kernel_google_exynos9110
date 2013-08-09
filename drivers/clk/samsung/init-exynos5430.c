/*
 * Copyright (c) 2013 Samsung Electronics Co., Ltd.
 * Author: Hyosang Jung
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Common Clock Framework support for Exynos5430 SoC.
 */

#include <linux/clk.h>
#include <linux/clkdev.h>
#include <linux/clk-provider.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <mach/regs-clock-exynos5430.h>

#include "clk.h"
#include "clk-pll.h"

void mfc_init_clock(void)
{
	exynos_set_parent("mout_aclk_mfc0_333_user", "aclk_mfc0_333");
	exynos_set_parent("mout_aclk_mfc1_333_user", "aclk_mfc1_333");
	exynos_set_parent("mout_aclk_hevc_400_user", "aclk_hevc_400");

	exynos_set_parent("mout_mfc_pll_user", "dout_mfc_pll");
	exynos_set_parent("mout_bus_pll_user", "dout_bus_pll");
}

void __init exynos5430_clock_init(void)
{
	mfc_init_clock();
}
