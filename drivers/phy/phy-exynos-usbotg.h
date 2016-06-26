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

#ifndef __PHY_EXYNOS_USB2_H__
#define __PHY_EXYNOS_USB2_H__

#include <linux/clk.h>
#include <linux/spinlock.h>
#include <linux/phy/phy.h>
#include <linux/platform_device.h>

/*******************************************************************************
 *
 * Register definitions
 *
 ******************************************************************************/
#define EXYNOS_USB2PHY_UPHYPWR		(0x00)
#define UPHYPWR_PHY0_MASK		(0x39 << 0)
#define UPHYPWR_PHY0_FORCE_SUSPEND	(0x1 << 0)
#define UPHYPWR_PHY0_ANALOG_POWERDOWN	(0x1 << 3)
#define UPHYPWR_PHY0_OTG_DISABLE	(0x1 << 4)
#define UPHYPWR_PHY0_SLEEP		(0x1 << 5)
#define UPHYPWR_PHY1_MASK		(0x7 << 6)
#define UPHYPWR_HSIC_MASK		(0xF << 9)

#define EXYNOS_USB2PHY_UPHYCLK		(0x04)
#define UPHYCLK_PHY_FSEL(val)		((val & 0x7) << 0)
#define UPHYCLK_PHY_0_ID_PULLUP0	(0x1 << 3)
#define UPHYCLK_PHY_0_COMMON_ON_N	(0x1 << 4)
#define UPHYCLK_PHY_1_COMMON_ON_N	(0x1 << 7)
#define UPHYCLK_REFCLKSEL(val)		((val & 0x3) << 8)

#define EXYNOS_USB2PHY_URSTCON		(0x08)
#define URSTCON_PHY_0_SWRST_MASK	(0x7 << 0)
#define URSTCON_PHY_0_SW_RST		(0x1 << 0)
#define URSTCON_OTG_HLINK_SW_RST	(0x1 << 1)
#define URSTCON_OTG_PHYLINK_SW_RST	(0x1 << 2)
#define URSTCON_PHY_1_SWRST_MASK	(0xF << 3)
#define URSTCON_HOST_LINK_SWRST_MASK	(0xF << 7)

/*******************************************************************************
 *
 * Definitions
 *
 ******************************************************************************/
/* Debug Log */
#undef PHY_EXYNOS_USB2_LOG

#ifdef PHY_EXYNOS_USB2_LOG
#define phy_log(phy_usb2, fmt, args...) \
	dev_info(phy_usb2->dev, fmt, ## args)
#else
#define phy_log(phy_usb2, fmt, args...) \
	dev_dbg(phy_usb2->dev, fmt, ## args)
#endif

/* Reference Clock */
enum exynos_usb2_phy_refclk {
	EXYNOS_FSEL_9MHZ6 = 0,
	EXYNOS_FSEL_10MHZ,
	EXYNOS_FSEL_12MHZ,
	EXYNOS_FSEL_19MHZ2,
	EXYNOS_FSEL_20MHZ,
	EXYNOS_FSEL_24MHZ,
	EXYNOS_FSEL_50MHZ = 0x7,
};

#ifndef MHZ
#define MHZ (1000*1000)
#endif

#ifndef KHZ
#define KHZ (1000)
#endif

/*******************************************************************************
 *
 * Structures
 *
 ******************************************************************************/
struct exynos_usb2_phy_info {
	unsigned int pmu_offset;
	unsigned int pmu_mask;
};

struct exynos_usb2_phy {
	struct phy	*phy;
	struct device	*dev;
	void __iomem	*reg_base;
	void __iomem	*pmu_base;
	struct clk	*bus_clk;
	struct clk	*ref_clk;
	unsigned int	phy_fsel;
	spinlock_t	lock;

	struct exynos_usb2_phy_info phy_info;
};

#endif /* End of __PHY_EXYNOS_USB2_H__ */
