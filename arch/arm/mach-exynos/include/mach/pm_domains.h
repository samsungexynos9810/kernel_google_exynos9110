/*
 * Exynos Generic power domain support.
 *
 * Copyright (c) 2013 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com
 *
 * Implementation of Exynos specific power domain control which is used in
 * conjunction with runtime-pm. Support for both device-tree and non-device-tree
 * based power domain support is included.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#ifndef __ASM_ARCH_PM_RUNTIME_H
#define __ASM_ARCH_PM_RUNTIME_H __FILE__

#include <linux/io.h>
#include <linux/err.h>
#include <linux/delay.h>
#include <linux/of_address.h>
#include <linux/platform_device.h>
#include <linux/pm_domain.h>
#include <linux/pm_runtime.h>
#include <linux/slab.h>
#include <linux/clk.h>
#include <linux/spinlock.h>
#include <linux/clk-private.h>

#include <mach/regs-clock.h>
#include <mach/regs-pmu.h>

#include <plat/devs.h>

enum EXYNOS_PROCESS_ORDER {
	EXYNOS_PROCESS_BEFORE	=	0x1,
	EXYNOS_PROCESS_AFTER	=	0x2,
};

enum EXYNOS_PROCESS_TYPE {
	EXYNOS_PROCESS_ON	=	0x1,
	EXYNOS_PROCESS_OFF	=	0x2,
	EXYNOS_PROCESS_ONOFF	=	EXYNOS_PROCESS_ON | EXYNOS_PROCESS_OFF,
};

struct exynos_pm_clk {
	struct list_head node;
	struct clk *clk;
};

struct exynos_pm_reg {
	struct list_head node;
	enum EXYNOS_PROCESS_TYPE reg_type;
	void __iomem *reg;
	unsigned int value;
};

struct exynos_pm_domain;

struct exynos_pm_callback {
	struct list_head node;
	int (*callback)(struct exynos_pm_domain *domain);
};

struct exynos_pm_domain {
	struct generic_pm_domain genpd;
	char const *name;
	void __iomem *base;
	struct list_head clk_list;
	struct list_head reg_before_list;
	struct list_head reg_after_list;
	struct list_head cb_pre_on;
	int (*on)(struct exynos_pm_domain *pd, int power_flags);
	struct list_head cb_post_on;
	struct list_head cb_pre_off;
	int (*off)(struct exynos_pm_domain *pd, int power_flags);
	struct list_head cb_post_off;
	spinlock_t interrupt_lock;

	unsigned int pd_option;
};

#endif /* __ASM_ARCH_PM_RUNTIME_H */
