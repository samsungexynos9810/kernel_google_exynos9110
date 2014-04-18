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

/* BLK_MFC clocks */
static struct exynos5430_pd_state iptop_mfc[] = {
	{ .reg = EXYNOS5430_ENABLE_IP_TOP,	.val = (1<<1), },
};
static struct exynos5430_pd_state aclktop_mfc[] = {
	{ .reg = EXYNOS5430_ENABLE_ACLK_TOP,    .val = (1<<3), },
};

/* BLK_HEVC clocks */
static struct exynos5430_pd_state iptop_hevc[] = {
	{ .reg = EXYNOS5430_ENABLE_IP_TOP,	.val = (1<<3), },
};
static struct exynos5430_pd_state aclktop_hevc[] = {
	{ .reg = EXYNOS5430_ENABLE_ACLK_TOP,    .val = (1<<5), },
};

/* BLK_GSCL clocks */
static struct exynos5430_pd_state iptop_gscl[] = {
	{ .reg = EXYNOS5430_ENABLE_IP_TOP,	.val = (1<<7), },
};
static struct exynos5430_pd_state aclktop_gscl[] = {
	{ .reg = EXYNOS5430_ENABLE_ACLK_TOP,    .val = (1<<14), },
	{ .reg = EXYNOS5430_ENABLE_ACLK_TOP,    .val = (1<<15), },
};

/* BLK_MSCL clocks */
static struct exynos5430_pd_state iptop_mscl[] = {
	{ .reg = EXYNOS5430_ENABLE_IP_TOP,	 .val = (1<<10), },
};
static struct exynos5430_pd_state aclktop_mscl[] = {
	{ .reg = EXYNOS5430_ENABLE_ACLK_TOP,     .val = (1<<19), },
};
static struct exynos5430_pd_state sclktop_mscl[] = {
	{ .reg = EXYNOS5430_ENABLE_SCLK_TOP_MSCL,       .val = (1<<0), },
};

/* BLK_G2D clocks */
static struct exynos5430_pd_state iptop_g2d[] = {
	{ .reg = EXYNOS5430_ENABLE_IP_TOP,	.val = (1<<0), },
};
static struct exynos5430_pd_state aclktop_g2d[] = {
	{ .reg = EXYNOS5430_ENABLE_ACLK_TOP,    .val = (1<<0), },
	{ .reg = EXYNOS5430_ENABLE_ACLK_TOP,    .val = (1<<2), },
};

/* BLK_ISP clocks */
static struct exynos5430_pd_state iptop_isp[] = {
	{ .reg = EXYNOS5430_ENABLE_IP_TOP,	 .val = (1<<4), },
};
static struct exynos5430_pd_state aclktop_isp[] = {
	{ .reg = EXYNOS5430_ENABLE_ACLK_TOP,     .val = (1<<6), },
	{ .reg = EXYNOS5430_ENABLE_ACLK_TOP,     .val = (1<<7), },
};

/* BLK_CAM0 clocks */
static struct exynos5430_pd_state iptop_cam0[] = {
	{ .reg = EXYNOS5430_ENABLE_IP_TOP,	 .val = (1<<5), },
};
static struct exynos5430_pd_state aclktop_cam0[] = {
	{ .reg = EXYNOS5430_ENABLE_ACLK_TOP,     .val = (1<<8), },
	{ .reg = EXYNOS5430_ENABLE_ACLK_TOP,     .val = (1<<9), },
	{ .reg = EXYNOS5430_ENABLE_ACLK_TOP,     .val = (1<<10), },
};

/* BLK_CAM1 clocks */
static struct exynos5430_pd_state iptop_cam1[] = {
	{ .reg = EXYNOS5430_ENABLE_IP_TOP,	 .val = (1<<6), },
};
static struct exynos5430_pd_state aclktop_cam1[] = {
	{ .reg = EXYNOS5430_ENABLE_ACLK_TOP,     .val = (1<<11), },
	{ .reg = EXYNOS5430_ENABLE_ACLK_TOP,     .val = (1<<12), },
	{ .reg = EXYNOS5430_ENABLE_ACLK_TOP,     .val = (1<<13), },
};
static struct exynos5430_pd_state sclktop_cam1[] = {
	{ .reg = EXYNOS5430_ENABLE_SCLK_TOP_CAM1,        .val = (1<<0), },
	{ .reg = EXYNOS5430_ENABLE_SCLK_TOP_CAM1,        .val = (1<<1), },
	{ .reg = EXYNOS5430_ENABLE_SCLK_TOP_CAM1,        .val = (1<<2), },
	{ .reg = EXYNOS5430_ENABLE_SCLK_TOP_CAM1,        .val = (1<<4), },
	{ .reg = EXYNOS5430_ENABLE_SCLK_TOP_CAM1,        .val = (1<<5), },
	{ .reg = EXYNOS5430_ENABLE_SCLK_TOP_CAM1,        .val = (1<<6), },
	{ .reg = EXYNOS5430_ENABLE_SCLK_TOP_CAM1,        .val = (1<<7), },
};

