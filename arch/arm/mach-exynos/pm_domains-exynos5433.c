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

static void exynos5_pd_enable_clk(struct exynos5430_pd_state *ptr, int nr_regs)
{
	int i;
	unsigned long val;
	spin_lock(&rpmlock_cmutop);
	for (i = 0; i < nr_regs; i++, ptr++) {
		val = __raw_readl(ptr->reg);
		__raw_writel(val | ptr->val, ptr->reg);
	}
	spin_unlock(&rpmlock_cmutop);
}

static void exynos5_pd_disable_clk(struct exynos5430_pd_state *ptr, int nr_regs)
{
	unsigned long val;
	spin_lock(&rpmlock_cmutop);
	for (; nr_regs > 0; nr_regs--, ptr++) {
		val = __raw_readl(ptr->reg);
		__raw_writel(val & ~(ptr->val), ptr->reg);
	}
	spin_unlock(&rpmlock_cmutop);
}

/*
#ifdef CONFIG_SOC_EXYNOS5430
static void exynos_pd_notify_power_state(struct exynos_pm_domain *pd, unsigned int turn_on)
{
#ifdef CONFIG_ARM_EXYNOS5430_BUS_DEVFREQ
	exynos5_int_notify_power_status(pd->genpd.name, true);
	exynos5_isp_notify_power_status(pd->genpd.name, true);
	exynos5_disp_notify_power_status(pd->genpd.name, true);
#endif
}
*/

static struct sleep_save exynos_pd_maudio_clk_save[] = {
	SAVE_ITEM(EXYNOS5430_ENABLE_IP_AUD0),
	SAVE_ITEM(EXYNOS5430_ENABLE_IP_AUD1),
};

/* exynos_pd_maudio_power_on_post - callback after power on.
 * @pd: power domain.
 */
static int exynos_pd_maudio_power_on_post(struct exynos_pm_domain *pd)
{
	unsigned int reg;

	s3c_pm_do_restore_core(exynos_pd_maudio_clk_save,
			ARRAY_SIZE(exynos_pd_maudio_clk_save));

	/* PAD retention release */
	reg = __raw_readl(EXYNOS_PAD_RET_MAUDIO_OPTION);
	reg |= (1 << 28);
	__raw_writel(reg, EXYNOS_PAD_RET_MAUDIO_OPTION);

	return 0;
}

static int exynos_pd_maudio_power_off_pre(struct exynos_pm_domain *pd)
{
	s3c_pm_do_save(exynos_pd_maudio_clk_save,
			ARRAY_SIZE(exynos_pd_maudio_clk_save));

	return 0;
}

static struct sleep_save exynos_pd_g3d_clk_save[] = {
	SAVE_ITEM(EXYNOS5430_ENABLE_IP_G3D0),
	SAVE_ITEM(EXYNOS5430_ENABLE_IP_G3D1),
};

static int exynos_pd_g3d_power_on_post(struct exynos_pm_domain *pd)
{
	s3c_pm_do_restore_core(exynos_pd_g3d_clk_save,
			ARRAY_SIZE(exynos_pd_g3d_clk_save));

	DEBUG_PRINT_INFO("EXYNOS5430_DIV_G3D: %08x\n", __raw_readl(EXYNOS5430_DIV_G3D));
	DEBUG_PRINT_INFO("EXYNOS5430_SRC_SEL_G3D: %08x\n", __raw_readl(EXYNOS5430_SRC_SEL_G3D));

	return 0;
}

static int exynos_pd_g3d_power_off_pre(struct exynos_pm_domain *pd)
{
	s3c_pm_do_save(exynos_pd_g3d_clk_save,
			ARRAY_SIZE(exynos_pd_g3d_clk_save));

	return 0;
}

static struct sleep_save exynos_pd_mfc_clk_save[] = {
	SAVE_ITEM(EXYNOS5430_ENABLE_IP_MFC00),
	SAVE_ITEM(EXYNOS5430_ENABLE_IP_MFC01),
	SAVE_ITEM(EXYNOS5430_ENABLE_IP_MFC0_SECURE_SMMU_MFC),
};

/* exynos_pd_mfc_power_on_pre - setup before power on.
 * @pd: power domain.
 *
 * enable top clock.
 */
static int exynos_pd_mfc_power_on_pre(struct exynos_pm_domain *pd)
{
	exynos5_pd_enable_clk(cmutop_mfc, ARRAY_SIZE(cmutop_mfc));
	return 0;
}

static int exynos_pd_mfc_power_on_post(struct exynos_pm_domain *pd)
{
	/*exynos_pd_notify_power_state(pd, true);*/

	s3c_pm_do_restore_core(exynos_pd_mfc_clk_save,
			ARRAY_SIZE(exynos_pd_mfc_clk_save));

	return 0;
}

