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

#include <linux/notifier.h>
#include <linux/mutex.h>
#include <soc/samsung/exynos-psmw.h>
#ifdef CONFIG_EXYNOS_PSMW_MEM
#include <linux/io.h>

#define EXYNOS3_PA_DMC			0x105F0000
#define MEMCONTROL			0x04
#define PRECHCONFIG0			0x14
#define PWRDNCONFIG			0x28

#define DSREF_CYC_SHIFT		16
#define DPWRDN_CYC_MASK		0xff

#define CYC_SHIFT_LVL		2
#endif

static BLOCKING_NOTIFIER_HEAD(psmw_notifier_list);
static DEFINE_MUTEX(psmw_lock);
#ifdef CONFIG_EXYNOS_PSMW_MEM
static void __iomem *psmw_drex_base = NULL;
#endif
static enum psmw_event psmw_active_state = PSMW_DISABLE;

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

void psmw_notify_active(void)
{
#ifdef CONFIG_EXYNOS_PSMW_MEM
	unsigned int reg, tmp1, tmp2;

	/* Active precharge power down : 0x0 [3:2] */
	reg = __raw_readl(psmw_drex_base + MEMCONTROL);
	reg &= ~(0x3 << 2);
	__raw_writel(reg, psmw_drex_base + MEMCONTROL);

	/* Open page policy : 0x0 [19:16], each bit means a memory port */
	reg = __raw_readl(psmw_drex_base + PRECHCONFIG0);
	reg &= ~(0xf << 16);
	__raw_writel(reg, psmw_drex_base + PRECHCONFIG0);

	/* increase dsref_cyc & dpwrdn_cyc about 2 times */
	reg = __raw_readl(psmw_drex_base + PWRDNCONFIG);
	tmp1 = (reg >> DSREF_CYC_SHIFT) << (DSREF_CYC_SHIFT + CYC_SHIFT_LVL);
	tmp2 = ((reg & DPWRDN_CYC_MASK) << CYC_SHIFT_LVL) & DPWRDN_CYC_MASK;
	__raw_writel(tmp1 | tmp2, psmw_drex_base + PWRDNCONFIG);
	PSMW_INFO("PWRDNCONFIG [0x%x] \n", __raw_readl(psmw_drex_base + PWRDNCONFIG));

#endif
	blocking_notifier_call_chain(&psmw_notifier_list, 1, NULL);
	PSMW_INFO("inactive zone ===============================>>> [END]\n");
}

void psmw_notify_inactive(void)
{
#ifdef CONFIG_EXYNOS_PSMW_MEM
	unsigned int reg, tmp1, tmp2;

	/* Forced precharge power down : 0x1 [3:2] */
	reg = __raw_readl(psmw_drex_base + MEMCONTROL);
	reg &= ~(0x3 << 2);
	reg |= (0x1 << 2);
	__raw_writel(reg, psmw_drex_base + MEMCONTROL);

	/* close page policy : 0xf [19:16], each bit means a memory port */
	reg = __raw_readl(psmw_drex_base + PRECHCONFIG0);
	reg |= (0xf << 16);
	__raw_writel(reg, psmw_drex_base + PRECHCONFIG0);

	/* reduce dsref_cyc & dpwrdn_cyc about 1/2 times */
	reg = __raw_readl(psmw_drex_base + PWRDNCONFIG);
	tmp1 = (reg >> (DSREF_CYC_SHIFT + CYC_SHIFT_LVL)) << DSREF_CYC_SHIFT;
	tmp2 = (reg & DPWRDN_CYC_MASK) >> CYC_SHIFT_LVL;
	__raw_writel(tmp1 | tmp2, psmw_drex_base + PWRDNCONFIG);
	PSMW_INFO("PWRDNCONFIG [0x%x]\n", __raw_readl(psmw_drex_base + PWRDNCONFIG));
#endif
	PSMW_INFO("inactive zone <<<============= [START]\n");
	blocking_notifier_call_chain(&psmw_notifier_list, 0, NULL);
}

void psmw_trigger_update(enum psmw_trigger who, enum psmw_event event)
{
	unsigned int tmp = 0;
	char *tmp_str;

	switch (who)	{
	case TOUCH_SCREEN:
		tmp = (1 << TOUCH_SCREEN);
		tmp_str = "TOUCH_SCREEN";
		break;
	case DECON_DISPLAY:
		tmp = (1 << DECON_DISPLAY);
		tmp_str = "DECON_DISPLAY";
		break;
	case FIMD_FB:
		/* ToDo: check that enable and recover the last skipped event */
		if (event == PSMW_ENABLE) {
			mutex_lock(&psmw_lock);
			psmw_active_state = event;
			mutex_unlock(&psmw_lock);
		} else {
			pr_err("[PSMW]: %s: Invalid event of FIMD_FB [%d]\n",
					__func__, event);
		}
		return;
	default:
		pr_err("[PSMW]: %s: Invalid psmw_trigger [%d]\n", __func__, who);
		return;
	}

	mutex_lock(&psmw_lock);

	if (psmw_active_state == PSMW_DISABLE) {
		PSMW_INFO(" disabled, skip event[%d] of %s\n", event, tmp_str);
		mutex_unlock(&psmw_lock);
		return;
	}

	switch (event)	{
	case PSMW_ACTIVE:
		PSMW_INFO("%s Active\n", tmp_str);
		if (event != psmw_active_state) {
			psmw_active_state = event;
			psmw_notify_active();
		}
		break;
	case PSMW_INACTIVE:
		PSMW_INFO("%s Inactive\n", tmp_str);
		if (who == DECON_DISPLAY && event != psmw_active_state) {
			psmw_active_state = event;
			psmw_notify_inactive();
		}
		break;
	case PSMW_ENABLE:
		PSMW_INFO("%s PSMW_ENABLE\n", tmp_str);
		if (event == PSMW_DISABLE)
			psmw_active_state = event;
		break;
	case PSMW_DISABLE:
		PSMW_INFO("%s PSMW_DISABLE\n", tmp_str);
		if (event != psmw_active_state) {
			if (psmw_active_state != PSMW_ACTIVE)
				psmw_notify_active();
			psmw_active_state = event;
		}
		return;
	default:
		pr_err("[PSMW]: %s: Invalid event[%d] of  %s\n",
				__func__, event, tmp_str);
		return;
	}
	mutex_unlock(&psmw_lock);
}

static int __init psmw_init(void)
{
#ifdef CONFIG_EXYNOS_PSMW_MEM
	unsigned int reg;
	psmw_drex_base = ioremap(EXYNOS3_PA_DMC, SZ_64K);
	if(!psmw_drex_base) {
		pr_err("[PSMW]: %s: failed to ioremap for drex\n", __func__);
		return -1;
	}
	reg = __raw_readl(psmw_drex_base + MEMCONTROL);
	if(!(reg & (1<<1))) {
		pr_err("[PSMW]: %s: Dynamic Power Down is not enabled\n", __func__);
	}
	if(!(reg & (1<<5))) {
		pr_err("[PSMW]: %s: Dynamic Self Refresh is not enabled\n", __func__);
	}
#endif
	pr_info("[PSMW] %s\n", __func__);
	psmw_notify_active();
	return 0;
}

arch_initcall(psmw_init);
