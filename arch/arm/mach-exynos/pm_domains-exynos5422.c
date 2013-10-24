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

#include <mach/pm_domains.h>

#ifdef CONFIG_SOC_EXYNOS5422

#define SHIFT_GSCL_BLK_300_DIV      4
#define SHIFT_GSCL_BLK_333_432_DIV  6
#define SHIFT_DISP1_BLK_DIV     16
#define SHIFT_MSCL_BLK_DIV      28
#define SHIFT_MFC_BLK_DIV       0
static DEFINE_SPINLOCK(clk_div2_ratio0_lock);

static struct clk *clk_g3d_parent;
static struct clk *clk_300_disp1_parent;
static struct clk *clk_200_disp1_parent;
static struct clk *clk_fimd1_parent;
static struct clk *clk_mdnie1_parent;
static struct clk *clk_dp1_ext_mst_vid_parent;
static struct clk *clk_mipi1_parent;
static struct clk *clk_mdnie_pwm1_parent;
static struct clk *clk_hdmi_parent;
#ifdef CONFIG_FB_MIPI_DSIM
void __iomem *hdmi_regs;
#endif

#define GET_CLK(a) __clk_lookup(a)
#define SET_PARENT(a, b) clk_set_parent(GET_CLK(a), GET_CLK(b))
#define ENA_CLK(a, b) {if (!__clk_is_enabled(GET_CLK(a))) { clk_prepare(GET_CLK(a)); b = clk_enable(GET_CLK(a)); if (b) pr_err(" %s: failed\n %d", __func__, b); } };
#define DIS_CLK(a) {if (__clk_is_enabled(GET_CLK(a))) { clk_disable(GET_CLK(a)); clk_unprepare(GET_CLK(a)); } };

static int exynos5_pd_g3d_power_on_post(struct exynos_pm_domain *pd)
{
	DEBUG_PRINT_INFO("%s: %08x %08x\n", __func__, __raw_readl(pd->base), __raw_readl(pd->base+4));
	clk_set_parent(GET_CLK("aclk_g3d"), clk_g3d_parent);
	return 0;
}

static int exynos5_pd_g3d_power_off_pre(struct exynos_pm_domain *pd)
{
	DEBUG_PRINT_INFO("%s: %08x %08x\n", __func__, __raw_readl(pd->base), __raw_readl(pd->base+4));
	clk_g3d_parent = clk_get_parent(GET_CLK("aclk_g3d"));
	return 0;
}

static int exynos5_pd_g3d_power_on_pre(struct exynos_pm_domain *pd)
{
	int ret = 0;
	DEBUG_PRINT_INFO("%s: %08x %08x\n", __func__, __raw_readl(pd->base), __raw_readl(pd->base+4));
	ENA_CLK("sclk_vpll", ret);
	udelay(70);
	SET_PARENT("mout_vpll", "fout_vpll");
	return 0;
}

static int exynos5_pd_g3d_power_off_post(struct exynos_pm_domain *pd)
{
	DEBUG_PRINT_INFO("%s: %08x %08x\n", __func__, __raw_readl(pd->base), __raw_readl(pd->base+4));
	SET_PARENT("mout_vpll", "fin_pll");
	DIS_CLK("sclk_vpll");
	return 0;
}

static int exynos5_pd_maudio_power_off_pre(struct exynos_pm_domain *pd)
{
	DEBUG_PRINT_INFO("%s: %08x %08x\n", __func__, __raw_readl(pd->base), __raw_readl(pd->base+4));
	__raw_writel(0, EXYNOS5_PAD_RETENTION_MAU_SYS_PWR_REG);
	return 0;
}

static int exynos5_pd_maudio_power_on_post(struct exynos_pm_domain *pd)
{
	DEBUG_PRINT_INFO("%s: %08x %08x\n", __func__, __raw_readl(pd->base), __raw_readl(pd->base+4));
	__raw_writel(0x10000000, EXYNOS_PAD_RET_MAUDIO_OPTION);
	return 0;
}
#ifdef CONFIG_FB_MIPI_DSIM
static int exynos5_pd_disp1_post_power_control(struct exynos_pm_domain *pd)
{
	DEBUG_PRINT_INFO("%s: %08x %08x\n", __func__, __raw_readl(pd->base), __raw_readl(pd->base+4));
	ENA_CLK("hdmi", ret);
	__raw_writel(0x1, hdmi_regs  + 0x30);
	pr_info("HDMI phy power off : %x\n", __raw_readl(hdmi_regs + 0x30));
	DIS_CLK("hdmi");

	return 0;
}
#endif

