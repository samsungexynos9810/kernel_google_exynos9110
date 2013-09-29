/*
 * Copyright (c) 2013 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * Base EXYNOS G2D resource and device definitions
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/kernel.h>
#include <linux/io.h>
#include <linux/errno.h>

#include <mach/map.h>
#include <plat/devs.h>
#include <plat/fimg2d.h>

#define EXYNOS5430_PA_SYSREG_G2D	0x124C0000
#define G2D_USER_CON			0x1000

#define G2DX_SHARED_VAL_MASK		(1 << 8)
#define G2DX_SHARED_VAL_SHIFT		(8)
#define G2DX_SHARED_SEL_MASK		(1 << 0)
#define G2DX_SHARED_SEL_SHIFT		(0)

static void __iomem *sysreg_g2d_base;

int g2d_cci_snoop_init(void)
{
	sysreg_g2d_base = ioremap(EXYNOS5430_PA_SYSREG_G2D, 0x2000);
	if (!sysreg_g2d_base) {
		pr_err("syrreg_g2d_base ioremap is failed\n");
		return -ENOMEM;
	}

	return 0;
}

void g2d_cci_snoop_remove(void)
{
	iounmap(sysreg_g2d_base);
}

int g2d_cci_snoop_control(enum g2d_shared_val val, enum g2d_shared_sel sel)
{
	void __iomem *control_reg;
	unsigned int cfg;

	if ((val >= SHAREABLE_VAL_END)
		|| (sel >= SHAREABLE_SEL_END)) {
		pr_err("g2d val or sel are out of range. val:%d, sel:%d\n", val, sel);
		return -EINVAL;
	}

	//printk("[%s:%d] val:%d, sel:%d\n", __func__, __LINE__, val, sel);

	control_reg = sysreg_g2d_base + G2D_USER_CON;

	cfg = readl(control_reg);

	cfg &= ~G2DX_SHARED_VAL_MASK;
	cfg |= val << G2DX_SHARED_VAL_SHIFT;

	cfg &= ~G2DX_SHARED_SEL_MASK;
	cfg |= sel << G2DX_SHARED_SEL_SHIFT;

	//printk("[%s:%d] control_reg:0x%p cfg:0x%x\n"
	//		, __func__, __LINE__, control_reg, cfg);

	writel(cfg, control_reg);

	return 0;
}
