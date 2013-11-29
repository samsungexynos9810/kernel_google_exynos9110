/* linux/drivers/video/decon_display/decon_pm_core.c
 *
 * Copyright (c) 2013 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/clk.h>
#include <linux/of.h>
#include <linux/fb.h>
#include <linux/pm_runtime.h>
#include <linux/of_gpio.h>
#include <linux/delay.h>
#include <linux/clk-private.h>
#include <linux/exynos_iovmm.h>

#include <linux/platform_device.h>
#include "regs-decon.h"
#include "decon_display_driver.h"
#include "decon_fb.h"
#include "decon_mipi_dsi.h"
#include "decon_dt.h"

#include <../drivers/clk/samsung/clk.h>

int decon_dbg = 5;
module_param(decon_dbg, int, 0644);

#define pm_debug(fmt, args...)						\
	do {								\
		if (decon_dbg >= 6)					\
			printk(KERN_DEBUG "[DEBUG]%s: "fmt "\n",	\
				__func__, ##args);		\
	} while (0)

#define call_pm_ops(q, ip, op, args...)			\
	(((q)->ip.ops->op) ? ((q)->ip.ops->op(args)) : 0)

#define call_block_pm_ops(q, op, args...)			\
		(((q)->pm_status.ops->op) ? ((q)->pm_status.ops->op(args)) : 0)

/* following values are for debugging */
unsigned int frame_done_count;
unsigned int te_count;

#ifdef CONFIG_EXYNOS5_DYNAMIC_CPU_HOTPLUG
extern int force_dynamic_hotplug(bool out_flag);
#endif

static void enable_mask(struct display_driver *dispdrv);
static void disable_mask(struct display_driver *dispdrv);
void display_block_clock_on(struct display_driver *dispdrv);
void display_block_clock_off(struct display_driver *dispdrv);
int display_hibernation_power_on(struct display_driver *dispdrv);
int display_hibernation_power_off(struct display_driver *dispdrv);
static void decon_clock_gating_handler(struct kthread_work *work);
static void decon_power_gating_handler(struct kthread_work *work);
extern void set_default_hibernation_mode(struct display_driver *dispdrv);

struct pm_ops display_block_ops = {
	.clk_on		= display_block_clock_on,
	.clk_off 	= display_block_clock_off,
	.pwr_on		= display_hibernation_power_on,
	.pwr_off 	= display_hibernation_power_off,
};

extern struct pm_ops decon_pm_ops;
#ifdef CONFIG_DECON_MIC
extern struct pm_ops mic_pm_ops;
#endif
extern struct pm_ops dsi_pm_ops;

extern struct mipi_dsim_device *dsim_for_decon;

int init_display_pm_status(struct display_driver *dispdrv) {
	dispdrv->pm_status.trigger_masked = 1;
	dispdrv->pm_status.clock_enabled = 0;
	atomic_set(&dispdrv->pm_status.lock_count, 0);
	dispdrv->pm_status.clk_idle_count = 0;
	dispdrv->pm_status.pwr_idle_count = 0;

	te_count = 0;
	frame_done_count = 0;
	return 0;
}

int init_display_pm(struct display_driver *dispdrv)
{
	init_display_pm_status(dispdrv);

	spin_lock_init(&dispdrv->pm_status.slock);
#ifdef CONFIG_FB_I80_COMMAND_MODE
	set_default_hibernation_mode(dispdrv);
#else
	dispdrv->pm_status.clock_gating_on = false;
	dispdrv->pm_status.power_gating_on = false;
	dispdrv->pm_status.hotplug_gating_on = false;
#endif

	init_kthread_worker(&dispdrv->pm_status.control_clock_gating);

	dispdrv->pm_status.control_clock_gating_thread = kthread_run(kthread_worker_fn,
			&dispdrv->pm_status.control_clock_gating,
			"decon_clk_thread");
	if (IS_ERR(dispdrv->pm_status.control_clock_gating_thread)) {
		int err = PTR_ERR(dispdrv->pm_status.control_clock_gating_thread);
		dispdrv->pm_status.control_clock_gating_thread = NULL;

		pr_err("failed to run control_clock_gating_thread\n");
		return err;
	}
	init_kthread_work(&dispdrv->pm_status.control_clock_gating_work,
		decon_clock_gating_handler);

	init_kthread_worker(&dispdrv->pm_status.control_power_gating);

	dispdrv->pm_status.control_power_gating_thread = kthread_run(kthread_worker_fn,
			&dispdrv->pm_status.control_power_gating,
			"decon_power_thread");
	if (IS_ERR(dispdrv->pm_status.control_power_gating_thread)) {
		int err = PTR_ERR(dispdrv->pm_status.control_power_gating_thread);
		dispdrv->pm_status.control_power_gating_thread = NULL;

		pr_err("failed to run control_power_gating_thread\n");
		return err;
	}
	init_kthread_work(&dispdrv->pm_status.control_power_gating_work,
		decon_power_gating_handler);

	dispdrv->pm_status.ops = &display_block_ops;
	dispdrv->decon_driver.ops = &decon_pm_ops;
	dispdrv->dsi_driver.ops = &dsi_pm_ops;
#ifdef CONFIG_DECON_MIC
	dispdrv->mic_driver.ops = &mic_pm_ops;
#endif
	return 0;
}