/* BLK_G3D clocks */
static struct exynos5430_pd_state iptop_g3d[] = {
	{ .reg = EXYNOS5430_ENABLE_IP_TOP,       .val = (1<<18), },
};
static struct exynos5430_pd_state aclktop_g3d[] = {
	{ .reg = EXYNOS5430_ENABLE_ACLK_TOP,     .val = (1<<30), },
};

/* BLK_DISP clocks */
static struct exynos5430_pd_state ipmif_disp[] = {
	{ .reg = EXYNOS5430_ENABLE_IP_MIF3,      .val = (1<<1), },
	{ .reg = EXYNOS5430_ENABLE_IP_MIF3,      .val = (1<<5), },
	{ .reg = EXYNOS5430_ENABLE_IP_MIF3,      .val = (1<<6), },
	{ .reg = EXYNOS5430_ENABLE_IP_MIF3,      .val = (1<<7), },
	{ .reg = EXYNOS5430_ENABLE_IP_MIF3,      .val = (1<<8), },
	{ .reg = EXYNOS5430_ENABLE_IP_MIF3,      .val = (1<<9), },
};
static struct exynos5430_pd_state aclkmif_disp[] = {
	{ .reg = EXYNOS5430_ENABLE_ACLK_MIF3,    .val = (1<<1), },
};
static struct exynos5430_pd_state sclkmif_disp[] = {
	{ .reg = EXYNOS5430_ENABLE_SCLK_MIF,     .val = (1<<5), },
	{ .reg = EXYNOS5430_ENABLE_SCLK_MIF,     .val = (1<<6), },
	{ .reg = EXYNOS5430_ENABLE_SCLK_MIF,     .val = (1<<7), },
	{ .reg = EXYNOS5430_ENABLE_SCLK_MIF,     .val = (1<<8), },
	{ .reg = EXYNOS5430_ENABLE_SCLK_MIF,     .val = (1<<9), },
};

static struct sleep_save exynos_pd_maudio_clk_save[] = {
	SAVE_ITEM(EXYNOS5430_SRC_SEL_AUD0),
	SAVE_ITEM(EXYNOS5430_ENABLE_IP_AUD0),
	SAVE_ITEM(EXYNOS5430_ENABLE_IP_AUD1),
};

static struct sleep_save exynos_pd_g3d_clk_save[] = {
	/* it causes sudden reset */
	/*SAVE_ITEM(EXYNOS5430_DIV_G3D),*/
	/* it causes system hang due to G3D_CLKOUT */
	/*SAVE_ITEM(EXYNOS5430_SRC_SEL_G3D),*/
	SAVE_ITEM(EXYNOS5430_ENABLE_IP_G3D0),
	SAVE_ITEM(EXYNOS5430_ENABLE_IP_G3D1),
};

static struct sleep_save exynos_pd_mfc_clk_save[] = {
	SAVE_ITEM(EXYNOS5430_DIV_MFC0),
	SAVE_ITEM(EXYNOS5430_SRC_SEL_MFC0),
	SAVE_ITEM(EXYNOS5430_ENABLE_IP_MFC00),
	SAVE_ITEM(EXYNOS5430_ENABLE_IP_MFC01),
	SAVE_ITEM(EXYNOS5430_ENABLE_IP_MFC0_SECURE_SMMU_MFC),
};

static struct sleep_save exynos_pd_hevc_clk_save[] = {
	SAVE_ITEM(EXYNOS5430_DIV_HEVC),
	SAVE_ITEM(EXYNOS5430_SRC_SEL_HEVC),
	SAVE_ITEM(EXYNOS5430_ENABLE_IP_HEVC0),
	SAVE_ITEM(EXYNOS5430_ENABLE_IP_HEVC1),
	SAVE_ITEM(EXYNOS5430_ENABLE_IP_HEVC_SECURE_SMMU_HEVC),
};

static struct sleep_save exynos_pd_gscl_clk_save[] = {
	SAVE_ITEM(EXYNOS5430_SRC_SEL_GSCL),
	SAVE_ITEM(EXYNOS5430_ENABLE_IP_GSCL0),
	SAVE_ITEM(EXYNOS5430_ENABLE_IP_GSCL1),
	SAVE_ITEM(EXYNOS5430_ENABLE_IP_GSCL_SECURE_SMMU_GSCL0),
	SAVE_ITEM(EXYNOS5430_ENABLE_IP_GSCL_SECURE_SMMU_GSCL1),
	SAVE_ITEM(EXYNOS5430_ENABLE_IP_GSCL_SECURE_SMMU_GSCL2),
};