#ifdef CONFIG_EXYNOS5_DEV_FIMC_IS
static int exynos5_pd_isp_power_control(struct exynos_pm_domain *pd, int power_flag)
{
	int ret = exynos_pm_domain_power_control(domain, power_flag);
	int tmp, timeout = 500;

	DEBUG_PRINT_INFO("%s: %08x %08x\n", __func__, __raw_readl(pd->base), __raw_readl(pd->base+4));
	if (ret) {
		__raw_writel(0, EXYNOS5420_ISP_ARM_OPTION);

		do {
			tmp = __raw_readl(domain->base + 0x4) & EXYNOS_INT_LOCAL_PWR_EN;
			usleep_range(80, 100);
			timeout--;
		} while ((tmp != power_flag) && timeout);

		if (!timeout) {
			pr_err("PM DOMAIN : ISP WFI unset power down fail(state:%x)\n",
			__raw_readl(domain->base + 0x4));

			tmp = __raw_readl(EXYNOS5420_LPI_BUS_MASK0);
			tmp |= (EXYNOS5420_LPI_BUS_MASK0_ISP0 |
				EXYNOS5420_LPI_BUS_MASK0_ISP1 |
				EXYNOS5420_LPI_BUS_MASK0_RSTOP_ISP |
				EXYNOS5420_LPI_BUS_MASK0_PRSTOP_ISP);
			__raw_writel(tmp, EXYNOS5420_LPI_BUS_MASK0);

			timeout = 100;

			do {
				tmp = __raw_readl(domain->base + 0x4) & EXYNOS_INT_LOCAL_PWR_EN;
					udelay(1);
					timeout--;
				} while ((tmp != power_flag) && timeout);

			if (!timeout) {
				pr_err("CG_STATUS0 : %08X\n", __raw_readl(EXYNOS5420_CG_STATUS0));

				tmp = __raw_readl(EXYNOS5420_LPI_MASK0);
				tmp |= EXYNOS5420_LPI_MASK0_FD;
				__raw_writel(tmp, EXYNOS5420_LPI_MASK0);
				pr_err("FD Disable(LPI : %08X)\n", __raw_readl(EXYNOS5420_LPI_MASK0));

				timeout = 100;
				do {
					tmp = __raw_readl(domain->base + 0x4) & EXYNOS_INT_LOCAL_PWR_EN;
					udelay(1);
					timeout--;
				} while ((tmp != power_flag) && timeout);

				if (!timeout) {
					pr_err("CG_STATUS0 : %08X\n", __raw_readl(EXYNOS5420_CG_STATUS0));

					tmp = __raw_readl(EXYNOS5420_LPI_MASK0);
					tmp |= EXYNOS5420_LPI_MASK0_OTHERS;
					__raw_writel(tmp, EXYNOS5420_LPI_MASK0);
					pr_err("Others Disable(LPI : %08X)\n", __raw_readl(EXYNOS5420_LPI_MASK0));

					timeout = 100;
					do {
						tmp = __raw_readl(domain->base + 0x4) & EXYNOS_INT_LOCAL_PWR_EN;
						udelay(1);
						timeout--;
					} while ((tmp != power_flag) && timeout);

					if (!timeout)
						pr_err("ISP force timeout fail\n");
				}
			} else {
				pr_err("PM DOMAIN : ISP force timeout success\n");
				tmp = __raw_readl(EXYNOS5420_LPI_BUS_MASK0);
				tmp &= ~(EXYNOS5420_LPI_BUS_MASK0_ISP0 |
				EXYNOS5420_LPI_BUS_MASK0_ISP1 |
				EXYNOS5420_LPI_BUS_MASK0_RSTOP_ISP |
				EXYNOS5420_LPI_BUS_MASK0_PRSTOP_ISP);
				__raw_writel(tmp, EXYNOS5420_LPI_BUS_MASK0);
			}
		} else {
			pr_err("PM DOMAIN : ISP WFI unset power down success\n");
		}
	}

	return 0;
}
#endif

#ifdef G3D_PM_NOTIFIER
static int exynos5410_pm_domain_notifier(struct notifier_block *notifier,
			unsigned long pm_event, void *unused)
{
	switch (pm_event) {
	case PM_SUSPEND_PREPARE:
		pm_genpd_poweron(&exynos54xx_pd_g3d.pd);
		break;
	case PM_POST_SUSPEND:
		bts_initialize("pd-eagle", true);
		bts_initialize("pd_kfc", true);
		break;
	}

	return NOTIFY_DONE;
}
#endif