static int exynos_pd_mfc_power_off_pre(struct exynos_pm_domain *pd)
{
	s3c_pm_do_save(exynos_pd_mfc_clk_save,
			ARRAY_SIZE(exynos_pd_mfc_clk_save));

	exynos5_pd_enable_clk(cmutop_mfc, ARRAY_SIZE(cmutop_mfc));

	return 0;
}

/* exynos_pd_mfc_power_off_post - clean up after power off.
 * @pd: power domain.
 *
 * disable top clock.
 */
static int exynos_pd_mfc_power_off_post(struct exynos_pm_domain *pd)
{
	exynos5_pd_disable_clk(cmutop_mfc, ARRAY_SIZE(cmutop_mfc));
	return 0;
}

static struct sleep_save exynos_pd_hevc_clk_save[] = {
	SAVE_ITEM(EXYNOS5430_ENABLE_IP_HEVC0),
	SAVE_ITEM(EXYNOS5430_ENABLE_IP_HEVC1),
	SAVE_ITEM(EXYNOS5430_ENABLE_IP_HEVC_SECURE_SMMU_HEVC),
};

/* exynos_pd_hevc_power_on_pre - setup before power on.
 * @pd: power domain.
 *
 * enable top clock.
 */
static int exynos_pd_hevc_power_on_pre(struct exynos_pm_domain *pd)
{
	exynos5_pd_enable_clk(cmutop_hevc, ARRAY_SIZE(cmutop_hevc));
	return 0;
}

static int exynos_pd_hevc_power_on_post(struct exynos_pm_domain *pd)
{
	/*exynos_pd_notify_power_state(pd, true);*/

	s3c_pm_do_restore_core(exynos_pd_hevc_clk_save,
			ARRAY_SIZE(exynos_pd_hevc_clk_save));

	return 0;
}

static int exynos_pd_hevc_power_off_pre(struct exynos_pm_domain *pd)
{
	s3c_pm_do_save(exynos_pd_hevc_clk_save,
			ARRAY_SIZE(exynos_pd_hevc_clk_save));

	exynos5_pd_enable_clk(cmutop_hevc, ARRAY_SIZE(cmutop_hevc));

	return 0;
}

/* exynos_pd_hevc_power_off_post - clean up after power off.
 * @pd: power domain.
 *
 * disable top clock.
 */
static int exynos_pd_hevc_power_off_post(struct exynos_pm_domain *pd)
{
	exynos5_pd_disable_clk(cmutop_hevc, ARRAY_SIZE(cmutop_hevc));
	return 0;
}

static struct sleep_save exynos_pd_gscl_clk_save[] = {
	SAVE_ITEM(EXYNOS5430_ENABLE_IP_GSCL0),
	SAVE_ITEM(EXYNOS5430_ENABLE_IP_GSCL1),
	SAVE_ITEM(EXYNOS5430_ENABLE_IP_GSCL_SECURE_SMMU_GSCL0),
	SAVE_ITEM(EXYNOS5430_ENABLE_IP_GSCL_SECURE_SMMU_GSCL1),
	SAVE_ITEM(EXYNOS5430_ENABLE_IP_GSCL_SECURE_SMMU_GSCL2),
};

/* exynos_pd_gscl_power_on_pre - setup before power on.
 * @pd: power domain.
 *
 * enable top clock.
 */
static int exynos_pd_gscl_power_on_pre(struct exynos_pm_domain *pd)
{
	exynos5_pd_enable_clk(cmutop_gscl, ARRAY_SIZE(cmutop_gscl));
	return 0;
}

static int exynos_pd_gscl_power_on_post(struct exynos_pm_domain *pd)
{
	s3c_pm_do_restore_core(exynos_pd_gscl_clk_save,
			ARRAY_SIZE(exynos_pd_gscl_clk_save));

	/*exynos_pd_notify_power_state(pd, true);*/

	return 0;
}

static int exynos_pd_gscl_power_off_pre(struct exynos_pm_domain *pd)
{
	s3c_pm_do_save(exynos_pd_gscl_clk_save,
			ARRAY_SIZE(exynos_pd_gscl_clk_save));

	exynos5_pd_enable_clk(cmutop_gscl, ARRAY_SIZE(cmutop_gscl));

	return 0;
}

/* exynos_pd_gscl_power_off_post - clean up after power off.
 * @pd: power domain.
 *
 * disable top clock.
 */
static int exynos_pd_gscl_power_off_post(struct exynos_pm_domain *pd)
{
	exynos5_pd_disable_clk(cmutop_gscl, ARRAY_SIZE(cmutop_gscl));
	return 0;
}

