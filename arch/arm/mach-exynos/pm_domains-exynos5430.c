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

#define PM_DOMAIN_PREFIX	"PM DOMAIN: "

#ifndef pr_fmt
#define pr_fmt(fmt) fmt
#endif

#ifdef PM_DOMAIN_DEBUG
#define DEBUG_PRINT_INFO(fmt, ...) printk(PM_DOMAIN_PREFIX pr_fmt(fmt), ##__VA_ARGS__)
#else
#define DEBUG_PRINT_INFO(fmt, ...)
#endif

#ifdef CONFIG_SOC_EXYNOS5430
/* exynos_pd_maudio_power_on_post - callback after power on.
 * @pd: power domain.
 *
 * release pad retention for maudio.
 */
static int exynos_pd_maudio_power_on_post(struct exynos_pm_domain *pd)
{
	unsigned int reg;

	reg = (1<<28);
	__raw_writel(reg, EXYNOS_PAD_RET_MAUDIO_OPTION);

	return 0;
}

static int exynos_pd_g3d_power_on_post(struct exynos_pm_domain *pd)
{
	DEBUG_PRINT_INFO("EXYNOS5430_DIV_G3D: %08x\n", __raw_readl(EXYNOS5430_DIV_G3D));
	DEBUG_PRINT_INFO("EXYNOS5430_SRC_SEL_G3D: %08x\n", __raw_readl(EXYNOS5430_SRC_SEL_G3D));

	return 0;
}

/* exynos_pd_mfc0_power_on_pre - setup before power on.
 * @pd: power domain.
 *
 * enable top clock.
 */
static int exynos_pd_mfc0_power_on_pre(struct exynos_pm_domain *pd)
{
	unsigned int reg;

	reg = __raw_readl(EXYNOS5430_ENABLE_IP_TOP);
	reg |= (1<<1);
	__raw_writel(reg, EXYNOS5430_ENABLE_IP_TOP);

	return 0;
}

/* exynos_pd_mfc0_power_off_post - clean up after power off.
 * @pd: power domain.
 *
 * disable top clock.
 */
static int exynos_pd_mfc0_power_off_post(struct exynos_pm_domain *pd)
{
	unsigned int reg;

	reg = __raw_readl(EXYNOS5430_ENABLE_IP_TOP);
	reg &= ~(1<<1);
	__raw_writel(reg, EXYNOS5430_ENABLE_IP_TOP);

	return 0;
}

/* exynos_pd_mfc1_power_on_pre - setup before power on.
 * @pd: power domain.
 *
 * enable top clock.
 */
static int exynos_pd_mfc1_power_on_pre(struct exynos_pm_domain *pd)
{
	unsigned int reg;

	reg = __raw_readl(EXYNOS5430_ENABLE_IP_TOP);
	reg |= (1<<2);
	__raw_writel(reg, EXYNOS5430_ENABLE_IP_TOP);

	return 0;
}

/* exynos_pd_mfc1_power_off_post - clean up after power off.
 * @pd: power domain.
 *
 * disable top clock.
 */
static int exynos_pd_mfc1_power_off_post(struct exynos_pm_domain *pd)
{
	unsigned int reg;

	reg = __raw_readl(EXYNOS5430_ENABLE_IP_TOP);
	reg &= ~(1<<2);
	__raw_writel(reg, EXYNOS5430_ENABLE_IP_TOP);

	return 0;
}

/* exynos_pd_hevc_power_on_pre - setup before power on.
 * @pd: power domain.
 *
 * enable top clock.
 */
static int exynos_pd_hevc_power_on_pre(struct exynos_pm_domain *pd)
{
	unsigned int reg;

	reg = __raw_readl(EXYNOS5430_ENABLE_IP_TOP);
	reg |= (1<<3);
	__raw_writel(reg, EXYNOS5430_ENABLE_IP_TOP);

	return 0;
}

/* exynos_pd_hevc_power_off_post - clean up after power off.
 * @pd: power domain.
 *
 * disable top clock.
 */
static int exynos_pd_hevc_power_off_post(struct exynos_pm_domain *pd)
{
	unsigned int reg;

	reg = __raw_readl(EXYNOS5430_ENABLE_IP_TOP);
	reg &= ~(1<<3);
	__raw_writel(reg, EXYNOS5430_ENABLE_IP_TOP);

	return 0;
}

/* exynos_pd_gscl_power_on_pre - setup before power on.
 * @pd: power domain.
 *
 * enable top clock.
 */
static int exynos_pd_gscl_power_on_pre(struct exynos_pm_domain *pd)
{
	unsigned int reg;

	reg = __raw_readl(EXYNOS5430_ENABLE_IP_TOP);
	reg |= (1<<7);
	__raw_writel(reg, EXYNOS5430_ENABLE_IP_TOP);

	return 0;
}

