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

#include "clk.h"
#include "clk-pll.h"
#include "clk-exynos5430.h"

enum exynos5430_clks {
	none,

	/* core clocks */
	fin_pll, mem_pll, bus_pll, mfc_pll, mphy_pll, disp_pll, isp_pll, aud_pll,

	/* temp clocks, to be modified */
	clk_uart_baud0 = 10, uart0, uart1, uart2, mct,

	/* gate for special clocks (sclk) */
	sclk_jpeg_top = 20,

	sclk_isp_spi0_top = 30, sclk_isp_spi1_top, sclk_isp_uart_top,
	sclk_isp_sensor0, sclk_isp_sensor1, sclk_isp_sensor2,

	sclk_hdmi_spdif_top = 40,

	sclk_usbdrd30_top = 50, sclk_ufsunipro_top, sclk_mmc0_top, sclk_mmc1_top, sclk_mmc2_top,

	sclk_spi0_top = 60, sclk_spi1_top, sclk_spi2_top, sclk_uart0_top, sclk_uart1_top, sclk_uart2_top,
	sclk_pcm1_top, sclk_i2s1_top, sclk_spdif_top, sclk_slimbus_top,

	sclk_hpm_mif = 80, sclk_decon_eclk_mif, sclk_decon_vclk_mif, sclk_dsd_mif,

	sclk_mphy_pll, sclk_ufs_mphy, sclk_lli_mphy,

	sclk_jpeg,

	sclk_decon_eclk = 100, sclk_decon_vclk, sclk_dsd, sclk_hdmi_spdif,

	sclk_pcm1 = 110, sclk_i2s1, sclk_spi0, sclk_spi1, sclk_spi2, sclk_uart0,
	sclk_uart1, sclk_uart2, sclk_slimbus, sclk_spdif,

	sclk_isp_spi0 = 130, sclk_isp_spi1, sclk_isp_uart, sclk_isp_mtcadc,

	sclk_usbdrd30 = 140, sclk_mmc0, sclk_mmc1, sclk_mmc2, sclk_ufsunipro, sclk_mphy,


	/* gate clocks */

	aclk_mifnm_200 = 300, aclk_mifnd_133, aclk_mif_133, aclk_mif_400, aclk_mif_200,
	clkm_phy, clk2x_phy, aclk_drex0, aclk_drex1,

	aclk_cpif_200 = 320, aclk_disp_333, aclk_disp_222, aclk_bus1_400, aclk_bus2_400,

	aclk_g2d_400 = 330, aclk_g2d_266, aclk_mfc0_333, aclk_mfc1_333, aclk_hevc_400,
	aclk_isp_400, aclk_isp_dis_400, aclk_cam0_552, aclk_cam0_400,
	aclk_cam0_333, aclk_cam1_552, aclk_cam1_400, aclk_cam1_333,
	aclk_gscl_333, aclk_gscl_111, aclk_fsys_200, aclk_mscl_400,
	aclk_peris_66, aclk_peric_66, aclk_imem_266, aclk_imem_200,

	phyclk_lli_tx0_symbol = 360, phyclk_lli_rx0_symbol,

	phyclk_mixer_pixel = 370, phyclk_hdmi_pixel, phyclk_hdmiphy_tmds_clko,
	phyclk_mipidphy_rxclkesc0, phyclk_mipidphy_bitclkdiv8,

	ioclk_i2s1_bclk = 390, ioclk_spi0_clk, ioclk_spi1_clk, ioclk_spi2_clk, ioclk_slimbus_clk,

	phyclk_rxbyteclkhs0_s4 = 400, phyclk_rxbyteclkhs0_s2a,

	phyclk_rxbyteclkhs0_s2b,

	phyclk_usbdrd30_udrd30_phyclock = 410, phyclk_usbdrd30_udrd30_pipe_pclk,
	phyclk_usbhost20_phy_freeclk, phyclk_usbhost20_phy_phyclock,
	phyclk_usbhost20_phy_clk48mohci, phyclk_usbhost20_phy_hsic1,
	phyclk_ufs_tx0_symbol, phyclk_ufs_tx1_symbol,
	phyclk_ufs_rx0_symbol, phyclk_ufs_rx1_symbol,

	aclk_bus1nd_400 = 430, aclk_bus1sw2nd_400, aclk_bus1np_133, aclk_bus1sw2np_133,
	aclk_ahb2apb_bus1p, pclk_bus1srvnd_133, pclk_sysreg_bus1, pclk_pmu_bus1,

	aclk_bus2rtnd_400 = 440, aclk_bus2bend_400, aclk_bus2np_133, aclk_ahb2apb_bus2p,
	pclk_bus2srvnd_133, pclk_sysreg_bus2, pclk_pmu_bus2,

	/* g2d gate */
	aclk_g2d = 450, aclk_g2dnd_400, aclk_xiu_g2dx, aclk_asyncaxi_sysx,
	aclk_axius_g2dx, aclk_alb_g2d, aclk_qe_g2d, aclk_smmu_g2d, aclk_ppmu_g2dx,
	aclk_mdma1 = 460, aclk_qe_mdma1, aclk_smmu_mdma1,
	aclk_g2dnp_133, aclk_ahb2apb_g2d0p, aclk_ahb2apb_g2d1p, pclk_g2d,
	pclk_sysreg_g2d = 470, pclk_pmu_g2d, pclk_asyncaxi_sysx, pclk_alb_g2d,
	pclk_qe_g2d, pclk_qe_mdma1, pclk_smmu_g2d, pclk_smmu_mdma1, pclk_ppmu_g2d,

	/* gscl gate */
	aclk_gscl0 = 480, aclk_gscl1, aclk_gscl2, aclk_gsd, aclk_gsclbend_333,
	aclk_gsclrtnd_333, aclk_xiu_gsclx, aclk_qe_gscl0, aclk_qe_gscl1, aclk_qe_gscl2,
	aclk_smmu_gscl0 = 490, aclk_smmu_gscl1, aclk_smmu_gscl2, aclk_ppmu_gscl0,
	aclk_ppmu_gscl1, aclk_ppmu_gscl2,
	aclk_ahb2apb_gsclp = 500, pclk_gscl0, pclk_gscl1, pclk_gscl2, pclk_sysreg_gscl,
	pclk_pmu_gscl, pclk_qe_gscl0, pclk_qe_gscl1, pclk_qe_gscl2,
	pclk_smmu_gscl0 = 510, pclk_smmu_gscl1, pclk_smmu_gscl2,
	pclk_ppmu_gscl0, pclk_ppmu_gscl1, pclk_ppmu_gscl2,

	/* mscl gate */
	aclk_m2mscaler0 = 520, aclk_m2mscaler1, aclk_jpeg, aclk_msclnd_400,
	aclk_xiu_msclx, aclk_qe_m2mscaler0, aclk_qe_m2mscaler1, aclk_qe_jpeg,
	aclk_smmu_m3mscaler0 = 530, aclk_smmu_m2mscaler1, aclk_smmu_jpeg,
	aclk_ppmu_m2mscaler0, aclk_ppmu_m2mscaler1,
	aclk_msclnp_100, aclk_ahb2apb_mscl0p, pclk_m2mscaler0, pclk_m2mscaler1,
	pclk_jpeg = 540, pclk_sysreg_mscl, pclk_pmu_mscl,
	pclk_qe_m2mscaler0, pclk_qe_m2mscaler1, pclk_qe_jpeg,
	pclk_smmu_m2mscaler0 = 550, pclk_smmu_m2mscaler1, pclk_smmu_jpeg,
	pclk_ppmu_m2mscaler0, pclk_ppmu_m2mscaler1, pclk_ppmu_jpeg,

	/* fsys gate */
	aclk_pdma = 560, aclk_usbdrd30, aclk_usbhost20, aclk_sromc, aclk_ufs,
	aclk_mmc0, aclk_mmc1, aclk_mmc2, aclk_tsi,
	aclk_fsysnp_200 = 570, aclk_fsysnd_200, aclk_xiu_fsyssx, aclk_xiu_fsysx,
	aclk_ahb_fsysh, aclk_ahb_usbhs, aclk_ahb_usblinkh,
	aclk_ahb2axi_usbhs = 580, aclk_ahb2apb_fsysp, aclk_axius_fsyssx,
	aclk_axius_usbhs, aclk_axius_pdma, aclk_qe_usbdrd30, aclk_qe_ufs,
	aclk_smmu_pdma = 590, aclk_smmu_mmc0, aclk_smmu_mmc1, aclk_smmu_mmc2, aclk_ppmu_fsys,
	pclk_gpio_fsys, pclk_pmu_fsys, pclk_sysreg_fsys, pclk_qe_usbdrd30,
	pclk_qe_ufs = 600, pclk_smmu_pdma, pclk_smmu_mmc0, pclk_smmu_mmc1, pclk_smmu_mmc2,
	pclk_ppmu_fsys,

	/* dis gate */
	aclk_decon = 610, aclk_disp0nd_333, aclk_disp1nd_333, aclk_xiu_disp1x,
	aclk_xiu_decon0x, aclk_xiu_decon1x, aclk_axius_disp1x,
	aclk_qe_deconm0 = 620, aclk_qe_deconm1, aclk_qe_deconm2, aclk_qe_deconm3,
	aclk_smmu_decon0x, aclk_smmu_decon1x, aclk_ppmu_decon0x, aclk_ppmu_decon1x,
	aclk_dispnp_100 = 630, aclk_ahb_disph, aclk_ahb2apb_dispsfr0p,
	aclk_ahb2apb_dispsfr1p, aclk_ahb2apb_dispsfr2p, pclk_decon, pclk_tv_mixer,
	pclk_dsim0 = 640, pclk_mdnie, pclk_mic, pclk_hdmi, pclk_hdmiphy,
	pclk_sysreg_disp, pclk_pmu_disp, pclk_asyncaxi_tvx,
	pclk_qe_deconm0 = 650, pclk_qe_deconm1, pclk_qe_deconm2, pclk_qe_deconm3, pclk_qe_deconm4,
	pclk_qe_mixerm0, pclk_qe_mixerm1, pclk_smmu_decon0x, pclk_smmu_decon1x,
	pclk_smmu_tvx = 660, pclk_ppmu_decon0x, pclk_ppmu_decon1x, pclk_ppmu_tvx,
	aclk_hdmi, aclk_tv_mixer, aclk_xiu_tvx, aclk_asyncaxis_tvx,
	aclk_qe_mixerm0 = 670, aclk_qe_mixerm1, aclk_smmu_tvx, aclk_ppmu_tvx,


	nr_clks,
};

static __initdata unsigned long exynos5430_clk_regs[] = {
	SRC_SEL_AUD0,
	SRC_SEL_AUD1,
	SRC_SEL_BUS1,
	SRC_SEL_BUS2,
	SRC_SEL_CAM00,
	SRC_SEL_CAM01,
	SRC_SEL_CAM02,
	SRC_SEL_CAM03,
	SRC_SEL_CAM04,
	SRC_SEL_CAM10,
	SRC_SEL_CAM11,
	SRC_SEL_CAM12,
	SRC_SEL_CPIF0,
	SRC_SEL_CPIF1,
	SRC_SEL_CPIF2,
	SRC_SEL_DISP0,
	SRC_SEL_DISP1,
	SRC_SEL_DISP2,
	SRC_SEL_DISP3,
	SRC_SEL_FSYS0,
	SRC_SEL_FSYS1,
	SRC_SEL_FSYS2,
	SRC_SEL_FSYS3,
	SRC_SEL_FSYS4,
	SRC_SEL_G2D,
	SRC_SEL_G3D,
	SRC_SEL_GSCL,
	SRC_SEL_HEVC,
	SRC_SEL_ISP,
	SRC_SEL_MFC0,
	SRC_SEL_MFC1,
	SRC_SEL_MIF0,
	SRC_SEL_MIF1,
	SRC_SEL_MIF2,
	SRC_SEL_MIF3,
	SRC_SEL_MIF4,
	SRC_SEL_MIF5,
	SRC_SEL_MSCL,
	SRC_SEL_TOP0,
	SRC_SEL_TOP1,
	SRC_SEL_TOP2,
	SRC_SEL_TOP3,
	SRC_SEL_TOP_MSCL,
	SRC_SEL_TOP_CAM1,
	SRC_SEL_TOP_DISP,
	SRC_SEL_TOP_FSYS0,
	SRC_SEL_TOP_FSYS1,
	SRC_SEL_TOP_PERIC0,
	SRC_SEL_TOP_PERIC1,
	DIV_AUD0,
	DIV_AUD1,
	DIV_AUD2,
	DIV_AUD3,
	DIV_AUD4,
	DIV_BUS1,
	DIV_BUS2,
	DIV_CAM00,
	DIV_CAM01,
	DIV_CAM02,
	DIV_CAM03,
	DIV_CAM10,
	DIV_CAM11,
	DIV_CPIF,
	DIV_DISP,
	DIV_G2D,
	DIV_G3D,
	DIV_HEVC,
	DIV_ISP,
	DIV_MFC0,
	DIV_MFC1,
	DIV_MIF0,
	DIV_MIF1,
	DIV_MIF2,
	DIV_MIF3,
	DIV_MIF4,
	DIV_MIF5,
	DIV_MSCL,
	DIV_TOP0,
	DIV_TOP1,
	DIV_TOP2,
	DIV_TOP3,
	DIV_TOP_MSCL,
	DIV_TOP_CAM10,
	DIV_TOP_CAM11,
	DIV_TOP_FSYS0,
	DIV_TOP_FSYS1,
	DIV_TOP_PERIC0,
	DIV_TOP_PERIC1,
	DIV_TOP_PERIC2,
	DIV_TOP_PERIC3,
};

/*
 * list of controller registers to be saved and restored during a
 * suspend/resume cycle.
 */