static struct sleep_save exynos_pd_disp_clk_save[] = {
	SAVE_ITEM(EXYNOS5430_ENABLE_IP_DISP0),
	SAVE_ITEM(EXYNOS5430_ENABLE_IP_DISP1),
	SAVE_ITEM(EXYNOS5430_DISP_PLL_LOCK),
	SAVE_ITEM(EXYNOS5430_DISP_PLL_CON0),
	SAVE_ITEM(EXYNOS5430_DISP_PLL_CON1),
	SAVE_ITEM(EXYNOS5430_DISP_PLL_FREQ_DET),
};

/* exynos_pd_disp_power_on_pre - setup before power on.
 * @pd: power domain.
 *
 * enable IP_MIF3 clk.
 */
static int exynos_pd_disp_power_on_pre(struct exynos_pm_domain *pd)
{
	unsigned int reg;

	reg = __raw_readl(EXYNOS5430_ENABLE_IP_MIF3);
	reg |= ((1 << 9) | (1 << 8 ) | (1 << 7) | (1 << 6) | (1 << 5) | (1 << 1));
	__raw_writel(reg, EXYNOS5430_ENABLE_IP_MIF3);

	return 0;
}

/* exynos_pd_disp_power_on_post - setup after power on.
 * @pd: power domain.
 *
 * enable DISP dynamic clock gating
 */
static int exynos_pd_disp_power_on_post(struct exynos_pm_domain *pd)
{
	/*exynos_pd_notify_power_state(pd, true);*/

	s3c_pm_do_restore_core(exynos_pd_disp_clk_save,
			ARRAY_SIZE(exynos_pd_disp_clk_save));

	return 0;
}

/* exynos_pd_disp_power_off_pre - setup before power off.
 * @pd: power domain.
 *
 * enable IP_MIF3 clk.
 * check Decon has been reset.
 */
static int exynos_pd_disp_power_off_pre(struct exynos_pm_domain *pd)
{
	unsigned int reg;
	unsigned timeout = 1000;

	DEBUG_PRINT_INFO("disp pre power off\n");
	reg = __raw_readl(EXYNOS5430_ENABLE_IP_MIF3);
	reg |= ((1 << 9) | (1 << 8 ) | (1 << 7) | (1 << 6) | (1 << 5) | (1 << 1));
	__raw_writel(reg, EXYNOS5430_ENABLE_IP_MIF3);

	if (!IS_ERR_OR_NULL(decon_vidcon0)) {
		do {
			reg = __raw_readl(decon_vidcon0);
			timeout --;

			if (!timeout) {
				printk("%s, decon timeout \n", __func__);
				return -ETIMEDOUT;
			}
		} while((reg >> 2) & 0x1);

		reg = __raw_readl(decon_vidcon0);
		reg &= ~(1 << 28);
		reg |= (1 << 28);
		__raw_writel(reg, decon_vidcon0);
	}

	timeout = 1000;

	if (!IS_ERR_OR_NULL(decontv_vidcon0)) {
		do {
			reg = __raw_readl(decontv_vidcon0);
			timeout --;

			if (!timeout) {
				printk("%s, decontv timeout \n", __func__);
				return -ETIMEDOUT;
			}
		} while((reg >> 2) & 0x1);

		reg = __raw_readl(decontv_vidcon0);
		reg &= ~(1 << 28);
		reg |= (1 << 28);
		__raw_writel(reg, decontv_vidcon0);
	}

	s3c_pm_do_save(exynos_pd_disp_clk_save,
			ARRAY_SIZE(exynos_pd_disp_clk_save));

	return 0;
}

/* exynos_pd_disp_power_off_post - clean up after power off.
 * @pd: power domain.
 *
 * disable IP_MIF3 clk.
 * disable SRC_SEL_MIF4/5
 */
static int exynos_pd_disp_power_off_post(struct exynos_pm_domain *pd)
{
	unsigned int reg;

	reg = __raw_readl(EXYNOS5430_ENABLE_IP_MIF3);
	reg &= ~((1 << 9) | (1 << 8 ) | (1 << 7) | (1 << 6) | (1 << 5) | (1 << 1));
	__raw_writel(reg, EXYNOS5430_ENABLE_IP_MIF3);

	reg = __raw_readl(EXYNOS5430_SRC_SEL_MIF4);
	reg &= ~((1 << 24) | (1 << 20) | (1 << 16) | (1 << 8) | (1 << 4) | (1 << 0));
	__raw_writel(reg, EXYNOS5430_SRC_SEL_MIF4);

	reg = __raw_readl(EXYNOS5430_SRC_SEL_MIF5);
	reg &= ~((1 << 24) | (1 << 20) | (1 << 16) | (1 << 8) | (1 << 4) | (1 << 0));
	__raw_writel(reg, EXYNOS5430_SRC_SEL_MIF5);

	reg = __raw_readl(EXYNOS5430_SRC_SEL_MIF6);
	reg &= ~((1 << 8) | (1 << 4) | (1 << 0));
	__raw_writel(reg, EXYNOS5430_SRC_SEL_MIF6);

	return 0;
}

