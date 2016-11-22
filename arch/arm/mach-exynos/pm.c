/*
 * Copyright (c) 2011-2012 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * EXYNOS - Power Management support
 *
 * Based on arch/arm/mach-s3c2410/pm.c
 * Copyright (c) 2006 Simtec Electronics
 *	Ben Dooks <ben@simtec.co.uk>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/delay.h>
#include <linux/init.h>
#include <linux/suspend.h>
#include <linux/syscore_ops.h>
#include <linux/cpu_pm.h>
#include <linux/io.h>
#include <linux/irqchip/arm-gic.h>
#include <linux/err.h>
#include <linux/clk.h>
#include <linux/gpio.h>
#include <linux/wakeup_reason.h>

#include <asm/cacheflush.h>
#include <asm/hardware/cache-l2x0.h>
#include <asm/smp_scu.h>
#include <asm/suspend.h>

#include <plat/pm-common.h>
#include <plat/regs-srom.h>

#include <mach/map.h>
#include <mach/smc.h>

#include "common.h"
#include "regs-pmu.h"
#include "regs-sys.h"

#define EXYNOS3_MIF_R_REG(x)		(EXYNOS3_VA_CMU_ACP + (x))
#define EXYNOS3_CLKSRC_ACP		EXYNOS3_MIF_R_REG(0x0300)
#define msecs_to_loops(t)		(loops_per_jiffy / 1000 * HZ * t)

#define EXYNOS_WAKEUP_STAT_EINT         (1 << 0)
#define EXYNOS_WAKEUP_STAT_RTC_ALARM    (1 << 1)
#define EXYNOS_EINT_PEND(b, x)      ((b) + 0xF40 + (((x) >> 3) * 4))

#define EXYNOS3_PA_CMU_BUS_TOP		0x10030000
#define EXYNOS3_PA_CMU_ACP		0x10450000

#define EXYNOS3_CLKSRC_MASK_TOP		0xC310
#define EXYNOS3_CLKSRC_MASK_CAM		0xC320
#define EXYNOS3_CLKSRC_MASK_LCD		0xC334
#define EXYNOS3_CLKSRC_MASK_ISP		0xC338
#define EXYNOS3_CLKSRC_MASK_FSYS	0xC340
#define EXYNOS3_CLKSRC_MASK_PERIL0	0xC350
#define EXYNOS3_CLKSRC_MASK_PERIL1	0xC354
#define EXYNOS3_CLKSRC_MASK_ACP		0x0304

#define REG_INFORM0		(sysram_ns_base_addr + 0x8)
#define REG_INFORM1		(sysram_ns_base_addr + 0xC)

extern u32 exynos_get_eint_base(void);
extern void exynos3250_enable_idle_clock_down(void);

static void __iomem *exynos_eint_base;
static void __iomem *exynos3250_cmu_bus_top;
static void __iomem *exynos3250_cmu_acp;

/**
 * struct exynos_wkup_irq - Exynos GIC to PMU IRQ mapping
 * @hwirq: Hardware IRQ signal of the GIC
 * @mask: Mask in PMU wake-up mask register
 */
struct exynos_wkup_irq {
	unsigned int hwirq;
	u32 mask;
};

static struct sleep_save exynos5_sys_save[] = {
	SAVE_ITEM(EXYNOS5_SYS_I2C_CFG),
};

static struct sleep_save exynos_core_save[] = {
	/* SROM side */
	SAVE_ITEM(S5P_SROM_BW),
	SAVE_ITEM(S5P_SROM_BC0),
	SAVE_ITEM(S5P_SROM_BC1),
	SAVE_ITEM(S5P_SROM_BC2),
	SAVE_ITEM(S5P_SROM_BC3),
};

/*
 * GIC wake-up support
 */

static u32 exynos_irqwake_intmask = 0xffffffff;

static const struct exynos_wkup_irq exynos4_wkup_irq[] = {
	{ 76, BIT(1) }, /* RTC alarm */
	{ 77, BIT(2) }, /* RTC tick */
	{ /* sentinel */ },
};

