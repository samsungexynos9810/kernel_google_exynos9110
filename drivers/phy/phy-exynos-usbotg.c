/*
 * Samsung EXYNOS SoC series USB 2.0 PHY driver
 *
 * Phy provider for S3C OTG, EHCI-S5P and OHCI-EXYNOS controllers.
 *
 * Copyright (C) 2016 Samsung Electronics Co., Ltd.
 * Author: Minho Lee <minho55.lee@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/mfd/syscon.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/phy/phy.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include <linux/regulator/consumer.h>
#include <linux/spinlock.h>
#include <linux/usb/samsung_usb_phy.h>

#include "phy-exynos-usbotg.h"

bool exynos_usb2_phy_is_on;

/******************************************************************************
 *
 * Static functions
 *
 *****************************************************************************/
static int exynos_usb2_phy_get_clk(struct exynos_usb2_phy *phy_usb2)
{
	phy_usb2->ref_clk = devm_clk_get(phy_usb2->dev, "refclk");
	if (IS_ERR_OR_NULL(phy_usb2->ref_clk)) {
		dev_err(phy_usb2->dev, "failed to get refclk\n");
		return -EINVAL;
	}

	phy_usb2->bus_clk = devm_clk_get(phy_usb2->dev, "busclk");
	if (IS_ERR_OR_NULL(phy_usb2->bus_clk)) {
		dev_err(phy_usb2->dev, "failed to get busclk\n");
		return -EINVAL;
	}

	phy_log(phy_usb2, "%s get refclk & busclk successfully\n", __func__);

	return 0;
}

static int exynos_usb2_phy_get_info(struct exynos_usb2_phy *phy_usb2)
{
	struct exynos_usb2_phy_info *phy_info = &phy_usb2->phy_info;
	struct device_node *node = phy_usb2->dev->of_node;
	unsigned long ref_clk_freq;
	int ret;

	ret = of_property_read_u32(node, "pmu-offset", &phy_info->pmu_offset);
	if (ret) {
		dev_err(phy_usb2->dev, "failed to get pmu offset\n");
		return ret;
	}

	ret = of_property_read_u32(node, "pmu-mask", &phy_info->pmu_mask);
	if (ret) {
		dev_err(phy_usb2->dev, "failed to get pmu mask\n");
		return ret;
	}

	ret = clk_prepare_enable(phy_usb2->ref_clk);
	if (ret) {
		dev_err(phy_usb2->dev, "failed to enable refclk\n");
		return ret;
	}

	ref_clk_freq = clk_get_rate(phy_usb2->ref_clk);
	switch (ref_clk_freq) {
	case 9600 * KHZ:
		phy_usb2->phy_fsel = EXYNOS_FSEL_9MHZ6;
		break;
	case 10 * MHZ:
		phy_usb2->phy_fsel = EXYNOS_FSEL_10MHZ;
		break;
	case 12 * MHZ:
		phy_usb2->phy_fsel = EXYNOS_FSEL_12MHZ;
		break;
	case 19200 * KHZ:
		phy_usb2->phy_fsel = EXYNOS_FSEL_19MHZ2;
		break;
	case 20 * MHZ:
		phy_usb2->phy_fsel = EXYNOS_FSEL_20MHZ;
		break;
	case 24 * MHZ:
		phy_usb2->phy_fsel = EXYNOS_FSEL_24MHZ;
		break;
	case 50 * MHZ:
		phy_usb2->phy_fsel = EXYNOS_FSEL_50MHZ;
		break;
	default:
		dev_err(phy_usb2->dev, "reference clock is wrong %d\n",
				(unsigned int)(ref_clk_freq / (100 * KHZ)));
		ret = -EINVAL;
		break;
	}

	clk_disable_unprepare(phy_usb2->ref_clk);

	dev_info(phy_usb2->dev,
		"phy info: pmu[offset:0x%04x, mask:0x%04x], refclk[%d]\n",
		phy_info->pmu_offset, phy_info->pmu_mask,
		phy_usb2->phy_fsel);

	return 0;
}

/******************************************************************************
 *
 * PHY operations
 *
 *****************************************************************************/