static struct sleep_save exynos_pd_mscl_clk_save[] = {
	SAVE_ITEM(EXYNOS5430_ENABLE_IP_MSCL0),
	SAVE_ITEM(EXYNOS5430_ENABLE_IP_MSCL1),
	SAVE_ITEM(EXYNOS5430_ENABLE_IP_MSCL_SECURE_SMMU_M2MSCALER0),
	SAVE_ITEM(EXYNOS5430_ENABLE_IP_MSCL_SECURE_SMMU_M2MSCALER1),
	SAVE_ITEM(EXYNOS5430_ENABLE_IP_MSCL_SECURE_SMMU_JPEG),
};

/* exynos_pd_mscl_power_on_pre - setup before power on.
 * @pd: power domain.
 *
 * enable top clock.
 */
static int exynos_pd_mscl_power_on_pre(struct exynos_pm_domain *pd)
{
	DEBUG_PRINT_INFO("%s is preparing power-on sequence.\n", pd->name);
	exynos5_pd_enable_clk(cmutop_mscl, ARRAY_SIZE(cmutop_mscl));
	return 0;
}

static int exynos_pd_mscl_power_on_post(struct exynos_pm_domain *pd)
{
	/* exynos_pd_notify_power_state(pd, true);*/

	s3c_pm_do_restore_core(exynos_pd_mscl_clk_save,
			ARRAY_SIZE(exynos_pd_mscl_clk_save));

	return 0;
}

static int exynos_pd_mscl_power_off_pre(struct exynos_pm_domain *pd)
{
	exynos5_pd_enable_clk(cmutop_mscl, ARRAY_SIZE(cmutop_mscl));

	s3c_pm_do_save(exynos_pd_mscl_clk_save,
			ARRAY_SIZE(exynos_pd_mscl_clk_save));

	return 0;
}

/* exynos_pd_mscl_power_off_post - clean up after power off.
 * @pd: power domain.
 *
 * disable top clock.
 */
static int exynos_pd_mscl_power_off_post(struct exynos_pm_domain *pd)
{
	DEBUG_PRINT_INFO("%s is clearing power-off sequence.\n", pd->name);
	exynos5_pd_disable_clk(cmutop_mscl, ARRAY_SIZE(cmutop_mscl));
	return 0;
}

static struct sleep_save exynos_pd_g2d_clk_save[] = {
	SAVE_ITEM(EXYNOS5430_ENABLE_IP_G2D0),
	SAVE_ITEM(EXYNOS5430_ENABLE_IP_G2D1),
	SAVE_ITEM(EXYNOS5430_ENABLE_IP_G2D_SECURE_SMMU_G2D),
};

/* exynos_pd_g2d_power_on_pre - setup before power on.
 * @pd: power domain.
 *
 * enable top clock.
 */
static int exynos_pd_g2d_power_on_pre(struct exynos_pm_domain *pd)
{
	DEBUG_PRINT_INFO("%s is preparing power-on sequence.\n", pd->name);
	exynos5_pd_enable_clk(cmutop_g2d, ARRAY_SIZE(cmutop_g2d));
	return 0;
}

static int exynos_pd_g2d_power_on_post(struct exynos_pm_domain *pd)
{
	/*exynos_pd_notify_power_state(pd, true);*/

	s3c_pm_do_restore_core(exynos_pd_g2d_clk_save,
			ARRAY_SIZE(exynos_pd_g2d_clk_save));

	return 0;
}

static int exynos_pd_g2d_power_off_pre(struct exynos_pm_domain *pd)
{
	exynos5_pd_enable_clk(cmutop_g2d, ARRAY_SIZE(cmutop_g2d));

	s3c_pm_do_save(exynos_pd_g2d_clk_save,
			ARRAY_SIZE(exynos_pd_g2d_clk_save));

	return 0;
}

/* exynos_pd_g2d_power_off_post - clean up after power off.
 * @pd: power domain.
 *
 * disable top clock.
 */
static int exynos_pd_g2d_power_off_post(struct exynos_pm_domain *pd)
{
	DEBUG_PRINT_INFO("%s is clearing power-off sequence.\n", pd->name);
	exynos5_pd_disable_clk(cmutop_g2d, ARRAY_SIZE(cmutop_g2d));
	return 0;
}

static struct sleep_save exynos_pd_isp_clk_save[] = {
	SAVE_ITEM(EXYNOS5430_ENABLE_IP_ISP0),
	SAVE_ITEM(EXYNOS5430_ENABLE_IP_ISP1),
	SAVE_ITEM(EXYNOS5430_ENABLE_IP_ISP2),
	SAVE_ITEM(EXYNOS5430_ENABLE_IP_ISP3),
};

/* exynos_pd_isp_power_on_pre - setup before power on.
 * @pd: power domain.
 *
 * enable top clock.
 */