static const struct exynos_wkup_irq exynos5250_wkup_irq[] = {
	{ 75, BIT(1) }, /* RTC alarm */
	{ 76, BIT(2) }, /* RTC tick */
	{ /* sentinel */ },
};

static int exynos_irq_set_wake(struct irq_data *data, unsigned int state)
{
	const struct exynos_wkup_irq *wkup_irq;

	if (soc_is_exynos5250())
		wkup_irq = exynos5250_wkup_irq;
	else
		wkup_irq = exynos4_wkup_irq;

	while (wkup_irq->mask) {
		if (wkup_irq->hwirq == data->hwirq) {
			if (!state)
				exynos_irqwake_intmask |= wkup_irq->mask;
			else
				exynos_irqwake_intmask &= ~wkup_irq->mask;
			return 0;
		}
		++wkup_irq;
	}

	return -ENOENT;
}

#define EXYNOS_BOOT_VECTOR_ADDR	(samsung_rev() == EXYNOS4210_REV_1_1 ? \
			pmu_base_addr + S5P_INFORM7 : \
			(samsung_rev() == EXYNOS4210_REV_1_0 ? \
			(sysram_base_addr + 0x24) : \
			pmu_base_addr + S5P_INFORM0))
#define EXYNOS_BOOT_VECTOR_FLAG	(samsung_rev() == EXYNOS4210_REV_1_1 ? \
			pmu_base_addr + S5P_INFORM6 : \
			(samsung_rev() == EXYNOS4210_REV_1_0 ? \
			(sysram_base_addr + 0x20) : \
			pmu_base_addr + S5P_INFORM1))

#define S5P_CHECK_AFTR  0xFCBA0D10
#define S5P_CHECK_SLEEP 0x00000BAD

/* For Cortex-A9 Diagnostic and Power control register */
static unsigned int save_arm_register[2];

static void exynos_cpu_save_register(void)
{
	unsigned long tmp;

	/* Save Power control register */
	asm ("mrc p15, 0, %0, c15, c0, 0"
	     : "=r" (tmp) : : "cc");

	save_arm_register[0] = tmp;

	/* Save Diagnostic register */
	asm ("mrc p15, 0, %0, c15, c0, 1"
	     : "=r" (tmp) : : "cc");

	save_arm_register[1] = tmp;
}

static void exynos_cpu_restore_register(void)
{
	unsigned long tmp;

	/* Restore Power control register */
	tmp = save_arm_register[0];

	asm volatile ("mcr p15, 0, %0, c15, c0, 0"
		      : : "r" (tmp)
		      : "cc");

	/* Restore Diagnostic register */
	tmp = save_arm_register[1];

	asm volatile ("mcr p15, 0, %0, c15, c0, 1"
		      : : "r" (tmp)
		      : "cc");
}

static void exynos_pm_central_suspend(void)
{
	unsigned long tmp;

	/* Setting Central Sequence Register for power down mode */
	tmp = pmu_raw_readl(S5P_CENTRAL_SEQ_CONFIGURATION);
	tmp &= ~S5P_CENTRAL_LOWPWR_CFG;
	pmu_raw_writel(tmp, S5P_CENTRAL_SEQ_CONFIGURATION);
}

static int exynos_pm_central_resume(void)
{
	unsigned long tmp;

	/*
	 * If PMU failed while entering sleep mode, WFI will be
	 * ignored by PMU and then exiting cpu_do_idle().
	 * S5P_CENTRAL_LOWPWR_CFG bit will not be set automatically
	 * in this situation.
	 */
	tmp = pmu_raw_readl(S5P_CENTRAL_SEQ_CONFIGURATION);
	if (!(tmp & S5P_CENTRAL_LOWPWR_CFG)) {
		tmp |= S5P_CENTRAL_LOWPWR_CFG;
		pmu_raw_writel(tmp, S5P_CENTRAL_SEQ_CONFIGURATION);
		/* No need to perform below restore code */
		return -1;
	}

	return 0;
}

