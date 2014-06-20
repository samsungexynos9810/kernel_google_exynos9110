/* drivers/gpu/mali400/mali/platform/exynos4270/exynos4_pmm.c
 *
 * Copyright 2011 by S.LSI. Samsung Electronics Inc.
 * San#24, Nongseo-Dong, Giheung-Gu, Yongin, Korea
 *
 * Samsung SoC Mali400 DVFS driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software FoundatIon.
 */

/**
 * @file exynos4_pmm.c
 * Platform specific Mali driver functions for the exynos 4XXX based platforms
 */
#include "mali_kernel_common.h"
#include "mali_osk.h"
#include "exynos3_pmm.h"
#include <linux/io.h>
#include <linux/clk.h>
#include <linux/err.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/regulator/consumer.h>
#include <mach/regs-pmu.h>

#if defined(CONFIG_MALI400_PROFILING)
#include "mali_osk_profiling.h"
#endif

/* Some defines changed names in later Odroid-A kernels. Make sure it works for both. */
#ifndef S5P_G3D_CONFIGURATION
#define S5P_G3D_CONFIGURATION EXYNOS4_G3D_CONFIGURATION
#endif
#ifndef S5P_G3D_STATUS
#define S5P_G3D_STATUS (EXYNOS4_G3D_CONFIGURATION + 0x4)
#endif
#ifndef S5P_INT_LOCAL_PWR_EN
#define S5P_INT_LOCAL_PWR_EN EXYNOS_INT_LOCAL_PWR_EN
#endif

#define EXTXTALCLK_NAME  "ext_xtal"
#define VPLLSRCCLK_NAME  "mout_vpllsrc"
#define FOUTVPLLCLK_NAME "fout_vpll"
#define SCLVPLLCLK_NAME  "mout_vpll"
#define GPUMOUT1CLK_NAME "mout_g3d1"
#define MPLLCLK_NAME     "mout_mpll"
#define GPUMOUT0CLK_NAME "mout_g3d0"
#define GPUMOUTCLK_NAME "mout_g3d"
#define GPUCLK_NAME      "sclk_g3d"
#define GPU_NAME      "g3d"
#define CLK_DIV_STAT_G3D 0x1003C62C
#define CLK_DESC         "clk-divider-status"

static struct clk *ext_xtal_clock = NULL;
static struct clk *vpll_src_clock = NULL;
static struct clk *mpll_clock = NULL;
static struct clk *mali_mout0_clock = NULL;
static struct clk *mali_mout_clock = NULL;
static struct clk *fout_vpll_clock = NULL;
static struct clk *sclk_vpll_clock = NULL;
static struct clk *mali_parent_clock = NULL;
static struct clk *mali_clock = NULL;
static struct clk *g3d_clock = NULL;

/* PegaW1 */
int mali_gpu_clk = 160;
static unsigned int GPU_MHZ	= 1000000;
static int nPowermode;
static atomic_t clk_active;

_mali_osk_mutex_t *mali_dvfs_lock;

/* export GPU frequency as a read-only parameter so that it can be read in /sys */
module_param(mali_gpu_clk, int, S_IRUSR | S_IRGRP | S_IROTH);
MODULE_PARM_DESC(mali_gpu_clk, "Mali Current Clock");