static int exynos_pd_isp_power_on_pre(struct exynos_pm_domain *pd)
{
	DEBUG_PRINT_INFO("%s is preparing power-on sequence.\n", pd->name);
	exynos5_pd_enable_clk(cmutop_isp, ARRAY_SIZE(cmutop_isp));

	return 0;
}

static int exynos_pd_isp_power_on_post(struct exynos_pm_domain *pd)
{
	/*exynos_pd_notify_power_state(pd, true);*/

	s3c_pm_do_restore_core(exynos_pd_isp_clk_save,
			ARRAY_SIZE(exynos_pd_isp_clk_save));

	return 0;
}

/* exynos_pd_isp_power_off_pre - setup before power off.
 * @pd: power domain.
 *
 * enable IP_ISP1 clk.
 */
static int exynos_pd_isp_power_off_pre(struct exynos_pm_domain *pd)
{
	unsigned int reg;

	/* For prevent FW clock gating */
	__raw_writel(0x0000007F, EXYNOS5430_ENABLE_ACLK_ISP_LOCAL);
	__raw_writel(0x0000003F, EXYNOS5430_ENABLE_PCLK_ISP_LOCAL);
	__raw_writel(0x0000003F, EXYNOS5430_ENABLE_SCLK_ISP_LOCAL);
	__raw_writel(0x0000007F, EXYNOS5430_ENABLE_IP_ISP_LOCAL0);
	__raw_writel(0x0000000F, EXYNOS5430_ENABLE_IP_ISP_LOCAL1);

	/* For prevent HOST clock gating */
	__raw_writel(0x0000007F, EXYNOS5430_ENABLE_ACLK_ISP0);
	__raw_writel(0x0003FFFF, EXYNOS5430_ENABLE_ACLK_ISP1);
	__raw_writel(0x00003FFF, EXYNOS5430_ENABLE_ACLK_ISP2);
	__raw_writel(0x03FFFFFF, EXYNOS5430_ENABLE_PCLK_ISP);
	__raw_writel(0x0000003F, EXYNOS5430_ENABLE_SCLK_ISP);
	__raw_writel(0x000003FF, EXYNOS5430_ENABLE_IP_ISP0);
	__raw_writel(0x0000FFFF, EXYNOS5430_ENABLE_IP_ISP1);
	__raw_writel(0x00003FFF, EXYNOS5430_ENABLE_IP_ISP2);
	__raw_writel(0x0000000F, EXYNOS5430_ENABLE_IP_ISP3);

	__raw_writel(0x0000003F, EXYNOS5430_LPI_MASK_ISP_BUSMASTER);

	exynos5_pd_enable_clk(cmutop_isp, ARRAY_SIZE(cmutop_isp));

	DEBUG_PRINT_INFO("%s is preparing power-off sequence.\n", pd->name);
	reg = __raw_readl(EXYNOS5430_ENABLE_IP_ISP1);
	reg |= (1 << 12 | 1 << 11 | 1 << 8 | 1 << 7 | 1 << 2);
	__raw_writel(reg, EXYNOS5430_ENABLE_IP_ISP1);

	s3c_pm_do_save(exynos_pd_isp_clk_save,
			ARRAY_SIZE(exynos_pd_isp_clk_save));

	return 0;
}

/* exynos_pd_isp_power_off_post - clean up after power off.
 * @pd: power domain.
 *
 * disable top clock.
 */
static int exynos_pd_isp_power_off_post(struct exynos_pm_domain *pd)
{
	DEBUG_PRINT_INFO("%s is clearing power-off sequence.\n", pd->name);
	exynos5_pd_disable_clk(cmutop_isp, ARRAY_SIZE(cmutop_isp));
	return 0;
}

static struct sleep_save exynos_pd_cam0_clk_save[] = {
	SAVE_ITEM(EXYNOS5430_ENABLE_IP_CAM00),
	SAVE_ITEM(EXYNOS5430_ENABLE_IP_CAM01),
	SAVE_ITEM(EXYNOS5430_ENABLE_IP_CAM02),
	SAVE_ITEM(EXYNOS5430_ENABLE_IP_CAM03),
};

/* exynos_pd_cam0_power_on_pre - setup before power on.
 * @pd: power domain.
 *
 * enable top clock.
 */
static int exynos_pd_cam0_power_on_pre(struct exynos_pm_domain *pd)
{
	DEBUG_PRINT_INFO("%s is preparing power-on sequence.\n", pd->name);
	exynos5_pd_enable_clk(cmutop_cam0, ARRAY_SIZE(cmutop_cam0));
	return 0;
}

static int exynos_pd_cam0_power_on_post(struct exynos_pm_domain *pd)
{
	/*exynos_pd_notify_power_state(pd, true);*/

	s3c_pm_do_restore_core(exynos_pd_cam0_clk_save,
			ARRAY_SIZE(exynos_pd_cam0_clk_save));

	return 0;
}