static int exynos_usb2_phy_init(struct phy *phy)
{
	struct exynos_usb2_phy *phy_usb2 = phy_get_drvdata(phy);
	unsigned long flags;
	unsigned int phypwr, phyclk, rstcon;
	int ret;

	phy_log(phy_usb2, "%s\n", __func__);

	ret = clk_enable(phy_usb2->bus_clk);
	if (ret) {
		dev_err(phy_usb2->dev,
				"%s: failed to enable bus_clk\n", __func__);
		return ret;
	}

	phy_log(phy_usb2, "%s: bus clock rate: %dMHz\n", __func__,
		(unsigned int)(clk_get_rate(phy_usb2->bus_clk) / MHZ));

	spin_lock_irqsave(&phy_usb2->lock, flags);

	phyclk = UPHYCLK_PHY_FSEL(phy_usb2->phy_fsel);
	phyclk &= ~(UPHYCLK_PHY_0_COMMON_ON_N | UPHYCLK_PHY_1_COMMON_ON_N);
	phyclk |= UPHYCLK_REFCLKSEL(0x2);
	writel(phyclk, phy_usb2->reg_base + EXYNOS_USB2PHY_UPHYCLK);

	phy_log(phy_usb2, "%s: phyclk register: 0x%08x\n", __func__, phyclk);

	phypwr = readl(phy_usb2->reg_base + EXYNOS_USB2PHY_UPHYPWR);
	phypwr &= ~UPHYPWR_PHY0_MASK;
	phypwr |= (UPHYPWR_PHY1_MASK | UPHYPWR_HSIC_MASK);
	writel(phypwr, phy_usb2->reg_base + EXYNOS_USB2PHY_UPHYPWR);

	phy_log(phy_usb2, "%s: phypwr register: 0x%08x\n", __func__, phypwr);

	rstcon = readl(phy_usb2->reg_base + EXYNOS_USB2PHY_URSTCON);
	rstcon |= URSTCON_PHY_0_SWRST_MASK;
	writel(rstcon, phy_usb2->reg_base + EXYNOS_USB2PHY_URSTCON);

	udelay(10);

	rstcon &= ~URSTCON_PHY_0_SWRST_MASK;
	writel(rstcon, phy_usb2->reg_base + EXYNOS_USB2PHY_URSTCON);

	udelay(80);

	phy_log(phy_usb2, "%s: rstcon register: 0x%08x\n", __func__, rstcon);

	spin_unlock_irqrestore(&phy_usb2->lock, flags);

	dev_info(phy_usb2->dev, "Exynos USB2.0 PHY was initialized\n");

	return ret;
}

static void exynos_usb2_phy_disable(struct exynos_usb2_phy *phy_usb2)
{
	unsigned long flags;
	unsigned int phypwr;

	phy_log(phy_usb2, "%s +++\n", __func__);

	spin_lock_irqsave(&phy_usb2->lock, flags);

	phypwr = readl(phy_usb2->reg_base + EXYNOS_USB2PHY_UPHYPWR);
	phypwr |= UPHYPWR_PHY0_MASK;
	writel(phypwr, phy_usb2->reg_base + EXYNOS_USB2PHY_UPHYPWR);

	phy_log(phy_usb2, "%s: phypwr register: 0x%08x\n", __func__, phypwr);

	spin_unlock_irqrestore(&phy_usb2->lock, flags);

	phy_log(phy_usb2, "%s ---\n", __func__);
}

static int exynos_usb2_phy_exit(struct phy *phy)
{
	struct exynos_usb2_phy *phy_usb2 = phy_get_drvdata(phy);

	exynos_usb2_phy_disable(phy_usb2);

	clk_disable(phy_usb2->bus_clk);

	dev_info(phy_usb2->dev, "Exynos USB2.0 PHY was exited\n");

	return 0;
}

static int exynos_usb2_phy_power_on(struct phy *phy)
{
	struct exynos_usb2_phy *phy_usb2 = phy_get_drvdata(phy);

	phy_log(phy_usb2, "%s\n", __func__);

	exynos_usb2_phy_is_on = true;

	regmap_update_bits(phy_usb2->pmu_base, phy_usb2->phy_info.pmu_offset,
		phy_usb2->phy_info.pmu_mask, phy_usb2->phy_info.pmu_mask);

	return 0;
}

static int exynos_usb2_phy_power_off(struct phy *phy)
{
	struct exynos_usb2_phy *phy_usb2 = phy_get_drvdata(phy);

	phy_log(phy_usb2, "%s\n", __func__);

	regmap_update_bits(phy_usb2->pmu_base, phy_usb2->phy_info.pmu_offset,
		phy_usb2->phy_info.pmu_mask, 0);

	exynos_usb2_phy_is_on = false;

	return 0;
}

static struct phy *exynos_usb2_phy_xlate(struct device *dev,
					struct of_phandle_args *args)
{
	struct exynos_usb2_phy *phy_usb2 = dev_get_drvdata(dev);

	return phy_usb2->phy;
}