mali_bool mali_clk_get(struct platform_device *pdev)
{
	if (ext_xtal_clock == NULL)	{
		ext_xtal_clock = clk_get(&pdev->dev, EXTXTALCLK_NAME);
		if (IS_ERR(ext_xtal_clock)) {
			MALI_PRINT(("MALI Error : failed to get source ext_xtal_clock\n"));
			return MALI_FALSE;
		}
	}

	if (vpll_src_clock == NULL)	{
		vpll_src_clock = clk_get(&pdev->dev, VPLLSRCCLK_NAME);
		if (IS_ERR(vpll_src_clock)) {
			MALI_PRINT(("MALI Error : failed to get source vpll_src_clock\n"));
			return MALI_FALSE;
		}
	}

	if (fout_vpll_clock == NULL) {
		fout_vpll_clock = clk_get(&pdev->dev, FOUTVPLLCLK_NAME);
		if (IS_ERR(fout_vpll_clock)) {
			MALI_PRINT(("MALI Error : failed to get source fout_vpll_clock\n"));
			return MALI_FALSE;
		}
	}

	if (mpll_clock == NULL)
	{
		mpll_clock = clk_get(&pdev->dev,MPLLCLK_NAME);

		if (IS_ERR(mpll_clock)) {
			MALI_PRINT( ("MALI Error : failed to get source mpll clock\n"));
			return MALI_FALSE;
		}
	}

	if (mali_mout0_clock == NULL)
	{
		mali_mout0_clock = clk_get(&pdev->dev, GPUMOUT0CLK_NAME);

		if (IS_ERR(mali_mout0_clock)) {
			MALI_PRINT( ( "MALI Error : failed to get source mali mout0 clock\n"));
			return MALI_FALSE;
		}
	}

	if (mali_mout_clock == NULL)
	{
		mali_mout_clock = clk_get(&pdev->dev, GPUMOUTCLK_NAME);

		if (IS_ERR(mali_mout_clock)) {
			MALI_PRINT( ( "MALI Error : failed to get source mali mout clock\n"));
			return MALI_FALSE;
		}
	}

	if (sclk_vpll_clock == NULL) {
		sclk_vpll_clock = clk_get(&pdev->dev, SCLVPLLCLK_NAME);
		if (IS_ERR(sclk_vpll_clock)) {
			MALI_PRINT(("MALI Error : failed to get source sclk_vpll_clock\n"));
			return MALI_FALSE;
		}
	}

	if (mali_parent_clock == NULL) {
		mali_parent_clock = clk_get(&pdev->dev, GPUMOUT1CLK_NAME);

		if (IS_ERR(mali_parent_clock)) {
			MALI_PRINT(("MALI Error : failed to get source mali parent clock\n"));
			return MALI_FALSE;
		}
	}

	if (mali_clock == NULL) {
		mali_clock = clk_get(&pdev->dev, GPUCLK_NAME);

		if (IS_ERR(mali_clock)) {
			MALI_PRINT(("MALI Error : failed to get source mali clock\n"));
			return MALI_FALSE;
		}
	}
	if (g3d_clock == NULL) {
		g3d_clock = clk_get(&pdev->dev, GPU_NAME);

		if (IS_ERR(g3d_clock)) {
			MALI_PRINT(("MALI Error : failed to get mali g3d clock\n"));
			return MALI_FALSE;
		}
	}

	return MALI_TRUE;
}

void mali_clk_put(mali_bool binc_mali_clock)
{
	if (mali_parent_clock)
	{
		clk_put(mali_parent_clock);
		mali_parent_clock = NULL;
	}

	if (sclk_vpll_clock)
	{
		clk_put(sclk_vpll_clock);
		sclk_vpll_clock = NULL;
	}

	if (binc_mali_clock && fout_vpll_clock)
	{
		clk_put(fout_vpll_clock);
		fout_vpll_clock = NULL;
	}

	if (mpll_clock)
	{
		clk_put(mpll_clock);
		mpll_clock = NULL;
	}

	if (mali_mout0_clock)
	{
		clk_put(mali_mout0_clock);
		mali_mout0_clock = NULL;
	}

	if (mali_mout_clock)
	{
		clk_put(mali_mout_clock);
		mali_mout_clock = NULL;
	}

	if (vpll_src_clock)
	{
		clk_put(vpll_src_clock);
		vpll_src_clock = NULL;
	}

	if (ext_xtal_clock)
	{
		clk_put(ext_xtal_clock);
		ext_xtal_clock = NULL;
	}

	if (binc_mali_clock && mali_clock)
	{
		clk_put(mali_clock);
		mali_clock = NULL;
	}

	if (g3d_clock)
	{
		clk_put(g3d_clock);
		g3d_clock = NULL;
	}
}

void mali_clk_set_rate(struct platform_device *pdev, unsigned int clk, unsigned int mhz)
{
	int err;
	unsigned long rate = (unsigned long)clk * (unsigned long)mhz;

	_mali_osk_mutex_wait(mali_dvfs_lock);
	MALI_DEBUG_PRINT(3, ("Mali platform: Setting frequency to %d mhz\n", clk));

	if (mali_clk_get(pdev) == MALI_FALSE) {
		_mali_osk_mutex_signal(mali_dvfs_lock);
		return;
	}

	err = clk_set_rate(mali_clock, rate);
	if (err)
		MALI_PRINT_ERROR(("Failed to set Mali clock: %d\n", err));

	rate = clk_get_rate(mali_clock);
	GPU_MHZ = mhz;

	mali_gpu_clk = rate / mhz;
	MALI_PRINT(("Mali frequency %dMhz\n", rate / mhz));
	mali_clk_put(MALI_FALSE);

	_mali_osk_mutex_signal(mali_dvfs_lock);
}