/* exynos_pd_cam0_power_off_pre - setup before power off.
 * @pd: power domain.
 *
 * enable IP_CAM01 clk.
 */
static int exynos_pd_cam0_power_off_pre(struct exynos_pm_domain *pd)
{
	unsigned int reg;

	DEBUG_PRINT_INFO("%s is preparing power-off sequence.\n", pd->name);
	/* For prevent FW clock gating */
	__raw_writel(0x0000007F, EXYNOS5430_ENABLE_ACLK_CAM0_LOCAL);
	__raw_writel(0x0000007F, EXYNOS5430_ENABLE_PCLK_CAM0_LOCAL);
	__raw_writel(0x0000007F, EXYNOS5430_ENABLE_SCLK_CAM0_LOCAL);
	__raw_writel(0x0000007F, EXYNOS5430_ENABLE_IP_CAM0_LOCAL0);
	__raw_writel(0x0000001F, EXYNOS5430_ENABLE_IP_CAM0_LOCAL1);

	/* For prevent HOST clock gating */
	__raw_writel(0x0000007F, EXYNOS5430_ENABLE_ACLK_CAM00);
	__raw_writel(0xFFFFFFFF, EXYNOS5430_ENABLE_ACLK_CAM01);
	__raw_writel(0x000003FF, EXYNOS5430_ENABLE_ACLK_CAM02);
	__raw_writel(0x03FFFFFF, EXYNOS5430_ENABLE_PCLK_CAM0);
	__raw_writel(0x000001FF, EXYNOS5430_ENABLE_SCLK_CAM0);
	__raw_writel(0x000003FF, EXYNOS5430_ENABLE_IP_CAM00);
	__raw_writel(0x007FFFFF, EXYNOS5430_ENABLE_IP_CAM01);
	__raw_writel(0x000003FF, EXYNOS5430_ENABLE_IP_CAM02);
	__raw_writel(0x0000001F, EXYNOS5430_ENABLE_IP_CAM03);

	/* LPI disable */
	__raw_writel(0x0000001F, EXYNOS5430_LPI_MASK_CAM0_BUSMASTER);

	/* Clock on */
	exynos5_pd_enable_clk(cmutop_cam0, ARRAY_SIZE(cmutop_cam0));

	/* Related async-bridge clock on */
	reg = __raw_readl(EXYNOS5430_ENABLE_IP_CAM01);
	reg |= (1 << 12);
	__raw_writel(reg, EXYNOS5430_ENABLE_IP_CAM01);

	s3c_pm_do_save(exynos_pd_cam0_clk_save,
			ARRAY_SIZE(exynos_pd_cam0_clk_save));

	return 0;
}

/* exynos_pd_cam0_power_off_post - clean up after power off.
 * @pd: power domain.
 *
 * disable top clock.
 */
static int exynos_pd_cam0_power_off_post(struct exynos_pm_domain *pd)
{
	DEBUG_PRINT_INFO("%s is clearing power-off sequence.\n", pd->name);
	exynos5_pd_disable_clk(cmutop_cam0, ARRAY_SIZE(cmutop_cam0));
	return 0;
}

static struct sleep_save exynos_pd_cam1_clk_save[] = {
	SAVE_ITEM(EXYNOS5430_ENABLE_IP_CAM10),
	SAVE_ITEM(EXYNOS5430_ENABLE_IP_CAM11),
	SAVE_ITEM(EXYNOS5430_ENABLE_IP_CAM12),
};

/* exynos_pd_cam1_power_on_pre - setup before power on.
 * @pd: power domain.
 *
 * enable top clock.
 */
static int exynos_pd_cam1_power_on_pre(struct exynos_pm_domain *pd)
{
	DEBUG_PRINT_INFO("%s is preparing power-on sequence.\n", pd->name);
	exynos5_pd_enable_clk(cmutop_cam1, ARRAY_SIZE(cmutop_cam1));
	return 0;
}

static int exynos_pd_cam1_power_on_post(struct exynos_pm_domain *pd)
{
	/*exynos_pd_notify_power_state(pd, true);*/

	s3c_pm_do_restore_core(exynos_pd_cam1_clk_save,
			ARRAY_SIZE(exynos_pd_cam1_clk_save));

	return 0;
}

/* exynos_pd_cam1_power_off_pre - setup before power off.
 * @pd: power domain.
 *
 * enable IP_CAM11 clk.
 */