/* TOP */
PNAME(mout_bus_pll_user_p) = { "fin_pll", "mout_bus_pll_sub" };
PNAME(mout_mfc_pll_user_p) = { "fin_pll", "dout_mfc_pll" };
PNAME(mout_mphy_pll_user_p) = { "fin_pll", "mout_mphy_pll" };
PNAME(mout_isp_pll_p) = { "fin_pll", "fout_isp_pll" };
PNAME(mout_aud_pll_p) = { "fin_pll", "fout_aud_pll" };
PNAME(mout_aud_dpll_p) = { "fin_pll", "fout_aud_dpll" };
PNAME(mout_aclk_g2d_400_a_p) = { "mout_bus_pll_user", "mout_mfc_pll_user" };
PNAME(mout_aclk_g2d_400_b_p) = { "mout_aclk_g2d_400_a", "mout_mphy_pll_user" };
PNAME(mout_aclk_gscl_333_p)	= { "mout_mfc_pll_user", "mout_bus_pll_user" };
PNAME(mout_aclk_mscl_400_a_p)	= { "mout_bus_pll_user", "mout_mfc_pll_user" };
PNAME(mout_aclk_mscl_400_b_p)	= { "mout_aclk_mscl_400_a", "mout_mphy_pll_user" };
PNAME(mout_aclk_mfc0_333_p)	= { "mout_mfc_pll_user", "mout_bus_pll_user" };
PNAME(mout_aclk_mfc1_333_p)	= { "mout_mfc_pll_user", "mout_bus_pll_user" };
PNAME(mout_aclk_hevc_400_p)	= { "mout_bus_pll_user", "mout_mfc_pll_user" };
PNAME(mout_aclk_isp_400_p)	= { "mout_bus_pll_user", "mout_mfc_pll_user" };
PNAME(mout_aclk_isp_dis_400_p)	= { "mout_bus_pll_user", "mout_mfc_pll_user" };
PNAME(mout_aclk_cam1_552_a_p)	= { "mout_isp_pll", "mout_bus_pll_user" };
PNAME(mout_aclk_cam1_552_b_p)	= { "mout_aclk_cam1_552_a", "mout_mfc_pll_user" };
PNAME(mout_sclk_audio0_p)	= { "ioclk_audiocdclk0", "oscclk", "mout_aud_pll_user", "mout_aud_dpll_user" };
PNAME(mout_sclk_audio1_p)	= { "ioclk_audiocdclk1", "oscclk", "mout_aud_pll_user", "mout_aud_dpll_user" };
PNAME(mout_sclk_spi_p)		= { "oscclk", "mout_bus_pll_user" };
PNAME(mout_sclk_uart_p)		= { "oscclk", "mout_bus_pll_user" };
PNAME(mout_sclk_slimbus_a_p)	= { "oscclk", "mout_aud_pll_user" };
PNAME(mout_sclk_slimbus_b_p)	= { "mout_sclk_slimbus_a", "mout_aud_dpll_user" };
PNAME(mout_sclk_spdif_p)	= { "dout_sclk_audio0", "dout_sclk_audio1", "ioclk_spdif_extclk", "1b0" };
PNAME(mout_sclk_usbdrd30_p)	= { "oscclk", "mout_bus_pll_user" };
PNAME(mout_sclk_mmc0_a_p)	= { "oscclk", "mout_bus_pll_user" };
PNAME(mout_sclk_mmc0_b_p)	= { "mout_sclk_mmc0_a", "mout_mfc_pll_user" };
PNAME(mout_sclk_mmc0_c_p)	= { "mout_sclk_mmc0_b", "mout_mphy_pll_user" };
PNAME(mout_sclk_mmc0_d_p)	= { "mout_sclk_mmc0_c", "mout_isp_pll" };
PNAME(mout_sclk_mmc1_a_p)	= { "oscclk", "mout_bus_pll_user" };
PNAME(mout_sclk_mmc1_b_p)	= { "mout_sclk_mmc1_a", "mout_mfc_pll_user" };
PNAME(mout_sclk_mmc2_a_p)	= { "oscclk", "mout_bus_pll_user" };
PNAME(mout_sclk_mmc2_b_p)	= { "mout_sclk_mmc2_a", "mout_mfc_pll_user" };
PNAME(mout_sclk_ufsunipro_p)	= { "oscclk", "mout_mphy_pll_user" };
PNAME(mout_sclk_jpeg_a_p)	= { "oscclk", "mout_bus_pll_user" };
PNAME(mout_sclk_jpeg_b_p)	= { "mout_sclk_jpeg_a", "mout_mfc_pll_user" };
PNAME(mout_sclk_jpeg_c_p)	= { "mout_sclk_jpeg_b", "mout_mphy_pll_user" };
PNAME(mout_sclk_isp_spi_p)	= { "oscclk", "mout_bus_pll_user" };
PNAME(mout_sclk_isp_uart_p)	= { "oscclk", "mout_bus_pll_user" };
PNAME(mout_sclk_isp_sensor_p)	= { "oscclk", "mout_bus_pll_user" };
PNAME(mout_sclk_hdmi_spdif_p)	= { "dout_sclk_audio1", "ioclk_spdif_extclk" };

/* MIF */
PNAME(mout_bus_dpll_p)		= { "fin_pll", "fout_bus_dpll" };
PNAME(mout_mem_pll_p)		= { "fin_pll", "fout_mem_pll" };
PNAME(mout_bus_pll_p)		= { "fin_pll", "fout_bus_pll" };
PNAME(mout_mfc_pll_p)		= { "fin_pll", "fout_mfc_pll" };
PNAME(mout_bus_pll_sub_p)	= { "mout_bus_dpll", "mout_bus_pll" };
PNAME(mout_aclk_mif_400_a_p)	= { "mout_bus_pll_sub", "dout_mfc_pll_div3" };
PNAME(mout_aclk_mif_400_b_p)	= { "dout_bus_pll", "mout_aclk_mif_400_a" };
PNAME(mout_clkm_phy_a_p)	= { "mout_bus_pll_sub", "dout_mfc_pll" };
PNAME(mout_clkm_phy_b_p)	= { "dout_bus_pll", "mout_clkm_phy_a" };
PNAME(mout_clkm_phy_c_p)	= { "dout_mem_pll", "mout_clkm_phy_b" };
PNAME(mout_clk2x_phy_a_p)	= { "mout_bus_pll_sub", "dout_mfc_pll" };
PNAME(mout_clk2x_phy_b_p)	= { "dout_bus_pll", "mout_clk2x_phy_a" };
PNAME(mout_clk2x_phy_c_p)	= { "dout_mem_pll", "mout_clk2x_phy_b" };
PNAME(mout_aclk_disp_333_a_p)	= { "dout_mfc_pll", "mout_mphy_pll" };
PNAME(mout_aclk_disp_333_b_p)	= { "mout_aclk_disp_333_a", "mout_bus_pll_sub" };
PNAME(mout_aclk_disp_222_p)	= { "dout_mfc_pll", "mout_bus_pll_sub" };
PNAME(mout_aclk_bus1_400_a_p)	= { "dout_bus_pll", "dout_mfc_pll_div3" };
PNAME(mout_aclk_bus1_400_b_p)	= { "mout_bus_pll_sub", "mout_aclk_bus1_400_a" };
PNAME(mout_sclk_decon_eclk_a_p) = { "oscclk", "mout_bus_pll_sub" };
PNAME(mout_sclk_decon_eclk_b_p) = { "mout_aclk_decon_eclk_a", "dout_mfc_pll" };
PNAME(mout_sclk_decon_eclk_c_p) = { "mout_aclk_decon_eclk_b", "dout_mphy_pll" };
PNAME(mout_sclk_decon_vclk_a_p) = { "oscclk", "mout_bus_pll_sub" };
PNAME(mout_sclk_decon_vclk_b_p) = { "mout_sclk_decon_vclk_a", "dout_mfc_pll" };
PNAME(mout_sclk_decon_vclk_c_p) = { "mout_sclk_decon_vclk_b", "dout_mphy_pll" };
PNAME(mout_sclk_decon_vclk_d_p) = { "dout_sclk_decon_vclk", "dout_sclk_decon_vclk_frac" };
PNAME(mout_sclk_dsd_a_p)	= { "oscclk", "dout_mfc_pll" };
PNAME(mout_sclk_dsd_b_p)	= { "mout_sclk_dsd_a", "mout_mphy_pll" };
PNAME(mout_sclk_dsd_c_p)	= { "mout_sclk_dsd_b", "mout_bus_pll_sub" };

/* CPIF */
PNAME(mout_phyclk_lli_tx0_symbol_user_p) = { "oscclk", "phyclk_lli_tx0_symbol", };
PNAME(mout_phyclk_lli_rx0_symbol_user_p) = { "oscclk", "phyclk_lli_rx0_symbol", };
PNAME(mout_mphy_pll_p)		= { "fin_pll", "fout_mphy_pll", };
PNAME(mout_phyclk_ufs_mphy_to_lli_user_p) = { "oscclk", "phyclk_ufs_mphy_to_lli", };
PNAME(mout_sclk_mphy_p)		= { "dout_sclk_mphy", "mout_phyclk_ufs_mphy_to_lli_user", };

/* BUS */
PNAME(mout_aclk_bus1_400_user_p) = { "oscclk", "aclk_bus1_400" };
PNAME(mout_aclk_bus2_400_user_p) = { "oscclk", "aclk_bus2_400" };
PNAME(mout_aclk_g2d_400_user_p) = { "oscclk", "aclk_g2d_400" };
PNAME(mout_aclk_g2d_266_user_p) = { "oscclk", "aclk_g2d_266" };

/* GSCL */
PNAME(mout_aclk_gscl_333_user_p) = { "oscclk", "aclk_gscl_333" };
PNAME(mout_aclk_gscl_111_user_p) = { "oscclk", "aclk_gscl_111" };

/* MSCL */
PNAME(mout_aclk_mscl_400_user_p) = { "oscclk", "aclk_mscl_400" };
PNAME(mout_sclk_jpeg_user_p)	= { "oscclk", "dout_sclk_jpeg" };

/* G3D */
PNAME(mout_g3d_pll_p)		= { "fin_pll", "fout_g3d_pll" };

/* MFC */
PNAME(mout_aclk_mfc0_333_user_p) = { "oscclk", "aclk_mfc0_333" };
PNAME(mout_aclk_mfc1_333_user_p) = { "oscclk", "aclk_mfc1_333" };

/* HEVC */
PNAME(mout_aclk_hevc_400_user_p) = { "oscclk", "aclk_hevc_400" };

/* DISP1 */
PNAME(mout_disp_pll_p)		= { "fin_pll", "fout_disp_pll" };
PNAME(mout_aclk_disp_333_user_p) = { "oscclk", "aclk_disp_333" };
PNAME(mout_aclk_disp_222_user_p) = { "oscclk", "aclk_disp_222" };
PNAME(mout_sclk_decon_eclk_user_p) = { "oscclk", "sclk_decon_eclk_top" };
PNAME(mout_sclk_decon_eclk_disp_p) = { "mout_disp_pll", "mout_sclk_decon_eclk_user" };
PNAME(mout_sclk_decon_vclk_user_p) = { "oscclk", "sclk_decon_vclk_top" };
PNAME(mout_sclk_decon_vclk_disp_p) = { "mout_disp_pll", "mout_sclk_decon_vclk_user" };
PNAME(mout_sclk_dsd_user_p)	= { "oscclk", "sclk_dsd" };
PNAME(mout_phyclk_hdmiphy_pixel_clko_user_p) = { "oscclk", "phyclk_hdmiphy_pixel_clko" };
PNAME(mout_phyclk_hdmiphy_tmds_clko_user_p) = { "oscclk", "phyclk_hdmiphy_tmds_clko" };
PNAME(mout_phyclk_mipidphy_rxclkesc0_user_p) = { "oscclk", "phyclk_mipidphy_rxclkesc0" };
PNAME(mout_phyclk_mipidphy_bitclkdiv8_user_p) = { "oscclk", "phyclk_mipidphy_bitclkdiv8" };


/* FSYS */
PNAME(mout_aclk_fsys_200_user_p) = { "oscclk", "aclk_fsys_200" };
PNAME(mout_sclk_usbdrd30_user_p) = { "oscclk", "sclk_usbdrd30_top" };
PNAME(mout_sclk_mmc0_user_p) = { "oscclk", "sclk_mmc0_top" };
PNAME(mout_sclk_mmc1_user_p) = { "oscclk", "sclk_mmc1_top" };
PNAME(mout_sclk_mmc2_user_p) = { "oscclk", "sclk_mmc2_top" };
PNAME(mout_sclk_ufsunipro_user_p) = { "oscclk", "sclk_ufsunipro_top" };
PNAME(mout_mphy_pll_26m_user_p) = { "oscclk", "sclk_mphy_pll_26m" };
PNAME(mout_phyclk_lli_mphy_to_ufs_user_p) = { "oscclk", "phyclk_lli_mphy_to_ufs" };
PNAME(mout_phyclk_usbdrd30_usbdrd30_phyclock_p) = { "oscclk", "phyclk_usbdrd30_udrd30_phyclock" };
PNAME(mout_phyclk_usbdrd30_udrd30_pipe_pclk_p) = { "oscclk", "phyclk_usbdrd30_udrd30_pipe_pclk" };
PNAME(mout_phyclk_usbhost20_phy_freeclk_p) = { "oscclk", "phyclk_usbhost20_phy_freeclk" };
PNAME(mout_phyclk_usbhost20_phy_phyclock_p) = { "oscclk", "phyclk_usbhost20_phy_phyclock" };
PNAME(mout_phyclk_usbhost20_phy_clk48mohci_p) = { "oscclk", "phyclk_usbhost20_phy_clk48mohci" };
PNAME(mout_phyclk_usbhost20_phy_hsic1_p) = { "oscclk", "phyclk_usbhost20_phy_hsc1" };
PNAME(mout_phyclk_ufs_tx0_symbol_user_p) = { "oscclk", "phyclk_ufs_tx0_symbol" };
PNAME(mout_phyclk_ufs_tx1_symbol_user_p) = { "oscclk", "phyclk_ufs_tx1_symbol" };
PNAME(mout_phyclk_ufs_rx0_symbol_user_p) = { "oscclk", "phyclk_ufs_rx0_symbol" };
PNAME(mout_phyclk_ufs_rx1_symbol_user_p) = { "oscclk", "phyclk_ufs_rx1_symbol" };

/* AUD0 */
PNAME(mout_aud_pll_user_p)	= { "fin_pll", "mout_aud_pll" };
PNAME(mout_aud_dpll_user_p)	= { "fin_pll", "mout_aud_dpll" };
PNAME(mout_aud_pll_sub_p)	= { "mout_aud_pll_user", "mout_aud_dpll_user" };
PNAME(mout_sclk_i2s_a_p)	= { "mout_aud_pll_sub", "ioclk_audiocdclk0" };
PNAME(mout_sclk_i2s_b_p)	= { "dout_sclk_i2s", "dout_sclk_i2s_frac" };
PNAME(mout_sclk_pcm_a_p)	= { "mout_aud_pll_sub", "ioclk_audiocdclk0" };
PNAME(mout_sclk_pcm_b_p)	= { "dout_sclk_pcm", "dout_sclk_pcm_frac" };
PNAME(mout_sclk_slimbus_p)	= { "dout_sclk_slimbus", "dout_sclk_slimbus_frac" };

/* CAM0 */
PNAME(mout_phyclk_rxbyteclkhs0_s4_p) = { "oscclk", "phyclk_rxbyteclkhs0_s4" };
PNAME(mout_phyclk_rxbyteclkhs0_s2a_p) = { "oscclk", "phyclk_rxbyteclkhs0_s2a" };

/* CAM1 */
PNAME(mout_sclk_isp_spi0_user_p) = { "oscclk", "sclk_isp_spi0_top" };
PNAME(mout_sclk_isp_spi1_user_p) = { "oscclk", "sclk_isp_spi1_top" };
PNAME(mout_sclk_isp_uart_user_p) = { "oscclk", "sclk_isp_uart_top" };
PNAME(mout_phyclk_rxbyteclkhs0_s2b_p) = { "oscclk", "phyclk_rxbyteclkhs0_s2b" };

struct samsung_fixed_rate_clock exynos5430_fixed_rate_clks[] __initdata = {
	FRATE(none, "oscclk", NULL, CLK_IS_ROOT, 24000000),
	FRATE(none, "phyclk_usbdrd30_udrd30_phyclock", NULL, CLK_IS_ROOT, 60000000),
	FRATE(none, "phyclk_usbdrd30_udrd30_pipe_pclk", NULL, CLK_IS_ROOT, 125000000),
	FRATE(none, "phyclk_usbhost20_phy_freeclk", NULL, CLK_IS_ROOT, 60000000),
	FRATE(none, "phyclk_usbhost20_phy_phyclock", NULL, CLK_IS_ROOT, 60000000),
	FRATE(none, "phyclk_usbhost20_phy_clk48mohci", NULL, CLK_IS_ROOT, 48000000),
	FRATE(none, "phyclk_usbhost20_phy_hsic1", NULL, CLK_IS_ROOT, 60000000),
	FRATE(none, "phyclk_ufs_tx0_symbol", NULL, CLK_IS_ROOT, 300000000),
	FRATE(none, "phyclk_ufs_tx1_symbol", NULL, CLK_IS_ROOT, 300000000),
	FRATE(none, "phyclk_ufs_rx0_symbol", NULL, CLK_IS_ROOT, 300000000),
	FRATE(none, "phyclk_ufs_rx1_symbol", NULL, CLK_IS_ROOT, 300000000),
	FRATE(none, "phyclk_lli_tx0_symbol", NULL, CLK_IS_ROOT, 150000000),
	FRATE(none, "phyclk_lli_rx0_symbol", NULL, CLK_IS_ROOT, 150000000),
	FRATE(none, "phyclk_rxbyteclkhs0_s4", NULL, CLK_IS_ROOT, 188000000),
	FRATE(none, "phyclk_rxbyteclkhs0_s2a", NULL, CLK_IS_ROOT, 188000000),
	FRATE(none, "phyclk_rxbyteclkhs0_s2b", NULL, CLK_IS_ROOT, 188000000),
	FRATE(none, "phyclk_hdmiphy_pixel_clko", NULL, CLK_IS_ROOT, 166000000),
	FRATE(none, "phyclk_hdmiphy_tmds_clko", NULL, CLK_IS_ROOT, 250000000),
	FRATE(none, "phyclk_mipidphy_rxclkesc0", NULL, CLK_IS_ROOT, 20000000),
	FRATE(none, "phyclk_mipidphy_bitclkdiv8", NULL, CLK_IS_ROOT, 188000000),