/* exynos_pd_gscl_power_off_post - clean up after power off.
 * @pd: power domain.
 *
 * disable top clock.
 */
static int exynos_pd_gscl_power_off_post(struct exynos_pm_domain *pd)
{
	unsigned int reg;

	reg = __raw_readl(EXYNOS5430_ENABLE_IP_TOP);
	reg &= ~(1<<7);
	__raw_writel(reg, EXYNOS5430_ENABLE_IP_TOP);

	return 0;
}

/* exynos_pd_disp_power_on_pre - setup before power on.
 * @pd: power domain.
 *
 * enable IP_MIF3 clk.
 */
static int exynos_pd_disp_power_on_pre(struct exynos_pm_domain *pd)
{
	unsigned int reg;

	reg = __raw_readl(EXYNOS5430_ENABLE_IP_MIF3);
	reg |= (1<<7 | 1<<6 | 1<<5 | 1<<2 | 1<<1);
	__raw_writel(reg, EXYNOS5430_ENABLE_IP_MIF3);

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
	struct device_node *np;
	void __iomem *decon_addr;

	DEBUG_PRINT_INFO("disp pre power off\n");
	reg = __raw_readl(EXYNOS5430_ENABLE_IP_MIF3);
	reg |= (1<<7 | 1<<6 | 1<<5 | 1<<2 | 1<<1);
	__raw_writel(reg, EXYNOS5430_ENABLE_IP_MIF3);

	np = of_find_node_by_name(NULL, "decon_fb");
	decon_addr = of_iomap(np, 0);

	if (unlikely(__raw_readl(decon_addr) & (1<<2)))
		pr_err(PM_DOMAIN_PREFIX "decon_fb is not reset.\n");
	else
		DEBUG_PRINT_INFO("decon con0: %08x\n", __raw_readl(decon_addr));
	iounmap(decon_addr);
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
	reg &= ~(1<<7 | 1<<6 | 1<<5 | 1<<2 | 1<<1);
	__raw_writel(reg, EXYNOS5430_ENABLE_IP_MIF3);

	__raw_writel(0x0, EXYNOS5430_SRC_SEL_MIF4);
	__raw_writel(0x0, EXYNOS5430_SRC_SEL_MIF5);

	return 0;
}

/* exynos_pd_mscl_power_on_pre - setup before power on.
 * @pd: power domain.
 *
 * enable top clock.
 */
static int exynos_pd_mscl_power_on_pre(struct exynos_pm_domain *pd)
{
	unsigned int reg;

	DEBUG_PRINT_INFO("%s is preparing power-on sequence.\n", pd->name);
	reg = __raw_readl(EXYNOS5430_ENABLE_IP_TOP);
	reg |= (1<<10);
	__raw_writel(reg, EXYNOS5430_ENABLE_IP_TOP);

	return 0;
}

/* exynos_pd_mscl_power_off_post - clean up after power off.
 * @pd: power domain.
 *
 * disable top clock.
 */
static int exynos_pd_mscl_power_off_post(struct exynos_pm_domain *pd)
{
	unsigned int reg;

	DEBUG_PRINT_INFO("%s is clearing power-off sequence.\n", pd->name);
	reg = __raw_readl(EXYNOS5430_ENABLE_IP_TOP);
	reg &= ~(1<<10);
	__raw_writel(reg, EXYNOS5430_ENABLE_IP_TOP);

	return 0;
}

/* exynos_pd_g2d_power_on_pre - setup before power on.
 * @pd: power domain.
 *
 * enable top clock.
 */
static int exynos_pd_g2d_power_on_pre(struct exynos_pm_domain *pd)
{
	unsigned int reg;

	DEBUG_PRINT_INFO("%s is preparing power-on sequence.\n", pd->name);
	reg = __raw_readl(EXYNOS5430_ENABLE_IP_TOP);
	reg |= (1<<0);
	__raw_writel(reg, EXYNOS5430_ENABLE_IP_TOP);

	return 0;
}

/* exynos_pd_g2d_power_off_post - clean up after power off.
 * @pd: power domain.
 *
 * disable top clock.
 */
static int exynos_pd_g2d_power_off_post(struct exynos_pm_domain *pd)
{
	unsigned int reg;

	DEBUG_PRINT_INFO("%s is clearing power-off sequence.\n", pd->name);
	reg = __raw_readl(EXYNOS5430_ENABLE_IP_TOP);
	reg &= ~(1<<0);
	__raw_writel(reg, EXYNOS5430_ENABLE_IP_TOP);

	return 0;
}

/* exynos_pd_isp_power_on_pre - setup before power on.
 * @pd: power domain.
 *
 * enable top clock.
 */
static int exynos_pd_isp_power_on_pre(struct exynos_pm_domain *pd)
{
	unsigned int reg;

	DEBUG_PRINT_INFO("%s is preparing power-on sequence.\n", pd->name);
	reg = __raw_readl(EXYNOS5430_ENABLE_IP_TOP);
	reg |= (1<<4);
	__raw_writel(reg, EXYNOS5430_ENABLE_IP_TOP);

	return 0;
}

