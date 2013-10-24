/*
 * Copyright (c) 2013 Samsung Electronics Co., Ltd.
 * Author: Jiyun Kim
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Common Clock Framework support for Exynos5422 SoC.
 */

#include <linux/clk.h>
#include <linux/clkdev.h>
#include <linux/clk-provider.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <mach/regs-clock.h>

#include "clk.h"
#include "clk-pll.h"
#include "clk-exynos5422.h"


enum exynos5422_clks {
	none,
/* SCLK */
	/* CMU_TOP - bus */
	fin_pll,
	sclk_bpll,
	sclk_cpll,
	sclk_dpll,
	sclk_epll,
	sclk_rpll,
	sclk_ipll,
	sclk_spll,
	sclk_vpll,
	sclk_mpll,

	/* CMU_TOP - function */
		/* DISP1 */
	sclk_hdmiphy = 20, sclk_hdmi, sclk_pixel, sclk_dp1_ext_mst_vid,	sclk_mipi1, sclk_fimd1,
		/* MAU */
	sclk_mau_audio0 = 30, sclk_mau_pcm0,
		/* FSYS */
		/* FSYS2 */
	sclk_usbdrd300 = 40, sclk_usbdrd301,
	sclk_usbphy300, sclk_usbphy301,
	sclk_mmc0, sclk_mmc1, sclk_mmc2,
	sclk_unipro,
		/* ISP */
	sclk_isp_sensor0 = 50, sclk_isp_sensor1, sclk_isp_sensor2,
	sclk_pwm_isp, sclk_uart_isp,
		/* ETC */
	sclk_hsic_12m = 60,
		/* GSCL */
	sclk_gscl_wa = 70,
	sclk_gscl_wb,
		/* PERIC */
	sclk_pwm = 80,
	sclk_uart0, sclk_uart1, sclk_uart2, sclk_uart3, sclk_uart4,
	sclk_spi0, sclk_spi1, sclk_spi2,
	sclk_pcm2,
	sclk_i2s2,
	sclk_pcm1,
	sclk_i2s1,
	sclk_spdif,

	/* CMU_CPU */
	sclk_apll,

	/* CMU_KFC */
	sclk_kpll,

	/* CMU_CDREX */
	/* CMU_CPERI */
	sclk_bpll_ccore,
	sclk_cdrex,
	/* CMU_ISP */
	/* MSCL */
	/* MFC */
	/* G2D */
	/* DISP1 */
	/* GSCL */
	/* PSGEN */
	/* etc */
/* ACLK, PCLK */
	/* CMU_TOP - bus */
	aclk_200_fsys = 100,
		tsi, pdma0, pdma1, rtic, usbh20, usbd300, usbd301,

	pclk_200_rstop_fsys = 120,

	aclk_100_rstop_noc = 140, pclk_100_noc_rstop,

	aclk_400_noc_mscl = 160,

	aclk_400_noc_gscl = 180,

	aclk_noc_isp = 200,

	aclk_400_noc_mfc = 220,

	aclk_400_rstop_mem0 = 240,

	aclk_200_fsys2 = 260,
		pclk_200_rstop_fsys2,
		mmc0, mmc1, mmc2, sromc, ufs,

	aclk_200_disp1 = 280,
		dsim1, dp1, hdmi,

	aclk_400_mscl = 300,
		mscl0, mscl1, mscl2, smmu_mscl0, smmu_mscl1, smmu_mscl2,

	aclk_400_isp = 320,

	aclk_333 = 340,
		mfc, smmu_mfcl, smmu_mfcr,

	aclk_166 = 360,

	aclk_266 = 380,
		rotator, mdma1,	smmu_rotator, smmu_mdma1, mixer,

	aclk_266_isp = 400,
		fimc_fd,

	aclk_66_peric = 420,
		i2c0, i2c1, i2c2, i2c3, i2c_hdmi,
		i2c4, i2c5, i2c6, i2c7, i2c8, i2c9, i2c10,
		usi0, usi1,	usi2, usi3,	usi4, usi5,	usi6,
		i2s1, i2s2, pcm1, pcm2, pwm, spdif, tsadc,
		spi0, spi1, spi2,
		uart0, uart1, uart2, uart3,
		keyif, seckey,

	aclk_66_psgen = 460,
		mc, wdt, sysreg, abb_apbif, chipid,
		mct, rtc, tmu, tmu_gpu, hdmi_cec,
		tzpc0, tzpc1, tzpc2, tzpc3, tzpc4, tzpc5, tzpc6, tzpc7,	tzpc8, tzpc9, tzpc10, tzpc11,

	pclk_66_gpio = 490,

	aclk_333_432_isp0 = 500,

	aclk_333_432_isp =  520,
		sysmmu_fimc_isp,

	aclk_333_432_gscl = 540,
		smmu_3aa, smmu_fimcl0, smmu_fimcl1, smmu_fimcl3, fimc_lite3,

	aclk_300_gscl = 560,
		smmu_gscl0, smmu_gscl1, gscl_wa, gscl_wb, gscl0, gscl1, clk_3aa,

	aclk_300_disp1 = 580,
		fimd1,	smmu_fimd1,

	aclk_300_jpeg = 600,
		jpeg, jpeg2, smmu_jpeg,

	aclk_300_g3d = 620,

	aclk_266_g2d = 640,
		sss, slim_sss, mdma0,

	aclk_333_g2d = 660,
		g2d,

	aclk_400_disp1 = 680,

	aclk_g3d = 700,
		g3d,

/* GATE */
	/* CMU_TOP - bus */
	gate_bpll = 1000,
	gate_cpll,
	gate_dpll,
	gate_epll,
	gate_rpll,
	gate_ipll,
	gate_spll,
	gate_vpll,
	gate_mpll,

	/* CMU_TOP - function */
		/* DISP1 */
	gate_hdmiphy = 1020, gate_hdmi, gate_pixel, gate_dp1_ext_mst_vid, gate_mipi1, gate_fimd1,
		/* MAU */
	gate_mau_audio0 = 1030, gate_mau_pcm0,
		/* FSYS */
		/* FSYS2 */
	gate_usbdrd300 = 1040, gate_usbdrd301,
	gate_usbphy300, gate_usbphy301,
	gate_mmc0, gate_mmc1, gate_mmc2,
	gate_unipro,
		/* ISP */
	gate_isp_sensor0 = 1050, gate_isp_sensor1, gate_isp_sensor2,
	gate_pwm_isp, gate_uart_isp,
		/* ETC */
	gate_hsic_12m = 1060,
		/* GSCL */
	gate_gscl_wa = 1070,
	gate_gscl_wb,
		/* PERIC */
	gate_pwm = 1080,
	gate_uart0, gate_uart1, gate_uart2, gate_uart3, gate_uart4,
	gate_spi0, gate_spi1, gate_spi2,
	gate_pcm2,
	gate_i2s2,
	gate_pcm1,
	gate_i2s1,
	gate_spdif,

	/* CMU_CPU */
	gate_apll,

	/* CMU_KFC */
	gate_kpll,

	/* CMU_CDREX */
	gate_cdrex,
	/* CMU_CPERI */
	gate_bpll_ccore,
	/* CMU_ISP */
	/* MSCL */
	/* MFC */
	/* G2D */
	/* DISP1 */
	/* GSCL */
	/* PSGEN */
	/* etc */

	/* from 3.4 kernel */
	gate_bus_cpu = 1200,
	gate_sclk_cpu,
	gate_ip_core,
	gate_ip_sysrgt,
	gate_ip_syslft,
	gate_ip_g2d,
	gate_ip_acp,
	gate_ip_isp0,
	gate_ip_isp1,
	gate_sclk_isp,
	gate_sclk_disp1,
	gate_bus_gscl0,
	gate_bus_gscl1,
	gate_bus_disp0,
	gate_bus_disp1,
	gate_bus_mfc,
	gate_bus_g3d,
	gate_bus_gen,
	gate_bus_fsys0,
	gate_bus_fsys1,
	gate_bus_peric,
	gate_bus_peric1,
	gate_top_sclk_gscl,
	gate_top_sclk_disp0,
	gate_top_sclk_disp1,
	gate_top_sclk_gen,
	gate_top_sclk_mau,
	gate_top_sclk_fsys,
	gate_top_sclk_peric,
	gate_top_sclk_isp,
	gate_ip_gscl0,
	gate_ip_gscl,
	gate_ip_disp0,
	gate_ip_disp1,
	gate_ip_mfc,
	gate_ip_g3d,
	gate_ip_gen,
	gate_ip_fsys,
	gate_ip_peric,
	gate_ip_peris,
	gate_ip_mscl,
	gate_block,
	gate_bus_syslft,
	gate_bus_cdrex,
	gate_ip_cdrex,

	clkm_phy0 = 1500,
	clkm_phy1,
	clk_usbhost20,

/* MUX */
	/* CMU_TOP - bus */
	mout_bpll_ctrl_user = 2000,
	mout_cpll_ctrl,
	mout_dpll_ctrl,
	mout_epll_ctrl,
	mout_rpll_ctrl,
	mout_ipll_ctrl,
	mout_spll_ctrl,
	mout_vpll_ctrl,
	mout_mpll_ctrl,

	mout_aclk_200_fsys,
	mout_pclk_200_fsys,
	mout_aclk_100_noc,
	mout_aclk_400_wcore,
	mout_aclk_400_wcore_bpll,

	mout_aclk_200_fsys_sw,
	mout_pclk_200_fsys_sw,
	mout_aclk_100_noc_sw,
	mout_aclk_400_wcore_sw,

	mout_aclk_200_fsys_user,
	mout_pclk_200_fsys_user,
	mout_aclk_100_noc_user,
	mout_aclk_400_wcore_user,

	mout_aclk_200_fsys2,
	mout_aclk_200,
	mout_aclk_400_mscl,
	mout_aclk_400_isp,
	mout_aclk_333,
	mout_aclk_166,
	mout_aclk_266,

	mout_aclk_200_fsys2_sw,
	mout_aclk_200_sw,
	mout_aclk_400_mscl_sw,
	mout_aclk_400_isp_sw,
	mout_aclk_333_sw,
	mout_aclk_166_sw,
	mout_aclk_266_sw,

	mout_aclk_200_fsys2_user,
	mout_aclk_200_disp1_user,
	mout_aclk_400_mscl_user,
	mout_aclk_400_isp_user,
	mout_aclk_333_user,
	mout_aclk_166_user,
	mout_aclk_266_user,
	mout_aclk_266_isp_user,

