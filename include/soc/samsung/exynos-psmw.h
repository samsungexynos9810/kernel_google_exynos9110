/*
 * Copyright (C) 2016 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * EXYNOS5 - Header file for Exynos Power Saving Manager support
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef _LINUX_EXYNOS_PSMW_H
#define _LINUX_EXYNOS_PSMW_H

#include <linux/kernel.h>
#include <linux/kthread.h>

/*
 * Debugging macro and defines
 */
#ifdef CONFIG_EXYNOS_PSMW_DEBUG
	#  define PSMW_DBG(fmt, args...) \
         printk(KERN_DEBUG "[PSMW] %s: "fmt, __func__, ##args)
	#  define PSMW_INFO(fmt, args...) \
         printk(KERN_INFO "[PSMW] %s: "fmt, __func__, ##args)
#else /* CONFIG_EXYNOS_PSMW_DEBUG */
	#  define PSMW_DBG(fmt, args...) do { } while (0)
	#  define PSMW_INFO(fmt, args...) do { } while (0)
#endif /* CONFIG_EXYNOS_PSMW_DEBUG */

struct psmw_listener_info {
	struct notifier_block nb;
	atomic_t is_vsync_requested;
	atomic_t sleep;
};

enum psmw_trigger {
	TOUCH_SCREEN = 0,
	DECON_DISPLAY,
	FIMD_FB,
};

enum psmw_event {
	PSMW_ACTIVE = 1,
	PSMW_INACTIVE,
	PSMW_ENABLE,
	PSMW_DISABLE,
};

extern int register_psmw_notifier(struct notifier_block *nb);
extern int unregister_psmw_notifier(struct notifier_block *nb);
extern void psmw_trigger_update(enum psmw_trigger who, enum psmw_event event);

#endif /* _LINUX_EXYNOS_PSMW_H */