/* Ext-GIC nIRQ/nFIQ is the only wakeup source in AFTR */
static void exynos_set_wakeupmask(long mask)
{
	pmu_raw_writel(mask, S5P_WAKEUP_MASK);
}

static void exynos_cpu_set_boot_vector(long flags)
{
	__raw_writel(virt_to_phys(cpu_resume), EXYNOS_BOOT_VECTOR_ADDR);
	__raw_writel(flags, EXYNOS_BOOT_VECTOR_FLAG);
}

static int exynos_aftr_finisher(unsigned long flags)
{
	exynos_set_wakeupmask(0x0000ff3e);
	exynos_cpu_set_boot_vector(S5P_CHECK_AFTR);
	/* Set value of power down register for aftr mode */
	exynos_sys_powerdown_conf(SYS_AFTR);
	cpu_do_idle();

	return 1;
}

void exynos_enter_aftr(void)
{
	cpu_pm_enter();

	exynos_pm_central_suspend();
	if (read_cpuid_part() == ARM_CPU_PART_CORTEX_A9)
		exynos_cpu_save_register();

	cpu_suspend(0, exynos_aftr_finisher);

	if (read_cpuid_part() == ARM_CPU_PART_CORTEX_A9) {
		scu_enable(S5P_VA_SCU);
		exynos_cpu_restore_register();
	}

	exynos_pm_central_resume();

	cpu_pm_exit();
}

static int exynos_cpu_suspend(unsigned long arg)
{
#ifndef CONFIG_SOC_EXYNOS3250
#ifdef CONFIG_CACHE_L2X0
	outer_flush_all();
#endif

	if (soc_is_exynos5250())
		flush_cache_all();

	/* issue the standby signal into the pm unit. */
	cpu_do_idle();

	pr_info("Failed to suspend the system\n");
	return 1; /* Aborting suspend */
#else
	unsigned int tmp;
	unsigned int i;

	/* Set clock source for PWI */
	tmp = __raw_readl(EXYNOS3_CLKSRC_ACP);
	tmp &= ~(0xF << 16);
	tmp |= (0x1 << 16);
	__raw_writel(tmp, EXYNOS3_CLKSRC_ACP);

	/* flush cache back to ram */
	flush_cache_all();

	/* For W/A code for prevent A7hotplug in fail */
	exynos_smc(SMC_CMD_REG, SMC_REG_ID_SFR_W(0x02020004), 0, 0);

	for (i = 1; i < 2; i++) {
		int loops;

		tmp = pmu_raw_readl(EXYNOS_ARM_CORE_CONFIGURATION(i));
		tmp |= S5P_CORE_LOCAL_PWR_EN;
		tmp |= EXYNOS_CORE_AUTOWAKEUP_EN;
		pmu_raw_writel(tmp, EXYNOS_ARM_CORE_CONFIGURATION(i));

		/* Wait until changing core status during 5ms */
		loops = msecs_to_loops(5);
		do {
			if (--loops == 0) {
				S3C_PMDBG("EXYNOS_ARM_CORE_STATUS(%d): 0x%x\n",
					i, pmu_raw_readl(EXYNOS_ARM_CORE_STATUS(i)));
				BUG();
			}
			pmu_raw_readl(EXYNOS_ARM_CORE_STATUS(i));
		} while ((pmu_raw_readl(EXYNOS_ARM_CORE_STATUS(i)) & 0x3) != 0x3);
	}

#ifdef CONFIG_ARM_TRUSTZONE
	exynos_smc(SMC_CMD_SLEEP, 0, 0, 0);
#else
	/* issue the standby signal into the pm unit. */
	cpu_do_idle();
#endif
	pr_info("sleep resumed to originator?");

	return 1; /* abort suspend */
#endif
}