static void exynos5_pd_set_fake_rate(void __iomem *regs, unsigned int shift_val)
{
	unsigned int clk_div2_ratio0_value;
	unsigned int clk_div2_ratio0_old;

	clk_div2_ratio0_value = __raw_readl(regs);
	clk_div2_ratio0_old = (clk_div2_ratio0_value >> shift_val) & 0x3;
	clk_div2_ratio0_value &= ~(0x3 << shift_val);
	clk_div2_ratio0_value |= (((clk_div2_ratio0_old + 1) & 0x3) << shift_val);
	__raw_writel(clk_div2_ratio0_value, regs);

	clk_div2_ratio0_value = __raw_readl(regs);
	clk_div2_ratio0_value &= ~(0x3 << shift_val);
	clk_div2_ratio0_value |= (((clk_div2_ratio0_old) & 0x3) << shift_val);
	__raw_writel(clk_div2_ratio0_value, regs);
}

static int exynos5_pd_disp1_power_on_post(struct exynos_pm_domain *pd)
{
	DEBUG_PRINT_INFO("%s: %08x %08x\n", __func__, __raw_readl(pd->base), __raw_readl(pd->base+4));
	spin_lock(&clk_div2_ratio0_lock);
	exynos5_pd_set_fake_rate(EXYNOS5_CLK_DIV2_RATIO0, SHIFT_DISP1_BLK_DIV);
	spin_unlock(&clk_div2_ratio0_lock);

	clk_set_parent(GET_CLK("sclk_300_disp1"), clk_300_disp1_parent);
	clk_set_parent(GET_CLK("sclk_200_disp1"), clk_200_disp1_parent);
	clk_set_parent(GET_CLK("sclk_fimd1"), clk_fimd1_parent);
	clk_set_parent(GET_CLK("sclk_mdnie1"), clk_mdnie1_parent);
	clk_set_parent(GET_CLK("sclk_dp1_ext_mst_vid"), clk_dp1_ext_mst_vid_parent);
	clk_set_parent(GET_CLK("sclk_mipi1"), clk_mipi1_parent);
	clk_set_parent(GET_CLK("sclk_mdnie_pwm1"), clk_mdnie_pwm1_parent);
	clk_set_parent(GET_CLK("sclk_hdmi"), clk_hdmi_parent);

	return 0;
}

static int exynos5_pd_disp1_power_off_pre(struct exynos_pm_domain *pd)
{
	DEBUG_PRINT_INFO("%s: %08x %08x\n", __func__, __raw_readl(pd->base), __raw_readl(pd->base+4));
	clk_300_disp1_parent = clk_get_parent(GET_CLK("sclk_300_disp1"));
	clk_200_disp1_parent = clk_get_parent(GET_CLK("sclk_200_disp1"));
	clk_fimd1_parent = clk_get_parent(GET_CLK("mout_fimd1"));
	clk_mdnie1_parent = clk_get_parent(GET_CLK("mout_mdnie1"));
	clk_dp1_ext_mst_vid_parent = clk_get_parent(GET_CLK("sclk_dp1_ext_mst_vid"));
	clk_mipi1_parent = clk_get_parent(GET_CLK("sclk_mipi1"));
	clk_mdnie_pwm1_parent = clk_get_parent(GET_CLK("sclk_mdnie_pwm1"));
	clk_hdmi_parent = clk_get_parent(GET_CLK("sclk_hdmi"));

	return 0;
}

static int exynos5_pd_mscl_power_on_post(struct exynos_pm_domain *pd)
{
	DEBUG_PRINT_INFO("%s: %08x %08x\n", __func__, __raw_readl(pd->base), __raw_readl(pd->base+4));
	spin_lock(&clk_div2_ratio0_lock);
	exynos5_pd_set_fake_rate(EXYNOS5_CLK_DIV2_RATIO0, SHIFT_MSCL_BLK_DIV);
	spin_unlock(&clk_div2_ratio0_lock);

	return 0;
}

static int exynos5_pd_gscl_power_on_post(struct exynos_pm_domain *pd)
{
	DEBUG_PRINT_INFO("%s: %08x %08x\n", __func__, __raw_readl(pd->base), __raw_readl(pd->base+4));
	spin_lock(&clk_div2_ratio0_lock);
	exynos5_pd_set_fake_rate(EXYNOS5_CLK_DIV2_RATIO0, SHIFT_GSCL_BLK_300_DIV);
	exynos5_pd_set_fake_rate(EXYNOS5_CLK_DIV2_RATIO0, SHIFT_GSCL_BLK_333_432_DIV);
	spin_unlock(&clk_div2_ratio0_lock);

	return 0;
}