void disp_debug_power_info(void)
{
	struct display_driver *dispdrv = get_display_driver();

	pm_debug("mask: %d, clk: %d, "
		"lock: %d, clk_idle: %d, pwr_idle: %d,	\
		frame_cnt: %d, te_cnt: %d",
		dispdrv->pm_status.trigger_masked,
		dispdrv->pm_status.clock_enabled,
		atomic_read(&dispdrv->pm_status.lock_count),
		dispdrv->pm_status.clk_idle_count,
		dispdrv->pm_status.pwr_idle_count,
		frame_done_count,
		te_count);
}

int disp_pm_init_status(struct display_driver *dispdrv)
{
	dispdrv->platform_status = DISP_STATUS_PM1;
	return 0;
}

void disp_pm_gate_lock(struct display_driver *dispdrv, bool increase)
{
	if (increase)
		atomic_inc(&dispdrv->pm_status.lock_count);
	else
		atomic_dec(&dispdrv->pm_status.lock_count);
}

static void enable_mask(struct display_driver *dispdrv)
{
	u32 val;
	struct s3c_fb *sfb = dispdrv->decon_driver.sfb;

	if(dispdrv->pm_status.trigger_masked)
		return;

	dispdrv->pm_status.trigger_masked = 1;
	/* MASK */
	val = readl(sfb->regs + TRIGCON);
	val &= ~(TRIGCON_HWTRIGMASK_I80_RGB);
	writel(val, sfb->regs + TRIGCON);

	pm_debug("Enable mask");
}

static void disable_mask(struct display_driver *dispdrv)
{
	u32 val;
	struct s3c_fb *sfb = dispdrv->decon_driver.sfb;

	if(!dispdrv->pm_status.trigger_masked)
		return;

	dispdrv->pm_status.trigger_masked = 0;

	/* UNMASK */
	val = readl(sfb->regs + TRIGCON);
	val |= (TRIGCON_HWTRIGMASK_I80_RGB);
	writel(val, sfb->regs + TRIGCON);

	pm_debug("Disable mask");
}

void debug_function(struct display_driver *dispdrv, const char *buf)
{
#ifndef CONFIG_FB_I80_COMMAND_MODE
	pm_debug("does not support");
	return;
#endif
	pr_info("calls [%s] to control gating function\n", buf);
	if (!strncmp(buf, "clk-gate-on", sizeof(buf))) {
		dispdrv->pm_status.clock_gating_on = true;
	} else if (!strncmp(buf, "clk-gate-off", sizeof(buf))) {
		dispdrv->pm_status.clock_gating_on = false;
	} else if (!strncmp(buf, "pwr-gate-on", sizeof(buf))) {
		dispdrv->pm_status.power_gating_on = true;
	} else if (!strncmp(buf, "pwr-gate-off", sizeof(buf))) {
		dispdrv->pm_status.power_gating_on = false;
	} else if (!strncmp(buf, "hotplug-gate-on", sizeof(buf))) {
		dispdrv->pm_status.hotplug_gating_on = true;
	} else if (!strncmp(buf, "hotplug-gate-off", sizeof(buf))) {
		dispdrv->pm_status.hotplug_gating_on = false;
	} else {
		pr_err("INVALID parameter: '%s'\n", buf);
	}
	pm_debug("CLOCK GATING MODE: %s\n",
		dispdrv->pm_status.clock_gating_on == true? "TRUE":"FALSE");
	pm_debug("POWER GATING MODE: %s\n",
		dispdrv->pm_status.power_gating_on == true? "TRUE":"FALSE");
	pm_debug("HOTPLUG GATING MODE: %s\n",
		dispdrv->pm_status.hotplug_gating_on == true? "TRUE":"FALSE");
}

int disp_pm_runtime_enable(struct display_driver *dispdrv)
{
#ifdef DISP_RUNTIME_PM_DEBUG
	pm_debug("runtime pm for disp-driver enabled\n");
#endif
	pm_runtime_enable(dispdrv->display_driver);
	return 0;
}