	mout_aclk_66,
	mout_aclk_66_sw,
	mout_aclk_66_peric_user,
	mout_aclk_66_psgen_user,
	mout_aclk_66_gpio_user,

	mout_aclk_333_432_isp0,
	mout_aclk_333_432_isp,
	mout_aclk_333_432_gscl,
	mout_aclk_300_gscl,
	mout_aclk_300_disp1,
	mout_aclk_300_jpeg,
	mout_aclk_g3d,
	mout_aclk_266_g2d,
	mout_aclk_333_g2d,
	mout_aclk_400_disp1,
	mout_mau_epll_clk,
	mout_mx_mspll_ccore,
	mout_mx_mspll_cpu,
	mout_mx_mspll_kfc,

	mout_aclk_333_432_isp0_sw,
	mout_aclk_333_432_isp_sw,
	mout_aclk_333_432_gscl_sw,
	mout_aclk_300_gscl_sw,
	mout_aclk_300_disp1_sw,
	mout_aclk_300_jpeg_sw,
	mout_aclk_g3d_sw,
	mout_aclk_266_g2d_sw,
	mout_aclk_333_g2d_sw,
	mout_aclk_400_disp1_sw,

	mout_aclk_333_432_isp0_user,
	mout_aclk_333_432_isp_user,
	mout_aclk_333_432_gscl_user,
	mout_aclk_300_gscl_user,
	mout_aclk_300_disp1_user,
	mout_aclk_300_jpeg_user,
	mout_aclk_g3d_user,
	mout_aclk_266_g2d_user,
	mout_aclk_333_g2d_user,
	mout_aclk_400_disp1_user,

	/* CMU_TOP - function */
		/* DISP1 */
	mout_pixel = 2100,
	mout_dp1_ext_mst_vid,
	mout_mipi1,
	mout_fimd1_opt,
	mout_fimd1,
	mout_fimd1_final,
	mout_mdnie1,
	mout_fimd1_mdnie1,
	mout_hdmi,

		/* MAU */
	mout_mau_audio0,

		/* FSYS */
		/* FSYS2 */
	mout_usbdrd300,	mout_usbdrd301,
	mout_mmc0,	mout_mmc1,	mout_mmc2,
	mout_unipro,

		/* ISP */
	mout_isp_sensor,
	mout_pwm_isp,
	mout_uart_isp,
	mout_spi0_isp,
	mout_spi1_isp,

		/* ETC */
	mout_mphy_refclk,

		/* GSCL */

		/* PERIC */
	mout_pwm,
	mout_uart0,	mout_uart1,	mout_uart2,	mout_uart3,	mout_uart4,
	mout_spi0, mout_spi1, mout_spi2,
	mout_audio2, mout_audio1, mout_audio0,
	mout_spdif,

	/* CMU_CPU */
	mout_apll_ctrl,
	mout_cpu,

	/* CMU_KFC */
	mout_kpll_ctrl,
	mout_cpu_kfc,

	/* CMU_CDREX */
	/* CMU_CPERI */
	mout_bpll_ctrl,
	mout_mclk_cdrex,

	/* CMU_ISP */

	/* MSCL */

	/* MFC */

	/* G2D */

	/* DISP1 */

	/* GSCL */

	/* PSGEN */

	/* etc */

/* DEVIDER */
	/* CMU_TOP - bus */
	dout_aclk_200_fsys = 4000,
	dout_pclk_200_fsys,
	dout_aclk_100_noc,
	dout_aclk_400_wcore,
	dout_aclk_200_fsys2,
	dout_aclk_200,
	dout_aclk_400_mscl,
	dout_aclk_400_isp,
	dout_aclk_333,
	dout_aclk_166,
	dout_aclk_266,
	dout_aclk_66,
	dout_aclk_333_432_isp0,
	dout_aclk_333_432_isp,
	dout_aclk_333_432_gscl,
	dout_aclk_300_gscl,
	dout_aclk_300_disp1,
	dout_aclk_300_jpeg,
	dout_aclk_g3d,
	dout_aclk_266_g2d,
	dout_aclk_333_g2d,
	dout_aclk_400_disp1,

	/* CMU_TOP - function */
		/* DISP1 */
	dout_hdmi_pixel = 4100,
	dout_dp1_ext_mst_vid,
	dout_mipi1,
	dout_fimd1,

		/* MAU */
	dout_mau_audio0,
	dout_mau_pcm0,

		/* FSYS */
		/* FSYS2 */
	dout_usbdrd300,	dout_usbdrd301,
	dout_usbphy300,	dout_usbphy301,
	dout_mmc0, dout_mmc1, dout_mmc2,
	dout_unipro,

		/* ISP */
	dout_isp_sensor0, dout_isp_sensor1, dout_isp_sensor2,
	dout_pwm_isp,
	dout_uart_isp,
	dout_spi0_isp, dout_spi1_isp, dout_spi0_isp_pre, dout_spi1_isp_pre,

		/* ETC */
	dout_mphy_refclk,
	dout_ff,

		/* GSCL */

		/* PERIC */
	dout_pwm,
	dout_uart0,	dout_uart1,	dout_uart2,	dout_uart3,	dout_uart4,
	dout_spi0, dout_spi1, dout_spi2,
	dout_spi0_pre, dout_spi1_pre, dout_spi2_pre,
	dout_audio2, dout_pcm2, dout_i2s2,
	dout_audio1, dout_pcm1, dout_i2s1,
	dout_audio0,

	/* CMU_CPU */
	dout_arm2,
	dout_arm2_l,
	dout_arm2_h,
	dout_cpud,
	dout_atb,
	dout_pclk_dbg,
	dout_apll,

	/* CMU_KFC */
	dout_kfc,
	dout_aclk,
	dout_pclk,
	dout_kpll,

	/* CMU_CDREX */
	/* CMU_CPERI */
	dout_sclk_cdrex,
	dout_clk2x_phy0,
	dout_cclk_drex0,
	dout_pclk_drex0,
	dout_cclk_drex1,
	dout_pclk_drex1,
	dout_aclk_cdrex1,
	dout_pclk_cdrex,
	dout_pclk_core_mem,

	/* CMU_ISP */
	dout_ispdiv0_0,
	dout_mcuispdiv0,
	dout_mcuispdiv1,
	dout_ispdiv0,
	dout_ispdiv1,
	dout_ispdiv2,


	/* MSCL */
	dout_mscl_blk,

	/* MFC */
	dout_mfc_blk,

	/* G2D */
	dout_acp_pclk,

	/* DISP1 */
	dout_disp1_blk,

	/* GSCL */
	dout_gscl_blk300,
	dout_gscl_blk333,


	/* PSGEN */
	dout_gen_blk,
	dout_jpg_blk,

	/* etc */

	nr_clks,
};

static __initdata void *exynos5422_clk_regs[] = {
	EXYNOS5_CLK_SRC_CPU,
	EXYNOS5_CLK_DIV_CPU0,
	EXYNOS5_CLK_DIV_CPU1,
	EXYNOS5_CLK_GATE_BUS_CPU,
	EXYNOS5_CLK_GATE_SCLK_CPU,
	EXYNOS5_CLK_SRC_TOP0,
	EXYNOS5_CLK_SRC_TOP1,
	EXYNOS5_CLK_SRC_TOP2,
	EXYNOS5_CLK_SRC_TOP3,
	EXYNOS5_CLK_SRC_TOP4,
	EXYNOS5_CLK_SRC_TOP5,
	EXYNOS5_CLK_SRC_TOP6,
	EXYNOS5_CLK_SRC_TOP7,
	EXYNOS5_CLK_SRC_DISP10,
	EXYNOS5_CLK_SRC_MAUDIO,
	EXYNOS5_CLK_SRC_FSYS,
	EXYNOS5_CLK_SRC_PERIC0,
	EXYNOS5_CLK_SRC_PERIC1,
	EXYNOS5_CLK_SRC_TOP10,
	EXYNOS5_CLK_SRC_TOP11,
	EXYNOS5_CLK_SRC_TOP12,
	EXYNOS5_CLK_SRC_MASK_DISP10,
	EXYNOS5_CLK_SRC_MASK_FSYS,
	EXYNOS5_CLK_SRC_MASK_PERIC0,
	EXYNOS5_CLK_SRC_MASK_PERIC1,
	EXYNOS5_CLK_DIV_TOP0,
	EXYNOS5_CLK_DIV_TOP1,
	EXYNOS5_CLK_DIV_TOP2,
	EXYNOS5_CLK_DIV_DISP10,
	EXYNOS5_CLK_DIV_MAUDIO,
	EXYNOS5_CLK_DIV_FSYS0,
	EXYNOS5_CLK_DIV_FSYS1,
	EXYNOS5_CLK_DIV_FSYS2,
	EXYNOS5_CLK_DIV_PERIC0,
	EXYNOS5_CLK_DIV_PERIC1,
	EXYNOS5_CLK_DIV_PERIC2,
	EXYNOS5_CLK_DIV_PERIC3,
	EXYNOS5_CLK_DIV_PERIC4,
	EXYNOS5_CLK_GATE_BUS_TOP,
	EXYNOS5_CLK_GATE_BUS_FSYS0,
	EXYNOS5_CLK_GATE_BUS_PERIC,
	EXYNOS5_CLK_GATE_BUS_PERIC1,
	EXYNOS5_CLK_GATE_BUS_PERIS0,
	EXYNOS5_CLK_GATE_BUS_PERIS1,
	EXYNOS5_CLK_GATE_IP_GSCL0,
	EXYNOS5_CLK_GATE_IP_GSCL1,
	EXYNOS5_CLK_GATE_IP_MFC,
	EXYNOS5_CLK_GATE_IP_DISP1,
	EXYNOS5_CLK_GATE_IP_G3D,
	EXYNOS5_CLK_GATE_IP_GEN,
	EXYNOS5_CLK_GATE_IP_MSCL,
	EXYNOS5_CLK_GATE_TOP_SCLK_GSCL,
	EXYNOS5_CLK_GATE_TOP_SCLK_DISP1,
	EXYNOS5_CLK_GATE_TOP_SCLK_MAU,
	EXYNOS5_CLK_GATE_TOP_SCLK_FSYS,
	EXYNOS5_CLK_GATE_TOP_SCLK_PERIC,
	EXYNOS5_CLK_SRC_CDREX,
	EXYNOS5_CLK_GATE_BUS_CDREX,
	EXYNOS5_CLK_GATE_BUS_CDREX1,
	EXYNOS5_CLK_GATE_IP_CDREX,
	EXYNOS5_CLK_GATE_IP_FSYS,
	EXYNOS5_CLK_SRC_KFC,
	EXYNOS5_CLK_DIV_KFC0,
};