static void s3c_pm_do_restore_clk(void)
{
	__raw_writel(0x00000001, exynos3250_cmu_bus_top + EXYNOS3_CLKSRC_MASK_TOP);
	__raw_writel(0xD1100001, exynos3250_cmu_bus_top + EXYNOS3_CLKSRC_MASK_CAM);
	__raw_writel(0x00001001, exynos3250_cmu_bus_top + EXYNOS3_CLKSRC_MASK_LCD);
	__raw_writel(0x00001111, exynos3250_cmu_bus_top + EXYNOS3_CLKSRC_MASK_ISP);
	__raw_writel(0x10000111, exynos3250_cmu_bus_top + EXYNOS3_CLKSRC_MASK_FSYS);
	__raw_writel(0x00011111, exynos3250_cmu_bus_top + EXYNOS3_CLKSRC_MASK_PERIL0);
	__raw_writel(0x00110011, exynos3250_cmu_bus_top + EXYNOS3_CLKSRC_MASK_PERIL1);
	__raw_writel(0x00010000, exynos3250_cmu_acp + EXYNOS3_CLKSRC_MASK_ACP);
}

static void exynos_pm_prepare(void)
{
	unsigned int tmp;

	/* Set wake-up mask registers */
	pmu_raw_writel(exynos_get_eint_wake_mask(), S5P_EINT_WAKEUP_MASK);
	pmu_raw_writel(exynos_irqwake_intmask & ~(1 << 31), S5P_WAKEUP_MASK);

	if (!soc_is_exynos3250())
		s3c_pm_do_save(exynos_core_save, ARRAY_SIZE(exynos_core_save));

	if (soc_is_exynos5250()) {
		s3c_pm_do_save(exynos5_sys_save, ARRAY_SIZE(exynos5_sys_save));
		/* Disable USE_RETENTION of JPEG_MEM_OPTION */
		tmp = pmu_raw_readl(EXYNOS5_JPEG_MEM_OPTION);
		tmp &= ~EXYNOS5_OPTION_USE_RETENTION;
		pmu_raw_writel(tmp, EXYNOS5_JPEG_MEM_OPTION);
	}

	if (soc_is_exynos3250()) {
		/* Decides whether to use retention capability */
		tmp = pmu_raw_readl(EXYNOS3_ARM_L2_OPTION);
		tmp &= ~EXYNOS3_OPTION_USE_RETENTION;
		pmu_raw_writel(tmp, EXYNOS3_ARM_L2_OPTION);
	}

	/* Set value of power down register for sleep mode */

	exynos_sys_powerdown_conf(SYS_SLEEP);
	__raw_writel(S5P_CHECK_SLEEP, REG_INFORM1);


	/* ensure at least INFORM0 has the resume address */

	__raw_writel(virt_to_phys(cpu_resume), REG_INFORM0);

	/*
	 * Before enter central sequence mode,
	 * clock src register have to set.
	 */
	s3c_pm_do_restore_clk();
}

static int exynos_pm_suspend(void)
{
	unsigned long tmp;

	exynos_pm_central_suspend();

	/* Setting SEQ_OPTION register */

	if (soc_is_exynos3250()) {
		/* FIXME: PMU Config before power down*/
		tmp = pmu_raw_readl(EXYNOS_CENTRAL_SEQ_CONFIGURATION_COREBLK);
		tmp &= ~EXYNOS_CENTRAL_LOWPWR_CFG;
		pmu_raw_writel(tmp, EXYNOS_CENTRAL_SEQ_CONFIGURATION_COREBLK);
#ifdef CONFIG_CPU_IDLE
	exynos3250_disable_idle_clock_down();
#endif
	} else {
		tmp = (S5P_USE_STANDBY_WFI0 | S5P_USE_STANDBY_WFE0);
		pmu_raw_writel(tmp, S5P_CENTRAL_SEQ_OPTION);
	}

	if (read_cpuid_part() == ARM_CPU_PART_CORTEX_A9)
		exynos_cpu_save_register();

	return 0;
}

static void exynos3_pm_rewrite_eint_pending(void)
{
	pr_info("Rewrite eint pending interrupt clear \n");
	__raw_writel(__raw_readl(exynos_eint_base + 0xF40), exynos_eint_base + 0xF40);
	__raw_writel(__raw_readl(exynos_eint_base + 0xF44), exynos_eint_base + 0xF44);
	__raw_writel(__raw_readl(exynos_eint_base + 0xF48), exynos_eint_base + 0xF48);
	__raw_writel(__raw_readl(exynos_eint_base + 0xF4C), exynos_eint_base + 0xF4C);
}