	FRATE(none, "phyclk_ufs_mphy_to_lli", NULL, CLK_IS_ROOT, 26000000),
	FRATE(none, "phyclk_lli_mphy_to_ufs", NULL, CLK_IS_ROOT, 26000000),

	FRATE(none, "ioclk_audiocdclk0", NULL, CLK_IS_ROOT, 0),
	FRATE(none, "ioclk_spdif_extclk", NULL, CLK_IS_ROOT, 36864000),
};

struct samsung_mux_clock exynos5430_mux_clks[] __initdata = {
	MUX(none, "mout_bus_pll_user", mout_bus_pll_user_p, SRC_SEL_TOP1, 0, 1),
	MUX(none, "mout_mfc_pll_user", mout_mfc_pll_user_p, SRC_SEL_TOP1, 4, 1),
	MUX(none, "mout_mphy_pll_user", mout_mphy_pll_user_p, SRC_SEL_TOP1, 8, 1),
	MUX(none, "mout_isp_pll", mout_isp_pll_p, SRC_SEL_TOP0, 0, 1),
	MUX(none, "mout_aud_pll", mout_aud_pll_p, SRC_SEL_TOP0, 4, 1),
	MUX(none, "mout_aud_dpll", mout_aud_dpll_p, SRC_SEL_TOP0, 8, 1),
	MUX(none, "mout_aclk_g2d_400_a", mout_aclk_g2d_400_a_p, SRC_SEL_TOP3, 0, 1),
	MUX(none, "mout_aclk_g2d_400_b", mout_aclk_g2d_400_b_p, SRC_SEL_TOP3, 4, 1),
	MUX(none, "mout_aclk_gscl_333", mout_aclk_gscl_333_p, SRC_SEL_TOP3, 8, 1),
	MUX(none, "mout_aclk_mscl_400_a", mout_aclk_mscl_400_a_p, SRC_SEL_TOP3, 12, 1),
	MUX(none, "mout_aclk_mscl_400_b", mout_aclk_mscl_400_b_p, SRC_SEL_TOP3, 16, 1),
	MUX(none, "mout_aclk_mfc0_333", mout_aclk_mfc0_333_p, SRC_SEL_TOP2, 16, 1),
	MUX(none, "mout_aclk_mfc1_333", mout_aclk_mfc1_333_p, SRC_SEL_TOP2, 20, 1),
	MUX(none, "mout_aclk_hevc_400", mout_aclk_hevc_400_p, SRC_SEL_TOP2, 24, 1),
	MUX(none, "mout_aclk_isp_400", mout_aclk_isp_400_p, SRC_SEL_TOP2, 0, 1),
	MUX(none, "mout_aclk_isp_dis_400", mout_aclk_isp_dis_400_p, SRC_SEL_TOP2, 4, 1),
	MUX(none, "mout_aclk_cam1_552_a", mout_aclk_cam1_552_a_p, SRC_SEL_TOP2, 8, 1),
	MUX(none, "mout_aclk_cam1_552_b", mout_aclk_cam1_552_b_p, SRC_SEL_TOP2, 12, 1),
	MUX(none, "mout_sclk_audio0", mout_sclk_audio0_p, SRC_SEL_TOP_PERIC1, 0, 2),
	MUX(none, "mout_sclk_audio1", mout_sclk_audio1_p, SRC_SEL_TOP_PERIC1, 4, 2),
	MUX(none, "mout_sclk_spi0", mout_sclk_spi_p, SRC_SEL_TOP_PERIC0, 0, 1),
	MUX(none, "mout_sclk_spi1", mout_sclk_spi_p, SRC_SEL_TOP_PERIC0, 4, 1),
	MUX(none, "mout_sclk_spi2", mout_sclk_spi_p, SRC_SEL_TOP_PERIC0, 8, 1),
	MUX(none, "mout_sclk_uart0", mout_sclk_uart_p, SRC_SEL_TOP_PERIC0, 12, 1),
	MUX(none, "mout_sclk_uart1", mout_sclk_uart_p, SRC_SEL_TOP_PERIC0, 16, 1),
	MUX(none, "mout_sclk_uart2", mout_sclk_uart_p, SRC_SEL_TOP_PERIC0, 20, 1),
	MUX(none, "mout_sclk_slimbus_a", mout_sclk_slimbus_a_p, SRC_SEL_TOP_PERIC1, 16, 1),
	MUX(none, "mout_sclk_slimbus_b", mout_sclk_slimbus_b_p, SRC_SEL_TOP_PERIC1, 20, 1),
	MUX_A(none, "mout_sclk_spdif", mout_sclk_spdif_p, SRC_SEL_TOP_PERIC1, 12, 2, "sclk_spdif"),
	MUX(none, "mout_sclk_usbdrd30", mout_sclk_usbdrd30_p, SRC_SEL_TOP_FSYS1, 0, 1),
	MUX(none, "mout_sclk_mmc0_a", mout_sclk_mmc0_a_p, SRC_SEL_TOP_FSYS0, 0, 1),
	MUX(none, "mout_sclk_mmc0_b", mout_sclk_mmc0_b_p, SRC_SEL_TOP_FSYS0, 4, 1),
	MUX(none, "mout_sclk_mmc0_c", mout_sclk_mmc0_c_p, SRC_SEL_TOP_FSYS0, 8, 1),
	MUX(none, "mout_sclk_mmc0_d", mout_sclk_mmc0_d_p, SRC_SEL_TOP_FSYS0, 12, 1),
	MUX(none, "mout_sclk_mmc1_a", mout_sclk_mmc1_a_p, SRC_SEL_TOP_FSYS0, 16, 1),
	MUX(none, "mout_sclk_mmc1_b", mout_sclk_mmc1_b_p, SRC_SEL_TOP_FSYS0, 20, 1),
	MUX(none, "mout_sclk_mmc2_a", mout_sclk_mmc2_a_p, SRC_SEL_TOP_FSYS0, 24, 1),
	MUX(none, "mout_sclk_mmc2_b", mout_sclk_mmc2_b_p, SRC_SEL_TOP_FSYS0, 28, 1),
	MUX(none, "mout_sclk_ufsunipro", mout_sclk_ufsunipro_p, SRC_SEL_TOP_FSYS1, 8, 1),
	MUX(none, "mout_sclk_jpeg_a", mout_sclk_jpeg_a_p, SRC_SEL_TOP_MSCL, 0, 1),
	MUX(none, "mout_sclk_jpeg_b", mout_sclk_jpeg_b_p, SRC_SEL_TOP_MSCL, 4, 1),
	MUX(none, "mout_sclk_jpeg_c", mout_sclk_jpeg_c_p, SRC_SEL_TOP_MSCL, 8, 1),
	MUX(none, "mout_sclk_isp_spi0", mout_sclk_isp_spi_p, SRC_SEL_TOP_CAM1, 0, 1),
	MUX(none, "mout_sclk_isp_spi1", mout_sclk_isp_spi_p, SRC_SEL_TOP_CAM1, 4, 1),
	MUX(none, "mout_sclk_isp_uart", mout_sclk_isp_uart_p, SRC_SEL_TOP_CAM1, 8, 1),
	MUX(none, "mout_sclk_isp_sensor0", mout_sclk_isp_sensor_p, SRC_SEL_TOP_CAM1, 16, 1),
	MUX(none, "mout_sclk_isp_sensor1", mout_sclk_isp_sensor_p, SRC_SEL_TOP_CAM1, 20, 1),
	MUX(none, "mout_sclk_isp_sensor2", mout_sclk_isp_sensor_p, SRC_SEL_TOP_CAM1, 24, 1),
	MUX(none, "mout_sclk_hdmi_spdif", mout_sclk_hdmi_spdif_p, SRC_SEL_TOP_DISP, 0, 1),

	MUX(none, "mout_bus_dpll", mout_bus_dpll_p, SRC_SEL_MIF0, 12, 1),
	MUX(none, "mout_mem_pll", mout_mem_pll_p, SRC_SEL_MIF0, 4, 1),
	MUX(none, "mout_bus_pll", mout_bus_pll_p, SRC_SEL_MIF0, 0, 1),
	MUX(none, "mout_mfc_pll", mout_mfc_pll_p, SRC_SEL_MIF0, 8, 1),
	MUX(none, "mout_bus_pll_sub", mout_bus_pll_sub_p, SRC_SEL_MIF1, 0, 1),
	MUX(none, "mout_aclk_mif_400_a", mout_aclk_mif_400_a_p, SRC_SEL_MIF2, 0, 1),
	MUX(none, "mout_aclk_mif_400_b", mout_aclk_mif_400_b_p, SRC_SEL_MIF2, 4, 1),
	MUX(none, "mout_clkm_phy_a", mout_clkm_phy_a_p, SRC_SEL_MIF1, 4, 1),
	MUX(none, "mout_clkm_phy_b", mout_clkm_phy_b_p, SRC_SEL_MIF1, 8, 1),
	MUX(none, "mout_clkm_phy_c", mout_clkm_phy_c_p, SRC_SEL_MIF1, 12, 1),
	MUX(none, "mout_clk2x_phy_a", mout_clk2x_phy_a_p, SRC_SEL_MIF1, 16, 1),
	MUX(none, "mout_clk2x_phy_b", mout_clk2x_phy_b_p, SRC_SEL_MIF1, 20, 1),
	MUX(none, "mout_clk2x_phy_c", mout_clk2x_phy_c_p, SRC_SEL_MIF1, 24, 1),
	MUX(none, "mout_aclk_disp_333_a", mout_aclk_disp_333_a_p, SRC_SEL_MIF3, 0, 1),
	MUX(none, "mout_aclk_disp_333_b", mout_aclk_disp_333_b_p, SRC_SEL_MIF3, 4, 1),
	MUX(none, "mout_aclk_disp_222", mout_aclk_disp_222_p, SRC_SEL_MIF3, 8, 1),
	MUX(none, "mout_aclk_bus1_400_a", mout_aclk_bus1_400_a_p, SRC_SEL_MIF3, 12, 1),
	MUX(none, "mout_aclk_bus1_400_b", mout_aclk_bus1_400_b_p, SRC_SEL_MIF3, 16, 1),
	MUX(none, "mout_sclk_decon_eclk_a", mout_sclk_decon_eclk_a_p, SRC_SEL_MIF4, 0, 1),
	MUX(none, "mout_sclk_decon_eclk_b", mout_sclk_decon_eclk_b_p, SRC_SEL_MIF4, 4, 1),
	MUX(none, "mout_sclk_decon_eclk_c", mout_sclk_decon_eclk_c_p, SRC_SEL_MIF4, 8, 1),
	MUX(none, "mout_sclk_decon_vclk_a", mout_sclk_decon_vclk_a_p, SRC_SEL_MIF4, 16, 1),
	MUX(none, "mout_sclk_decon_vclk_b", mout_sclk_decon_vclk_b_p, SRC_SEL_MIF4, 20, 1),
	MUX(none, "mout_sclk_decon_vclk_c", mout_sclk_decon_vclk_c_p, SRC_SEL_MIF4, 24, 1),
	MUX(none, "mout_sclk_decon_vclk_d", mout_sclk_decon_vclk_d_p, SRC_SEL_MIF4, 28, 1),
	MUX(none, "mout_sclk_dsd_a", mout_sclk_dsd_a_p, SRC_SEL_MIF5, 0, 1),
	MUX(none, "mout_sclk_dsd_b", mout_sclk_dsd_b_p, SRC_SEL_MIF5, 4, 1),
	MUX(none, "mout_sclk_dsd_c", mout_sclk_dsd_c_p, SRC_SEL_MIF5, 8, 1),

	MUX(none, "mout_phyclk_lli_tx0_symbol_user", mout_phyclk_lli_tx0_symbol_user_p, SRC_SEL_CPIF1, 4, 1),
	MUX(none, "mout_phyclk_lli_rx0_symbol_user", mout_phyclk_lli_rx0_symbol_user_p, SRC_SEL_CPIF1, 8, 1),
	MUX(none, "mout_mphy_pll", mout_mphy_pll_p, SRC_SEL_CPIF0, 0, 1),
	MUX(none, "mout_phyclk_ufs_mphy_to_lli_user", mout_phyclk_ufs_mphy_to_lli_user_p, SRC_SEL_CPIF1, 0, 1),
	MUX(none, "mout_sclk_mphy", mout_sclk_mphy_p, SRC_SEL_CPIF2, 0, 1),

	MUX(none, "mout_aclk_bus1_400_user", mout_aclk_bus1_400_user_p, SRC_SEL_BUS1, 0, 1),
	MUX(none, "mout_aclk_bus2_400_user", mout_aclk_bus2_400_user_p, SRC_SEL_BUS2, 0, 1),

	MUX(none, "mout_aclk_g2d_400_user", mout_aclk_g2d_400_user_p, SRC_SEL_G2D, 0, 1),
	MUX(none, "mout_aclk_g2d_266_user", mout_aclk_g2d_266_user_p, SRC_SEL_G2D, 4, 1),

	MUX(none, "mout_aclk_gscl_333_user", mout_aclk_gscl_333_user_p, SRC_SEL_GSCL, 0, 1),
	MUX(none, "mout_aclk_gscl_111_user", mout_aclk_gscl_111_user_p, SRC_SEL_GSCL, 4, 1),

	MUX(none, "mout_aclk_mscl_400_user", mout_aclk_mscl_400_user_p, SRC_SEL_MSCL, 0, 1),
	MUX(none, "mout_sclk_jpeg_user", mout_sclk_jpeg_user_p, SRC_SEL_MSCL, 4, 1),

	MUX(none, "mout_g3d_pll", mout_g3d_pll_p, SRC_SEL_G3D, 0, 1),

	MUX(none, "mout_aclk_mfc0_333_user", mout_aclk_mfc0_333_user_p, SRC_SEL_MFC0, 0, 1),
	MUX(none, "mout_aclk_mfc1_333_user", mout_aclk_mfc1_333_user_p, SRC_SEL_MFC1, 0, 1),

	MUX(none, "mout_aclk_hevc_400_user", mout_aclk_hevc_400_user_p, SRC_SEL_HEVC, 0, 1),

	MUX(none, "mout_disp_pll", mout_disp_pll_p, SRC_SEL_DISP0, 0, 1),
	MUX(none, "mout_aclk_disp_333_user", mout_aclk_disp_333_user_p, SRC_SEL_DISP1, 0, 1),
	MUX(none, "mout_aclk_disp_222_user", mout_aclk_disp_222_user_p, SRC_SEL_DISP1, 4, 1),
	MUX(none, "mout_sclk_decon_eclk_user", mout_sclk_decon_eclk_user_p, SRC_SEL_DISP1, 8, 1),
	MUX(none, "mout_sclk_decon_eclk_disp", mout_sclk_decon_eclk_disp_p, SRC_SEL_DISP3, 0, 1),
	MUX(none, "mout_sclk_decon_vclk_user", mout_sclk_decon_vclk_user_p, SRC_SEL_DISP1, 12, 1),
	MUX(none, "mout_sclk_decon_vclk_disp", mout_sclk_decon_vclk_disp_p, SRC_SEL_DISP3, 4, 1),
	MUX(none, "mout_sclk_dsd_user", mout_sclk_dsd_user_p, SRC_SEL_DISP1, 16, 1),
	MUX(none, "mout_phyclk_hdmiphy_pixel_clko_user", mout_phyclk_hdmiphy_pixel_clko_user_p, SRC_SEL_DISP2, 0, 1),
	MUX(none, "mout_phyclk_hdmiphy_tmds_clko_user", mout_phyclk_hdmiphy_tmds_clko_user_p, SRC_SEL_DISP2, 4, 1),
	MUX(none, "mout_phyclk_mipidphy_rxclkesc0_user", mout_phyclk_mipidphy_rxclkesc0_user_p, SRC_SEL_DISP2, 8, 1),
	MUX(none, "mout_phyclk_mipidphy_bitclkdiv8_user", mout_phyclk_mipidphy_bitclkdiv8_user_p, SRC_SEL_DISP2, 12, 1),