/* list of all parent clocks */
/* CMU_CPU  */
PNAME(mout_mx_mspll_cpu_p)     = { "sclk_cpll", "sclk_dpll", "sclk_mpll", "sclk_spll" };
PNAME(mout_apll_ctrl_p)                = { "fin_pll", "sclk_apll", };
PNAME(mout_cpu_p)              = { "mout_apll_ctrl" , "mout_mx_mspll_cpu" };

/* CMU_TOP */
PNAME(mout_bpll_ctrl_user_p) = { "fin_pll", "sclk_bpll", };
PNAME(mout_cpll_ctrl_p)		= { "fin_pll", "sclk_cpll", };
PNAME(mout_dpll_ctrl_p)		= { "fin_pll", "sclk_dpll", };
PNAME(mout_epll_ctrl_p)		= { "fin_pll", "sclk_epll", };
PNAME(mout_rpll_ctrl_p)		= { "fin_pll", "sclk_rpll", };
PNAME(mout_ipll_ctrl_p)		= { "fin_pll", "sclk_ipll", };
PNAME(mout_spll_ctrl_p)		= { "fin_pll", "sclk_spll", };
PNAME(mout_vpll_ctrl_p)		= { "fin_pll", "sclk_vpll", };
PNAME(mout_mpll_ctrl_p)		= { "fin_pll", "sclk_mpll", };

/*  group1_p child mux list

	mout_aclk_200_fsys,
	mout_pclk_200_fsys,
	mout_aclk_100_noc,
	mout_aclk_400_wcore,
	mout_aclk_200_fsys2,
	mout_aclk_200,
	mout_aclk_400_mscl,
	mout_aclk_400_isp,
	mout_aclk_333,
	mout_aclk_166,
	mout_aclk_266,
	mout_aclk_66,
*/
PNAME(group1_p)		= { "sclk_cpll", "sclk_dpll", "sclk_mpll" };
PNAME(group2_p)		= { "fin_pll", "sclk_cpll", "sclk_dpll", "sclk_mpll", "sclk_spll", "sclk_ipll", "sclk_epll", "sclk_rpll" };
PNAME(group3_p)		= { "sclk_rpll", "sclk_spll" };
PNAME(group4_p)		= { "sclk_ipll", "sclk_dpll", "sclk_mpll" };
PNAME(group5_p)		= { "sclk_vpll", "sclk_dpll" };

PNAME(mout_fimd1_mdnie1_p) = { "mout_fimd1", "mout_mdnie1" };
PNAME(mout_aclk_66_sw_p)	= { "dout_aclk66", "sclk_spll" };
PNAME(mout_aclk_66_peric_user_p)	= { "fin_pll", "mout_sw_aclk66" };

PNAME(mout_aclk_200_fsys_sw_p) = { "dout_aclk_200_fsys", "sclk_spll"};
PNAME(mout_aclk_200_fsys_user_p)	= { "fin_pll", "mout_aclk_200_fsys_sw" };

PNAME(mout_aclk_200_fsys2_sw_p) = { "dout_aclk_200_fsys2", "sclk_spll"};
PNAME(mout_aclk_200_fsys2_user_p)	= { "fin_pll", "mout_aclk200_fsys2_sw" };

PNAME(mout_aclk_200_sw_p) = { "dout_aclk_200", "sclk_spll"};

PNAME(mout_aclk_400_mscl_sw_p) = { "dout_aclk_400_mscl", "sclk_spll"};
PNAME(mout_aclk_400_mscl_user_p)	= { "fin_pll", "mout_aclk_400_mscl_sw" };

PNAME(mout_aclk_333_sw_p) = { "dout_aclk_333", "sclk_spll"};
PNAME(mout_aclk_333_user_p)	= { "fin_pll", "mout_aclk_333_sw" };

PNAME(mout_aclk_166_sw_p) = { "dout_aclk_166", "sclk_spll"};
PNAME(mout_aclk_166_user_p)	= { "fin_pll", "mout_aclk_166_sw" };

PNAME(mout_aclk_266_sw_p) = { "dout_aclk_266", "sclk_spll"};
PNAME(mout_aclk_266_user_p)	= { "fin_pll", "mout_aclk_266_sw" };

PNAME(mout_aclk_333_432_gscl_sw_p) = { "dout_aclk_333_432_gscl", "sclk_spll"};
PNAME(mout_aclk_333_432_gscl_user_p)	= { "fin_pll", "mout_aclk_333_432_gscl_sw" };

PNAME(mout_aclk_300_gscl_sw_p) = { "dout_aclk_300_gscl", "sclk_spll"};
PNAME(mout_aclk_300_gscl_user_p)	= { "fin_pll", "mout_aclk_300_gscl_sw" };

PNAME(mout_aclk_200_disp1_user_p)	= { "fin_pll", "mout_aclk_200_disp1_sw" };

PNAME(mout_aclk_300_disp1_sw_p) = { "dout_aclk_300_disp1", "sclk_spll"};
PNAME(mout_aclk_300_disp1_user_p)	= { "fin_pll", "mout_aclk_300_disp1_sw" };

PNAME(mout_aclk_300_jpeg_sw_p) = { "dout_aclk_300_jpeg", "sclk_spll"};
PNAME(mout_aclk_300_jpeg_user_p)	= { "fin_pll", "mout_aclk_300_jpeg_sw" };

PNAME(mout_aclk_g3d_sw_p) = { "dout_aclk_g3d", "sclk_spll"};
PNAME(mout_aclk_g3d_user_p)	= { "fin_pll", "mout_aclk_g3d_sw" };

PNAME(mout_aclk_266_g2d_sw_p) = { "dout_aclk_266_g2d", "sclk_spll"};
PNAME(mout_aclk_266_g2d_user_p)	= { "fin_pll", "mout_aclk266_g2d_sw" };

PNAME(mout_aclk_333_g2d_sw_p) = { "dout_aclk_333_g2d", "sclk_spll"};
PNAME(mout_aclk_333_g2d_user_p)	= { "fin_pll", "mout_aclk_333_g2d_sw" };

PNAME(mout_audio0_p)	= { "fin_pll", "cdclk0", "sclk_dpll", "sclk_mpll",
		  "sclk_spll", "sclk_ipll", "sclk_epll", "sclk_rpll" };
PNAME(mout_audio1_p)	= { "fin_pll", "cdclk1", "sclk_dpll", "sclk_mpll",
		  "sclk_spll", "sclk_ipll", "sclk_epll", "sclk_rpll" };
PNAME(mout_audio2_p)	= { "fin_pll", "cdclk2", "sclk_dpll", "sclk_mpll",
		  "sclk_spll", "sclk_ipll", "sclk_epll", "sclk_rpll" };
PNAME(mout_spdif_p)	= { "fin_pll", "dout_audio0", "dout_audio1", "dout_audio2",
		  "spdif_extclk", "sclk_ipll", "sclk_epll", "sclk_rpll" };
PNAME(mout_hdmi_p)	= { "sclk_hdmiphy", "dout_hdmi_pixel" };
PNAME(mout_mau_audio0_p)	= { "fin_pll", "mau_audiocdclk", "sclk_dpll", "sclk_mpll",
			  "sclk_spll", "sclk_ipll", "sclk_epll", "sclk_rpll" };
PNAME(mout_mclk_cdrex_p)	= { "sclk_bpll", "mout_mx_mspll_ccore" };
PNAME(mout_mx_mspll_ccore_p)	= { "sclk_cpll", "sclk_dpll", "sclk_mpll", "sclk_spll"};