/* exynos_pd_isp_power_off_post - clean up after power off.
 * @pd: power domain.
 *
 * disable top clock.
 */
static int exynos_pd_isp_power_off_post(struct exynos_pm_domain *pd)
{
	unsigned int reg;

	DEBUG_PRINT_INFO("%s is clearing power-off sequence.\n", pd->name);
	reg = __raw_readl(EXYNOS5430_ENABLE_IP_TOP);
	reg &= ~(1<<4);
	__raw_writel(reg, EXYNOS5430_ENABLE_IP_TOP);

	return 0;
}

/* exynos_pd_cam0_power_on_pre - setup before power on.
 * @pd: power domain.
 *
 * enable top clock.
 */
static int exynos_pd_cam0_power_on_pre(struct exynos_pm_domain *pd)
{
	unsigned int reg;

	DEBUG_PRINT_INFO("%s is preparing power-on sequence.\n", pd->name);
	reg = __raw_readl(EXYNOS5430_ENABLE_IP_TOP);
	reg |= (1<<5);
	__raw_writel(reg, EXYNOS5430_ENABLE_IP_TOP);

	return 0;
}

/* exynos_pd_cam0_power_off_post - clean up after power off.
 * @pd: power domain.
 *
 * disable top clock.
 */
static int exynos_pd_cam0_power_off_post(struct exynos_pm_domain *pd)
{
	unsigned int reg;

	DEBUG_PRINT_INFO("%s is clearing power-off sequence.\n", pd->name);
	reg = __raw_readl(EXYNOS5430_ENABLE_IP_TOP);
	reg &= ~(1<<5);
	__raw_writel(reg, EXYNOS5430_ENABLE_IP_TOP);

	return 0;
}

/* exynos_pd_cam1_power_on_pre - setup before power on.
 * @pd: power domain.
 *
 * enable top clock.
 */
static int exynos_pd_cam1_power_on_pre(struct exynos_pm_domain *pd)
{
	unsigned int reg;

	DEBUG_PRINT_INFO("%s is preparing power-on sequence.\n", pd->name);
	reg = __raw_readl(EXYNOS5430_ENABLE_IP_TOP);
	reg |= (1<<6);
	__raw_writel(reg, EXYNOS5430_ENABLE_IP_TOP);

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
	reg = __raw_readl(EXYNOS5430_ENABLE_IP_TOP);
	reg &= ~(1<<6);
	__raw_writel(reg, EXYNOS5430_ENABLE_IP_TOP);

	return 0;
}

static struct exynos_pd_callback pd_callback_list[] = {
	{
		.name = "pd-maudio",
		.on_post = exynos_pd_maudio_power_on_post,
	}, {
		.name = "pd-mfc0",
		.on_pre = exynos_pd_mfc0_power_on_pre,
		.off_post = exynos_pd_mfc0_power_off_post,
	}, {
		.name = "pd-mfc1",
		.on_pre = exynos_pd_mfc1_power_on_pre,
		.off_post = exynos_pd_mfc1_power_off_post,
	}, {
		.name = "pd-hevc",
		.on_pre = exynos_pd_hevc_power_on_pre,
		.off_post = exynos_pd_hevc_power_off_post,
	}, {
		.name = "pd-gscl",
		.on_pre = exynos_pd_gscl_power_on_pre,
		.off_post = exynos_pd_gscl_power_off_post,
	}, {
		.name = "pd-g3d",
		.on_post = exynos_pd_g3d_power_on_post,
	}, {
		.name = "pd-disp",
		.on_pre = exynos_pd_disp_power_on_pre,
		.off_pre = exynos_pd_disp_power_off_pre,
		.off_post = exynos_pd_disp_power_off_post,
	}, {
		.name = "pd-mscl",
		.on_pre = exynos_pd_mscl_power_on_pre,
		.off_post = exynos_pd_mscl_power_off_post,
	}, {
		.name = "pd-g2d",
		.on_pre = exynos_pd_g2d_power_on_pre,
		.off_post = exynos_pd_g2d_power_off_post,
	}, {
		.name = "pd-isp",
		.on_pre = exynos_pd_isp_power_on_pre,
		.off_post = exynos_pd_isp_power_off_post,
	}, {
		.name = "pd-cam0",
		.on_pre = exynos_pd_cam0_power_on_pre,
		.off_post = exynos_pd_cam0_power_off_post,
	}, {
		.name = "pd-cam1",
		.on_pre = exynos_pd_cam1_power_on_pre,
		.off_post = exynos_pd_cam1_power_off_post,
	},
};

struct exynos_pd_callback * exynos_pd_find_callback(struct exynos_pm_domain *pd)
{
	struct exynos_pd_callback *cb = NULL;
	int i;

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