	MUX(none, "mout_aclk_fsys_200_user", mout_aclk_fsys_200_user_p, SRC_SEL_FSYS0, 0, 1),
	MUX(none, "mout_sclk_usbdrd30_user", mout_sclk_usbdrd30_user_p, SRC_SEL_FSYS1, 0, 1),
	MUX(none, "mout_sclk_mmc0_user", mout_sclk_mmc0_user_p, SRC_SEL_FSYS1, 12, 1),
	MUX(none, "mout_sclk_mmc1_user", mout_sclk_mmc1_user_p, SRC_SEL_FSYS1, 16, 1),
	MUX(none, "mout_sclk_mmc2_user", mout_sclk_mmc2_user_p, SRC_SEL_FSYS1, 20, 1),
	MUX(none, "mout_sclk_ufsunipro_user", mout_sclk_ufsunipro_user_p, SRC_SEL_FSYS1, 24, 1),
	MUX(none, "mout_mphy_pll_26m_user", mout_mphy_pll_26m_user_p, SRC_SEL_FSYS0, 4, 1),
	MUX(none, "mout_phyclk_lli_mphy_to_ufs_user", mout_phyclk_lli_mphy_to_ufs_user_p, SRC_SEL_FSYS3, 0, 1),
	MUX(none, "mout_phyclk_usbdrd30_usbdrd30_phyclock", mout_phyclk_usbdrd30_usbdrd30_phyclock_p, SRC_SEL_FSYS2, 0, 1),
	MUX(none, "mout_phyclk_usbdrd30_udrd30_pipe_pclk", mout_phyclk_usbdrd30_udrd30_pipe_pclk_p, SRC_SEL_FSYS2, 4, 1),
	MUX(none, "mout_phyclk_usbhost20_phy_freeclk", mout_phyclk_usbhost20_phy_freeclk_p, SRC_SEL_FSYS2, 8, 1),
	MUX(none, "mout_phyclk_usbhost20_phy_phyclock", mout_phyclk_usbhost20_phy_phyclock_p, SRC_SEL_FSYS2, 12, 1),
	MUX(none, "mout_phyclk_usbhost20_phy_clk48mohci", mout_phyclk_usbhost20_phy_clk48mohci_p, SRC_SEL_FSYS2, 16, 1),
	MUX(none, "mout_phyclk_usbhost20_phy_hsic1", mout_phyclk_usbhost20_phy_hsic1_p, SRC_SEL_FSYS2, 20, 1),
	MUX(none, "mout_phyclk_ufs_tx0_symbol_user", mout_phyclk_ufs_tx0_symbol_user_p, SRC_SEL_FSYS3, 4, 1),
	MUX(none, "mout_phyclk_ufs_tx1_symbol_user", mout_phyclk_ufs_tx1_symbol_user_p, SRC_SEL_FSYS3, 8, 1),
	MUX(none, "mout_phyclk_ufs_rx0_symbol_user", mout_phyclk_ufs_rx0_symbol_user_p, SRC_SEL_FSYS3, 12, 1),
	MUX(none, "mout_phyclk_ufs_rx1_symbol_user", mout_phyclk_ufs_rx1_symbol_user_p, SRC_SEL_FSYS3, 16, 1),

	MUX(none, "mout_aud_pll_user", mout_aud_pll_user_p, SRC_SEL_AUD0, 0, 1),
	MUX(none, "mout_aud_dpll_user", mout_aud_dpll_user_p, SRC_SEL_AUD0, 4, 1),
	MUX(none, "mout_aud_pll_sub", mout_aud_pll_sub_p, SRC_SEL_AUD0, 8, 1),
	MUX(none, "mout_sclk_i2s_a", mout_sclk_i2s_a_p, SRC_SEL_AUD1, 0, 1),
	MUX(none, "mout_sclk_i2s_b", mout_sclk_i2s_b_p, SRC_SEL_AUD1, 4, 1),
	MUX(none, "mout_sclk_pcm_a", mout_sclk_pcm_a_p, SRC_SEL_AUD1, 8, 1),
	MUX(none, "mout_sclk_pcm_b", mout_sclk_pcm_b_p, SRC_SEL_AUD1, 12, 1),
	MUX(none, "mout_sclk_slimbus", mout_sclk_slimbus_p, SRC_SEL_AUD1, 16, 1),

	MUX(none, "mout_phyclk_rxbyteclkhs0_s4", mout_phyclk_rxbyteclkhs0_s4_p, SRC_SEL_CAM01, 4, 1),
	MUX(none, "mout_phyclk_rxbyteclkhs0_s2a", mout_phyclk_rxbyteclkhs0_s2a_p, SRC_SEL_CAM01, 0, 1),

	MUX(none, "mout_sclk_isp_spi0_user", mout_sclk_isp_spi0_user_p, SRC_SEL_CAM10, 12, 1),
	MUX(none, "mout_sclk_isp_spi1_user", mout_sclk_isp_spi1_user_p, SRC_SEL_CAM10, 16, 1),
	MUX(none, "mout_sclk_isp_uart_user", mout_sclk_isp_uart_user_p, SRC_SEL_CAM10, 20, 1),
	MUX(none, "mout_phyclk_rxbyteclkhs0_s2b", mout_phyclk_rxbyteclkhs0_s2b_p, SRC_SEL_CAM11, 0, 1),
};

struct samsung_div_clock exynos5430_div_clks[] __initdata = {
	DIV(none, "dout_aclk_g2d_400", "mout_aclk_g2d_400_b", DIV_TOP1, 0, 3),
	DIV(none, "dout_aclk_g2d_266", "mout_bus_pll_user", DIV_TOP1, 8, 3),
	DIV(none, "dout_aclk_gscl_333", "mout_aclk_gscl_333", DIV_TOP1, 24, 3),
	DIV(none, "dout_aclk_gscl_111", "mout_aclk_gscl_111", DIV_TOP1, 28, 3),
	DIV(none, "dout_aclk_mscl_400", "mout_aclk_mscl_400_b", DIV_TOP2, 4, 3),
	DIV(none, "dout_aclk_imem_266", "mout_bus_pll_user", DIV_TOP3, 16, 3),
	DIV(none, "dout_aclk_imem_200", "mout_bus_pll_user", DIV_TOP3, 20, 3),
	DIV(none, "dout_aclk_peris_66_a", "mout_bus_pll_user", DIV_TOP3, 0, 3),
	DIV(none, "dout_aclk_peris_66_b", "dout_aclk_peris_66_a", DIV_TOP3, 4, 3),
	DIV(none, "dout_aclk_peric_66_a", "mout_bus_pll_user", DIV_TOP3, 8, 3),
	DIV(none, "dout_aclk_peric_66_b", "dout_aclk_peric_66_a", DIV_TOP3, 12, 3),
	DIV(none, "dout_aclk_mfc0_333", "mout_aclk_mfc0_333", DIV_TOP1, 12, 3),
	DIV(none, "dout_aclk_mfc1_333", "mout_aclk_mfc1_333", DIV_TOP1, 16, 3),
	DIV(none, "dout_aclk_hevc_400", "mout_aclk_hevc_400", DIV_TOP1, 20, 3),
	DIV(none, "dout_aclk_fsys_200", "mout_bus_pll_user", DIV_TOP2, 0, 3),
	DIV(none, "dout_aclk_isp_400", "mout_aclk_isp_400", DIV_TOP0, 0, 3),
	DIV(none, "dout_aclk_isp_dis_400", "mout_aclk_isp_dis_400", DIV_TOP0, 4, 3),
	DIV(none, "dout_aclk_cam0_552", "mout_isp_pll", DIV_TOP0, 8, 3),
	DIV(none, "dout_aclk_cam0_400", "mout_bus_pll_user", DIV_TOP0, 12, 3),
	DIV(none, "dout_aclk_cam0_333", "mout_mfc_pll_user", DIV_TOP0, 16, 3),
	DIV(none, "dout_aclk_cam1_552", "mout_aclk_cam1_552_b", DIV_TOP0, 20, 3),
	DIV(none, "dout_aclk_cam1_400", "mout_bus_pll_user", DIV_TOP0, 24, 3),
	DIV(none, "dout_aclk_cam1_333", "mout_mfc_pll_user", DIV_TOP0, 28, 3),
	DIV(none, "dout_sclk_audio0", "mout_sclk_audio0", DIV_TOP_PERIC3, 0, 4),
	DIV(none, "dout_sclk_audio1", "mout_sclk_audio1", DIV_TOP_PERIC3, 4, 4),
	DIV(none, "dout_sclk_pcm1", "dout_sclk_audio1", DIV_TOP_PERIC3, 8, 8),
	DIV(none, "dout_sclk_i2s1", "dout_sclk_audio1", DIV_TOP_PERIC3, 16, 6),
	DIV(none, "dout_sclk_spi0_a", "mout_sclk_spi0", DIV_TOP_PERIC0, 0, 4),
	DIV(none, "dout_sclk_spi1_a", "mout_sclk_spi1", DIV_TOP_PERIC0, 12, 4),
	DIV(none, "dout_sclk_spi2_a", "mout_sclk_spi2", DIV_TOP_PERIC1, 0, 4),
	DIV(none, "dout_sclk_spi0_b", "mout_sclk_spi0", DIV_TOP_PERIC0, 4, 8),
	DIV(none, "dout_sclk_spi1_b", "mout_sclk_spi1", DIV_TOP_PERIC0, 16, 8),
	DIV(none, "dout_sclk_spi2_b", "mout_sclk_spi2", DIV_TOP_PERIC1, 4, 8),
	DIV(none, "dout_sclk_uart0", "mout_sclk_uart0", DIV_TOP_PERIC2, 0, 4),
	DIV(none, "dout_sclk_uart1", "mout_sclk_uart1", DIV_TOP_PERIC2, 4, 4),
	DIV(none, "dout_sclk_uart2", "mout_sclk_uart2", DIV_TOP_PERIC2, 8, 4),
	DIV(none, "dout_sclk_slimbus", "mout_sclk_slimbus_b", DIV_TOP_PERIC3, 24, 5),
	DIV(none, "dout_usbdrd30", "mout_sclk_usbdrd30", DIV_TOP_FSYS2, 0, 4),
	DIV(none, "dout_mmc0_a", "mout_sclk_mmc0_d", DIV_TOP_FSYS0, 0, 4),
	DIV(none, "dout_mmc0_b", "dout_mmc0_a", DIV_TOP_FSYS0, 4, 8),
	DIV(none, "dout_mmc1_a", "mout_sclk_mmc1_b", DIV_TOP_FSYS0, 12, 4),
	DIV(none, "dout_mmc1_b", "dout_mmc1_a", DIV_TOP_FSYS0, 16, 8),
	DIV(none, "dout_mmc2_a", "mout_sclk_mmc2_b", DIV_TOP_FSYS1, 0, 4),
	DIV(none, "dout_mmc2_b", "dout_mmc2_a", DIV_TOP_FSYS1, 4, 8),
	DIV(none, "dout_ufsunipro", "mout_sclk_ufsunipro", DIV_TOP_FSYS2, 4, 4),
#if 0
	DIV(none, "dout_sclk_jpeg", "mout_sclk_jpeg_c", DIV_TOP_MSCL, 0, 4),
	DIV(none, "dout_sclk_isp_spi0_a", "mout_sclk_isp_spi0", DIV_TOP_CAM10, 0, 4),
	DIV(none, "dout_sclk_isp_spi0_b", "dout_sclk_isp_spi0_a", DIV_TOP_CAM10, 4, 8),
	DIV(none, "dout_sclk_isp_spi1_a", "mout_sclk_isp_spi1", DIV_TOP_CAM10, 12, 4),
	DIV(none, "dout_sclk_isp_spi1_b", "dout_sclk_isp_spi1_b", DIV_TOP_CAM10, 16, 8),
	DIV(none, "dout_sclk_isp_uart", "mout_sclk_isp_uart", DIV_TOP_CAM10, 24, 4),
	DIV(none, "dout_sclk_isp_sensor0_a", "mout_sclk_isp_sensor0", DIV_TOP_CAM11, 0, 4),
	DIV(none, "dout_sclk_isp_sensor0_b", "mout_sclk_isp_sensor0_a", DIV_TOP_CAM11, 4, 4),
	DIV(none, "dout_sclk_isp_sensor1_a", "mout_sclk_isp_sensor1", DIV_TOP_CAM11, 8, 4),
	DIV(none, "dout_sclk_isp_sensor1_b", "mout_sclk_isp_sensor1_a", DIV_TOP_CAM11, 12, 4),
	DIV(none, "dout_sclk_isp_sensor2_a", "mout_sclk_isp_sensor2", DIV_TOP_CAM11, 16, 4),
	DIV(none, "dout_sclk_isp_sensor2_b", "mout_sclk_isp_sensor2_a", DIV_TOP_CAM11, 20, 4),
#endif

	DIV(none, "dout_mem_pll", "mout_mem_pll", DIV_MIF0, 4, 2),
	DIV(none, "dout_bus_pll", "mout_bus_pll", DIV_MIF0, 0, 2),
	DIV(none, "dout_mfc_pll", "mout_mfc_pll", DIV_MIF0, 8, 2),
	DIV(none, "dout_mfc_pll_div3", "mout_mfc_pll", DIV_MIF0, 12, 2),
	DIV_A(aclk_mifnm_200, "dout_aclk_mifnm_200", "mout_bus_pll_sub", DIV_MIF2, 8, 3, "aclk_mifnm_200"),
	DIV_A(aclk_mifnd_133, "dout_aclk_mifnd_133", "mout_bus_pll_sub", DIV_MIF2, 16, 4, "aclk_mifnd_200"),
	DIV_A(aclk_mif_133, "dout_aclk_mif_133", "mout_bus_pll_sub", DIV_MIF2, 12, 4, "aclk_mif_133"),
	DIV_A(aclk_mif_400, "dout_aclk_mif_400", "mout_aclk_mif_400_b", DIV_MIF2, 0, 3, "aclk_mif_400"),
	DIV_A(aclk_mif_200, "dout_aclk_mif_200", "dout_aclk_mif_400", DIV_MIF2, 4, 2, "aclk_mif_200"),
	DIV_A(clkm_phy, "dout_clkm_phy", "mout_clkm_phy_c", DIV_MIF1, 0, 2, "clkm_phy"),
	DIV_A(clk2x_phy, "dout_clk2x_phy", "mout_clk2x_phy_c", DIV_MIF1, 4, 4, "clk2x_phy"),
	DIV_A(aclk_drex0, "dout_aclk_drex0", "dout_clk2x_phy", DIV_MIF1, 8, 2, "aclk_drex0"),
	DIV_A(aclk_drex1, "dout_aclk_drex1", "dout_clk2x_phy", DIV_MIF1, 12, 2, "aclk_drex1"),
	DIV(none, "dout_sclk_hpm_mif", "dout_clk2x_phy", DIV_MIF1, 16, 2),
	DIV(none, "dout_aclk_cpif_200", "mout_bus_pll_sub", DIV_MIF3, 0, 3),
	DIV(none, "dout_aclk_disp_333", "mout_aclk_disp_333_b", DIV_MIF3, 4, 3),
	DIV(none, "dout_aclk_disp_222", "mout_aclk_disp_222", DIV_MIF3, 8, 3),
	DIV(none, "dout_aclk_bus1_400", "mout_aclk_bus1_400_b", DIV_MIF3, 12, 4),
	DIV(none, "dout_aclk_bus2_400", "mout_bus_pll_sub", DIV_MIF3, 16, 4),
	DIV(none, "dout_sclk_decon_eclk", "mout_sclk_decon_eclk_c", DIV_MIF4, 0, 4),
	DIV(none, "dout_sclk_decon_vclk", "mout_sclk_decon_vclk_c", DIV_MIF4, 4, 4),
	DIV(none, "dout_sclk_decon_vclk_frac", "mout_sclk_decon_vclk_c", DIV_MIF5, 24, 8),