#define CFRATE(_id, pname, f, rate) \
		FRATE(_id, #_id, pname, f, rate)
/* fixed rate clocks generated outside the soc */
struct samsung_fixed_rate_clock exynos5422_fixed_rate_ext_clks[] __initdata = {
	FRATE(fin_pll, "fin_pll", NULL, CLK_IS_ROOT, 0),
};

/* fixed rate clocks generated inside the soc */
struct samsung_fixed_rate_clock exynos5422_fixed_rate_clks[] __initdata = {
	FRATE(none, "sclk_hdmiphy", NULL, CLK_IS_ROOT, 24000000),
	FRATE(none, "sclk_pwi", NULL, CLK_IS_ROOT, 24000000),
	FRATE(none, "sclk_usbh20", NULL, CLK_IS_ROOT, 48000000),
	FRATE(none, "mphy_refclk_ixtal24", NULL, CLK_IS_ROOT, 48000000),
	FRATE(none, "sclk_usbh20_scan_clk", NULL, CLK_IS_ROOT, 480000000),
};

struct samsung_fixed_factor_clock exynos5422_fixed_factor_clks[] __initdata = {
	FFACTOR(none, "sclk_hsic_12m", "fin_pll", 1, 2, 0),
};

#define CMX(_id, cname, pnames, o, s, w) \
		MUX(_id, cname, pnames, (unsigned long)o, s, w)
#define CMUX(_id, o, s, w) \
		MUX(_id, #_id, _id##_p, (unsigned long)o, s, w)
#define CMUX_A(_id, o, s, w, a) \
		MUX_A(_id, #_id, _id##_p, (unsigned long)o, s, w, a)
struct samsung_mux_clock exynos5422_mux_clks[] __initdata = {
	CMUX(mout_mx_mspll_ccore, EXYNOS5_CLK_SRC_TOP7, 16, 2),
	CMUX(mout_mx_mspll_cpu, EXYNOS5_CLK_SRC_TOP7, 12, 2),

	CMUX(mout_apll_ctrl, EXYNOS5_CLK_SRC_CPU, 0, 1),
	CMUX(mout_cpu, EXYNOS5_CLK_SRC_CPU, 16, 1),
	CMUX(mout_bpll_ctrl_user, EXYNOS5_CLK_SRC_CDREX, 0, 1),
	MUX_A(none, "mout_aclk_400_mscl", group1_p,
			(unsigned long)EXYNOS5_CLK_SRC_TOP0, 4, 2, "aclk400_mscl"),
	CMX(mout_aclk_200, "mout_aclk_200", group1_p, EXYNOS5_CLK_SRC_TOP0, 8, 2),
	CMX(mout_aclk_200_fsys2, "mout_aclk_200_fsys2", group1_p, EXYNOS5_CLK_SRC_TOP0, 12, 2),
	CMX(mout_aclk_200_fsys, "mout_aclk_200_fsys", group1_p, EXYNOS5_CLK_SRC_TOP0, 28, 2),

	CMX(mout_aclk_333_432_gscl, "mout_aclk_333_432_gscl", group4_p, EXYNOS5_CLK_SRC_TOP1, 0, 2),
	CMX(mout_aclk_66_peric_user, "mout_aclk_66_peric_user", group1_p, EXYNOS5_CLK_SRC_TOP1, 8, 2),
	CMX(mout_aclk_266, "mout_aclk_266", group1_p, EXYNOS5_CLK_SRC_TOP1, 20, 2),
	CMX(mout_aclk_166, "mout_aclk_166", group1_p, EXYNOS5_CLK_SRC_TOP1, 24, 2),
	CMX(mout_aclk_333, "mout_aclk_333", group1_p, EXYNOS5_CLK_SRC_TOP1, 28, 2),

	CMX(mout_aclk_333_g2d, "mout_aclk_333_g2d", group1_p, EXYNOS5_CLK_SRC_TOP2, 8, 2),
	CMX(mout_aclk_266_g2d, "mout_aclk_266_g2d", group1_p, EXYNOS5_CLK_SRC_TOP2, 12, 2),
	CMX(mout_aclk_g3d, "mout_aclk_g3d", group5_p, EXYNOS5_CLK_SRC_TOP2, 16, 1),
	CMX(mout_aclk_300_jpeg, "mout_aclk_300_jpeg", group1_p, EXYNOS5_CLK_SRC_TOP2, 20, 2),
	CMX(mout_aclk_300_disp1, "mout_aclk_300_disp1", group1_p, EXYNOS5_CLK_SRC_TOP2, 24, 2),
	CMX(mout_aclk_300_gscl, "mout_aclk_300_gscl", group1_p, EXYNOS5_CLK_SRC_TOP2, 28, 2),

	CMUX(mout_aclk_400_mscl_user, EXYNOS5_CLK_SRC_TOP3, 4, 1),

	CMUX_A(mout_aclk_200_disp1_user, EXYNOS5_CLK_SRC_TOP3, 8, 1, "aclk_200_disp1"),

	CMUX(mout_aclk_200_fsys2_user, EXYNOS5_CLK_SRC_TOP3, 12, 1),
	CMUX(mout_aclk_200_fsys_user, EXYNOS5_CLK_SRC_TOP3, 28, 1),

	CMUX(mout_aclk_333_432_gscl_user, EXYNOS5_CLK_SRC_TOP4, 0, 1),
	CMUX(mout_aclk_66_peric_user, EXYNOS5_CLK_SRC_TOP4, 8, 1),
	CMUX(mout_aclk_266_user, EXYNOS5_CLK_SRC_TOP4, 20, 1),
	CMUX(mout_aclk_166_user, EXYNOS5_CLK_SRC_TOP4, 24, 1),
	CMUX(mout_aclk_333_user, EXYNOS5_CLK_SRC_TOP4, 28, 1),

	CMX(mout_aclk_66_psgen_user, "mout_aclk_66_psgen_user", mout_aclk_66_peric_user_p, EXYNOS5_CLK_SRC_TOP5, 4, 1),
	CMUX(mout_aclk_333_g2d_user, EXYNOS5_CLK_SRC_TOP5, 8, 1),
	CMUX(mout_aclk_266_g2d_user, EXYNOS5_CLK_SRC_TOP5, 12, 1),
	CMUX_A(mout_aclk_g3d_user, EXYNOS5_CLK_SRC_TOP5, 16, 1, "mout_aclk_g3d_user"),
	CMUX(mout_aclk_300_jpeg_user, EXYNOS5_CLK_SRC_TOP5, 20, 1),
	CMUX(mout_aclk_300_disp1_user, EXYNOS5_CLK_SRC_TOP5, 24, 1),
	CMUX(mout_aclk_300_gscl_user, EXYNOS5_CLK_SRC_TOP5, 28, 1),

	CMUX(mout_mpll_ctrl, EXYNOS5_CLK_SRC_TOP6, 0, 1),
	CMUX(mout_vpll_ctrl, EXYNOS5_CLK_SRC_TOP6, 4, 1),
	CMUX(mout_spll_ctrl, EXYNOS5_CLK_SRC_TOP6, 8, 1),
	CMUX(mout_ipll_ctrl, EXYNOS5_CLK_SRC_TOP6, 12, 1),
	CMUX(mout_rpll_ctrl, EXYNOS5_CLK_SRC_TOP6, 16, 1),
	CMUX(mout_epll_ctrl, EXYNOS5_CLK_SRC_TOP6, 20, 1),
	CMUX(mout_dpll_ctrl, EXYNOS5_CLK_SRC_TOP6, 24, 1),
	CMUX(mout_cpll_ctrl, EXYNOS5_CLK_SRC_TOP6, 28, 1),

	CMUX(mout_aclk_400_mscl_sw, EXYNOS5_CLK_SRC_TOP10, 4, 1),
	CMUX(mout_aclk_200_sw, EXYNOS5_CLK_SRC_TOP10, 8, 1),
	CMUX(mout_aclk_200_fsys2_sw, EXYNOS5_CLK_SRC_TOP10, 12, 1),
	CMUX(mout_aclk_200_fsys_sw, EXYNOS5_CLK_SRC_TOP10, 28, 1),

	CMUX(mout_aclk_333_432_gscl_sw, EXYNOS5_CLK_SRC_TOP11, 0, 1),
	CMUX(mout_aclk_66_sw, EXYNOS5_CLK_SRC_TOP11, 8, 1),
	CMUX(mout_aclk_266_sw, EXYNOS5_CLK_SRC_TOP11, 20, 1),
	CMUX(mout_aclk_166_sw, EXYNOS5_CLK_SRC_TOP11, 24, 1),
	CMUX(mout_aclk_333_sw, EXYNOS5_CLK_SRC_TOP11, 28, 1),


	CMUX(mout_aclk_333_g2d_sw, EXYNOS5_CLK_SRC_TOP12, 8, 1),
	CMUX(mout_aclk_266_g2d_sw, EXYNOS5_CLK_SRC_TOP12, 12, 1),
	CMUX(mout_aclk_g3d_sw, EXYNOS5_CLK_SRC_TOP12, 16, 1),
	CMUX(mout_aclk_300_jpeg_sw, EXYNOS5_CLK_SRC_TOP12, 20, 1),
	CMUX(mout_aclk_300_disp1_sw, EXYNOS5_CLK_SRC_TOP12, 24, 1),
	CMUX(mout_aclk_300_gscl_sw, EXYNOS5_CLK_SRC_TOP12, 28, 1),


	CMX(mout_fimd1, "mout_fimd1", group3_p, EXYNOS5_CLK_SRC_DISP10, 4, 1),
	CMX(mout_mdnie1, "mout_mdine1", group2_p, EXYNOS5_CLK_SRC_DISP10, 8, 3),
	CMX(mout_fimd1_mdnie1, "mout_fimd1_mdnie1", mout_fimd1_mdnie1_p, EXYNOS5_CLKSRC_CMUTOP_SPARE2, 8, 1),
	CMX(mout_mipi1, "mout_mipi1", group2_p, EXYNOS5_CLK_SRC_DISP10, 16, 3),
	CMX(mout_dp1_ext_mst_vid, "mout_dp1_ext_mst_vid", group2_p, EXYNOS5_CLK_SRC_DISP10, 20, 3),
	CMX(mout_pixel, "mout_pixel", group2_p, EXYNOS5_CLK_SRC_DISP10, 24, 3),
	CMUX(mout_hdmi, EXYNOS5_CLK_SRC_DISP10, 28, 1),

	CMUX(mout_mau_audio0, EXYNOS5_CLK_SRC_MAUDIO, 28, 3),

	CMX(mout_usbdrd301, "mout_usbdrd301", group2_p, EXYNOS5_CLK_SRC_FSYS, 4, 3),
	CMX(mout_mmc0, "mout_mmc0", group2_p, EXYNOS5_CLK_SRC_FSYS, 8, 3),
	CMX(mout_mmc1, "mout_mmc1", group2_p, EXYNOS5_CLK_SRC_FSYS, 12, 3),
	CMX(mout_mmc2, "mout_mmc2", group2_p, EXYNOS5_CLK_SRC_FSYS, 16, 3),
	CMX(mout_usbdrd300, "mout_usbdrd300", group2_p, EXYNOS5_CLK_SRC_FSYS, 20, 3),
	CMX(mout_unipro, "mout_unipro", group2_p, EXYNOS5_CLK_SRC_FSYS, 24, 3),

	CMX(mout_uart0, "mout_uart0", group2_p, EXYNOS5_CLK_SRC_PERIC0, 4, 3),
	CMX(mout_uart1, "mout_uart1", group2_p, EXYNOS5_CLK_SRC_PERIC0, 8, 3),
	CMX(mout_uart2, "mout_uart2", group2_p, EXYNOS5_CLK_SRC_PERIC0, 12, 3),
	CMX(mout_uart3, "mout_uart3", group2_p, EXYNOS5_CLK_SRC_PERIC0, 16, 3),
	CMX(mout_pwm, "mout_pwm", group2_p, EXYNOS5_CLK_SRC_PERIC0, 24, 3),
	CMUX(mout_spdif, EXYNOS5_CLK_SRC_PERIC0, 28, 3),

	CMUX(mout_audio0, EXYNOS5_CLK_SRC_PERIC1, 8, 3),
	CMUX(mout_audio1, EXYNOS5_CLK_SRC_PERIC1, 12, 3),
	CMUX(mout_audio2, EXYNOS5_CLK_SRC_PERIC1, 16, 3),

	CMX(mout_spi0, "mout_spi0", group2_p, EXYNOS5_CLK_SRC_PERIC1, 20, 3),
	CMX(mout_spi1, "mout_spi1", group2_p, EXYNOS5_CLK_SRC_PERIC1, 24, 3),
	CMX(mout_spi2, "mout_spi2", group2_p, EXYNOS5_CLK_SRC_PERIC1, 28, 3),
	CMUX(mout_mclk_cdrex, EXYNOS5_CLK_SRC_PERIC1, 28, 3),
};

#define CDV(_id, cname, pname, o, s, w) \
		DIV(_id, cname, pname, (unsigned long)o, s, w)
#define CDIV(_id, pname, o, s, w) \
		DIV(_id, #_id, pname, (unsigned long)o, s, w)
#define CDIV_A(_id, pname, o, s, w, a) \
		DIV_A(_id, #_id, pname, (unsigned long)o, s, w, a)
struct samsung_div_clock exynos5422_div_clks[] __initdata = {
	CDIV(dout_arm2_l, "mout_cpu", EXYNOS5_CLK_DIV_CPU0, 0, 3),
	CDIV(dout_arm2_h, "mout_cpu", EXYNOS5_CLK_DIV_CPU0, 28, 3),
	CDIV(dout_apll, "mout_apll_ctrl", EXYNOS5_CLK_DIV_CPU0, 24, 3),
	CDIV(dout_cpud, "dout_arm2_l", EXYNOS5_CLK_DIV_CPU0, 4, 3),
	CDIV(dout_atb, "dout_arm2_l", EXYNOS5_CLK_DIV_CPU0, 16, 3),
	CDIV(dout_pclk_dbg, "dout_arm2_l", EXYNOS5_CLK_DIV_CPU0, 20, 3),

    /* CMU_TOP */
	CDIV(dout_aclk_200_fsys, "mout_aclk_200_fsys", EXYNOS5_CLK_DIV_TOP0, 28, 3),

    CDIV(dout_pclk_200_fsys, "mout_pclk_200_fsys", EXYNOS5_CLK_DIV_TOP0, 24, 3),
    CDIV(dout_aclk_100_noc, "mout_aclk_100_noc_user", EXYNOS5_CLK_DIV_TOP0, 20, 3),
    CDIV(dout_aclk_400_wcore, "mout_aclk_400_wcore_user", EXYNOS5_CLK_DIV_TOP0, 16, 3),
    CDIV(dout_aclk_200_fsys2, "mout_aclk_200_fsys2", EXYNOS5_CLK_DIV_TOP0, 12, 3),
    CDIV(dout_aclk_200, "mout_aclk_200", EXYNOS5_CLK_DIV_TOP0, 8, 3),
    CDIV(dout_aclk_400_mscl, "mout_aclk_400_mscl", EXYNOS5_CLK_DIV_TOP0, 4, 3),
    CDIV(dout_aclk_400_isp, "mout_aclk_400_isp", EXYNOS5_CLK_DIV_TOP0, 0, 3),
	CDIV(dout_aclk_333, "mout_aclk_333", EXYNOS5_CLK_DIV_TOP1, 28, 3),

    CDIV(dout_aclk_166, "mout_aclk_166", EXYNOS5_CLK_DIV_TOP1, 24, 3),
    CDIV(dout_aclk_266, "mout_aclk_266", EXYNOS5_CLK_DIV_TOP1, 20, 3),
    CDIV(dout_aclk_66, "mout_aclk_66", EXYNOS5_CLK_DIV_TOP1, 8, 6),
    /* TODO: dout_aclk_333_432_isp0 */
    CDIV(dout_aclk_333_432_isp, "mout_aclk_333_432_isp",    EXYNOS5_CLK_DIV_TOP1, 4, 3),
    CDIV(dout_aclk_333_432_gscl, "mout_aclk_333_432_gscl",  EXYNOS5_CLK_DIV_TOP1, 0, 3),
    CDIV(dout_aclk_300_disp1, "mout_aclk_300_disp1", EXYNOS5_CLK_DIV_TOP2, 24, 3),
	CDIV(dout_aclk_300_jpeg, "mout_aclk_300_jpeg", EXYNOS5_CLK_DIV_TOP2, 20, 3),

    CDIV(dout_aclk_g3d, "mout_aclk_g3d", EXYNOS5_CLK_DIV_TOP2, 16, 3),
    CDIV(dout_aclk_266_g2d, "mout_aclk_266_g2d", EXYNOS5_CLK_DIV_TOP2, 12, 3),
    CDIV(dout_aclk_333_g2d, "mout_aclk_333_g2d", EXYNOS5_CLK_DIV_TOP2, 8, 3),
    CDIV(dout_aclk_400_disp1, "mout_aclk_400_disp1", EXYNOS5_CLK_DIV_TOP2, 4, 3),
	CDIV(dout_aclk_300_gscl, "mout_aclk_300_gscl", EXYNOS5_CLK_DIV_TOP2, 28, 3),

	/* DISP1 Block */
	CDIV(dout_fimd1, "mout_fimd1_mdnie1", EXYNOS5_CLK_DIV_DISP10, 0, 4),
	CDIV(dout_mipi1, "mout_mipi1", EXYNOS5_CLK_DIV_DISP10, 16, 8),
	CDIV(dout_dp1_ext_mst_vid, "mout_dp1_ext_mst_vid", EXYNOS5_CLK_DIV_DISP10, 24, 4),
	CDIV(dout_hdmi_pixel, "mout_pixel", EXYNOS5_CLK_DIV_DISP10, 28, 4),

	/* Audio Block */
	CDIV(dout_mau_audio0, "mout_mau_audio0", EXYNOS5_CLK_DIV_MAUDIO, 20, 4),
	CDIV(dout_mau_pcm0, "dout_mau_audio0", EXYNOS5_CLK_DIV_MAUDIO, 24, 8),

	/* USB3.0 */
	CDIV(dout_usbphy301, "mout_usbdrd301", EXYNOS5_CLK_DIV_FSYS0, 12, 4),
	CDIV(dout_usbphy300, "mout_usbdrd300", EXYNOS5_CLK_DIV_FSYS0, 16, 4),
	CDIV(dout_usbdrd301, "mout_usbdrd301", EXYNOS5_CLK_DIV_FSYS0, 20, 4),
	CDIV(dout_usbdrd300, "mout_usbdrd300", EXYNOS5_CLK_DIV_FSYS0, 24, 4),

	/* MMC */
	CDIV(dout_mmc0, "mout_mmc0", EXYNOS5_CLK_DIV_FSYS1, 0, 10),
	CDIV(dout_mmc1, "mout_mmc1", EXYNOS5_CLK_DIV_FSYS1, 10, 10),
	CDIV(dout_mmc2, "mout_mmc2", EXYNOS5_CLK_DIV_FSYS1, 20, 10),

	CDIV(dout_unipro, "mout_unipro", EXYNOS5_CLK_DIV_FSYS2, 24, 8),

	/* UART and PWM */
	CDIV(dout_uart0, "mout_uart0", EXYNOS5_CLK_DIV_PERIC0, 8, 4),
	CDIV(dout_uart1, "mout_uart1", EXYNOS5_CLK_DIV_PERIC0, 12, 4),
	CDIV(dout_uart2, "mout_uart2", EXYNOS5_CLK_DIV_PERIC0, 16, 4),
	CDIV(dout_uart3, "mout_uart3", EXYNOS5_CLK_DIV_PERIC0, 20, 4),
	CDIV(dout_pwm, "mout_pwm", EXYNOS5_CLK_DIV_PERIC0, 28, 4),

	/* SPI */
	CDIV(dout_spi0, "mout_spi0", EXYNOS5_CLK_DIV_PERIC1, 20, 4),
	CDIV(dout_spi1, "mout_spi1", EXYNOS5_CLK_DIV_PERIC1, 24, 4),
	CDIV(dout_spi2, "mout_spi2", EXYNOS5_CLK_DIV_PERIC1, 28, 4),

	/* PCM */
	CDIV(dout_pcm1, "dout_audio1", EXYNOS5_CLK_DIV_PERIC2, 16, 8),
	CDIV(dout_pcm2, "dout_audio2", EXYNOS5_CLK_DIV_PERIC2, 24, 8),

	/* Audio - I2S */
	CDIV(dout_i2s1, "dout_audio1", EXYNOS5_CLK_DIV_PERIC3, 6, 6),
	CDIV(dout_i2s2, "dout_audio2", EXYNOS5_CLK_DIV_PERIC3, 12, 6),
	CDIV(dout_audio0, "mout_audio0", EXYNOS5_CLK_DIV_PERIC3, 20, 4),
	CDIV(dout_audio1, "mout_audio1", EXYNOS5_CLK_DIV_PERIC3, 24, 4),
	CDIV(dout_audio2, "mout_audio2", EXYNOS5_CLK_DIV_PERIC3, 28, 4),

	/* SPI Pre-Ratio */
	CDIV(dout_spi0_pre, "dout_spi0", EXYNOS5_CLK_DIV_PERIC4, 8, 8),
	CDIV(dout_spi1_pre, "dout_spi1", EXYNOS5_CLK_DIV_PERIC4, 16, 8),
	CDIV(dout_spi2_pre, "dout_spi2", EXYNOS5_CLK_DIV_PERIC4, 24, 8),

	/* DREX */
	CDIV(dout_sclk_cdrex, "mout_mclk_cdrex", EXYNOS5_CLK_DIV_CDREX0, 24, 3),
};

#define CGATE(_id, cname, pname, o, b, f, gf) \
		GATE(_id, cname, pname, (unsigned long)o, b, f, gf)

struct samsung_gate_clock exynos5422_gate_clks[] __initdata = {

    GATE_A(mct, "pclk_st", "aclk66_psgen",
            (unsigned long) EXYNOS5_CLK_GATE_BUS_PERIS1, 2, 0, 0, "mct"),

	/* CMU_TOP */
	CGATE(aclk_200_fsys, "aclk_200_fsys", "mout_aclk_200_fsys_user",
			EXYNOS5_CLK_GATE_BUS_FSYS0, 9, CLK_IGNORE_UNUSED, 0),
	/* TODO: pclk_200_rstop_fsys, aclk_100_rstop_noc, aclk_100_noc_rstop,
	   aclk_400_noc_mscl, aclk_400_noc_gscl, aclk_400_noc_isp,
	   aclk_400_noc_mfc, aclk_400_rstop_mem0
	   */
	CGATE(aclk_200_fsys2, "aclk_200_fsys2", "mout_aclk200_fsys2_user",
			EXYNOS5_CLK_GATE_BUS_FSYS0, 10, CLK_IGNORE_UNUSED, 0),
	/* TODO: pclk_200_rstop_fsys2 */
	CGATE(aclk_200_disp1, "aclk_200_disp1", "mout_aclk_200_disp1_user",
			EXYNOS5_CLK_GATE_BUS_TOP, 18, CLK_IGNORE_UNUSED, 0),
	CGATE(aclk_400_mscl, "aclk_400_mscl", "mout_aclk_400_mscl_user",
			EXYNOS5_CLK_GATE_BUS_TOP, 17, CLK_IGNORE_UNUSED, 0),
	CGATE(aclk_400_isp, "aclk_400_isp", "mout_aclk_400_isp_user",
			EXYNOS5_CLK_GATE_BUS_TOP, 16, CLK_IGNORE_UNUSED, 0),
	CGATE(aclk_66_peric, "aclk_66_peric", "mout_aclk_66_peric_user",
			EXYNOS5_CLK_GATE_BUS_TOP, 11, 0, 0),
	CGATE(aclk_66_psgen, "aclk_66_psgen", "mout_aclk_66_psgen_user",
			EXYNOS5_CLK_GATE_BUS_TOP, 10, CLK_IGNORE_UNUSED, 0),
	CGATE(pclk_66_gpio, "pclk_66_gpio", "mout_pclk_66_gpio_user",
			EXYNOS5_CLK_GATE_BUS_TOP, 9, CLK_IGNORE_UNUSED, 0),
	CGATE(aclk_333_432_isp, "aclk_333_432_isp", "mout_aclk_333_432_isp_user",
			EXYNOS5_CLK_GATE_BUS_TOP, 8, CLK_IGNORE_UNUSED, 0),
	CGATE(aclk_333_432_gscl, "aclk_333_432_gscl", "mout_aclk_333_432_gscl_user",
			EXYNOS5_CLK_GATE_BUS_TOP, 7, CLK_IGNORE_UNUSED, 0),
	CGATE(aclk_300_gscl, "aclk_300_gscl", "mout_aclk_300_gscl_user",
			EXYNOS5_CLK_GATE_BUS_TOP, 6, CLK_IGNORE_UNUSED, 0),
	CGATE(aclk_333_432_isp0, "aclk_333_432_isp0", "mout_aclk_300_gscl_jpeg",
			EXYNOS5_CLK_GATE_BUS_TOP, 5, CLK_IGNORE_UNUSED, 0),
	CGATE(aclk_300_jpeg, "aclk_300_jpeg", "mout_aclk_300_jpeg_user",
			EXYNOS5_CLK_GATE_BUS_TOP, 4, CLK_IGNORE_UNUSED, 0),
	CGATE(aclk_333_g2d, "aclk_333_g2d", "mout_aclk_333_g2d_user",
			EXYNOS5_CLK_GATE_BUS_TOP, 0, CLK_IGNORE_UNUSED, 0),
	CGATE(aclk_266_g2d, "aclk_266_g2d", "mout_aclk_266_g2d_user",
			EXYNOS5_CLK_GATE_BUS_TOP, 1, CLK_IGNORE_UNUSED, 0),
	CGATE(aclk_166, "aclk_166", "mout_aclk_166_user",
			EXYNOS5_CLK_GATE_BUS_TOP, 14, CLK_IGNORE_UNUSED, 0),
	CGATE(aclk_333, "aclk_333", "mout_aclk_333",
			EXYNOS5_CLK_GATE_BUS_TOP, 15, CLK_IGNORE_UNUSED, 0),

	/* sclk */
	CGATE(sclk_uart0, "sclk_uart0", "dout_uart0",
		EXYNOS5_CLK_GATE_TOP_SCLK_PERIC, 0, CLK_SET_RATE_PARENT, 0),
	CGATE(sclk_uart1, "sclk_uart1", "dout_uart1",
		EXYNOS5_CLK_GATE_TOP_SCLK_PERIC, 1, CLK_SET_RATE_PARENT, 0),
	CGATE(sclk_uart2, "sclk_uart2", "dout_uart2",
		EXYNOS5_CLK_GATE_TOP_SCLK_PERIC, 2, CLK_SET_RATE_PARENT, 0),
	CGATE(sclk_uart3, "sclk_uart3", "dout_uart3",
		EXYNOS5_CLK_GATE_TOP_SCLK_PERIC, 3, CLK_SET_RATE_PARENT, 0),
	CGATE(sclk_spi0, "sclk_spi0", "dout_pre_spi0",
		EXYNOS5_CLK_GATE_TOP_SCLK_PERIC, 6, CLK_SET_RATE_PARENT, 0),
	CGATE(sclk_spi1, "sclk_spi1", "dout_pre_spi1",
		EXYNOS5_CLK_GATE_TOP_SCLK_PERIC, 7, CLK_SET_RATE_PARENT, 0),
	CGATE(sclk_spi2, "sclk_spi2", "dout_pre_spi2",
		EXYNOS5_CLK_GATE_TOP_SCLK_PERIC, 8, CLK_SET_RATE_PARENT, 0),
	CGATE(sclk_spdif, "sclk_spdif", "mout_spdif",
		EXYNOS5_CLK_GATE_TOP_SCLK_PERIC, 9, CLK_SET_RATE_PARENT, 0),
	CGATE(sclk_pwm, "sclk_pwm", "dout_pwm",
		EXYNOS5_CLK_GATE_TOP_SCLK_PERIC, 11, CLK_SET_RATE_PARENT, 0),
	CGATE(sclk_pcm1, "sclk_pcm1", "dout_pcm1",
		EXYNOS5_CLK_GATE_TOP_SCLK_PERIC, 15, CLK_SET_RATE_PARENT, 0),
	CGATE(sclk_pcm2, "sclk_pcm2", "dout_pcm2",
		EXYNOS5_CLK_GATE_TOP_SCLK_PERIC, 16, CLK_SET_RATE_PARENT, 0),
	CGATE(sclk_i2s1, "sclk_i2s1", "dout_i2s1",
		EXYNOS5_CLK_GATE_TOP_SCLK_PERIC, 17, CLK_SET_RATE_PARENT, 0),
	CGATE(sclk_i2s2, "sclk_i2s2", "dout_i2s2",
		EXYNOS5_CLK_GATE_TOP_SCLK_PERIC, 18, CLK_SET_RATE_PARENT, 0),

	CGATE(sclk_mmc0, "sclk_mmc0", "dout_mmc0",
		EXYNOS5_CLK_GATE_TOP_SCLK_FSYS, 0, CLK_SET_RATE_PARENT, 0),
	CGATE(sclk_mmc1, "sclk_mmc1", "dout_mmc1",
		EXYNOS5_CLK_GATE_TOP_SCLK_FSYS, 1, CLK_SET_RATE_PARENT, 0),
	CGATE(sclk_mmc2, "sclk_mmc2", "dout_mmc2",
		EXYNOS5_CLK_GATE_TOP_SCLK_FSYS, 2, CLK_SET_RATE_PARENT, 0),
	CGATE(sclk_usbphy301, "sclk_usbphy301", "dout_usbphy301",
		EXYNOS5_CLK_GATE_TOP_SCLK_FSYS, 7, CLK_SET_RATE_PARENT, 0),
	CGATE(sclk_usbphy300, "sclk_usbphy300", "dout_usbphy300",
		EXYNOS5_CLK_GATE_TOP_SCLK_FSYS, 8, CLK_SET_RATE_PARENT, 0),
	CGATE(sclk_usbdrd300, "sclk_usbdrd300", "dout_usbdrd300",
		EXYNOS5_CLK_GATE_TOP_SCLK_FSYS, 9, CLK_SET_RATE_PARENT, 0),
	CGATE(sclk_usbdrd301, "sclk_usbdrd301", "dout_usbdrd301",
		EXYNOS5_CLK_GATE_TOP_SCLK_FSYS, 10, CLK_SET_RATE_PARENT, 0),

	CGATE(sclk_usbdrd301, "sclk_unipro", "dout_unipro",
		EXYNOS5_CLK_SRC_MASK_FSYS, 24, CLK_SET_RATE_PARENT, 0),

	CGATE(sclk_gscl_wa, "sclk_gscl_wa", "aclK333_432_gscl",
		EXYNOS5_CLK_GATE_TOP_SCLK_GSCL, 6, CLK_SET_RATE_PARENT, 0),
	CGATE(sclk_gscl_wb, "sclk_gscl_wb", "aclk333_432_gscl",
		EXYNOS5_CLK_GATE_TOP_SCLK_GSCL, 7, CLK_SET_RATE_PARENT, 0),

	/* Display */
	CGATE(sclk_fimd1, "sclk_fimd1", "dout_fimd1",
		EXYNOS5_CLK_GATE_TOP_SCLK_DISP1, 0, CLK_SET_RATE_PARENT, 0),
	CGATE(sclk_mipi1, "sclk_mipi1", "dout_mipi1",
		EXYNOS5_CLK_GATE_TOP_SCLK_DISP1, 3, CLK_SET_RATE_PARENT, 0),
	CGATE(sclk_hdmi, "sclk_hdmi", "mout_hdmi",
		EXYNOS5_CLK_GATE_TOP_SCLK_DISP1, 9, CLK_SET_RATE_PARENT, 0),
	CGATE(sclk_pixel, "sclk_pixel", "dout_hdmi_pixel",
		EXYNOS5_CLK_GATE_TOP_SCLK_DISP1, 10, CLK_SET_RATE_PARENT, 0),
	CGATE(sclk_dp1_ext_mst_vid, "sclk_dp1_ext_mst_vid", "dout_dp1_ext_mst_vid",
		EXYNOS5_CLK_GATE_TOP_SCLK_DISP1, 20, CLK_SET_RATE_PARENT, 0),

	/* Maudio Block */
	CGATE(sclk_mau_audio0, "sclk_mau_audio0", "dout_mau_audio0",
		EXYNOS5_CLK_GATE_TOP_SCLK_MAU, 0, CLK_SET_RATE_PARENT, 0),
	CGATE(sclk_mau_pcm0, "sclk_mau_pcm0", "dout_mau_pcm0",
		EXYNOS5_CLK_GATE_TOP_SCLK_MAU, 1, CLK_SET_RATE_PARENT, 0),

	/* FSYS */
	CGATE(tsi, "tsi", "aclk_200_fsys", EXYNOS5_CLK_GATE_BUS_FSYS0, 0, 0, 0),
	CGATE(pdma0, "pdma0", "aclk_200_fsys", EXYNOS5_CLK_GATE_BUS_FSYS0, 1, 0, 0),
	CGATE(pdma1, "pdma1", "aclk_200_fsys", EXYNOS5_CLK_GATE_BUS_FSYS0, 2, 0, 0),
	CGATE(ufs, "ufs", "aclk_200_fsys2", EXYNOS5_CLK_GATE_BUS_FSYS0, 3, 0, 0),
	CGATE(rtic, "rtic", "aclk_200_fsys", EXYNOS5_CLK_GATE_BUS_FSYS0, 5, 0, 0),
	CGATE(mmc0, "mmc0", "aclk_200_fsys2", EXYNOS5_CLK_GATE_BUS_FSYS0, 12, 0, 0),
	CGATE(mmc1, "mmc1", "aclk_200_fsys2", EXYNOS5_CLK_GATE_BUS_FSYS0, 13, 0, 0),
	CGATE(mmc2, "mmc2", "aclk_200_fsys2", EXYNOS5_CLK_GATE_BUS_FSYS0, 14, 0, 0),
	CGATE(sromc, "sromc", "aclk-200_fsys2",
			EXYNOS5_CLK_GATE_BUS_FSYS0, 19, CLK_IGNORE_UNUSED, 0),

	CGATE(sclk_usbdrd300, "usbdrd300", "aclk_200_fsys", EXYNOS5_CLK_GATE_BUS_FSYS0, 21, 0, 0),
	CGATE(sclk_usbdrd301, "usbdrd301", "aclk_200_fsys", EXYNOS5_CLK_GATE_BUS_FSYS0, 28, 0, 0),
	CGATE(clk_usbhost20, "usbhost20", "aclk_200_fsys", EXYNOS5_CLK_GATE_IP_FSYS, 18, 0, 0),

	/* UART */
	CGATE(uart0, "uart0", "aclk_66_peric", EXYNOS5_CLK_GATE_BUS_PERIC, 4, 0, 0),
	CGATE(uart1, "uart1", "aclk_66_peric", EXYNOS5_CLK_GATE_BUS_PERIC, 5, 0, 0),
	CGATE(uart3, "uart3", "aclk_66_peric", EXYNOS5_CLK_GATE_BUS_PERIC, 7, 0, 0),
	/* I2C */
	CGATE(i2c0, "i2c0", "aclk_66_peric", EXYNOS5_CLK_GATE_BUS_PERIC, 9, 0, 0),
	CGATE(i2c1, "i2c1", "aclk_66_peric", EXYNOS5_CLK_GATE_BUS_PERIC, 10, 0, 0),
	CGATE(i2c2, "i2c2", "aclk_66_peric", EXYNOS5_CLK_GATE_BUS_PERIC, 11, 0, 0),
	CGATE(i2c3, "i2c3", "aclk_66_peric", EXYNOS5_CLK_GATE_BUS_PERIC, 12, 0, 0),
	CGATE(i2c4, "i2c4", "aclk_66_peric", EXYNOS5_CLK_GATE_BUS_PERIC, 13, 0, 0),
	CGATE(i2c5, "i2c5", "aclk_66_peric", EXYNOS5_CLK_GATE_BUS_PERIC, 14, 0, 0),
	CGATE(i2c6, "i2c6", "aclk_66_peric", EXYNOS5_CLK_GATE_BUS_PERIC, 15, 0, 0),
	CGATE(i2c7, "i2c7", "aclk_66_peric", EXYNOS5_CLK_GATE_BUS_PERIC, 16, 0, 0),
	CGATE(i2c_hdmi, "i2c_hdmi", "aclk_66_peric", EXYNOS5_CLK_GATE_BUS_PERIC, 17, 0, 0),
	CGATE(tsadc, "tsadc", "aclk_66_peric", EXYNOS5_CLK_GATE_BUS_PERIC, 18, 0, 0),
	/* SPI */
	CGATE(spi0, "spi0", "aclk_66_peric", EXYNOS5_CLK_GATE_BUS_PERIC, 19, 0, 0),
	CGATE(spi1, "spi1", "aclk_66_peric", EXYNOS5_CLK_GATE_BUS_PERIC, 20, 0, 0),
	CGATE(spi2, "spi2", "aclk_66_peric", EXYNOS5_CLK_GATE_BUS_PERIC, 21, 0, 0),
	CGATE(keyif, "keyif", "aclk_66_peric", EXYNOS5_CLK_GATE_BUS_PERIC, 22, 0, 0),
	/* I2S */
	CGATE(i2s1, "i2s1", "aclk_66_peric", EXYNOS5_CLK_GATE_BUS_PERIC, 23, 0, 0),
	CGATE(i2s2, "i2s2", "aclk_66_peric", EXYNOS5_CLK_GATE_BUS_PERIC, 24, 0, 0),
	/* PCM */
	CGATE(pcm1, "pcm1", "aclk_66_peric", EXYNOS5_CLK_GATE_BUS_PERIC, 25, 0, 0),
	CGATE(pcm2, "pcm2", "aclk_66_peric", EXYNOS5_CLK_GATE_BUS_PERIC, 26, 0, 0),
	/* PWM */
	CGATE(pwm, "pwm", "aclk_66_peric", EXYNOS5_CLK_GATE_BUS_PERIC, 27, 0, 0),
	/* SPDIF */
	CGATE(spdif, "spdif", "aclk_66_peric", EXYNOS5_CLK_GATE_BUS_PERIC, 29, 0, 0),

	CGATE(i2c8, "i2c8", "aclk_66_peric", EXYNOS5_CLK_GATE_BUS_PERIC1, 0, 0, 0),
	CGATE(i2c9, "i2c9", "aclk_66_peric", EXYNOS5_CLK_GATE_BUS_PERIC1, 1, 0, 0),
	CGATE(i2c10, "i2c10", "aclk_66_peric", EXYNOS5_CLK_GATE_BUS_PERIC1, 2, 0, 0),

	CGATE(abb_apbif, "abb_apbif", "aclk_66_psgen",
			EXYNOS5_CLK_GATE_BUS_PERIS0, 11, 0, 0),
	CGATE(chipid, "chipid", "aclk_66_psgen",
			EXYNOS5_CLK_GATE_BUS_PERIS0, 12, CLK_IGNORE_UNUSED, 0),
	CGATE(sysreg, "sysreg", "aclk_66_psgen",
			EXYNOS5_CLK_GATE_BUS_PERIS0, 13, CLK_IGNORE_UNUSED, 0),
	CGATE(tzpc0, "tzpc0", "aclk_66_psgen", EXYNOS5_CLK_GATE_BUS_PERIS0, 18, 0, 0),
	CGATE(tzpc1, "tzpc1", "aclk_66_psgen", EXYNOS5_CLK_GATE_BUS_PERIS0, 19, 0, 0),
	CGATE(tzpc2, "tzpc2", "aclk_66_psgen", EXYNOS5_CLK_GATE_BUS_PERIS0, 20, 0, 0),
	CGATE(tzpc3, "tzpc3", "aclk_66_psgen", EXYNOS5_CLK_GATE_BUS_PERIS0, 21, 0, 0),
	CGATE(tzpc4, "tzpc4", "aclk_66_psgen", EXYNOS5_CLK_GATE_BUS_PERIS0, 22, 0, 0),
	CGATE(tzpc5, "tzpc5", "aclk_66_psgen", EXYNOS5_CLK_GATE_BUS_PERIS0, 23, 0, 0),
	CGATE(tzpc6, "tzpc6", "aclk_66_psgen", EXYNOS5_CLK_GATE_BUS_PERIS0, 24, 0, 0),
	CGATE(tzpc7, "tzpc7", "aclk_66_psgen", EXYNOS5_CLK_GATE_BUS_PERIS0, 25, 0, 0),
	CGATE(tzpc8, "tzpc8", "aclk_66_psgen", EXYNOS5_CLK_GATE_BUS_PERIS0, 26, 0, 0),
	CGATE(tzpc9, "tzpc9", "aclk_66_psgen", EXYNOS5_CLK_GATE_BUS_PERIS0, 27, 0, 0),

	CGATE(hdmi_cec, "hdmi_cec", "aclk_66_psgen", EXYNOS5_CLK_GATE_BUS_PERIS1, 0, 0, 0),
	CGATE(seckey, "seckey", "aclk_66_psgen", EXYNOS5_CLK_GATE_BUS_PERIS1, 1, 0, 0),
	CGATE(wdt, "wdt", "aclk_66_psgen", EXYNOS5_CLK_GATE_BUS_PERIS1, 3, 0, 0),
	CGATE(rtc, "rtc", "aclk_66_psgen", EXYNOS5_CLK_GATE_BUS_PERIS1, 4, 0, 0),
	CGATE(tmu, "tmu", "aclk_66_psgen", EXYNOS5_CLK_GATE_BUS_PERIS1, 5, 0, 0),
	CGATE(tmu_gpu, "tmu_gpu", "aclk_66_psgen", EXYNOS5_CLK_GATE_BUS_PERIS1, 6, 0, 0),

	CGATE(gscl0, "gscl0", "aclk_300_gscl", EXYNOS5_CLK_GATE_IP_GSCL0, 0, 0, 0),
	CGATE(gscl1, "gscl1", "aclk_300_gscl", EXYNOS5_CLK_GATE_IP_GSCL0, 1, 0, 0),
	CGATE(clk_3aa, "clk_3aa", "aclk_300_gscl", EXYNOS5_CLK_GATE_IP_GSCL0, 4, 0, 0),

	CGATE(smmu_3aa, "smmu_3aa", "aclk_333_432_gscl", EXYNOS5_CLK_GATE_IP_GSCL1, 2, 0, 0),
	CGATE(smmu_fimcl0, "smmu_fimcl0", "aclk_333_432_gscl",
			EXYNOS5_CLK_GATE_IP_GSCL1, 3, 0, 0),
	CGATE(smmu_fimcl1, "smmu_fimcl1", "aclk_333_432_gscl",
			EXYNOS5_CLK_GATE_IP_GSCL1, 4, 0, 0),
	CGATE(smmu_gscl0, "smmu_gscl0", "aclk_300_gscl", EXYNOS5_CLK_GATE_IP_GSCL1, 6, 0, 0),
	CGATE(smmu_gscl1, "smmu_gscl1", "aclk_300_gscl", EXYNOS5_CLK_GATE_IP_GSCL1, 7, 0, 0),
	CGATE(gscl_wa, "gscl_wa", "aclk_300_gscl", EXYNOS5_CLK_GATE_IP_GSCL1, 12, 0, 0),
	CGATE(gscl_wb, "gscl_wb", "aclk_300_gscl", EXYNOS5_CLK_GATE_IP_GSCL1, 13, 0, 0),
	CGATE(smmu_fimcl3, "smmu_fimcl3,", "aclk_333_432_gscl",
			EXYNOS5_CLK_GATE_IP_GSCL1, 16, 0, 0),
	CGATE(fimc_lite3, "fimc_lite3", "aclk_333_432_gscl",
			EXYNOS5_CLK_GATE_IP_GSCL1, 17, 0, 0),

	CGATE(fimd1, "fimd1", "mout_aclk_300_disp1_user", EXYNOS5_CLK_GATE_IP_DISP1, 0, 0, 0),
	CGATE(dsim1, "dsim1", "aclk_200_disp1", EXYNOS5_CLK_GATE_IP_DISP1, 3, 0, 0),
	CGATE(dp1, "dp1", "aclk_200_disp1", EXYNOS5_CLK_GATE_IP_DISP1, 4, 0, 0),
	CGATE(mixer, "mixer", "aclk_166", EXYNOS5_CLK_GATE_IP_DISP1, 5, 0, 0),
	CGATE(hdmi, "hdmi", "aclk_200_disp1", EXYNOS5_CLK_GATE_IP_DISP1, 6, 0, 0),
	CGATE(smmu_fimd1, "smmu_fimd1", "aclk_300_disp1", EXYNOS5_CLK_GATE_IP_DISP1, 8, 0, 0),

	CGATE(mfc, "mfc", "aclk_333", EXYNOS5_CLK_GATE_IP_MFC, 0, 0, 0),
	CGATE(smmu_mfcl, "smmu_mfcl", "aclk_333", EXYNOS5_CLK_GATE_IP_MFC, 1, 0, 0),
	CGATE(smmu_mfcr, "smmu_mfcr", "aclk_333", EXYNOS5_CLK_GATE_IP_MFC, 2, 0, 0),

	CGATE(aclk_g3d, "aclk_g3d", "mout_aclk_g3d_user", EXYNOS5_CLK_GATE_IP_G3D, 9, 0, 0),

	CGATE(rotator, "rotator", "aclk_266", EXYNOS5_CLK_GATE_IP_GEN, 1, 0, 0),
	CGATE(jpeg, "jpeg", "aclk_300_jpeg", EXYNOS5_CLK_GATE_IP_GEN, 2, 0, 0),
	CGATE(jpeg2, "jpeg2", "aclk_300_jpeg", EXYNOS5_CLK_GATE_IP_GEN, 3, 0, 0),
	CGATE(mdma1, "mdma1", "aclk_266", EXYNOS5_CLK_GATE_IP_GEN, 4, 0, 0),
	CGATE(smmu_rotator, "smmu_rotator", "aclk_266", EXYNOS5_CLK_GATE_IP_GEN, 6, 0, 0),
	CGATE(smmu_jpeg, "smmu_jpeg", "aclk_300_jpeg", EXYNOS5_CLK_GATE_IP_GEN, 7, 0, 0),
	CGATE(smmu_mdma1, "smmu_mdma1", "aclk_266", EXYNOS5_CLK_GATE_IP_GEN, 9, 0, 0),

	CGATE(mscl0, "mscl0", "aclk_400_mscl", EXYNOS5_CLK_GATE_IP_MSCL, 0, 0, 0),
	CGATE(mscl1, "mscl1", "aclk_400_mscl", EXYNOS5_CLK_GATE_IP_MSCL, 1, 0, 0),
	CGATE(mscl2, "mscl2", "aclk_400_mscl", EXYNOS5_CLK_GATE_IP_MSCL, 2, 0, 0),
	CGATE(smmu_mscl0, "smmu_mscl0", "aclk_400_mscl", EXYNOS5_CLK_GATE_IP_MSCL, 8, 0, 0),
	CGATE(smmu_mscl1, "smmu_mscl1", "aclk_400_mscl", EXYNOS5_CLK_GATE_IP_MSCL, 9, 0, 0),
	CGATE(smmu_mscl2, "smmu_mscl2", "aclk_400_mscl", EXYNOS5_CLK_GATE_IP_MSCL, 10, 0, 0),

	CGATE(clkm_phy0, "clkm_phy0", "dout_sclk_cdrex", EXYNOS5_CLK_GATE_BUS_CDREX, 0, 0, 0),
	CGATE(clkm_phy1, "clkm_phy1", "dout_sclk_cdrex", EXYNOS5_CLK_GATE_BUS_CDREX, 1, 0, 0),
};

static __initdata struct of_device_id ext_clk_match[] = {
	{ .compatible = "samsung,exynos5422-oscclk", .data = (void *)0, },
	{ },
};

struct samsung_pll_rate_table cpll_rate_table[] = {
	/* rate		p	m	s	k */
	{ 666000000U,   4,  222,    1,  0},
    { 640000000U,   3,  160,    1,  0},
    { 320000000U,   3,  160,    2,  0},
};

struct samsung_pll_rate_table epll_rate_table[] = {
	/* rate		p	m	s	k */
	{  45158400U,   3,  181,    5,  0},
    {  49152000U,   3,  197,    5,  0},
    {  67737600U,   5,  452,    5,  0},
    { 180633600U,   5,  301,    3,  0},
    { 100000000U,   3,  200,    4,  0},
    { 200000000U,   3,  200,    3,  0},
    { 400000000U,   3,  200,    2,  0},
    { 600000000U,   2,  100,    1,  0},
};

struct samsung_pll_rate_table ipll_rate_table[] = {
	/* rate		p	m	s	k */
	{ 370000000U,   3,  185,    2,  0},
    { 432000000U,   4,  288,    2,  0},
    { 666000000U,	4,  222,    1,  0},
    { 864000000U,	4,  288,    1,  0},
};

struct samsung_pll_rate_table vpll_rate_table[] = {
	/* rate		p	m	s	k */
	{ 100000000U,   3,  200,    4,  0},
    { 177000000U,   2,  118,    3,  0},
    { 266000000U,   3,  266,    3,  0},
    { 350000000U,   3,  175,    2,  0},
    { 420000000U,   2,  140,    2,  0},
    { 480000000U,   2,  160,    2,  0},
    { 533000000U,   6,  533,    2,  0},
    { 600000000U,   2,  200,    2,  0},
};

struct samsung_pll_rate_table spll_rate_table[] = {
	/* rate		p	m	s	k */
	{  66000000U,   4,  352,    5,  0},
    { 400000000U,   3,  200,    2,  0},
};

struct samsung_pll_rate_table mpll_rate_table[] = {
	/* rate		p	m	s	k */
	{ 532000000U,   3,  266,    2,  0},
};


struct samsung_pll_rate_table rpll_rate_table[] = {
	/* rate		p	m	s	k */
	{ 266000000U,   3,  266,    3, 0},
    { 300000000U,   2,  100,    2, 0},
};

struct samsung_pll_rate_table bpll_rate_table[] = {
	/* rate		p	m	s	k */
	{ 933000000U,   4,  311,    1,  0},
    { 800000000U,   3,  200,    1,  0},
    { 733000000U,   2,  122,    1,  0},
    { 667000000U,   2,  111,    1,  0},
    { 533000000U,   3,  266,    2,  0},
    { 400000000U,   3,  200,    2,  0},
    { 266000000U,   3,  266,    3,  0},
    { 200000000U,   3,  200,    3,  0},
    { 160000000U,   3,  160,    3,  0},
    { 133000000U,   3,  266,    4,  0},
};

struct samsung_pll_rate_table apll_rate_table[] = {
	/* rate		p	m	s	k */
	{  66000000U,   4,  352,    5,  0},
};

struct samsung_pll_rate_table dpll_rate_table[] = {
	/* rate		p	m	s	k */
	{  66000000U,   4,  352,    5,  0},
};

/* register exynos5422 clocks */
void __init exynos5422_clk_init(struct device_node *np)
{
	struct clk *apll, *bpll, *cpll, *dpll, *ipll, *mpll, *spll, *vpll;
	struct clk *epll, *rpll;

	samsung_clk_init(np, 0, nr_clks, (unsigned long *) exynos5422_clk_regs,
			ARRAY_SIZE(exynos5422_clk_regs), NULL, 0);

	samsung_clk_of_register_fixed_ext(exynos5422_fixed_rate_ext_clks,
			ARRAY_SIZE(exynos5422_fixed_rate_ext_clks),
			ext_clk_match);

	apll = samsung_clk_register_pll35xx("sclk_apll", "fin_pll",
			EXYNOS5_APLL_LOCK, EXYNOS5_APLL_CON0, apll_rate_table, ARRAY_SIZE(apll_rate_table));

	bpll = samsung_clk_register_pll35xx("sclk_bpll", "fin_pll",
			EXYNOS5_BPLL_LOCK, EXYNOS5_BPLL_CON0, bpll_rate_table, ARRAY_SIZE(bpll_rate_table));

	cpll = samsung_clk_register_pll35xx("sclk_cpll", "fin_pll",
			EXYNOS5_CPLL_LOCK, EXYNOS5_CPLL_CON0, cpll_rate_table, ARRAY_SIZE(cpll_rate_table));

	dpll = samsung_clk_register_pll35xx("sclk_dpll", "fin_pll",
			EXYNOS5_DPLL_LOCK, EXYNOS5_DPLL_CON0, dpll_rate_table, ARRAY_SIZE(dpll_rate_table));

	ipll = samsung_clk_register_pll35xx("sclk_ipll", "fin_pll",
			EXYNOS5_IPLL_LOCK, EXYNOS5_IPLL_CON0, ipll_rate_table, ARRAY_SIZE(ipll_rate_table));
	/* KPLL WILL BE INITIALIZED HERE */
	mpll = samsung_clk_register_pll35xx("sclk_mpll", "fin_pll",
			EXYNOS5_MPLL_LOCK, EXYNOS5_MPLL_CON0, mpll_rate_table, ARRAY_SIZE(mpll_rate_table));

	spll = samsung_clk_register_pll35xx("sclk_spll", "fin_pll",
			EXYNOS5_SPLL_LOCK, EXYNOS5_SPLL_CON0, spll_rate_table, ARRAY_SIZE(spll_rate_table));

	vpll = samsung_clk_register_pll35xx("sclk_vpll", "fin_pll",
			EXYNOS5_VPLL_LOCK, EXYNOS5_VPLL_CON0, vpll_rate_table, ARRAY_SIZE(vpll_rate_table));

	epll = samsung_clk_register_pll36xx("sclk_epll", "fin_pll",
			EXYNOS5_EPLL_LOCK, EXYNOS5_EPLL_CON0, epll_rate_table, ARRAY_SIZE(epll_rate_table));

	rpll = samsung_clk_register_pll36xx("sclk_rpll", "fin_pll",
			EXYNOS5_RPLL_LOCK, EXYNOS5_RPLL_CON0, rpll_rate_table, ARRAY_SIZE(rpll_rate_table));

	samsung_clk_register_mux(exynos5422_mux_clks,
			ARRAY_SIZE(exynos5422_mux_clks));

	samsung_clk_register_div(exynos5422_div_clks,
			ARRAY_SIZE(exynos5422_div_clks));

	samsung_clk_register_gate(exynos5422_gate_clks,
			ARRAY_SIZE(exynos5422_gate_clks));

	exynos5422_clock_init();
	pr_info("Exynos5422: clock setup completed\n");
}
CLK_OF_DECLARE(exynos5422_clk, "samsung,exynos5422-clock", exynos5422_clk_init);
