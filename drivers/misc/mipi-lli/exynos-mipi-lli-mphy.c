/*
 * Exynos MIPI-LLI-MPHY driver
 *
 * Copyright (C) 2013 Samsung Electronics Co.Ltd
 * Author: Yulgon Kim <yulgon.kim@samsung.com>
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */

#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>

#include "exynos-mipi-lli-mphy.h"

struct device *lli_mphy;

int exynos_mphy_init(struct exynos_mphy *phy)
{
	writel(0x45, phy->loc_regs + PHY_TX_HS_SYNC_LENGTH(0));
	/* if TX_LCC is disable, M-TX should enter SLEEP or STALL state
	based on the current value of the TX_MODE upon getting a TOB REQ */
	writel(0x0, phy->loc_regs + PHY_TX_LCC_ENABLE(0));

	return 0;
}

int exynos_mphy_cmn_init(struct exynos_mphy *phy)
{
	if (phy->is_shared_clk)
		writel(0x00, phy->loc_regs + (0x4f*4));
	else
		writel(0xF9, phy->loc_regs + (0x4f*4));

	/* PLL power off */
	writel(0x0, phy->loc_regs + (0x1A*4));

	return 0;
}

int exynos_mphy_ovtm_init(struct exynos_mphy *phy)
{
	if (!phy->is_shared_clk)
		writel(0, phy->loc_regs + (0x20*4));
	else
		writel(1, phy->loc_regs + (0x20*4));

	writel(0x02, phy->loc_regs + (0x16*4));
	writel(0x8F, phy->loc_regs + (0x45*4));
	/* For PWM3,4,5G */
	writel(0x1A, phy->loc_regs + (0x40*4));

	/* Reset-On-Error REQ timing configuration */
	writel(0x30, phy->loc_regs + (0x5c*4));
	writel(0x1D, phy->loc_regs + (0x77*4));

	return 0;
}

static int exynos_mphy_shutdown(struct exynos_mphy *phy)
{
	return 0;
}

static int exynos_mipi_lli_mphy_probe(struct platform_device *pdev)
{
	struct exynos_mphy *mphy;
	struct device *dev = &pdev->dev;
	struct resource *res;
	void __iomem *regs;
	int ret = 0;

	mphy = devm_kzalloc(dev, sizeof(struct exynos_mphy), GFP_KERNEL);
	if (!mphy) {
		dev_err(dev, "not enough memory\n");
		return -ENOMEM;
	}

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		dev_err(dev, "cannot find register resource 0\n");
		return -ENXIO;
	}

	regs = devm_request_and_ioremap(dev, res);
	if (!regs) {
		dev_err(dev, "cannot request_and_map registers\n");
		return -EADDRNOTAVAIL;
	}

	spin_lock_init(&mphy->lock);
	mphy->dev = dev;
	mphy->loc_regs = regs;
	mphy->init = exynos_mphy_init;
	mphy->cmn_init = exynos_mphy_cmn_init;
	mphy->ovtm_init = exynos_mphy_ovtm_init;
	mphy->shutdown = exynos_mphy_shutdown;

	mphy->lane = 1;
	mphy->default_mode = PWM_G1;
	mphy->is_shared_clk = true;

	platform_set_drvdata(pdev, mphy);
	lli_mphy = dev;

	return ret;
}

static int exynos_mipi_lli_mphy_remove(struct platform_device *pdev)
{
	return 0;
}

struct device *exynos_get_mphy(void)
{
	return lli_mphy;
}
EXPORT_SYMBOL(exynos_get_mphy);

#ifdef CONFIG_OF
static const struct of_device_id exynos_mphy_dt_match[] = {
	{
		.compatible = "samsung,exynos-mipi-lli-mphy"
	},
	{},
};
MODULE_DEVICE_TABLE(of, exynos_mphy_dt_match);
#endif

static struct platform_driver exynos_mipi_lli_mphy_driver = {
	.probe		= exynos_mipi_lli_mphy_probe,
	.remove		= exynos_mipi_lli_mphy_remove,
	.driver		= {
		.name	= "exynos-mipi-lli-mphy",
		.owner	= THIS_MODULE,
		.of_match_table = of_match_ptr(exynos_mphy_dt_match),
	},
};

module_platform_driver(exynos_mipi_lli_mphy_driver);

MODULE_DESCRIPTION("Exynos MIPI-LLI MPHY driver");
MODULE_AUTHOR("Yulgon Kim <yulgon.kim@samsung.com>");
MODULE_LICENSE("GPL");
