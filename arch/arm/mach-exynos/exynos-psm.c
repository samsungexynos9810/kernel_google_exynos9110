/* linux/arch/arm/mach-exynos/exynos-psm.c
 *
 * Copyright (c) 2015 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * EXYNOS - Power Saving Manager
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
#include <mach/exynos-psm.h>

#define TAIL_PERIOD		200

static BLOCKING_NOTIFIER_HEAD(psm_notifier_list);
static DEFINE_MUTEX(psm_lock);
static struct task_struct *psm_task;
static unsigned int ActiveStatus;

/**
 *	register_psm_notifier - Register function to be called when display is updated.
 *	@nb: Info about notifier function to be called
 *
 *	Registers a function with the list of functions
 *	to be called when display is updated
 *
 *	Currently always returns zero, as blocking_notifier_chain_register()
 *	always returns zero.
 */
int register_psm_notifier(struct notifier_block *nb)
{
	return blocking_notifier_chain_register(&psm_notifier_list, nb);
}

/**
 *	unregister_psm_notifier - Unregister previously registered psm notifier
 *	@nb: Hook to be unregistered
 *
 *	Unregisters a previously registered psm notifier function.
 *
 *	Returns zero on success, or %-ENOENT on failure.
 */
int unregister_psm_notifier(struct notifier_block *nb)
{
	return blocking_notifier_chain_unregister(&psm_notifier_list, nb);
}

void psm_trigger_update(enum psm_trigger who, enum psm_event event)
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
			pr_err("[PSM]: %s: Invalid psm_trigger [%d]\n", __func__, who);
			return;
	}

	mutex_lock(&psm_lock);
	if (event == PSM_ACTIVE) {
		ActiveStatus = ActiveStatus | tmp;
		PSM_INFO("%s Active \n", tmp_str);
	}
	else if (event == PSM_INACTIVE) {
		ActiveStatus = ActiveStatus & ~(tmp);
		PSM_INFO("%s Inactive \n", tmp_str);
	}
	else {
		pr_err("[PSM]: %s: Invalid psm_trigger [%d]\n", __func__, who);
		return;
	}
	mutex_unlock(&psm_lock);
	wake_up_process(psm_task);
}

static int psm_handle(void *data) {
	int tail_activate = 0;
	int integratedStatus = 0;

	do {
		PSM_DBG("[sleep] integratedStatus [0x%x], ActiveStatus [0x%x]\n",
												integratedStatus, ActiveStatus);
		set_current_state(TASK_INTERRUPTIBLE);
		schedule();
		set_current_state(TASK_RUNNING);
		PSM_DBG("[wakeup] ActiveStatus [0x%x]\n", ActiveStatus);

		if(integratedStatus == 0) {
			mutex_lock(&psm_lock);
			if (ActiveStatus != 0) {
				integratedStatus = 1;
				// Start working
				PSM_INFO("Notifying: active\n");
				blocking_notifier_call_chain(&psm_notifier_list, 1, NULL);
			}
			mutex_unlock(&psm_lock);
		} else {
			if (ActiveStatus == 0) {
				integratedStatus = 0;
				if(tail_activate) {
					// Stop activate after some time.
					tail_activate = 0;
					PSM_DBG("Deferred-Waiting for event : 0x%x, 0x%x\n",
												integratedStatus, ActiveStatus);
					set_current_state(TASK_INTERRUPTIBLE);
					schedule_timeout_interruptible(msecs_to_jiffies(TAIL_PERIOD));
					set_current_state(TASK_RUNNING);
					PSM_DBG("Receive the event 100(0x%x)\n", ActiveStatus);
					mutex_lock(&psm_lock);

					// Stop working
					if(ActiveStatus == 0) {
						PSM_INFO("Notifying: inactive\n");
						blocking_notifier_call_chain(&psm_notifier_list, 0, NULL);
					}
					else {
						integratedStatus = 1;
					}
					mutex_unlock(&psm_lock);
				} else {
					// Stop working immediately.
					PSM_INFO("Notifying: inactive\n");
					blocking_notifier_call_chain(&psm_notifier_list, 0, NULL);
				}
			}

			if((tail_activate != 1) && (ActiveStatus & (1 << TOUCH_SCREEN)) &&
										(ActiveStatus & (1 << DECON_DISPLAY))) {
				tail_activate = 1;
			}
		}

	} while(1);

	return 0;
}

static int __init psm_init(void)
{
	psm_task = kthread_run(&psm_handle, NULL, "psm_thread");
	if (IS_ERR(psm_task)) {
		pr_err("Failed to creat psm_thread.\n");
		psm_task = NULL;
		return -ENOMEM;
	}
	wake_up_process(psm_task);
	PSM_DBG("Power Saving Manager (PSM) is initialized\n");
	return 0;
}

arch_initcall(psm_init);
