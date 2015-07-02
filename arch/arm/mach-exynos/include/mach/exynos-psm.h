/* linux/arm/arm/mach-exynos/include/mach/exynos-psm.h
 *
 * Copyright (C) 2015 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * EXYNOS5 - Header file for Exynos Power Saving Manager support
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef _LINUX_EXYNOS_PSM_H
#define _LINUX_EXYNOS_PSM_H

#include <linux/kernel.h>
#include <linux/kthread.h>

/*
 * Debugging macro and defines
 */
#ifdef CONFIG_EXYNOS_PSM_DEBUG
	#  define PSM_DBG(fmt, args...) \
         printk(KERN_DEBUG "[PSM] %s: "fmt, __func__, ##args)
	#  define PSM_INFO(fmt, args...) \
         printk(KERN_INFO "[PSM] %s: "fmt, __func__, ##args)
#else /* CONFIG_EXYNOS_PSM_DEBUG */
	#  define PSM_DBG(fmt, args...) do { } while (0)
	#  define PSM_INFO(fmt, args...) do { } while (0)
#endif /* CONFIG_EXYNOS_PSM_DEBUG */

struct psm_listener_info {
	struct notifier_block nb;
	atomic_t is_vsync_requested;
	atomic_t sleep;
};

enum psm_trigger {
	TOUCH_SCREEN = 0,
	DECON_DISPLAY,
};

enum psm_event {
	PSM_ACTIVE,
	PSM_INACTIVE,
};

extern int register_psm_notifier(struct notifier_block *nb);
extern int unregister_psm_notifier(struct notifier_block *nb);
extern void psm_trigger_update(enum psm_trigger who, enum psm_event event);

#endif /* _LINUX_EXYNOS_PSM_H */