static int exynos_pd_cam1_power_off_pre(struct exynos_pm_domain *pd)
{
	unsigned int reg;

	DEBUG_PRINT_INFO("%s is preparing power-off sequence.\n", pd->name);

	/* For prevent FW clock gating */
	__raw_writel(0x0000001F, EXYNOS5430_ENABLE_ACLK_CAM1_LOCAL);
	__raw_writel(0x00003FFF, EXYNOS5430_ENABLE_PCLK_CAM1_LOCAL);
	__raw_writel(0x00000007, EXYNOS5430_ENABLE_SCLK_CAM1_LOCAL);
	__raw_writel(0x00007FFF, EXYNOS5430_ENABLE_IP_CAM1_LOCAL0);
	__raw_writel(0x00000007, EXYNOS5430_ENABLE_IP_CAM1_LOCAL1);

	/* For prevent HOST clock gating */
	__raw_writel(0x0000001F, EXYNOS5430_ENABLE_ACLK_CAM10);
	__raw_writel(0x3FFFFFFF, EXYNOS5430_ENABLE_ACLK_CAM11);
	__raw_writel(0x000007FF, EXYNOS5430_ENABLE_ACLK_CAM12);
	__raw_writel(0x0FFFFFFF, EXYNOS5430_ENABLE_PCLK_CAM1);
	__raw_writel(0x0000FFFF, EXYNOS5430_ENABLE_SCLK_CAM1);
	__raw_writel(0x00FFFFFF, EXYNOS5430_ENABLE_IP_CAM10);
	__raw_writel(0x003FFFFF, EXYNOS5430_ENABLE_IP_CAM11);
	__raw_writel(0x000007FF, EXYNOS5430_ENABLE_IP_CAM12);

	/* LPI disable */
	__raw_writel(0x00000003, EXYNOS5430_LPI_MASK_CAM1_BUSMASTER);

	exynos5_pd_enable_clk(cmutop_cam1, ARRAY_SIZE(cmutop_cam1));

	reg = __raw_readl(EXYNOS5430_ENABLE_IP_CAM11);
	reg |= (1 << 19 | 1 << 18 | 1 << 16 | 1 << 15 | 1 << 14 | 1 << 13);
	__raw_writel(reg, EXYNOS5430_ENABLE_IP_CAM11);

	s3c_pm_do_save(exynos_pd_cam1_clk_save,
			ARRAY_SIZE(exynos_pd_cam1_clk_save));

	return 0;
}

/* exynos_pd_cam1_power_off_post - clean up after power off.
 * @pd: power domain.
 *
 * disable top clock.
 */
static int exynos_pd_cam1_power_off_post(struct exynos_pm_domain *pd)
{
	unsigned int reg;

	DEBUG_PRINT_INFO("%s is clearing power-off sequence.\n", pd->name);
	exynos5_pd_disable_clk(cmutop_cam1, ARRAY_SIZE(cmutop_cam1));

	reg = __raw_readl(EXYNOS5430_SRC_SEL_TOP_CAM1);
	reg &= ~((1 << 24) | (1 << 20) | (1 << 16) | (1 << 8) | (1 << 4) | (1 << 0));
	__raw_writel(reg, EXYNOS5430_SRC_SEL_TOP_CAM1);

	return 0;
}

static unsigned int check_power_status(struct exynos_pm_domain *pd, int power_flags,
					unsigned int timeout)
{
	/* check STATUS register */
	while ((__raw_readl(pd->base+0x4) & EXYNOS_INT_LOCAL_PWR_EN) != power_flags) {
		if (timeout == 0) {
			pr_err("%s@%p: %08x, %08x, %08x\n",
					pd->genpd.name,
					pd->base,
					__raw_readl(pd->base),
					__raw_readl(pd->base+4),
					__raw_readl(pd->base+8));
			return 0;
		}
		--timeout;
		cpu_relax();
		usleep_range(8, 10);
	}

	return timeout;
}

#define TIMEOUT_COUNT	5000 /* about 50ms, based on 1ms */
static int exynos_pd_power_off_custom(struct exynos_pm_domain *pd, int power_flags)
{
	unsigned long timeout;

	if (unlikely(!pd))
		return -EINVAL;

	mutex_lock(&pd->access_lock);
	if (likely(pd->base)) {
		/* sc_feedback to OPTION register */
		__raw_writel(0x0102, pd->base+0x8);

		/* on/off value to CONFIGURATION register */
		__raw_writel(power_flags, pd->base);

		timeout = check_power_status(pd, power_flags, TIMEOUT_COUNT);

		if (unlikely(!timeout)) {
			pr_err(PM_DOMAIN_PREFIX "%s can't control power, timeout\n",
					pd->name);
			mutex_unlock(&pd->access_lock);
			return -ETIMEDOUT;
		} else {
			pr_info(PM_DOMAIN_PREFIX "%s power down success\n", pd->name);
		}

		if (unlikely(timeout < (TIMEOUT_COUNT >> 1))) {
			pr_warn("%s@%p: %08x, %08x, %08x\n",
					pd->name,
					pd->base,
					__raw_readl(pd->base),
					__raw_readl(pd->base+4),
					__raw_readl(pd->base+8));
			pr_warn(PM_DOMAIN_PREFIX "long delay found during %s is %s\n",
					pd->name, power_flags ? "on":"off");
		}
	}
	pd->status = power_flags;
	mutex_unlock(&pd->access_lock);

	DEBUG_PRINT_INFO("%s@%p: %08x, %08x, %08x\n",
				pd->genpd.name, pd->base,
				__raw_readl(pd->base),
				__raw_readl(pd->base+4),
				__raw_readl(pd->base+8));

	return 0;
}