int disp_pm_runtime_get_sync(struct display_driver *dispdrv)
{
	unsigned long flags;

	if (!dispdrv->pm_status.clock_gating_on) {
		pm_runtime_get_sync(dispdrv->display_driver);
		return 0;
	}

	spin_lock_irqsave(&dispdrv->pm_status.slock, flags);
	dispdrv->pm_status.clk_idle_count = 0;
	dispdrv->pm_status.pwr_idle_count = 0;
	spin_unlock_irqrestore(&dispdrv->pm_status.slock, flags);

	/* guarantee clock and power gating */
	flush_kthread_worker(&dispdrv->pm_status.control_clock_gating);
	flush_kthread_worker(&dispdrv->pm_status.control_power_gating);
	pm_runtime_get_sync(dispdrv->display_driver);
	if (dispdrv->platform_status > DISP_STATUS_PM0)
		display_block_clock_on(dispdrv);
	return 0;
}

int disp_pm_runtime_put_sync(struct display_driver *dispdrv)
{
	if (!dispdrv->pm_status.clock_gating_on) {
		pm_runtime_put_sync(dispdrv->display_driver);
		return 0;
	}

	flush_kthread_worker(&dispdrv->pm_status.control_clock_gating);
	pm_runtime_put_sync(dispdrv->display_driver);
	return 0;
}

/* disp_pm_te_triggered - check clock gating or not.
 * this function is called in the TE interrupt handler */
void disp_pm_te_triggered(struct display_driver *dispdrv)
{
	te_count++;

	if (!dispdrv->pm_status.clock_gating_on) return;

	spin_lock(&dispdrv->pm_status.slock);
	if (dispdrv->platform_status > DISP_STATUS_PM0 &&
		atomic_read(&dispdrv->pm_status.lock_count) == 0) {
		if (dispdrv->pm_status.clock_enabled) {
			if (!dispdrv->pm_status.trigger_masked) {
				enable_mask(dispdrv);
			}

			++dispdrv->pm_status.clk_idle_count;
			if (dispdrv->pm_status.clk_idle_count > MAX_CLK_GATING_COUNT) {
				disp_pm_gate_lock(dispdrv, true);
				pm_debug("display_block_clock_off +");
				queue_kthread_work(&dispdrv->pm_status.control_clock_gating,
						&dispdrv->pm_status.control_clock_gating_work);
			}
		} else {
			++dispdrv->pm_status.pwr_idle_count;
			if (dispdrv->pm_status.power_gating_on &&
				dispdrv->pm_status.pwr_idle_count > MAX_PWR_GATING_COUNT) {
				disp_pm_gate_lock(dispdrv, true);
				pm_debug("##### display_hibernation_power_off +");
				queue_kthread_work(&dispdrv->pm_status.control_power_gating,
						&dispdrv->pm_status.control_power_gating_work);
			}
		}

	}
	spin_unlock(&dispdrv->pm_status.slock);
}

/* disp_pm_add_efcount - it is called in the early start of the
 * set_win_config */
int disp_pm_add_refcount(struct display_driver *dispdrv)
{
	unsigned long flags;

	if (!dispdrv->pm_status.clock_gating_on) return 0;

	flush_kthread_worker(&dispdrv->pm_status.control_clock_gating);
	spin_lock_irqsave(&dispdrv->pm_status.slock, flags);
	if (dispdrv->pm_status.trigger_masked) {
		disable_mask(dispdrv);
	}
	spin_unlock_irqrestore(&dispdrv->pm_status.slock, flags);
	return 0;
}

/* disp_pm_dec_refcount - it is called at the DSI frame done */
int disp_pm_dec_refcount(struct display_driver *dispdrv)
{
	if (!dispdrv->pm_status.clock_gating_on) return 0;

	++frame_done_count;
	return 0;
}

static void decon_clock_gating_handler(struct kthread_work *work)
{
	struct display_driver *dispdrv = get_display_driver();
	unsigned long flags;

	if (dispdrv->pm_status.clk_idle_count > MAX_CLK_GATING_COUNT)
		display_block_clock_off(dispdrv);

	spin_lock_irqsave(&dispdrv->pm_status.slock, flags);
	dispdrv->pm_status.clk_idle_count = 0;
	disp_pm_gate_lock(dispdrv, false);
	spin_unlock_irqrestore(&dispdrv->pm_status.slock, flags);
	pm_debug("display_block_clock_off -");
}

static void decon_power_gating_handler(struct kthread_work *work)
{
	struct display_driver *dispdrv = get_display_driver();
	unsigned long flags;

	if (dispdrv->pm_status.pwr_idle_count > MAX_PWR_GATING_COUNT)
		display_hibernation_power_off(dispdrv);

	spin_lock_irqsave(&dispdrv->pm_status.slock, flags);
	dispdrv->pm_status.pwr_idle_count = 0;
	disp_pm_gate_lock(dispdrv, false);
	spin_unlock_irqrestore(&dispdrv->pm_status.slock, flags);
	pm_debug("##### display_hibernation_power_off -\n");
}