	DIV(none, "dout_sclk_mphy", "mout_mphy_pll", DIV_CPIF, 0, 5),

	DIV(none, "dout_pclk_bus1_133", "mout_aclk_bus1_400_user", DIV_BUS1, 0, 3),
	DIV(none, "dout_pclk_bus2_133", "mout_aclk_bus2_400_user", DIV_BUS2, 0, 3),

#if 0
	DIV(none, "dout_pclk_g2d", "mout_aclk_g2d_266_user", DIV_G2D, 0, 2),

	DIV(none, "dout_pclk_mscl", "mout_aclk_mscl_400_user", DIV_MSCL, 0, 3),

	DIV(none, "dout_aclk_g3d", "mout_g3d_pll", DIV_G3D, 0, 3),
	DIV(none, "dout_pclk_g3d", "dout_aclk_g3d", DIV_G3D, 4, 3),
	DIV(none, "dout_sclk_hpm_g3d", "mout_g3d_pll", DIV_G3D, 8, 2),

	DIV(none, "dout_pclk_mfc0", "mout_aclk_mfc0_333_user", DIV_MFC0, 0, 2),
	DIV(none, "dout_pclk_mfc1", "mout_aclk_mfc1_333_user", DIV_MFC1, 0, 2),

	DIV(none, "dout_pclk_hevc", "mout_aclk_hevc_400_user", DIV_HEVC, 0, 2),
#endif
	DIV(none, "dout_pclk_disp", "mout_aclk_disp_333_user", DIV_DISP, 0, 2),
	DIV(none, "dout_sclk_decon_eclk_disp", "mout_sclk_decon_eclk_disp", DIV_DISP, 4, 3),
	DIV(none, "dout_sclk_decon_vclk_disp", "mout_sclk_decon_vclk_disp", DIV_DISP, 8, 3),

#if 0
	DIV(none, "dout_aud_ca5", "mout_aud_pll_sub", DIV_AUD0, 0, 4),
	DIV(none, "dout_aclk_aud", "dout_aud_ca5", DIV_AUD0, 4, 4),
	DIV(none, "dout_aud_pclk_dbg", "dout_aud_ca5", DIV_AUD0, 8, 4),
	DIV(none, "dout_sclk_i2s", "mout_sclk_i2s_a", DIV_AUD1, 0, 4),
	DIV(none, "dout_sclk_i2s_frac", "mout_sclk_i2s_a", DIV_AUD2, 24, 8),
	DIV(none, "dout_sclk_pcm", "mout_sclk_pcm_a", DIV_AUD1, 4, 8),
	DIV(none, "dout_sclk_pcm_frac", "mout_sclk_pcm_a", DIV_AUD3, 24, 8),
	DIV(none, "dout_sclk_slimbus", "mout_aud_pll_sub", DIV_AUD1, 16, 5),
	DIV(none, "dout_sclk_slimbus_frac", "mout_aud_pll_sub", DIV_AUD4, 24, 8),
	DIV(none, "dout_sclk_uart", "mout_aud_pll_sub", DIV_AUD1, 12, 4),
#endif
};

struct samsung_gate_clock exynos5430_gate_clks[] __initdata = {
	/* TOP */
	GATE(aclk_g2d_400, "aclk_g2d_400", "dout_aclk_g2d_400", ENABLE_ACLK_TOP, 0, CLK_SET_RATE_PARENT, 0),
	GATE(aclk_g2d_266, "aclk_g2d_266", "dout_aclk_g2d_266", ENABLE_ACLK_TOP, 2, CLK_SET_RATE_PARENT, 0),
	GATE(aclk_mfc0_333, "aclk_mfc0_333", "dout_aclk_mfc0_333", ENABLE_ACLK_TOP, 3, CLK_SET_RATE_PARENT, 0),
	GATE(aclk_mfc1_333, "aclk_mfc1_333", "dout_aclk_mfc1_333", ENABLE_ACLK_TOP, 4, CLK_SET_RATE_PARENT, 0),
	GATE(aclk_hevc_400, "aclk_hevc_400", "dout_aclk_hevc_400", ENABLE_ACLK_TOP, 5, CLK_SET_RATE_PARENT, 0),
	GATE(aclk_isp_400, "aclk_isp_400", "dout_aclk_isp_400", ENABLE_ACLK_TOP, 6, CLK_SET_RATE_PARENT, 0),
	GATE(aclk_isp_dis_400, "aclk_isp_dis_400", "dout_aclk_isp_dis_400", ENABLE_ACLK_TOP, 7, CLK_SET_RATE_PARENT, 0),
	GATE(aclk_cam0_552, "aclk_cam0_552", "dout_aclk_cam0_552", ENABLE_ACLK_TOP, 8, CLK_SET_RATE_PARENT, 0),
	GATE(aclk_cam0_400, "aclk_cam0_400", "dout_aclk_cam0_400", ENABLE_ACLK_TOP, 9, CLK_SET_RATE_PARENT, 0),
	GATE(aclk_cam0_333, "aclk_cam0_333", "dout_aclk_cam0_333", ENABLE_ACLK_TOP, 10, CLK_SET_RATE_PARENT, 0),
	GATE(aclk_cam1_552, "aclk_cam1_552", "dout_aclk_cam1_552", ENABLE_ACLK_TOP, 11, CLK_SET_RATE_PARENT, 0),
	GATE(aclk_cam1_400, "aclk_cam1_400", "dout_aclk_cam1_400", ENABLE_ACLK_TOP, 12, CLK_SET_RATE_PARENT, 0),
	GATE(aclk_cam1_333, "aclk_cam1_333", "dout_aclk_cam1_333", ENABLE_ACLK_TOP, 13, CLK_SET_RATE_PARENT, 0),
	GATE(aclk_gscl_333, "aclk_gscl_333", "dout_aclk_gscl_333", ENABLE_ACLK_TOP, 14, CLK_SET_RATE_PARENT, 0),
	GATE(aclk_gscl_111, "aclk_gscl_111", "dout_aclk_gscl_111", ENABLE_ACLK_TOP, 15, CLK_SET_RATE_PARENT, 0),
	GATE(aclk_fsys_200, "aclk_fsys_200", "dout_aclk_fsys_200", ENABLE_ACLK_TOP, 18, CLK_SET_RATE_PARENT, 0),
	GATE(aclk_mscl_400, "aclk_mscl_400", "dout_aclk_mscl_400", ENABLE_ACLK_TOP, 19, CLK_SET_RATE_PARENT, 0),
	GATE(aclk_peris_66, "aclk_peris_66", "dout_aclk_peris_66_b", ENABLE_ACLK_TOP, 21, CLK_SET_RATE_PARENT, 0),
	GATE(aclk_peric_66, "aclk_peric_66", "dout_aclk_peric_66_b", ENABLE_ACLK_TOP, 22, CLK_SET_RATE_PARENT, 0),
	GATE(aclk_imem_266, "aclk_imem_266", "dout_aclk_imem_266", ENABLE_ACLK_TOP, 23, CLK_SET_RATE_PARENT, 0),
	GATE(aclk_imem_200, "aclk_imem_200", "dout_aclk_imem_200", ENABLE_ACLK_TOP, 24, CLK_SET_RATE_PARENT, 0),

	/* sclk TOP */

#if 0
	/* sclk TOP_MSCL */
	GATE(sclk_jpeg_top, "sclk_jpeg_top", "dout_sclk_jpeg", ENABLE_SCLK_TOP_MSCL, 0, 0, 0),

	/* sclk TOP_CAM1 */
	GATE(sclk_isp_spi0_top, "sclk_isp_spi0_top", "dout_sclk_isp_spi0_b", ENABLE_SCLK_TOP_CAM1, 0, 0, 0),
	GATE(sclk_isp_spi1_top, "sclk_isp_spi1_top", "dout_sclk_isp_spi1_b", ENABLE_SCLK_TOP_CAM1, 1, 0, 0),
	GATE(sclk_isp_uart_top, "sclk_isp_uart_top", "dout_sclk_isp_uart", ENABLE_SCLK_TOP_CAM1, 2, 0, 0),
	GATE(sclk_isp_sensor0, "sclk_isp_sensor0", "dout_sclk_isp_sensor0_b", ENABLE_SCLK_TOP_CAM1, 5, 0, 0),
	GATE(sclk_isp_sensor1, "sclk_isp_sensor1", "dout_sclk_isp_sensor1_b", ENABLE_SCLK_TOP_CAM1, 6, 0, 0),
	GATE(sclk_isp_sensor2, "sclk_isp_sensor2", "dout_sclk_isp_sensor2_b", ENABLE_SCLK_TOP_CAM1, 7, 0, 0),
#endif

	/* sclk TOP_DISP */
	GATE(sclk_hdmi_spdif_top, "sclk_hdmi_spdif_top", "mout_sclk_hdmi_spdif", ENABLE_SCLK_TOP_DISP, 0, 0, 0),

	/* sclk TOP_FSYS */
	GATE(sclk_usbdrd30_top, "sclk_usbdrd30_top", "dout_usbdrd30", ENABLE_SCLK_TOP_FSYS, 0, 0, 0),
	GATE(sclk_ufsunipro_top, "sclk_ufsunipro_top", "dout_ufsunipro", ENABLE_SCLK_TOP_FSYS, 3, 0, 0),
	GATE(sclk_mmc0_top, "sclk_mmc0_top", "dout_mmc0_b", ENABLE_SCLK_TOP_FSYS, 4, 0, 0),
	GATE(sclk_mmc1_top, "sclk_mmc1_top", "dout_mmc1_b", ENABLE_SCLK_TOP_FSYS, 5, 0, 0),
	GATE(sclk_mmc2_top, "sclk_mmc2_top", "dout_mmc2_b", ENABLE_SCLK_TOP_FSYS, 6, 0, 0),

	/* sclk TOP_PERIC */
	GATE(sclk_spi0_top, "sclk_spi0_top", "dout_sclk_spi0_b", ENABLE_SCLK_TOP_PERIC, 0, 0, 0),
	GATE(sclk_spi1_top, "sclk_spi1_top", "dout_sclk_spi1_b", ENABLE_SCLK_TOP_PERIC, 1, 0, 0),
	GATE(sclk_spi2_top, "sclk_spi2_top", "dout_sclk_spi2_b", ENABLE_SCLK_TOP_PERIC, 2, 0, 0),
	GATE(sclk_uart0_top, "sclk_uart0_top", "dout_sclk_uart0", ENABLE_SCLK_TOP_PERIC, 3, 0, 0),
	GATE(sclk_uart1_top, "sclk_uart1_top", "dout_sclk_uart1", ENABLE_SCLK_TOP_PERIC, 4, 0, 0),
	GATE(sclk_uart2_top, "sclk_uart2_top", "dout_sclk_uart2", ENABLE_SCLK_TOP_PERIC, 5, 0, 0),
	GATE(sclk_pcm1_top, "sclk_pcm1_top", "dout_sclk_pcm1", ENABLE_SCLK_TOP_PERIC, 7, 0, 0),
	GATE(sclk_i2s1_top, "sclk_i2s1_top", "dout_sclk_i2s1", ENABLE_SCLK_TOP_PERIC, 8, 0, 0),
	GATE(sclk_spdif_top, "sclk_spdif_top", "dout_sclk_spdif", ENABLE_SCLK_TOP_PERIC, 9, 0, 0),
	GATE(sclk_slimbus_top, "sclk_slimbus_top", "dout_sclk_slimbus", ENABLE_SCLK_TOP_PERIC, 10, 0, 0),

	/* MIF */
	GATE(sclk_hpm_mif, "sclk_hpm_mif", "dout_sclk_hpm_mif", ENABLE_SCLK_MIF, 4, 0, 0),
	GATE(aclk_cpif_200, "aclk_cpif_200", "dout_aclk_cpif_200", ENABLE_ACLK_MIF3, 0, CLK_IGNORE_UNUSED, 0),
	GATE(aclk_disp_333, "aclk_disp_333", "dout_aclk_disp_333", ENABLE_ACLK_MIF3, 1, 0, 0),
	GATE(aclk_disp_222, "aclk_disp_222", "dout_aclk_disp_222", ENABLE_ACLK_MIF3, 2, 0, 0),
	GATE(aclk_bus1_400, "aclk_bus1_400", "dout_aclk_bus1_400", ENABLE_ACLK_MIF3, 3, 0, 0),
	GATE(aclk_bus2_400, "aclk_bus2_400", "dout_aclk_bus2_400", ENABLE_ACLK_MIF3, 4, 0, 0),

	GATE(sclk_decon_eclk_mif, "sclk_decon_eclk_mif", "dout_sclk_decon_eclk", ENABLE_SCLK_MIF, 5, 0, 0),
	GATE(sclk_decon_vclk_mif, "sclk_decon_vclk_mif", "mout_sclk_decon_vclk_d", ENABLE_SCLK_MIF, 6, 0, 0),
	GATE(sclk_dsd_mif, "sclk_dsd_mif", "dout_sclk_dsd", ENABLE_SCLK_MIF, 7, 0, 0),

#if 0
	/* CPIF */
	GATE(phyclk_lli_tx0_symbol, "phyclk_lli_tx0_symbol", "mout_phyclk_lli_tx0_symbol_user", ENABLE_SCLK_CPIF, 5, 0, 0),
	GATE(phyclk_lli_rx0_symbol, "phyclk_lli_rx0_symbol", "mout_phyclk_lli_rx0_symbol_user", ENABLE_SCLK_CPIF, 6, 0, 0),
	GATE(sclk_mphy_pll, "sclk_mphy_pll", "mout_mphy_pll", ENABLE_SCLK_CPIF, 9, 0, 0),
	GATE(sclk_ufs_mphy, "sclk_ufs_mphy", "dout_sclk_mphy", ENABLE_SCLK_CPIF, 4, 0, 0),
	GATE(sclk_lli_mphy, "sclk_lli_mphy", "mout_sclk_mphy", ENABLE_SCLK_CPIF, 3, 0, 0),
#endif

	/* DISP */
	GATE(sclk_decon_eclk, "sclk_decon_eclk", "dout_sclk_decon_eclk_disp", ENABLE_SCLK_DISP, 2, 0, 0),
	GATE(sclk_decon_vclk, "sclk_decon_vclk", "dout_sclk_decon_vclk_disp", ENABLE_SCLK_DISP, 3, 0, 0),
#if 0
	GATE(sclk_dsd, "sclk_dsd", "mout_sclk_dsd_user", ENABLE_SCLK_DISP, 5, 0, 0),
#endif
	GATE(sclk_hdmi_spdif, "sclk_hdmi_spdif", "sclk_hdmi_spdif_top", ENABLE_SCLK_DISP, 4, 0, 0),
	GATE(phyclk_mixer_pixel, "phyclk_mixer_pixel", "mout_phyclk_hdmiphy_pixel_clko_user", ENABLE_SCLK_DISP, 9, 0, 0),
	GATE(phyclk_hdmi_pixel, "phyclk_hdmi_pixel", "mout_phyclk_hdmiphy_pixel_clko_user", ENABLE_SCLK_DISP, 10, 0, 0),
	GATE(phyclk_hdmiphy_tmds_clko, "phyclk_hdmiphy_tmds_clko", "mout_phyclk_hdmiphy_tmds_clko", ENABLE_SCLK_DISP, 11, 0, 0),
	GATE(phyclk_mipidphy_rxclkesc0, "phyclk_mipidphy_rxclkesc0", "mout_phyclk_mipidphy_rxclkesc0_user", ENABLE_SCLK_DISP, 12, 0, 0),
	GATE(phyclk_mipidphy_bitclkdiv8, "phyclk_mipidphy_bitclkdiv8", "mout_phyclk_mipidphy_bitclkdiv8_user", ENABLE_SCLK_DISP, 13, 0, 0),