static struct exynos_pd_callback pd_callback_list[] = {
	{
		.name = "pd-maudio",
		.on_post = exynos_pd_maudio_power_on_post,
		.off_pre = exynos_pd_maudio_power_off_pre,
	}, {
		.name = "pd-mfc",
		.on_pre = exynos_pd_mfc_power_on_pre,
		.on_post = exynos_pd_mfc_power_on_post,
		.off_pre = exynos_pd_mfc_power_off_pre,
		.off_post = exynos_pd_mfc_power_off_post,
	}, {
		.name = "pd-hevc",
		.on_pre = exynos_pd_hevc_power_on_pre,
		.on_post = exynos_pd_hevc_power_on_post,
		.off_pre = exynos_pd_hevc_power_off_pre,
		.off_post = exynos_pd_hevc_power_off_post,
	}, {
		.name = "pd-gscl",
		.on_pre = exynos_pd_gscl_power_on_pre,
		.on_post = exynos_pd_gscl_power_on_post,
		.off_pre = exynos_pd_gscl_power_off_pre,
		.off_post = exynos_pd_gscl_power_off_post,
	}, {
		.name = "pd-g3d",
		.on_post = exynos_pd_g3d_power_on_post,
		.off_pre = exynos_pd_g3d_power_off_pre,
	}, {
		.name = "pd-disp",
		.on_pre = exynos_pd_disp_power_on_pre,
		.on_post = exynos_pd_disp_power_on_post,
		.off_pre = exynos_pd_disp_power_off_pre,
		.off_post = exynos_pd_disp_power_off_post,
	}, {
		.name = "pd-mscl",
		.on_pre = exynos_pd_mscl_power_on_pre,
		.on_post = exynos_pd_mscl_power_on_post,
		.off_pre = exynos_pd_mscl_power_off_pre,
		.off_post = exynos_pd_mscl_power_off_post,
	}, {
		.name = "pd-g2d",
		.on_pre = exynos_pd_g2d_power_on_pre,
		.on_post = exynos_pd_g2d_power_on_post,
		.off_pre = exynos_pd_g2d_power_off_pre,
		.off_post = exynos_pd_g2d_power_off_post,
	}, {
		.name = "pd-isp",
		.on_pre = exynos_pd_isp_power_on_pre,
		.on_post = exynos_pd_isp_power_on_post,
		.off_pre = exynos_pd_isp_power_off_pre,
		.off = exynos_pd_power_off_custom,
		.off_post = exynos_pd_isp_power_off_post,
	}, {
		.name = "pd-cam0",
		.on_pre = exynos_pd_cam0_power_on_pre,
		.on_post = exynos_pd_cam0_power_on_post,
		.off_pre = exynos_pd_cam0_power_off_pre,
		.off = exynos_pd_power_off_custom,
		.off_post = exynos_pd_cam0_power_off_post,
	}, {
		.name = "pd-cam1",
		.on_pre = exynos_pd_cam1_power_on_pre,
		.on_post = exynos_pd_cam1_power_on_post,
		.off_pre = exynos_pd_cam1_power_off_pre,
		.off = exynos_pd_power_off_custom,
		.off_post = exynos_pd_cam1_power_off_post,
	},
};

struct exynos_pd_callback * exynos_pd_find_callback(struct exynos_pm_domain *pd)
{
	struct exynos_pd_callback *cb = NULL;
	int i;

	decon_vidcon0 = ioremap(EXYNOS5433_PA_DECON, SZ_4K);
	if (IS_ERR_OR_NULL(decon_vidcon0))
		pr_err("PM DOMAIN : ioremap of decon VIDCON0 register fail\n");

	decontv_vidcon0 = ioremap(EXYNOS5433_PA_DECONTV, SZ_4K);
	if (IS_ERR_OR_NULL(decontv_vidcon0))
		pr_err("PM DOMAIN : ioremap of decontv VIDCON0 register fail\n");

	/* find callback function for power domain */
	for (i=0, cb = &pd_callback_list[0]; i<ARRAY_SIZE(pd_callback_list); i++, cb++) {
		if (strcmp(cb->name, pd->name))
			continue;

		DEBUG_PRINT_INFO("%s: found callback function\n", pd->name);
		break;
	}

	pd->cb = cb;
	return cb;
}