static int __display_hibernation_power_on(struct display_driver *dispdrv)
{
	/* already clocks are on */

	/* DSIM -> MIC -> DECON */
	call_pm_ops(dispdrv, dsi_driver, pwr_on, dispdrv);
#ifdef CONFIG_DECON_MIC
	call_pm_ops(dispdrv, mic_driver, pwr_on, dispdrv);
#endif
	call_pm_ops(dispdrv, decon_driver, pwr_on, dispdrv);
	return 0;
}

static int __display_hibernation_power_off(struct display_driver *dispdrv)
{
	call_block_pm_ops(dispdrv, clk_on, dispdrv);

	/* MIC -> DSIM -> DECON */
#ifdef CONFIG_DECON_MIC
	call_pm_ops(dispdrv, mic_driver, pwr_off, dispdrv);
#endif
	call_pm_ops(dispdrv, dsi_driver, pwr_off, dispdrv);
	call_pm_ops(dispdrv, decon_driver, pwr_off, dispdrv);

	call_block_pm_ops(dispdrv, clk_off, dispdrv);
	return 0;
}

static void __display_block_clock_on(struct display_driver *dispdrv)
{
	/* (MDNIE) -> DSIM -> MIC -> DECON -> SMMU */
	call_pm_ops(dispdrv, dsi_driver, clk_on, dispdrv);
#ifdef CONFIG_DECON_MIC
	call_pm_ops(dispdrv, mic_driver, clk_on, dispdrv);
#endif
	call_pm_ops(dispdrv, decon_driver, clk_on, dispdrv);

#ifdef CONFIG_ION_EXYNOS
	if (iovmm_activate(dispdrv->decon_driver.sfb->dev) < 0)
		pr_err("%s: failed to reactivate vmm\n", __func__);
#endif
}

static void __display_block_clock_off(struct display_driver *dispdrv)
{
	/* DECON -> (MDNIE) -> MIC -> DSIM -> SMMU */
	call_pm_ops(dispdrv, decon_driver, clk_off, dispdrv);
#ifdef CONFIG_DECON_MIC
	call_pm_ops(dispdrv, mic_driver, clk_off, dispdrv);
#endif
	call_pm_ops(dispdrv, dsi_driver, clk_off, dispdrv);

#ifdef CONFIG_ION_EXYNOS
	iovmm_deactivate(dispdrv->decon_driver.sfb->dev);
#endif
}

static void request_dynamic_hotplug(bool hotplug)
{
#ifdef CONFIG_EXYNOS5_DYNAMIC_CPU_HOTPLUG
	struct display_driver *dispdrv = get_display_driver();
	if (dispdrv->pm_status.hotplug_gating_on)
		force_dynamic_hotplug(hotplug);
#endif
}

int display_hibernation_power_on(struct display_driver *dispdrv)
{
	int ret = 0;
	struct s3c_fb *sfb = dispdrv->decon_driver.sfb;

	if (sfb->power_state == POWER_ON) {
		pr_info("%s, DECON are already power on state\n", __func__);
		return ret;
	}

	pm_debug("##### +");
	disp_pm_gate_lock(dispdrv, true);

	request_dynamic_hotplug(false);

	mutex_lock(&sfb->output_lock);
	disp_pm_runtime_get_sync(dispdrv);
	__display_hibernation_power_on(dispdrv);
	mutex_unlock(&sfb->output_lock);

	disp_pm_gate_lock(dispdrv, false);

	disp_debug_power_info();
	pm_debug("##### -\n");
	return ret;
}

int display_hibernation_power_off(struct display_driver *dispdrv)
{
	int ret = 0;
	struct s3c_fb *sfb = dispdrv->decon_driver.sfb;

	if (sfb->power_state == POWER_DOWN) {
		pr_info("%s, DECON are already power off state\n", __func__);
		return ret;
	}

	mutex_lock(&sfb->output_lock);
	sfb->power_state = POWER_HIBER_DOWN;
	__display_hibernation_power_off(dispdrv);
	disp_pm_runtime_put_sync(dispdrv);
	sfb->output_on = false;
	mutex_unlock(&sfb->output_lock);

	request_dynamic_hotplug(true);

	disp_debug_power_info();

	return ret;
}

void display_block_clock_on(struct display_driver *dispdrv)
{
	if (!dispdrv->pm_status.clock_enabled) {
		pm_debug("+");
		__display_block_clock_on(dispdrv);
		dispdrv->pm_status.clock_enabled = 1;
		pm_debug("-");
	}
}

void display_block_clock_off(struct display_driver *dispdrv)
{
	if (dispdrv->pm_status.clock_enabled) {
		dispdrv->pm_status.clock_enabled = 0;
		__display_block_clock_off(dispdrv);
	}
}