	/* PERIC */
	GATE(sclk_pcm1, "sclk_pcm1", "sclk_pcm1_top", ENABLE_SCLK_PERIC, 7, 0, 0),
	GATE(sclk_i2s1, "sclk_i2s1", "sclk_i2s1_top", ENABLE_SCLK_PERIC, 6, 0, 0),
	GATE(sclk_spi0, "sclk_spi0", "sclk_spi0_top", ENABLE_SCLK_PERIC, 3, 0, 0),
	GATE(sclk_spi1, "sclk_spi1", "sclk_spi1_top", ENABLE_SCLK_PERIC, 4, 0, 0),
	GATE(sclk_spi2, "sclk_spi2", "sclk_spi2_top", ENABLE_SCLK_PERIC, 5, 0, 0),
	GATE(sclk_uart0, "sclk_uart0", "sclk_uart0_top", ENABLE_SCLK_PERIC, 0, 0, 0),
	GATE(sclk_uart1, "sclk_uart1", "sclk_uart1_top", ENABLE_SCLK_PERIC, 1, 0, 0),
	GATE(sclk_uart2, "sclk_uart2", "sclk_uart2_top", ENABLE_SCLK_PERIC, 2, 0, 0),
	GATE(sclk_slimbus, "sclk_slimbus", "sclk_slimbus_top", ENABLE_SCLK_PERIC, 9, 0, 0),
	GATE(sclk_spdif, "sclk_spdif", "sclk_spdif_top", ENABLE_SCLK_PERIC, 8, 0, 0),
	GATE(ioclk_i2s1_bclk, "ioclk_i2s1_bclk", NULL, ENABLE_SCLK_PERIC, 10, 0, 0),
	GATE(ioclk_spi0_clk, "ioclk_spi0_clk", NULL, ENABLE_SCLK_PERIC, 11, 0, 0),
	GATE(ioclk_spi1_clk, "ioclk_spi1_clk", NULL, ENABLE_SCLK_PERIC, 12, 0, 0),
	GATE(ioclk_spi2_clk, "ioclk_spi2_clk", NULL, ENABLE_SCLK_PERIC, 13, 0, 0),
	GATE(ioclk_slimbus_clk, "ioclk_slimbus_clk", NULL, ENABLE_SCLK_PERIC, 14, 0, 0),

#if 0
	/* MSCL */
	GATE(sclk_jpeg, "sclk_jpeg", "mout_sclk_jpeg_user", ENABLE_SCLK_MSCL, 0, 0, 0),

	/* CAM0 */
	GATE(phyclk_rxbyteclkhs0_s4, "phyclk_rxbyteclkhs0_s4", "mout_phyclk_rxbyteclkhs0_s4", ENABLE_SCLK_CAM0, 8, 0, 0),
	GATE(phyclk_rxbyteclkhs0_s2a, "phyclk_rxbyteclkhs0_s2a", "mout_phyclk_rxbyteclkhs0_s2a", ENABLE_SCLK_CAM0, 7, 0, 0),

	/* CAM1 */
	GATE(sclk_isp_spi0, "sclk_isp_spi0", "mout_sclk_isp_spi0_user", ENABLE_SCLK_CAM1, 4, 0, 0),
	GATE(sclk_isp_spi1, "sclk_isp_spi1", "mout_sclk_isp_spi1_user", ENABLE_SCLK_CAM1, 5, 0, 0),
	GATE(sclk_isp_uart, "sclk_isp_uart", "mout_sclk_isp_uart_user", ENABLE_SCLK_CAM1, 6, 0, 0),
	GATE(sclk_isp_mtcadc, "sclk_isp_mtcadc", NULL, ENABLE_SCLK_CAM1, 7, 0, 0),
	GATE(phyclk_rxbyteclkhs0_s2b, "phyclk_rxbyteclkhs0_s2b", "mout_phyclk_rxbyteclkhs0_s2b", ENABLE_SCLK_CAM1, 11, 0, 0),
#endif

	/* FSYS */
	GATE(sclk_usbdrd30, "sclk_usbdrd30", "mout_sclk_usbdrd30_user", ENABLE_SCLK_FSYS, 0, 0, 0),
	GATE(sclk_mmc0, "sclk_mmc0", "mout_sclk_mmc0_user", ENABLE_SCLK_FSYS, 2, 0, 0),
	GATE(sclk_mmc1, "sclk_mmc1", "mout_sclk_mmc1_user", ENABLE_SCLK_FSYS, 3, 0, 0),
	GATE(sclk_mmc2, "sclk_mmc2", "mout_sclk_mmc2_user", ENABLE_SCLK_FSYS, 4, 0, 0),
	GATE(sclk_ufsunipro, "sclk_ufsunipro", "mout_sclk_ufsunipro_user", ENABLE_SCLK_FSYS, 5, 0, 0),
	GATE(sclk_mphy, "sclk_mphy", "mout_sclk_mphy", ENABLE_SCLK_FSYS, 6, 0, 0),
	GATE(phyclk_usbdrd30_udrd30_phyclock, "phyclk_usbdrd30_udrd30_phyclock", "mout_phyclk_usbdrd30_udrd30_phyclock", ENABLE_SCLK_FSYS, 7, 0, 0),
	GATE(phyclk_usbdrd30_udrd30_pipe_pclk, "phyclk_usbdrd30_udrd30_pipe_pclk", "mout_phyclk_usbdrd30_udrd30_pipe_pclk", ENABLE_SCLK_FSYS, 8, 0, 0),
	GATE(phyclk_usbhost20_phy_freeclk, "phyclk_usbhost20_phy_freeclk", "mout_phyclk_usbhost20_phy_freeclk", ENABLE_SCLK_FSYS, 9, 0, 0),
	GATE(phyclk_usbhost20_phy_phyclock, "phyclk_usbhost20_phy_phyclock", "mout_phyclk_usbhost20_phy_phyclock", ENABLE_SCLK_FSYS, 10, 0, 0),
	GATE(phyclk_usbhost20_phy_clk48mohci, "phyclk_usbhost20_phy_clk48mohci", "mout_phyclk_usbhost20_phy_clk48mohci", ENABLE_SCLK_FSYS, 11, 0, 0),
	GATE(phyclk_usbhost20_phy_hsic1, "phyclk_usbhost20_phy_hsic1", "mout_phyclk_usbhost20_phy_hsic1", ENABLE_SCLK_FSYS, 12, 0, 0),
	GATE(phyclk_ufs_tx0_symbol, "phyclk_ufs_tx0_symbol", "mout_phyclk_ufs_tx0_symbol_user", ENABLE_SCLK_FSYS, 13, 0, 0),
	GATE(phyclk_ufs_tx1_symbol, "phyclk_ufs_tx1_symbol", "mout_phyclk_ufs_tx1_symbol_user", ENABLE_SCLK_FSYS, 14, 0, 0),
	GATE(phyclk_ufs_rx0_symbol, "phyclk_ufs_rx0_symbol", "mout_phyclk_ufs_rx0_symbol_user", ENABLE_SCLK_FSYS, 15, 0, 0),
	GATE(phyclk_ufs_rx1_symbol, "phyclk_ufs_rx1_symbol", "mout_phyclk_ufs_rx1_symbol_user", ENABLE_SCLK_FSYS, 16, 0, 0),

	/* BUS1 */
	GATE(aclk_bus1nd_400, "aclk_bus1nd_400", "mout_aclk_bus1_400_user", ENABLE_ACLK_BUS1, 0, 0, 0),
	GATE(aclk_bus1sw2nd_400, "aclk_bus1sw2nd_400", "mout_aclk_bus1_400_user", ENABLE_ACLK_BUS1, 1, 0, 0),
	GATE(aclk_bus1np_133, "aclk_bus1np_133", "dout_pclk_bus1_133", ENABLE_ACLK_BUS1, 2, 0, 0),
	GATE(aclk_bus1sw2np_133, "aclk_bus1sw2np_133", "dout_pclk_bus1_133", ENABLE_ACLK_BUS1, 3, 0, 0),
	GATE(aclk_ahb2apb_bus1p, "aclk_ahb2apb_bus1p", "dout_pclk_bus1_133", ENABLE_ACLK_BUS1, 4, 0, 0),
	GATE(pclk_bus1srvnd_133, "pclk_bus1srvnd_133", "dout_pclk_bus1_133", ENABLE_PCLK_BUS1, 2, 0, 0),
	GATE(pclk_sysreg_bus1, "pclk_sysreg_bus1", "dout_pclk_bus1_133", ENABLE_PCLK_BUS1, 1, 0, 0),
	GATE(pclk_pmu_bus1, "pclk_pmu_bus1", "dout_pclk_bus1_133", ENABLE_PCLK_BUS1, 0, 0, 0),

	/* BUS2 */
	GATE(aclk_bus2rtnd_400, "aclk_bus2rtnd_400", "mout_aclk_bus2_400_user", ENABLE_ACLK_BUS2, 0, 0, 0),
	GATE(aclk_bus2bend_400, "aclk_bus2bend_400", "mout_aclk_bus2_400_user", ENABLE_ACLK_BUS2, 1, 0, 0),
	GATE(aclk_bus2np_133, "aclk_bus2np_133", "dout_pclk_bus2_133", ENABLE_ACLK_BUS2, 2, 0, 0),
	GATE(aclk_ahb2apb_bus2p, "aclk_ahb2apb_bus2p", "dout_pclk_bus2_133", ENABLE_ACLK_BUS2, 3, 0, 0),
	GATE(pclk_bus2srvnd_133, "pclk_bus2srvnd_133", "dout_pclk_bus2_133", ENABLE_PCLK_BUS2, 2, 0, 0),
	GATE(pclk_sysreg_bus2, "pclk_sysreg_bus2", "dout_pclk_bus2_133", ENABLE_PCLK_BUS2, 0, 0, 0),
	GATE(pclk_pmu_bus2, "pclk_pmu_bus2", "dout_pclk_bus2_133", ENABLE_PCLK_BUS2, 1, 0, 0),

#if 0
	/* G2D */
	GATE(aclk_g2d, "aclk_g2d", "mout_aclk_g2d_400_user", ENABLE_ACLK_G2D, 0, 0, 0),
	GATE(aclk_g2dnd_400, "aclk_g2dnd_400", "mout_aclk_g2d_400_user", ENABLE_ACLK_G2D, 2, 0, 0),
	GATE(aclk_xiu_g2dx, "aclk_xiu_g2dx", "mout_aclk_g2d_400_user", ENABLE_ACLK_G2D, 4, 0, 0),
	GATE(aclk_asyncaxi_sysx, "aclk_asyncaxi_sysx", "mout_aclk_g2d_400_user", ENABLE_ACLK_G2D, 7, 0, 0),
	GATE(aclk_axius_g2dx, "aclk_axius_g2dx", "mout_aclk_g2d_400_user", ENABLE_ACLK_G2D, 8, 0, 0),
	GATE(aclk_alb_g2d, "aclk_alb_g2d", "mout_aclk_g2d_400_user", ENABLE_ACLK_G2D, 9, 0, 0),
	GATE(aclk_qe_g2d, "aclk_qe_g2d", "mout_aclk_g2d_400_user", ENABLE_ACLK_G2D, 10, 0, 0),
	GATE(aclk_smmu_g2d, "aclk_smmu_g2d", "mout_aclk_g2d_400_user", ENABLE_ACLK_G2D_SECURE_SMMU_G2D, 0, 0, 0),
	GATE(aclk_ppmu_g2dx, "aclk_ppmu_g2dx", "mout_aclk_g2d_400_user", ENABLE_ACLK_G2D, 13, 0, 0),

	GATE(aclk_mdma1, "aclk_mdma1", "mout_aclk_g2d_266_user", ENABLE_ACLK_G2D, 1, 0, 0),
	GATE(aclk_qe_mdma1, "aclk_qe_mdma1", "mout_aclk_g2d_266_user", ENABLE_ACLK_G2D, 11, 0, 0),
	GATE(aclk_smmu_mdma1, "aclk_smmu_mdma1", "mout_aclk_g2d_266_user", ENABLE_ACLK_G2D, 12, 0, 0),

	GATE(aclk_g2dnp_133, "aclk_g2dnp_133", "dout_pclk_g2d", ENABLE_ACLK_G2D, 3, 0, 0),
	GATE(aclk_ahb2apb_g2d0p, "aclk_ahb2apb_g2d0p", "dout_pclk_g2d", ENABLE_ACLK_G2D, 5, 0, 0),
	GATE(aclk_ahb2apb_g2d1p, "aclk_ahb2apb_g2d1p", "dout_pclk_g2d", ENABLE_ACLK_G2D, 6, 0, 0),
	GATE(pclk_g2d, "pclk_g2d", "dout_pclk_g2d", ENABLE_PCLK_G2D, 0, 0, 0),
	GATE(pclk_sysreg_g2d, "pclk_sysreg_g2d", "dout_pclk_g2d", ENABLE_PCLK_G2D, 1, 0, 0),
	GATE(pclk_pmu_g2d, "pclk_pmu_g2d", "dout_pclk_g2d", ENABLE_PCLK_G2D, 2, 0, 0),
	GATE(pclk_asyncaxi_sysx, "pclk_asyncaxi_sysx", "dout_pclk_g2d", ENABLE_PCLK_G2D, 3, 0, 0),
	GATE(pclk_alb_g2d, "pclk_alb_g2d", "dout_pclk_g2d", ENABLE_PCLK_G2D, 4, 0, 0),
	GATE(pclk_qe_g2d, "pclk_qe_g2d", "dout_pclk_g2d", ENABLE_PCLK_G2D, 5, 0, 0),
	GATE(pclk_qe_mdma1, "pclk_qe_mdma1", "dout_pclk_g2d", ENABLE_PCLK_G2D, 6, 0, 0),
	GATE(pclk_smmu_g2d, "pclk_smmu_g2d", "dout_pclk_g2d", ENABLE_PCLK_G2D_SECURE_SMMU_G2D, 0, 0, 0),
	GATE(pclk_smmu_mdma1, "pclk_smmu_mdma1", "dout_pclk_g2d", ENABLE_PCLK_G2D, 7, 0, 0),
	GATE(pclk_ppmu_g2d, "pclk_ppmu_g2d", "dout_pclk_g2d", ENABLE_PCLK_G2D, 8, 0, 0),