/******************************************************************************
 *
 * Structures
 *
 *****************************************************************************/
static struct phy_ops exynos_usb2_phy_ops = {
	.init		= exynos_usb2_phy_init,
	.exit		= exynos_usb2_phy_exit,
	.power_on	= exynos_usb2_phy_power_on,
	.power_off	= exynos_usb2_phy_power_off,
	.owner		= THIS_MODULE,
};

static const struct of_device_id exynos_usb2_phy_of_match[] = {
	{
		.compatible = "samsung,exynos-usb2-phy",
	},
	{ },
};
MODULE_DEVICE_TABLE(of, exynos_usb2_phy_of_match);

/******************************************************************************
 *
 * Probe function
 *
 *****************************************************************************/
static int exynos_usb2_phy_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *node = dev->of_node;
	struct exynos_usb2_phy *phy_usb2;
	struct phy_provider *phy_provider;
	struct resource *res;
	int ret;

	dev_info(dev, "%s\n", __func__);

	phy_usb2 = devm_kzalloc(dev, sizeof(*phy_usb2), GFP_KERNEL);
	if (!phy_usb2)
		return -ENOMEM;

	dev_set_drvdata(dev, phy_usb2);
	phy_usb2->dev = dev;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	phy_usb2->reg_base = devm_ioremap_resource(dev, res);
	if (IS_ERR(phy_usb2->reg_base)) {
		dev_err(dev, "failed to remap registers for USB2.0 PHY\n");
		return PTR_ERR(phy_usb2->reg_base);
	}

	phy_usb2->pmu_base = syscon_regmap_lookup_by_phandle(node,
							"samsung,pmu-syscon");
	if (IS_ERR(phy_usb2->pmu_base)) {
		dev_err(dev, "failed to remap PMU register\n");
		return PTR_ERR(phy_usb2->pmu_base);
	}

	ret = exynos_usb2_phy_get_clk(phy_usb2);
	if (ret)
		return ret;

	ret = exynos_usb2_phy_get_info(phy_usb2);
	if (ret)
		return ret;

	spin_lock_init(&phy_usb2->lock);

	phy_usb2->phy = devm_phy_create(dev, NULL, &exynos_usb2_phy_ops, NULL);
	if (IS_ERR(phy_usb2->phy)) {
		dev_err(dev, "failed to create phy\n");
		return PTR_ERR(phy_usb2->phy);
	}
	phy_set_drvdata(phy_usb2->phy, phy_usb2);

	phy_provider = devm_of_phy_provider_register(dev,
							exynos_usb2_phy_xlate);
	if (IS_ERR(phy_provider)) {
		dev_err(dev, "failed to register phy provider\n");
		return PTR_ERR(phy_provider);
	}

	ret = clk_prepare(phy_usb2->bus_clk);
	if (ret) {
		dev_err(dev, "failed to prepare bus clock\n");
		return ret;
	}

	dev_info(dev, "End of %s\n", __func__);

	return 0;
}

/******************************************************************************
 *
 * Power Management
 *
 *****************************************************************************/
#ifdef CONFIG_PM_SLEEP
static int exynos_usb2_phy_resume(struct device *dev)
{
	int ret;
	struct exynos_usb2_phy *phy_usb2 = dev_get_drvdata(dev);

	phy_log(phy_usb2, "%s +++\n", __func__);

	ret = clk_enable(phy_usb2->bus_clk);
	if (ret) {
		dev_err(dev, "%s: Failed to enable bus clk\n", __func__);
		return ret;
	}

	exynos_usb2_phy_disable(phy_usb2);

	clk_disable(phy_usb2->bus_clk);

	phy_log(phy_usb2, "%s ---\n", __func__);

	return 0;
}

static const struct dev_pm_ops exynos_usb2_phy_dev_pm_ops = {
	.resume = exynos_usb2_phy_resume,
};
#endif

/******************************************************************************
 *
 * platform_driver
 *
 *****************************************************************************/
static struct platform_driver phy_exynos_usb2 = {
	.probe	= exynos_usb2_phy_probe,
	.driver = {
		.of_match_table	= exynos_usb2_phy_of_match,
		.name		= "phy_exynos_usb2",
#ifdef CONFIG_PM_SLEEP
		.pm		= &(exynos_usb2_phy_dev_pm_ops),
#endif
	}
};

module_platform_driver(phy_exynos_usb2);
MODULE_DESCRIPTION("Samsung EXYNOS SoCs USB 2.0 PHY driver");
MODULE_AUTHOR("Minho Lee <minho55.lee@samsung.com>");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:phy_exynos_usb2");
