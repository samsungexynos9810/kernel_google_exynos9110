/* linux/arch/arm/mach-exynos/exynos-psmw.c
 *
 * Copyright (c) 2015 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * EXYNOS - Power Saving Manager for Wearable
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/kobject.h>
#include <linux/export.h>
#include <linux/notifier.h>
#include <linux/device.h>
#include <linux/stat.h>
#include <linux/mutex.h>
#include <linux/delay.h>
#include <linux/kthread.h>
#include <mach/exynos-psmw.h>
#ifdef CONFIG_EXYNOS_PSMW_MEM
#include <linux/io.h>
#include <mach/map.h>

#define MEMCONTROL		0x04
#define PRECHCONFIG0	0x14
#endif

#define TAIL_PERIOD		200

static BLOCKING_NOTIFIER_HEAD(psmw_notifier_list);
static DEFINE_MUTEX(psmw_lock);
static struct task_struct *psmw_task;
static unsigned int ActiveStatus;

/**
 *	register_psmw_notifier - Register function to be called when display is updated.
 *	@nb: Info about notifier function to be called
 *
 *	Registers a function with the list of functions
 *	to be called when display is updated
 *
 *	Currently always returns zero, as blocking_notifier_chain_register()
 *	always returns zero.
 */
int register_psmw_notifier(struct notifier_block *nb)
{
	return blocking_notifier_chain_register(&psmw_notifier_list, nb);
}

/**
 *	unregister_psmw_notifier - Unregister previously registered psmw notifier
 *	@nb: Hook to be unregistered
 *
 *	Unregisters a previously registered psmw notifier function.
 *
 *	Returns zero on success, or %-ENOENT on failure.
 */
int unregister_psmw_notifier(struct notifier_block *nb)
{
	return blocking_notifier_chain_unregister(&psmw_notifier_list, nb);
}

void psmw_trigger_update(enum psmw_trigger who, enum psmw_event event)
{
	unsigned int tmp = 0;
	char *tmp_str;
	switch(who)	{
		case TOUCH_SCREEN :
			tmp = (1 << TOUCH_SCREEN);
			tmp_str = "TOUCH_SCREEN";
			break;
		case DECON_DISPLAY :
			tmp = (1 << DECON_DISPLAY);
			tmp_str = "DECON_DISPLAY";
			break;
		default:
			pr_err("[PSMW]: %s: Invalid psmw_trigger [%d]\n", __func__, who);
			return;
	}

	mutex_lock(&psmw_lock);
	if (event == PSMW_ACTIVE) {
		ActiveStatus = ActiveStatus | tmp;
		PSMW_INFO("%s Active \n", tmp_str);
	}
	else if (event == PSMW_INACTIVE) {
		ActiveStatus = ActiveStatus & ~(tmp);
		PSMW_INFO("%s Inactive \n", tmp_str);
	}
	else {
		pr_err("[PSMW]: %s: Invalid psmw_trigger [%d]\n", __func__, who);
		return;
	}
	mutex_unlock(&psmw_lock);
	wake_up_process(psmw_task);
}

void psmw_notify_active(void *data)
{
#ifdef CONFIG_EXYNOS_PSMW_MEM
	unsigned int reg;
	void __iomem *drex_base = (void __iomem *)data;

	/* Active precharge power down : 0x0 [3:2] */
	reg = __raw_readl(drex_base + MEMCONTROL);
	reg &= ~(0x3 << 2);
	__raw_writel(reg, drex_base + MEMCONTROL);

	/* Open page policy : 0x0 [19:16], each bit means a memory sport */
	reg = __raw_readl(drex_base + PRECHCONFIG0);
	reg &= ~(0xf << 16);
	__raw_writel(reg, drex_base + PRECHCONFIG0);
#endif
	PSMW_INFO("active zone ######################################## [START]\n");
	blocking_notifier_call_chain(&psmw_notifier_list, 1, NULL);
}

void psmw_notify_inactive(void *data)
{
#ifdef CONFIG_EXYNOS_PSMW_MEM
	unsigned int reg;
	void __iomem *drex_base = (void __iomem *)data;

	/* Forced precharge power down : 0x1 [3:2] */
	reg = __raw_readl(drex_base + MEMCONTROL);
	reg &= ~(0x3 << 2);
	reg |= (0x1 << 2);
	__raw_writel(reg, drex_base + MEMCONTROL);

	/* Open page policy : 0xf [19:16], each bit means a memory sport */
	reg = __raw_readl(drex_base + PRECHCONFIG0);
	reg |= (0xf << 16);
	__raw_writel(reg, drex_base + PRECHCONFIG0);
#endif
	PSMW_INFO("active zone ======================================= [END]\n");
	blocking_notifier_call_chain(&psmw_notifier_list, 0, NULL);
}

static int psmw_handle(void *data) {
	int tail_activate = 0;
	int integratedStatus = 0;
#ifdef CONFIG_EXYNOS_PSMW_MEM
	unsigned int reg;
	void __iomem *drex_base = ioremap(EXYNOS3_PA_DMC, SZ_64K);
	if(!drex_base) {
		pr_err("[PSMW]: %s: failed to ioremap for drex\n", __func__);
		return -1;
	}
	reg = __raw_readl(drex_base + MEMCONTROL);
	if(!(reg & (1<<1))) {
		pr_err("[PSMW]: Dynamic Power Down is not enabled \n", __func__);
	}
#endif
	do {
		PSMW_DBG("[sleep] integratedStatus [0x%x], ActiveStatus [0x%x]\n",
												integratedStatus, ActiveStatus);
		set_current_state(TASK_INTERRUPTIBLE);
		schedule();
		set_current_state(TASK_RUNNING);
		PSMW_DBG("[wakeup] ActiveStatus [0x%x]\n", ActiveStatus);

		if(integratedStatus == 0) {
			mutex_lock(&psmw_lock);
			if (ActiveStatus != 0) {
				integratedStatus = 1;
				psmw_notify_active(drex_base);
			}
			mutex_unlock(&psmw_lock);
		} else {
			if (ActiveStatus == 0) {
				integratedStatus = 0;
				if(tail_activate) {
					// Stop activate after some time.
					tail_activate = 0;
					PSMW_DBG("Deferred-Waiting for event : 0x%x, 0x%x\n",
												integratedStatus, ActiveStatus);
					set_current_state(TASK_INTERRUPTIBLE);
					schedule_timeout_interruptible(msecs_to_jiffies(TAIL_PERIOD));
					set_current_state(TASK_RUNNING);
					PSMW_DBG("Receive the event 100(0x%x)\n", ActiveStatus);
					mutex_lock(&psmw_lock);
					// Stop working
					if(ActiveStatus == 0)
						psmw_notify_inactive(drex_base);
					else
						integratedStatus = 1;
					mutex_unlock(&psmw_lock);
				} else {
					// Stop working immediately.
					psmw_notify_inactive(drex_base);
				}
			}
			if((tail_activate != 1) && (ActiveStatus & (1 << TOUCH_SCREEN)) &&
										(ActiveStatus & (1 << DECON_DISPLAY)))
				tail_activate = 1;
		}
	} while(1);
	return 0;
}

static int __init psmw_init(void)
{
	psmw_task = kthread_run(&psmw_handle, NULL, "psmw_thread");
	if (IS_ERR(psmw_task)) {
		pr_err("Failed to creat psmw_thread.\n");
		psmw_task = NULL;
		return -ENOMEM;
	}
	wake_up_process(psmw_task);
	PSMW_DBG("Power Saving Manager (PSM) is initialized\n");
	return 0;
}

arch_initcall(psmw_init);