	/* GSCL */
	GATE(aclk_gscl0, "aclk_gscl0", "mout_aclk_gscl_333_user", ENABLE_ACLK_GSCL, 0, 0, 0),
	GATE(aclk_gscl1, "aclk_gscl1", "mout_aclk_gscl_333_user", ENABLE_ACLK_GSCL, 1, 0, 0),
	GATE(aclk_gscl2, "aclk_gscl2", "mout_aclk_gscl_333_user", ENABLE_ACLK_GSCL, 2, 0, 0),
	GATE(aclk_gsd, "aclk_gsd", "mout_aclk_gscl_333_user", ENABLE_ACLK_GSCL, 3, 0, 0),
	GATE(aclk_gsclbend_333, "aclk_gsclbend_333", "mout_aclk_gscl_333_user", ENABLE_ACLK_GSCL, 4, 0, 0),
	GATE(aclk_gsclrtnd_333, "aclk_gsclrtnd_333", "mout_aclk_gscl_333_user", ENABLE_ACLK_GSCL, 5, 0, 0),
	GATE(aclk_xiu_gsclx, "clk_xiu_gsclx", "mout_aclk_gscl_333_user", ENABLE_ACLK_GSCL, 7, 0, 0),
	GATE(aclk_qe_gscl0, "clk_qe_gscl0", "mout_aclk_gscl_333_user", ENABLE_ACLK_GSCL, 9, 0, 0),
	GATE(aclk_qe_gscl1, "clk_qe_gscl1", "mout_aclk_gscl_333_user", ENABLE_ACLK_GSCL, 10, 0, 0),
	GATE(aclk_qe_gscl2, "clk_qe_gscl2", "mout_aclk_gscl_333_user", ENABLE_ACLK_GSCL, 11, 0, 0),
	GATE(aclk_smmu_gscl0, "aclk_smmu_gscl0", "mout_aclk_gscl_333_user", ENABLE_ACLK_GSCL_SECURE_SMMU_GSCL0, 0, 0, 0),
	GATE(aclk_smmu_gscl1, "aclk_smmu_gscl1", "mout_aclk_gscl_333_user", ENABLE_ACLK_GSCL_SECURE_SMMU_GSCL1, 0, 0, 0),
	GATE(aclk_smmu_gscl2, "aclk_smmu_gscl2", "mout_aclk_gscl_333_user", ENABLE_ACLK_GSCL_SECURE_SMMU_GSCL2, 0, 0, 0),
	GATE(aclk_ppmu_gscl0, "aclk_ppmu_gscl0", "mout_aclk_gscl_333_user", ENABLE_ACLK_GSCL, 15, 0, 0),
	GATE(aclk_ppmu_gscl1, "aclk_ppmu_gscl1", "mout_aclk_gscl_333_user", ENABLE_ACLK_GSCL, 16, 0, 0),
	GATE(aclk_ppmu_gscl2, "aclk_ppmu_gscl2", "mout_aclk_gscl_333_user", ENABLE_ACLK_GSCL, 17, 0, 0),

	GATE(aclk_ahb2apb_gsclp, "aclk_ahb2apb_gsclp", "mout_aclk_gscl_111_user", ENABLE_ACLK_GSCL, 8, 0, 0),
	GATE(pclk_gscl0, "pclk_gscl0", "mout_aclk_gscl_111_user", ENABLE_PCLK_GSCL, 0, 0, 0),
	GATE(pclk_gscl1, "pclk_gscl1", "mout_aclk_gscl_111_user", ENABLE_PCLK_GSCL, 1, 0, 0),
	GATE(pclk_gscl2, "pclk_gscl2", "mout_aclk_gscl_111_user", ENABLE_PCLK_GSCL, 2, 0, 0),
	GATE(pclk_sysreg_gscl, "pclk_sysreg_gscl", "mout_aclk_gscl_111_user", ENABLE_PCLK_GSCL, 3, 0, 0),
	GATE(pclk_pmu_gscl, "pclk_pmu_gscl", "mout_aclk_gscl_111_user", ENABLE_PCLK_GSCL, 4, 0, 0),
	GATE(pclk_qe_gscl0, "pclk_pk_qe_gscl0", "mout_aclk_gscl_111_user", ENABLE_PCLK_GSCL, 5, 0, 0),
	GATE(pclk_qe_gscl1, "pclk_pk_qe_gscl1", "mout_aclk_gscl_111_user", ENABLE_PCLK_GSCL, 6, 0, 0),
	GATE(pclk_qe_gscl2, "pclk_pk_qe_gscl2", "mout_aclk_gscl_111_user", ENABLE_PCLK_GSCL, 7, 0, 0),
	GATE(pclk_smmu_gscl0, "pclk_smmu_gscl0", "mout_aclk_gscl_111_user", ENABLE_PCLK_GSCL_SECURE_SMMU_GSCL0, 0, 0, 0),
	GATE(pclk_smmu_gscl1, "pclk_smmu_gscl1", "mout_aclk_gscl_111_user", ENABLE_PCLK_GSCL_SECURE_SMMU_GSCL1, 0, 0, 0),
	GATE(pclk_smmu_gscl2, "pclk_smmu_gscl2", "mout_aclk_gscl_111_user", ENABLE_PCLK_GSCL_SECURE_SMMU_GSCL2, 0, 0, 0),
	GATE(pclk_ppmu_gscl0, "pclk_ppmu_gscl0", "mout_aclk_gscl_111_user", ENABLE_PCLK_GSCL, 11, 0, 0),
	GATE(pclk_ppmu_gscl1, "pclk_ppmu_gscl1", "mout_aclk_gscl_111_user", ENABLE_PCLK_GSCL, 12, 0, 0),
	GATE(pclk_ppmu_gscl2, "pclk_ppmu_gscl2", "mout_aclk_gscl_111_user", ENABLE_PCLK_GSCL, 13, 0, 0),

	/*MSCL*/
	GATE(aclk_m2mscaler0, "aclk_m2mscaler0", "mout_aclk_mscl_400_user", ENABLE_ACLK_MSCL, 0, 0, 0),
	GATE(aclk_m2mscaler1, "aclk_m2mscaler1", "mout_aclk_mscl_400_user", ENABLE_ACLK_MSCL, 1, 0, 0),
	GATE(aclk_jpeg, "aclk_jpeg", "mout_aclk_mscl_400_user", ENABLE_ACLK_MSCL, 2, 0, 0),
	GATE(aclk_msclnd_400, "aclk_msclnd_400", "mout_aclk_mscl_400_user", ENABLE_ACLK_MSCL, 3, 0, 0),
	GATE(aclk_xiu_msclx, "aclk_xiu_msclx", "mout_aclk_mscl_400_user", ENABLE_ACLK_MSCL, 5, 0, 0),
	GATE(aclk_qe_m2mscaler0, "aclk_qe_m2mscaler0", "mout_aclk_mscl_400_user", ENABLE_ACLK_MSCL, 7, 0, 0),
	GATE(aclk_qe_m2mscaler1, "aclk_qe_m2mscaler1", "mout_aclk_mscl_400_user", ENABLE_ACLK_MSCL, 8, 0, 0),
	GATE(aclk_qe_jpeg, "aclk_qe_jpeg", "mout_aclk_mscl_400_user", ENABLE_ACLK_MSCL, 9, 0, 0),
	GATE(aclk_smmu_m2mscaler0, "aclk_smmu_m2mscaler0", "mout_aclk_mscl_400_user", ENABLE_ACLK_MSCL_SECURE_SMMU_M2MSCALER0, 0, 0, 0),
	GATE(aclk_smmu_m2mscaler1, "aclk_smmu_m2mscaler1", "mout_aclk_mscl_400_user", ENABLE_ACLK_MSCL_SECURE_SMMU_M2MSCALER1, 0, 0, 0),
	GATE(aclk_smmu_jpeg, "aclk_smmu_jpeg", "mout_aclk_mscl_400_user", ENABLE_ACLK_MSCL_SECURE_SMMU_JPEG, 0, 0, 0),
	GATE(aclk_ppmu_m2mscaler0, "aclk_ppmu_m2mscaler0", "mout_aclk_mscl_400_user", ENABLE_ACLK_MSCL, 10, 0, 0),
	GATE(aclk_ppmu_m2mscaler1, "aclk_ppmu_m2mscaler1", "mout_aclk_mscl_400_user", ENABLE_ACLK_MSCL, 11, 0, 0),

	GATE(aclk_msclnp_100, "aclk_msclnp_100", "dout_pclk_mscl", ENABLE_ACLK_MSCL, 4, 0, 0),
	GATE(aclk_ahb2apb_mscl0p, "aclk_ahb2apb_mscl0p", "dout_pclk_mscl", ENABLE_ACLK_MSCL, 0, 0, 0),
	GATE(pclk_m2mscaler0, "pclk_m2mscaler0", "dout_pclk_mscl", ENABLE_PCLK_MSCL, 0, 0, 0),
	GATE(pclk_m2mscaler1, "pclk_m2mscaler1", "dout_pclk_mscl", ENABLE_PCLK_MSCL, 1, 0, 0),
	GATE(pclk_jpeg, "pclk_jpeg", "dout_pclk_mscl", ENABLE_PCLK_MSCL, 2, 0, 0),
	GATE(pclk_sysreg_mscl, "pclk_sysreg_mscl", "dout_pclk_mscl", ENABLE_PCLK_MSCL, 3, 0, 0),
	GATE(pclk_pmu_mscl, "pclk_pmu_mscl", "dout_pclk_mscl", ENABLE_PCLK_MSCL, 4, 0, 0),
	GATE(pclk_qe_m2mscaler0, "pclk_qe_m2mscaler0", "dout_pclk_mscl", ENABLE_PCLK_MSCL, 5, 0, 0),
	GATE(pclk_qe_m2mscaler1, "pclk_qe_m2mscaler1", "dout_pclk_mscl", ENABLE_PCLK_MSCL, 6, 0, 0),
	GATE(pclk_qe_jpeg, "pclk_qe_jpeg", "dout_pclk_mscl", ENABLE_PCLK_MSCL, 7, 0, 0),
	GATE(pclk_smmu_m2mscaler0, "pclk_smmu_m2mscaler0", "dout_pclk_mscl", ENABLE_PCLK_MSCL_SECURE_SMMU_M2MSCALER0, 0, 0, 0),
	GATE(pclk_smmu_m2mscaler1, "pclk_smmu_m2mscaler1", "dout_pclk_mscl", ENABLE_PCLK_MSCL_SECURE_SMMU_M2MSCALER1, 0, 0, 0),
	GATE(pclk_smmu_jpeg, "pclk_smmu_jpeg", "dout_pclk_mscl", ENABLE_PCLK_MSCL_SECURE_SMMU_JPEG, 0, 0, 0),
	GATE(pclk_ppmu_m2mscaler0, "pclk_ppmu_m2mscaler0", "dout_pclk_mscl", ENABLE_PCLK_MSCL, 8, 0, 0),
	GATE(pclk_ppmu_m2mscaler1, "pclk_ppmu_m2mscaler1", "dout_pclk_mscl", ENABLE_PCLK_MSCL, 9, 0, 0),
	GATE(pclk_ppmu_jpeg, "pclk_ppmu_jpeg", "dout_pclk_mscl", ENABLE_PCLK_MSCL, 10, 0, 0),
#endif

	/*FSYS*/
	GATE(aclk_pdma, "aclk_pdma", "mout_aclk_fsys_200_user", ENABLE_ACLK_FSYS0, 0, 0, 0),
	GATE(aclk_usbdrd30, "aclk_usbdrd30", "mout_aclk_fsys_200_user", ENABLE_ACLK_FSYS0, 1, 0, 0),
	GATE(aclk_usbhost20, "aclk_usbhost20", "mout_aclk_fsys_200_user", ENABLE_ACLK_FSYS0, 3, 0, 0),
	GATE(aclk_sromc, "aclk_sromc", "mout_aclk_fsys_200_user", ENABLE_ACLK_FSYS0, 4, 0, 0),
	GATE(aclk_ufs, "aclk_ufs", "mout_aclk_fsys_200_user", ENABLE_ACLK_FSYS0, 5, 0, 0),
	GATE(aclk_mmc0, "aclk_mmc0", "mout_aclk_fsys_200_user", ENABLE_ACLK_FSYS0, 6, 0, 0),
	GATE(aclk_mmc1, "aclk_mmc1", "mout_aclk_fsys_200_user", ENABLE_ACLK_FSYS0, 7, 0, 0),
	GATE(aclk_mmc2, "aclk_mmc2", "mout_aclk_fsys_200_user", ENABLE_ACLK_FSYS0, 8, 0, 0),
	GATE(aclk_tsi, "aclk_tsi", "mout_aclk_fsys_200_user", ENABLE_ACLK_FSYS0, 10, 0, 0),
	GATE(aclk_fsysnp_200, "aclk_fsysnp_200", "mout_aclk_fsys_200_user", ENABLE_ACLK_FSYS1, 1, 0, 0),
	GATE(aclk_fsysnd_200, "aclk_fsysnd_200", "mout_aclk_fsys_200_user", ENABLE_ACLK_FSYS1, 0, 0, 0),
	GATE(aclk_xiu_fsyssx, "aclk_xiu_fsyssx", "mout_aclk_fsys_200_user", ENABLE_ACLK_FSYS1, 2, 0, 0),
	GATE(aclk_xiu_fsysx, "aclk_xiu_fsysx", "mout_aclk_fsys_200_user", ENABLE_ACLK_FSYS1, 3, 0, 0),
	GATE(aclk_ahb_fsysh, "aclk_ahb_fsysh", "mout_aclk_fsys_200_user", ENABLE_ACLK_FSYS1, 4, 0, 0),
	GATE(aclk_ahb_usbhs, "aclk_ahb_usbhs", "mout_aclk_fsys_200_user", ENABLE_ACLK_FSYS1, 5, 0, 0),
	GATE(aclk_ahb_usblinkh, "aclk_ahb_usblinkh", "mout_aclk_fsys_200_user", ENABLE_ACLK_FSYS1, 6, 0, 0),
	GATE(aclk_ahb2axi_usbhs, "aclk_ahb2axi_usbhs", "mout_aclk_fsys_200_user", ENABLE_ACLK_FSYS1, 7, 0, 0),
	GATE(aclk_ahb2apb_fsysp, "aclk_ahb2apb_fsysp", "mout_aclk_fsys_200_user", ENABLE_ACLK_FSYS1, 8, 0, 0),
	GATE(aclk_axius_fsyssx, "aclk_axius_fsyssx", "mout_aclk_fsys_200_user", ENABLE_ACLK_FSYS1, 9, 0, 0),
	GATE(aclk_axius_usbhs, "aclk_axius_usbhs", "mout_aclk_fsys_200_user", ENABLE_ACLK_FSYS1, 10, 0, 0),
	GATE(aclk_axius_pdma, "aclk_axius_pdma", "mout_aclk_fsys_200_user", ENABLE_ACLK_FSYS1, 11, 0, 0),
	GATE(aclk_qe_usbdrd30, "aclk_qe_usbdrd30", "mout_aclk_fsys_200_user", ENABLE_ACLK_FSYS1, 12, 0, 0),
	GATE(aclk_qe_ufs, "aclk_qe_ufs", "mout_aclk_fsys_200_user", ENABLE_ACLK_FSYS1, 14, 0, 0),
	GATE(aclk_smmu_pdma, "aclk_smmu_pdma", "mout_aclk_fsys_200_user", ENABLE_ACLK_FSYS1, 17, 0, 0),
	GATE(aclk_smmu_mmc0, "aclk_smmu_mmc0", "mout_aclk_fsys_200_user", ENABLE_ACLK_FSYS1, 18, 0, 0),
	GATE(aclk_smmu_mmc1, "aclk_smmu_mmc1", "mout_aclk_fsys_200_user", ENABLE_ACLK_FSYS1, 19, 0, 0),
	GATE(aclk_smmu_mmc2, "aclk_smmu_mmc2", "mout_aclk_fsys_200_user", ENABLE_ACLK_FSYS1, 20, 0, 0),
	GATE(aclk_ppmu_fsys, "aclk_ppmu_fsys", "mout_aclk_fsys_200_user", ENABLE_ACLK_FSYS1, 21, 0, 0),
	GATE(pclk_gpio_fsys, "pclk_gpio_fsys", "mout_aclk_fsys_200_user", ENABLE_PCLK_FSYS, 2, 0, 0),
	GATE(pclk_pmu_fsys, "pclk_pmu_fsys", "mout_aclk_fsys_200_user", ENABLE_PCLK_FSYS, 1, 0, 0),
	GATE(pclk_sysreg_fsys, "pclk_sysreg_fsys", "mout_aclk_fsys_200_user", ENABLE_PCLK_FSYS, 0, 0, 0),
	GATE(pclk_qe_usbdrd30, "pclk_qe_usbdrd30", "mout_aclk_fsys_200_user", ENABLE_PCLK_FSYS, 3, 0, 0),
	GATE(pclk_qe_ufs, "pclk_qe_ufs", "mout_aclk_fsys_200_user", ENABLE_PCLK_FSYS, 5, 0, 0),
	GATE(pclk_smmu_pdma, "pclk_smmu_pdma", "mout_aclk_fsys_200_user", ENABLE_PCLK_FSYS, 8, 0, 0),
	GATE(pclk_smmu_mmc0, "pclk_smmu_mmc0", "mout_aclk_fsys_200_user", ENABLE_PCLK_FSYS, 9, 0, 0),
	GATE(pclk_smmu_mmc1, "pclk_smmu_mmc1", "mout_aclk_fsys_200_user", ENABLE_PCLK_FSYS, 10, 0, 0),
	GATE(pclk_smmu_mmc2, "pclk_smmu_mmc2", "mout_aclk_fsys_200_user", ENABLE_PCLK_FSYS, 11, 0, 0),
	GATE(pclk_ppmu_fsys, "pclk_ppmu_fsys", "mout_aclk_fsys_200_user", ENABLE_PCLK_FSYS, 12, 0, 0),