static void exynos_show_wakeup_reason_eint(void)
{
	int bit;
	int i, size;
	long unsigned int ext_int_pend;
	u64 eint_wakeup_mask;
	bool found = 0;
	int eint_base;

	eint_wakeup_mask = pmu_raw_readl(EXYNOS_EINT_WAKEUP_MASK);
	eint_base = exynos_get_eint_base();

	for (i = 0, size = 8; i < 32; i += size) {

		ext_int_pend =
			__raw_readl(EXYNOS_EINT_PEND(exynos_eint_base, i));

		for_each_set_bit(bit, &ext_int_pend, size) {
			u32 gpio;
			int irq;

			if (eint_wakeup_mask & (1 << (i + bit)))
				continue;

			gpio = eint_base + (i + bit);
			irq = gpio_to_irq(gpio);

			log_wakeup_reason(irq);
			pr_info("cause of wakeup: XEINT_%d\n", i + bit);
			update_wakeup_reason_stats(irq, i + bit);
			found = 1;
		}
	}

	if (!found)
		pr_info("Resume caused by unknown EINT\n");
}

static void exynos_show_wakeup_registers(unsigned long wakeup_stat)
{
	pr_debug("WAKEUP_STAT: 0x%08lx\n", wakeup_stat);
	pr_debug("EINT_PEND: 0x%02x, 0x%02x 0x%02x, 0x%02x\n",
			__raw_readl(EXYNOS_EINT_PEND(exynos_eint_base, 0)),
			__raw_readl(EXYNOS_EINT_PEND(exynos_eint_base, 8)),
			__raw_readl(EXYNOS_EINT_PEND(exynos_eint_base, 16)),
			__raw_readl(EXYNOS_EINT_PEND(exynos_eint_base, 24)));
}

static void exynos_show_wakeup_reason(void)
{
	unsigned long wakeup_stat;

	wakeup_stat = pmu_raw_readl(S5P_WAKEUP_STAT);

	exynos_show_wakeup_registers(wakeup_stat);

	if (wakeup_stat & EXYNOS_WAKEUP_STAT_RTC_ALARM)
		pr_info("Resume caused by RTC alarm\n");
	else if (wakeup_stat & EXYNOS_WAKEUP_STAT_EINT)
		exynos_show_wakeup_reason_eint();
	else
		pr_info("Resume caused by wakeup_stat=0x%08lx\n",
			wakeup_stat);
}

static void exynos_pm_resume(void)
{
	pmu_raw_writel(EXYNOS3_USE_STANDBY_WFI_ALL,
		EXYNOS_CENTRAL_SEQ_OPTION);

	if (exynos_pm_central_resume())
		goto early_wakeup;

	if (read_cpuid_part() == ARM_CPU_PART_CORTEX_A9)
		exynos_cpu_restore_register();

	/* For release retention */

	pmu_raw_writel((1 << 28), S5P_PAD_RET_MAUDIO_OPTION);
	pmu_raw_writel((1 << 28), S5P_PAD_RET_GPIO_OPTION);
	pmu_raw_writel((1 << 28), S5P_PAD_RET_UART_OPTION);
	pmu_raw_writel((1 << 28), S5P_PAD_RET_MMCA_OPTION);
	pmu_raw_writel((1 << 28), S5P_PAD_RET_MMCB_OPTION);
	pmu_raw_writel((1 << 28), S5P_PAD_RET_EBIA_OPTION);
	pmu_raw_writel((1 << 28), S5P_PAD_RET_EBIB_OPTION);
	pmu_raw_writel((1 << 28), EXYNOS3_PAD_RETENTION_MMC2_OPTION);
	pmu_raw_writel((1 << 28), EXYNOS3_PAD_RETENTION_SPI_OPTION);

	if (soc_is_exynos5250())
		s3c_pm_do_restore(exynos5_sys_save,
			ARRAY_SIZE(exynos5_sys_save));

	if (!soc_is_exynos3250()) {
		s3c_pm_do_restore_core(exynos_core_save, ARRAY_SIZE(exynos_core_save));
	} else {
		if (pmu_raw_readl(S5P_WAKEUP_STAT) == 0x0)
			exynos3_pm_rewrite_eint_pending();
	}

	if (read_cpuid_part() == ARM_CPU_PART_CORTEX_A9)
		scu_enable(S5P_VA_SCU);

early_wakeup:

#ifdef CONFIG_CPU_IDLE
	exynos3250_enable_idle_clock_down();
#endif

	/* Clear SLEEP mode set in INFORM1 */
	__raw_writel(0x0, REG_INFORM1);
	exynos_show_wakeup_reason();

	return;
}

