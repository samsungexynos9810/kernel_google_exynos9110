/*
 * Copyright (C) 2013 Samsung Electronics Co., Ltd.
 *
 * EXYNOS5 - Helper functions for MIPI-CSIS control
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/export.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/spinlock.h>
#include <mach/regs-clock.h>

#define S5P_CAM0_MIPI0_DPHY_RESET			(1 << 0)
#define S5P_CAM0_MIPI1_DPHY_RESET			(1 << 1)
#define S5P_CAM1_MIPI2_DPHY_RESET			(1 << 0)

static int __exynos5_mipi_phy_control(int id, bool on, u32 reset)
{
	static DEFINE_SPINLOCK(lock);
	void __iomem *addr;
	unsigned long flags;
	u32 cfg;

	addr = S5P_MIPI_DPHY_CONTROL(id);

	spin_lock_irqsave(&lock, flags);

	/* PHY reset */
	switch(id) {
	case 0:
		cfg = __raw_readl(S5P_VA_SYSREG_CAM0 + 0x1014);
		cfg |= S5P_CAM0_MIPI0_DPHY_RESET;
		__raw_writel(cfg, S5P_VA_SYSREG_CAM0 + 0x1014);
		break;
	case 1:
		cfg = __raw_readl(S5P_VA_SYSREG_CAM0 + 0x1014);
		cfg |= S5P_CAM0_MIPI1_DPHY_RESET;
		__raw_writel(cfg, S5P_VA_SYSREG_CAM0 + 0x1014);
		break;
	case 2:
		cfg = __raw_readl(S5P_VA_SYSREG_CAM1 + 0x1020);
		cfg |= S5P_CAM1_MIPI2_DPHY_RESET;
		__raw_writel(cfg, S5P_VA_SYSREG_CAM1 + 0x1020);
		break;
	default:
		pr_err("id(%d) is invalid", id);
		return -EINVAL;
	}

	/* PHY PMU enable */
	cfg = __raw_readl(addr);

	if (on)
		cfg |= S5P_MIPI_DPHY_ENABLE;
	else
		cfg &= ~S5P_MIPI_DPHY_ENABLE;

	__raw_writel(cfg, addr);
	spin_unlock_irqrestore(&lock, flags);

	return 0;
}

int exynos5_csis_phy_enable(int id, bool on)
{
	return __exynos5_mipi_phy_control(id, on, S5P_MIPI_DPHY_SRESETN);
}
EXPORT_SYMBOL(exynos5_csis_phy_enable);