static struct sleep_save exynos_pd_disp_clk_save[] = {
	SAVE_ITEM(EXYNOS5430_DIV_DISP),
	SAVE_ITEM(EXYNOS5430_SRC_SEL_MIF3),
	SAVE_ITEM(EXYNOS5430_SRC_SEL_DISP1),
	SAVE_ITEM(EXYNOS5430_SRC_SEL_MIF4),
	SAVE_ITEM(EXYNOS5430_SRC_SEL_MIF5),
	SAVE_ITEM(EXYNOS5430_SRC_SEL_MIF6),
	SAVE_ITEM(EXYNOS5430_SRC_SEL_DISP0),
	SAVE_ITEM(EXYNOS5430_ENABLE_IP_DISP0),
	SAVE_ITEM(EXYNOS5430_ENABLE_IP_DISP1),
	/* Because DISP_PLL_CON SFR is in DISP_BLK,
	 * it can lose its contents when blk power is down.
	 */
	SAVE_ITEM(EXYNOS5430_DISP_PLL_LOCK),
	SAVE_ITEM(EXYNOS5430_DISP_PLL_CON0),
	SAVE_ITEM(EXYNOS5430_DISP_PLL_CON1),
	SAVE_ITEM(EXYNOS5430_DISP_PLL_FREQ_DET),
};

static struct sleep_save exynos_pd_mscl_clk_save[] = {
	SAVE_ITEM(EXYNOS5430_DIV_MSCL),
	SAVE_ITEM(EXYNOS5430_SRC_SEL_MSCL0),
	SAVE_ITEM(EXYNOS5430_SRC_SEL_TOP_MSCL),
	SAVE_ITEM(EXYNOS5430_ENABLE_IP_MSCL0),
	SAVE_ITEM(EXYNOS5430_ENABLE_IP_MSCL1),
	SAVE_ITEM(EXYNOS5430_ENABLE_IP_MSCL_SECURE_SMMU_M2MSCALER0),
	SAVE_ITEM(EXYNOS5430_ENABLE_IP_MSCL_SECURE_SMMU_M2MSCALER1),
	SAVE_ITEM(EXYNOS5430_ENABLE_IP_MSCL_SECURE_SMMU_JPEG),
};

static struct sleep_save exynos_pd_g2d_clk_save[] = {
	SAVE_ITEM(EXYNOS5430_DIV_G2D),
	SAVE_ITEM(EXYNOS5430_SRC_SEL_G2D),
	SAVE_ITEM(EXYNOS5430_ENABLE_IP_G2D0),
	SAVE_ITEM(EXYNOS5430_ENABLE_IP_G2D1),
	SAVE_ITEM(EXYNOS5430_ENABLE_IP_G2D_SECURE_SMMU_G2D),
};

static struct sleep_save exynos_pd_isp_clk_save[] = {
	SAVE_ITEM(EXYNOS5430_DIV_ISP),
	SAVE_ITEM(EXYNOS5430_SRC_SEL_ISP),
	SAVE_ITEM(EXYNOS5430_ENABLE_IP_ISP0),
	SAVE_ITEM(EXYNOS5430_ENABLE_IP_ISP1),
	SAVE_ITEM(EXYNOS5430_ENABLE_IP_ISP2),
	SAVE_ITEM(EXYNOS5430_ENABLE_IP_ISP3),
};

static struct sleep_save exynos_pd_cam0_clk_save[] = {
	SAVE_ITEM(EXYNOS5430_DIV_CAM01),
	SAVE_ITEM(EXYNOS5430_DIV_CAM02),
	SAVE_ITEM(EXYNOS5430_DIV_CAM03),
	SAVE_ITEM(EXYNOS5430_SRC_SEL_CAM00),
	SAVE_ITEM(EXYNOS5430_SRC_SEL_CAM01),
	SAVE_ITEM(EXYNOS5430_ENABLE_IP_CAM00),
	SAVE_ITEM(EXYNOS5430_ENABLE_IP_CAM01),
	SAVE_ITEM(EXYNOS5430_ENABLE_IP_CAM02),
	SAVE_ITEM(EXYNOS5430_ENABLE_IP_CAM03),
};

static struct sleep_save exynos_pd_cam1_clk_save[] = {
	SAVE_ITEM(EXYNOS5430_DIV_CAM10),
	SAVE_ITEM(EXYNOS5430_DIV_CAM11),
	SAVE_ITEM(EXYNOS5430_SRC_SEL_CAM10),
	SAVE_ITEM(EXYNOS5430_SRC_SEL_TOP_CAM1),
	SAVE_ITEM(EXYNOS5430_SRC_SEL_CAM11),
	SAVE_ITEM(EXYNOS5430_ENABLE_IP_CAM10),
	SAVE_ITEM(EXYNOS5430_ENABLE_IP_CAM11),
	SAVE_ITEM(EXYNOS5430_ENABLE_IP_CAM12),
};