static struct syscore_ops exynos_pm_syscore_ops = {
	.suspend	= exynos_pm_suspend,
	.resume		= exynos_pm_resume,
};

/*
 * Suspend Ops
 */

static int exynos_suspend_enter(suspend_state_t state)
{
	int ret;

	s3c_pm_debug_init();

	S3C_PMDBG("%s: suspending the system...\n", __func__);

	S3C_PMDBG("%s: wakeup masks: %08x,%08x\n", __func__,
			exynos_irqwake_intmask, exynos_get_eint_wake_mask());

	if (exynos_irqwake_intmask == -1U
	    && exynos_get_eint_wake_mask() == -1U) {
		pr_err("%s: No wake-up sources!\n", __func__);
		pr_err("%s: Aborting sleep\n", __func__);
		return -EINVAL;
	}

	s3c_pm_save_uarts();
	exynos_pm_prepare();
	if (!soc_is_exynos3250())
		flush_cache_all();
	s3c_pm_check_store();

	ret = cpu_suspend(0, exynos_cpu_suspend);
	if (ret)
		return ret;

	s3c_pm_restore_uarts();

	S3C_PMDBG("%s: wakeup stat: %08x\n", __func__,
			pmu_raw_readl(S5P_WAKEUP_STAT));

	s3c_pm_check_restore();

	S3C_PMDBG("%s: resuming the system...\n", __func__);

	return 0;
}

static int exynos_suspend_prepare(void)
{
	s3c_pm_check_prepare();

	return 0;
}

static void exynos_suspend_finish(void)
{
	s3c_pm_check_cleanup();
}

static const struct platform_suspend_ops exynos_suspend_ops = {
	.enter		= exynos_suspend_enter,
	.prepare	= exynos_suspend_prepare,
	.finish		= exynos_suspend_finish,
	.valid		= suspend_valid_only_mem,
};

void __init exynos_pm_init(void)
{
	u32 tmp;

	exynos_eint_base = ioremap(EXYNOS3250_PA_GPIO2, SZ_8K);

	if (exynos_eint_base == NULL) {
		pr_err("%s: unable to ioremap for EINT base address\n",
				__func__);
		BUG();
	}

	exynos3250_cmu_bus_top = ioremap(EXYNOS3_PA_CMU_BUS_TOP, SZ_4K);

	if (exynos3250_cmu_bus_top == NULL) {
		pr_err("%s: unable to ioremap \n",
				__func__);
		BUG();
	}

	exynos3250_cmu_acp = ioremap(EXYNOS3_PA_CMU_ACP, SZ_4K);

	if (exynos3250_cmu_acp == NULL) {
		pr_err("%s: unable to ioremap \n",
				__func__);
		BUG();
	}

	/* Platform-specific GIC callback */
	gic_arch_extn.irq_set_wake = exynos_irq_set_wake;

	/* All wakeup disable */
	tmp = pmu_raw_readl(S5P_WAKEUP_MASK);
	tmp |= ((0xFF << 8) | (0x1F << 1));
	pmu_raw_writel(tmp, S5P_WAKEUP_MASK);

	register_syscore_ops(&exynos_pm_syscore_ops);
	suspend_set_ops(&exynos_suspend_ops);
}