static int exynos5_pd_mfc_power_on_post(struct exynos_pm_domain *pd)
{
	DEBUG_PRINT_INFO("%s: %08x %08x\n", __func__, __raw_readl(pd->base), __raw_readl(pd->base+4));
	/* Do not acquire lock to synchronize for MFC BLK */
	exynos5_pd_set_fake_rate(EXYNOS5_CLK_DIV4_RATIO, SHIFT_MFC_BLK_DIV);

	return 0;
}

static int exynos5_pd_maudio_power_on_pre(struct exynos_pm_domain *pd)
{
	int ret = 0;
	DEBUG_PRINT_INFO("%s: %08x %08x\n", __func__, __raw_readl(pd->base), __raw_readl(pd->base+4));
	/* Turn on EPLL before maudio block on */
	ENA_CLK("sclk_epll", ret);
	udelay(100);
	return 0;
}

static int exynos5_pd_maudio_power_off_post(struct exynos_pm_domain *pd)
{
	DEBUG_PRINT_INFO("%s: %08x %08x\n", __func__, __raw_readl(pd->base), __raw_readl(pd->base+4));
	SET_PARENT("mout_epll", "fim_pll");
	/* Turn off EPLL after maudio block off */
	DIS_CLK("sclk_epll");
	return 0;
}

static struct exynos_pd_callback pd_callback_list[] = {
	{
		.on_pre = exynos5_pd_g3d_power_on_pre,
		.on_post = exynos5_pd_g3d_power_on_post,
		.name = "pd-g3d",
		.off_pre = exynos5_pd_g3d_power_off_pre,
		.off_post = exynos5_pd_g3d_power_off_post,
	} , {
		.on_post = exynos5_pd_mfc_power_on_post,
		.name = "pd-mfc",
	} , {
		.on_pre = exynos5_pd_maudio_power_on_pre,
		.on_post = exynos5_pd_maudio_power_on_post,
		.name = "pd-maudio",
		.off_pre = exynos5_pd_maudio_power_off_pre,
		.off_post = exynos5_pd_maudio_power_off_post,
	} , {
#ifdef CONFIG_FB_MIPI_DSIM
		.on_post = exynos5_pd_disp1_post_power_control,
#else
		.on_post = exynos5_pd_disp1_power_on_post,
#endif
		.name = "pd-disp1",
		.off_pre = exynos5_pd_disp1_power_off_pre,
	} , {
		.on_post = exynos5_pd_gscl_power_on_post,
		.name = "pd-gscl",
	} , {
		.name = "pd-isp",
	} , {
		.name = "pd-csis0",
	} , {
		.name = "pd-csis1",
	} , {
		.name = "pd-csis2",
	} , {
		.name = "pd-flite0",
	} , {
		.name = "pd-flite1",
	} , {
		.name = "pd-flite2",
	} , {
		.name = "pd-fimd1",
	} , {
		.name = "pd-mixer",
	} , {
		.name = "pd-dp",
	} , {
		.name = "pd-dsim1",
	} , {
		.name = "pd-gscaler0",
	} , {
		.name = "pd-gscaler1",
	} , {
		.name = "pd-fimclite",
	} , {
		.name = "pd-csis",
	} , {
		.on_post = exynos5_pd_mscl_power_on_post,
		.name = "pd-mscl",
	} , {
		.name = "pd-mscl0",
	} , {
		.name = "pd-mscl1",
	} , {
		.name = "pd-mscl2",
	} , {
		.name = "pd-fsys",
	} , {
		.name = "pd-usbdrd30",
	} , {
		.name = "pd-pdma",
	} , {
		.name = "pd-usbhost20",
	} , {
		.name = "pd-fsys2",
	} , {
		.name = "pd-mmc",
	} , {
		.name = "pd-sromc",
	} , {
		.name = "pd-ufs",
	} , {
		.name = "pd-psgen",
	} , {
		.name = "pd-psgengen",
	} , {
		.name = "pd-rotator",
	} , {
		.name = "pd-mdma",
	} , {
		.name = "pd-jpeg",
	} , {
		.name = "pd-psgenperis",
	} , {
		.name = "pd-sysreg",
	} , {
		.name = "pd-tzpc",
	} , {
		.name = "pd-tmu",
	} , {
		.name = "pd-seckey",
	} , {
		.name = "pd-peric",
	},
};

struct exynos_pd_callback * exynos_pd_find_callback(struct exynos_pm_domain *pd)
{
	struct exynos_pd_callback *cb = NULL;
	int i;
#ifdef CONFIG_FB_MIPI_DSIM
	hdmi_regs = ioremap(0x14530000, SZ_32);
	if (IS_ERR_OR_NULL(hdmi_regs))
		pr_err("PM DOMAIN : can't remap of hdmi phy address\n");
#endif

	spin_lock_init(&clk_div2_ratio0_lock);
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

#endif
