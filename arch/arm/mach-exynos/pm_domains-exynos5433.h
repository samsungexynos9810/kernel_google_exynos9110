/*
 * Exynos Generic power domain support.
 *
 * Copyright (c) 2013 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * Implementation of Exynos specific power domain control which is used in
 * conjunction with runtime-pm. Support for both device-tree and non-device-tree
 * based power domain support is included.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <plat/pm.h>
#include <mach/pm_domains.h>
#include <mach/devfreq.h>

static DEFINE_SPINLOCK(rpmlock_cmutop);

void __iomem *decon_vidcon0;
void __iomem *decontv_vidcon0;

struct exynos5430_pd_state {
	void __iomem *reg;
	unsigned long val;
};

static struct exynos5430_pd_state cmutop_mfc[] = {
	{ .reg = EXYNOS5430_ENABLE_IP_TOP,	.val = (1<<1), },
};

static struct exynos5430_pd_state cmutop_hevc[] = {
	{ .reg = EXYNOS5430_ENABLE_IP_TOP,	.val = (1<<3), },
};

static struct exynos5430_pd_state cmutop_gscl[] = {
	{ .reg = EXYNOS5430_ENABLE_IP_TOP,	.val = (1<<7), },
};

static struct exynos5430_pd_state cmutop_mscl[] = {
	{ .reg = EXYNOS5430_ENABLE_IP_TOP,	 .val = (1<<10), },
};

static struct exynos5430_pd_state cmutop_g2d[] = {
	{ .reg = EXYNOS5430_ENABLE_IP_TOP,	.val = (1<<0), },
};

static struct exynos5430_pd_state cmutop_isp[] = {
	{ .reg = EXYNOS5430_ENABLE_IP_TOP,	 .val = (1<<4), },
};

static struct exynos5430_pd_state cmutop_cam0[] = {
	{ .reg = EXYNOS5430_ENABLE_IP_TOP,	 .val = (1<<5), },
};

static struct exynos5430_pd_state cmutop_cam1[] = {
	{ .reg = EXYNOS5430_ENABLE_IP_TOP,	 .val = (1<<6), },
};

static struct sleep_save exynos_pd_maudio_clk_save[] = {
	SAVE_ITEM(EXYNOS5430_ENABLE_IP_AUD0),
	SAVE_ITEM(EXYNOS5430_ENABLE_IP_AUD1),
};

static struct sleep_save exynos_pd_g3d_clk_save[] = {
	SAVE_ITEM(EXYNOS5430_ENABLE_IP_G3D0),
	SAVE_ITEM(EXYNOS5430_ENABLE_IP_G3D1),
};

static struct sleep_save exynos_pd_mfc_clk_save[] = {
	SAVE_ITEM(EXYNOS5430_ENABLE_IP_MFC00),
	SAVE_ITEM(EXYNOS5430_ENABLE_IP_MFC01),
	SAVE_ITEM(EXYNOS5430_ENABLE_IP_MFC0_SECURE_SMMU_MFC),
};

static struct sleep_save exynos_pd_hevc_clk_save[] = {
	SAVE_ITEM(EXYNOS5430_ENABLE_IP_HEVC0),
	SAVE_ITEM(EXYNOS5430_ENABLE_IP_HEVC1),
	SAVE_ITEM(EXYNOS5430_ENABLE_IP_HEVC_SECURE_SMMU_HEVC),
};

static struct sleep_save exynos_pd_gscl_clk_save[] = {
	SAVE_ITEM(EXYNOS5430_ENABLE_IP_GSCL0),
	SAVE_ITEM(EXYNOS5430_ENABLE_IP_GSCL1),
	SAVE_ITEM(EXYNOS5430_ENABLE_IP_GSCL_SECURE_SMMU_GSCL0),
	SAVE_ITEM(EXYNOS5430_ENABLE_IP_GSCL_SECURE_SMMU_GSCL1),
	SAVE_ITEM(EXYNOS5430_ENABLE_IP_GSCL_SECURE_SMMU_GSCL2),
};

static struct sleep_save exynos_pd_disp_clk_save[] = {
	SAVE_ITEM(EXYNOS5430_ENABLE_IP_DISP0),
	SAVE_ITEM(EXYNOS5430_ENABLE_IP_DISP1),
	SAVE_ITEM(EXYNOS5430_DISP_PLL_LOCK),
	SAVE_ITEM(EXYNOS5430_DISP_PLL_CON0),
	SAVE_ITEM(EXYNOS5430_DISP_PLL_CON1),
	SAVE_ITEM(EXYNOS5430_DISP_PLL_FREQ_DET),
};

static struct sleep_save exynos_pd_mscl_clk_save[] = {
	SAVE_ITEM(EXYNOS5430_ENABLE_IP_MSCL0),
	SAVE_ITEM(EXYNOS5430_ENABLE_IP_MSCL1),
	SAVE_ITEM(EXYNOS5430_ENABLE_IP_MSCL_SECURE_SMMU_M2MSCALER0),
	SAVE_ITEM(EXYNOS5430_ENABLE_IP_MSCL_SECURE_SMMU_M2MSCALER1),
	SAVE_ITEM(EXYNOS5430_ENABLE_IP_MSCL_SECURE_SMMU_JPEG),
};

static struct sleep_save exynos_pd_g2d_clk_save[] = {
	SAVE_ITEM(EXYNOS5430_ENABLE_IP_G2D0),
	SAVE_ITEM(EXYNOS5430_ENABLE_IP_G2D1),
	SAVE_ITEM(EXYNOS5430_ENABLE_IP_G2D_SECURE_SMMU_G2D),
};

static struct sleep_save exynos_pd_isp_clk_save[] = {
	SAVE_ITEM(EXYNOS5430_ENABLE_IP_ISP0),
	SAVE_ITEM(EXYNOS5430_ENABLE_IP_ISP1),
	SAVE_ITEM(EXYNOS5430_ENABLE_IP_ISP2),
	SAVE_ITEM(EXYNOS5430_ENABLE_IP_ISP3),
};

static struct sleep_save exynos_pd_cam0_clk_save[] = {
	SAVE_ITEM(EXYNOS5430_ENABLE_IP_CAM00),
	SAVE_ITEM(EXYNOS5430_ENABLE_IP_CAM01),
	SAVE_ITEM(EXYNOS5430_ENABLE_IP_CAM02),
	SAVE_ITEM(EXYNOS5430_ENABLE_IP_CAM03),
};

static struct sleep_save exynos_pd_cam1_clk_save[] = {
	SAVE_ITEM(EXYNOS5430_ENABLE_IP_CAM10),
	SAVE_ITEM(EXYNOS5430_ENABLE_IP_CAM11),
	SAVE_ITEM(EXYNOS5430_ENABLE_IP_CAM12),
};