	/*DISP*/
	GATE(aclk_decon, "aclk_decon", "mout_aclk_disp_333_user", ENABLE_ACLK_DISP0, 0, 0, 0),
	GATE(aclk_disp0nd_333, "aclk_disp0nd_333", "mout_aclk_disp_333_user", ENABLE_ACLK_DISP1, 0, 0, 0),
	GATE(aclk_disp1nd_333, "aclk_disp1nd_333", "mout_aclk_disp_333_user", ENABLE_ACLK_DISP1, 1, 0, 0),
	GATE(aclk_xiu_disp1x, "aclk_xiu_disp1x", "mout_aclk_disp_333_user", ENABLE_ACLK_DISP1, 3, 0, 0),
	GATE(aclk_xiu_decon0x, "aclk_xiu_decon0x", "mout_aclk_disp_333_user", ENABLE_ACLK_DISP1, 4, 0, 0),
	GATE(aclk_xiu_decon1x, "aclk_xiu_decon1x", "mout_aclk_disp_333_user", ENABLE_ACLK_DISP1, 5, 0, 0),
	/*GATE(aclk_asyncaximm_tvx, "aclk_asyncaximm_tvx", "mout_aclk_disp_333_user", ENABLE_ACLK_DISP1, 5, 0, 0),*/
	GATE(aclk_axius_disp1x, "aclk_axius_disp1x", "mout_aclk_disp_333_user", ENABLE_ACLK_DISP1, 15, 0, 0),
	GATE(aclk_qe_deconm0, "aclk_qe_deconm0", "mout_aclk_disp_333_user", ENABLE_ACLK_DISP1, 19, 0, 0),
	GATE(aclk_qe_deconm1, "aclk_qe_deconm1", "mout_aclk_disp_333_user", ENABLE_ACLK_DISP1, 20, 0, 0),
	GATE(aclk_qe_deconm2, "aclk_qe_deconm2", "mout_aclk_disp_333_user", ENABLE_ACLK_DISP1, 21, 0, 0),
	GATE(aclk_qe_deconm3, "aclk_qe_deconm3", "mout_aclk_disp_333_user", ENABLE_ACLK_DISP1, 22, 0, 0),
	GATE(aclk_smmu_decon0x, "aclk_smmu_decon0x", "mout_aclk_disp_333_user", ENABLE_ACLK_DISP1, 26, 0, 0),
	GATE(aclk_smmu_decon1x, "aclk_smmu_decon1x", "mout_aclk_disp_333_user", ENABLE_ACLK_DISP1, 27, 0, 0),
	GATE(aclk_ppmu_decon0x, "aclk_ppmu_decon0x", "mout_aclk_disp_333_user", ENABLE_ACLK_DISP1, 29, 0, 0),
	GATE(aclk_ppmu_decon1x, "aclk_ppmu_decon1x", "mout_aclk_disp_333_user", ENABLE_ACLK_DISP1, 30, 0, 0),

	GATE(aclk_dispnp_100, "aclk_dispnp_100", "dout_pclk_disp", ENABLE_ACLK_DISP1, 2, 0, 0),
	GATE(aclk_ahb_disph, "aclk_ahb_disph", "dout_pclk_disp", ENABLE_ACLK_DISP1, 7, 0, 0),
	GATE(aclk_ahb2apb_dispsfr0p, "aclk_ahb2apb_dispsfr0p", "dout_pclk_disp", ENABLE_ACLK_DISP1, 16, 0, 0),
	GATE(aclk_ahb2apb_dispsfr1p, "aclk_ahb2apb_dispsfr1p", "dout_pclk_disp", ENABLE_ACLK_DISP1, 17, 0, 0),
	GATE(aclk_ahb2apb_dispsfr2p, "aclk_ahb2apb_dispsfr2p", "dout_pclk_disp", ENABLE_ACLK_DISP1, 18, 0, 0),
	GATE(pclk_decon, "pclk_decon", "dout_pclk_disp", ENABLE_PCLK_DISP, 0, 0, 0),
	GATE(pclk_tv_mixer, "pclk_tv_mixer", "dout_pclk_disp", ENABLE_PCLK_DISP, 1, 0, 0),
	GATE(pclk_dsim0, "pclk_dsim0", "dout_pclk_disp", ENABLE_PCLK_DISP, 2, 0, 0),
	GATE(pclk_mdnie, "pclk_mdnie", "dout_pclk_disp", ENABLE_PCLK_DISP, 4, 0, 0),
	GATE(pclk_mic, "pclk_mic", "dout_pclk_disp", ENABLE_PCLK_DISP, 5, 0, 0),
	GATE(pclk_hdmi, "pclk_hdmi", "dout_pclk_disp", ENABLE_PCLK_DISP, 6, 0, 0),
	GATE(pclk_hdmiphy, "pclk_hdmiphy", "dout_pclk_disp", ENABLE_PCLK_DISP, 7, 0, 0),
	GATE(pclk_sysreg_disp, "pclk_sysreg_disp", "dout_pclk_disp", ENABLE_PCLK_DISP, 8, 0, 0),
	GATE(pclk_pmu_disp, "pclk_pmu_disp", "dout_pclk_disp", ENABLE_PCLK_DISP, 9, 0, 0),
	GATE(pclk_asyncaxi_tvx, "pclk_asyncaxi_tvx", "dout_pclk_disp", ENABLE_PCLK_DISP, 10, 0, 0),
	GATE(pclk_qe_deconm0, "pclk_qe_deconm0", "dout_pclk_disp", ENABLE_PCLK_DISP, 11, 0, 0),
	GATE(pclk_qe_deconm1, "pclk_qe_deconm1", "dout_pclk_disp", ENABLE_PCLK_DISP, 12, 0, 0),
	GATE(pclk_qe_deconm2, "pclk_qe_deconm2", "dout_pclk_disp", ENABLE_PCLK_DISP, 13, 0, 0),
	GATE(pclk_qe_deconm3, "pclk_qe_deconm3", "dout_pclk_disp", ENABLE_PCLK_DISP, 14, 0, 0),
	GATE(pclk_qe_deconm4, "pclk_qe_deconm4", "dout_pclk_disp", ENABLE_PCLK_DISP, 15, 0, 0),
	GATE(pclk_qe_mixerm0, "pclk_qe_mixerm0", "dout_pclk_disp", ENABLE_PCLK_DISP, 16, 0, 0),
	GATE(pclk_qe_mixerm1, "pclk_qe_mixerm1", "dout_pclk_disp", ENABLE_PCLK_DISP, 17, 0, 0),
	GATE(pclk_smmu_decon0x, "pclk_smmu_decon0x", "dout_pclk_disp", ENABLE_PCLK_DISP, 18, 0, 0),
	GATE(pclk_smmu_decon1x, "pclk_smmu_decon1x", "dout_pclk_disp", ENABLE_PCLK_DISP, 19, 0, 0),
	GATE(pclk_smmu_tvx, "pclk_smmu_tvx", "dout_pclk_disp", ENABLE_PCLK_DISP, 20, 0, 0),
	GATE(pclk_ppmu_decon0x, "pclk_ppmu_decon0x", "dout_pclk_disp", ENABLE_PCLK_DISP, 21, 0, 0),
	GATE(pclk_ppmu_decon1x, "pclk_ppmu_decon1x", "dout_pclk_disp", ENABLE_PCLK_DISP, 22, 0, 0),
	GATE(pclk_ppmu_tvx, "pclk_ppmu_tvx", "dout_pclk_disp", ENABLE_PCLK_DISP, 23, 0, 0),

	GATE(aclk_hdmi, "aclk_hdmi", "mout_aclk_disp_222_user", ENABLE_ACLK_DISP0, 1, 0, 0),
	GATE(aclk_tv_mixer, "aclk_tv_mixer", "mout_aclk_disp_222_user", ENABLE_ACLK_DISP0, 2, 0, 0),
	GATE(aclk_xiu_tvx, "aclk_xiu_tvx", "mout_aclk_disp_222_user", ENABLE_ACLK_DISP1, 6, 0, 0),
	GATE(aclk_asyncaxis_tvx, "aclk_asyncaxis_tvx", "mout_aclk_disp_222_user", ENABLE_ACLK_DISP1, 9, 0, 0),
	GATE(aclk_qe_mixerm0, "aclk_qe_mixerm0", "mout_aclk_disp_222_user", ENABLE_ACLK_DISP1, 24, 0, 0),
	GATE(aclk_qe_mixerm1, "aclk_qe_mixerm1", "mout_aclk_disp_222_user", ENABLE_ACLK_DISP1, 25, 0, 0),
	GATE(aclk_smmu_tvx, "aclk_smmu_tvx", "mout_aclk_disp_222_user", ENABLE_ACLK_DISP1, 28, 0, 0),
	GATE(aclk_ppmu_tvx, "aclk_ppmu_tvx", "mout_aclk_disp_222_user", ENABLE_ACLK_DISP1, 31, 0, 0),

};

/* fixed rate clocks generated outside the soc */
struct samsung_fixed_rate_clock exynos5430_fixed_rate_ext_clks[] __initdata = {
	FRATE(fin_pll, "fin_pll", NULL, CLK_IS_ROOT, 0),
};

static __initdata struct of_device_id ext_clk_match[] = {
	{ .compatible = "samsung,exynos5430-oscclk", .data = (void *)0, },
	{ },
};

unsigned long base_regs[nr_cmu];

static inline __iomem void *get_va(unsigned long input_addr)
{
	int base_idx;
	unsigned long offset;
	__iomem void *ret;

	base_idx = input_addr >> 20;
	offset = input_addr & 0xfffff;
	ret = base_regs[base_idx] + (void *)offset;

	return ret;
}

/* register exynos5430 clocks */
void __init exynos5430_clk_init(struct device_node *np)
{
	struct clk *egl_pll, *kfc_pll, *mem_pll, *bus_pll, *mfc_pll, *mphy_pll,
		*g3d_pll, *disp_pll, *aud_pll, *bus_dpll, *isp_pll;
	int i;

	if (np) {
		for (i = 0; i < nr_cmu; i++) {
			base_regs[i] = (unsigned long)of_iomap(np, i);
		if (!base_regs[i])
			panic("%s: failed to map registers (%d)\n", __func__, i);
		}
	} else {
		panic("%s: unable to determine soc\n", __func__);
	}

	for (i = 0; i < ARRAY_SIZE(exynos5430_clk_regs); i++) {
		exynos5430_clk_regs[i] = (unsigned long)get_va(exynos5430_clk_regs[i]);
	}

	for (i = 0; i < ARRAY_SIZE(exynos5430_mux_clks); i++) {
		exynos5430_mux_clks[i].offset = (unsigned long)get_va(exynos5430_mux_clks[i].offset);
	}

	for (i = 0; i < ARRAY_SIZE(exynos5430_div_clks); i++) {
		exynos5430_div_clks[i].offset = (unsigned long)get_va(exynos5430_div_clks[i].offset);
	}

	for (i = 0; i < ARRAY_SIZE(exynos5430_gate_clks); i++) {
		exynos5430_gate_clks[i].offset = (unsigned long)get_va(exynos5430_gate_clks[i].offset);
	}

	samsung_clk_init(np, 0, nr_clks, exynos5430_clk_regs,
			ARRAY_SIZE(exynos5430_clk_regs), NULL, 0);

	samsung_clk_of_register_fixed_ext(exynos5430_fixed_rate_ext_clks,
			ARRAY_SIZE(exynos5430_fixed_rate_ext_clks),
			ext_clk_match);


	egl_pll = samsung_clk_register_pll35xx("fout_egl_pll", "fin_pll",
			get_va(EGL_PLL_LOCK), get_va(EGL_PLL_CON0), NULL, 0);

	kfc_pll = samsung_clk_register_pll35xx("fout_kfc_pll", "fin_pll",
			get_va(KFC_PLL_LOCK), get_va(KFC_PLL_CON0), NULL, 0);

	mem_pll = samsung_clk_register_pll35xx("fout_mem_pll", "fin_pll",
			get_va(MEM_PLL_LOCK), get_va(MEM_PLL_CON0), NULL, 0);

	bus_pll = samsung_clk_register_pll35xx("fout_bus_pll", "fin_pll",
			get_va(BUS_PLL_LOCK), get_va(BUS_PLL_CON0), NULL, 0);

	bus_dpll = samsung_clk_register_pll35xx("fout_bus_dpll", "fin_pll",
			get_va(BUS_DPLL_LOCK), get_va(BUS_DPLL_CON0), NULL, 0);

	mfc_pll = samsung_clk_register_pll35xx("fout_mfc_pll", "fin_pll",
			get_va(MFC_PLL_LOCK), get_va(MFC_PLL_CON0), NULL, 0);

	mphy_pll = samsung_clk_register_pll35xx("fout_mphy_pll", "fin_pll",
			get_va(MPHY_PLL_LOCK), get_va(MPHY_PLL_CON0), NULL, 0);

	g3d_pll = samsung_clk_register_pll35xx("fout_g3d_pll", "fin_pll",
			get_va(G3D_PLL_LOCK), get_va(G3D_PLL_CON0), NULL, 0);

	disp_pll = samsung_clk_register_pll35xx("fout_disp_pll", "fin_pll",
			get_va(DISP_PLL_LOCK), get_va(DISP_PLL_CON0), NULL, 0);

	isp_pll = samsung_clk_register_pll35xx("fout_isp_pll", "fin_pll",
			get_va(ISP_PLL_LOCK), get_va(ISP_PLL_CON0), NULL, 0);

	aud_pll = samsung_clk_register_pll36xx("fout_aud_pll", "fin_pll",
			get_va(AUD_PLL_LOCK), get_va(AUD_PLL_CON0), NULL, 0);

	samsung_clk_register_fixed_rate(exynos5430_fixed_rate_clks,
			ARRAY_SIZE(exynos5430_fixed_rate_clks));
#if 0
	samsung_clk_register_fixed_factor(exynos5430_fixted_factor_clks,
			ARRAY_SIZE(exynos5430_fixted_factor_clks));
#endif
	samsung_clk_register_mux(exynos5430_mux_clks,
			ARRAY_SIZE(exynos5430_mux_clks));
	samsung_clk_register_div(exynos5430_div_clks,
			ARRAY_SIZE(exynos5430_div_clks));
	samsung_clk_register_gate(exynos5430_gate_clks,
			ARRAY_SIZE(exynos5430_gate_clks));

	pr_info("Exynos5430: clock setup completed\n");
}
CLK_OF_DECLARE(exynos5430_clk, "samsung,exynos5430-clock", exynos5430_clk_init);