static mali_bool init_mali_clock(struct platform_device *pdev)
{
	int err = 0;
	mali_bool ret = MALI_TRUE;
	nPowermode = MALI_POWER_MODE_DEEP_SLEEP;

	if (mali_clock != 0)
		return ret; /* already initialized */

	mali_dvfs_lock = _mali_osk_mutex_init(_MALI_OSK_LOCKFLAG_ORDERED, 0);

	if (mali_dvfs_lock == NULL)
		return _MALI_OSK_ERR_FAULT;

	if (!mali_clk_get(pdev))
	{
		MALI_PRINT(("Error: Failed to get Mali clock\n"));
		goto err_clk;
	}

	err = clk_set_rate(fout_vpll_clock, (unsigned int)mali_gpu_clk * GPU_MHZ);
	if (err)
		MALI_PRINT_ERROR(("Failed to set fout_vpll clock:\n"));

	err = clk_set_parent(vpll_src_clock, ext_xtal_clock);
	if (err)
		MALI_PRINT_ERROR(("vpll_src_clock set parent to ext_xtal_clock failed\n"));

	err = clk_set_parent(sclk_vpll_clock, fout_vpll_clock);
	if (err)
		MALI_PRINT_ERROR(("sclk_vpll_clock set parent to fout_vpll_clock failed\n"));

	err = clk_set_parent(mali_parent_clock, sclk_vpll_clock);
	if (err)
		MALI_PRINT_ERROR(("mali_parent_clock set parent to sclk_vpll_clock failed\n"));

	err = clk_set_parent(mali_mout_clock, mali_parent_clock);
	if (err)
		MALI_PRINT_ERROR(("mali_clock set parent to mali_parent_clock failed\n"));

	if (!atomic_read(&clk_active)) {
		if ((clk_prepare_enable(mali_clock)  < 0) || (clk_prepare_enable(g3d_clock)  < 0)) {
			MALI_PRINT(("Error: Failed to enable clock\n"));
			goto err_clk;
		}
		atomic_set(&clk_active, 1);
	}

	mali_clk_set_rate(pdev,(unsigned int)mali_gpu_clk, GPU_MHZ);
	MALI_PRINT(("init_mali_clock mali_clock %x\n", mali_clock));
	mali_clk_put(MALI_FALSE);

	return MALI_TRUE;

err_clk:
	mali_clk_put(MALI_TRUE);

	return ret;
}

static mali_bool deinit_mali_clock(void)
{
	if (mali_clock == 0 || g3d_clock == 0)
		return MALI_TRUE;

	mali_clk_put(MALI_TRUE);
	return MALI_TRUE;
}

static _mali_osk_errcode_t enable_mali_clocks(void)
{
	int err;
	err = clk_prepare_enable(mali_clock);
	err = clk_prepare_enable(g3d_clock);
	MALI_DEBUG_PRINT(3,("enable_mali_clocks mali_clock %p error %d \n", mali_clock, err));

	MALI_SUCCESS;
}

static _mali_osk_errcode_t disable_mali_clocks(void)
{
	if (atomic_read(&clk_active)) {
		clk_disable_unprepare(mali_clock);
		clk_disable_unprepare(g3d_clock);
		MALI_DEBUG_PRINT(3, ("disable_mali_clocks mali_clock %p g3d %p\n", mali_clock,g3d_clock));
		atomic_set(&clk_active, 0);
	}

	MALI_SUCCESS;
}

_mali_osk_errcode_t mali_platform_init(struct platform_device *pdev)
{
	MALI_CHECK(init_mali_clock(pdev), _MALI_OSK_ERR_FAULT);
	mali_platform_power_mode_change(MALI_POWER_MODE_ON);

	MALI_SUCCESS;
}

_mali_osk_errcode_t mali_platform_deinit(void)
{
	mali_platform_power_mode_change(MALI_POWER_MODE_DEEP_SLEEP);
	deinit_mali_clock();

	MALI_SUCCESS;
}

_mali_osk_errcode_t mali_platform_power_mode_change(mali_power_mode power_mode)
{
	switch (power_mode)
	{
	case MALI_POWER_MODE_ON:
		MALI_DEBUG_PRINT(3, ("Mali platform: Got MALI_POWER_MODE_ON event, %s\n",
					nPowermode ? "powering on" : "already on"));
		if (nPowermode == MALI_POWER_MODE_LIGHT_SLEEP || nPowermode == MALI_POWER_MODE_DEEP_SLEEP)	{
			MALI_DEBUG_PRINT(4, ("enable clock\n"));
			enable_mali_clocks();
			nPowermode = power_mode;
		}
		break;
	case MALI_POWER_MODE_DEEP_SLEEP:
	case MALI_POWER_MODE_LIGHT_SLEEP:
		MALI_DEBUG_PRINT(3, ("Mali platform: Got %s event, %s\n", power_mode == MALI_POWER_MODE_LIGHT_SLEEP ?
					"MALI_POWER_MODE_LIGHT_SLEEP" : "MALI_POWER_MODE_DEEP_SLEEP",
					nPowermode ? "already off" : "powering off"));
		if (nPowermode == MALI_POWER_MODE_ON)	{
			disable_mali_clocks();
			nPowermode = power_mode;
		}
		break;
	}
	MALI_SUCCESS;
}
