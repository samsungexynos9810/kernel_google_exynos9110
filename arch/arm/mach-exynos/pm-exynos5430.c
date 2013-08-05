/* linux/arch/arm/mach-exynos/pm.c
 *
 * Copyright (c) 2011 Samsung Electronics Co., Ltd.
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

#include <linux/init.h>
#include <linux/suspend.h>
#include <linux/syscore_ops.h>
#include <linux/io.h>
#include <linux/err.h>
#include <linux/clk.h>
#include <linux/interrupt.h>

#include <asm/cacheflush.h>
#include <asm/hardware/cache-l2x0.h>
#include <asm/smp_scu.h>
#include <asm/cputype.h>

#include <plat/cpu.h>
#include <plat/pm.h>
#include <plat/pll.h>
#include <plat/regs-srom.h>
#include <plat/bts.h>

#include <mach/regs-irq.h>
#include <mach/regs-gpio.h>
#include <mach/regs-clock.h>
#include <mach/regs-pmu.h>
#include <mach/pm-core.h>
#include <mach/pmu.h>
#include <mach/smc.h>
#include <mach/pm_interrupt_domains.h>

#ifdef CONFIG_ARM_TRUSTZONE
#define REG_INFORM0            (S5P_VA_SYSRAM_NS + 0x8)
#define REG_INFORM1            (S5P_VA_SYSRAM_NS + 0xC)
#else
#define REG_INFORM0            (EXYNOS_INFORM0)
#define REG_INFORM1            (EXYNOS_INFORM1)
#endif

#define EXYNOS_I2C_CFG		(S3C_VA_SYS + 0x234)

#define EXYNOS_WAKEUP_STAT_EINT		(1 << 0)
#define EXYNOS_WAKEUP_STAT_RTC_ALARM	(1 << 1)

static struct sleep_save exynos5420_set_clksrc[] = {
	{ .reg = EXYNOS5420_CLKSRC_MASK_CPERI,		.val = 0xffffffff, },
	{ .reg = EXYNOS5_CLKSRC_MASK_TOP0,		.val = 0xffffffff, },
	{ .reg = EXYNOS5_CLKSRC_MASK_TOP1,		.val = 0xffffffff, },
	{ .reg = EXYNOS5_CLKSRC_MASK_TOP2,		.val = 0xffffffff, },
	{ .reg = EXYNOS5_CLKSRC_MASK_TOP7,		.val = 0xffffffff, },
	{ .reg = EXYNOS5_CLKSRC_MASK_DISP1_0,		.val = 0xffffffff, },
	{ .reg = EXYNOS5_CLKSRC_MASK_MAUDIO,		.val = 0xffffffff, },
	{ .reg = EXYNOS5_CLKSRC_MASK_FSYS,		.val = 0xffffffff, },
	{ .reg = EXYNOS5_CLKSRC_MASK_PERIC0,		.val = 0xffffffff, },
	{ .reg = EXYNOS5_CLKSRC_MASK_PERIC1,		.val = 0xffffffff, },
	{ .reg = EXYNOS5_CLKSRC_MASK_ISP,		.val = 0xffffffff, },
	{ .reg = EXYNOS5_CLKGATE_BUS_DISP1,		.val = 0xffffffff, },
};

static struct sleep_save exynos54xx_enable_xxti[] = {
	{ .reg = EXYNOS5_XXTI_SYS_PWR_REG,		.val = 0x1, },
};

static struct sleep_save exynos_core_save[] = {
	/* SROM side */
	SAVE_ITEM(S5P_SROM_BW),
	SAVE_ITEM(S5P_SROM_BC0),
	SAVE_ITEM(S5P_SROM_BC1),
	SAVE_ITEM(S5P_SROM_BC2),
	SAVE_ITEM(S5P_SROM_BC3),

	/* I2C CFG */
	SAVE_ITEM(EXYNOS_I2C_CFG),
};

static struct sleep_save exynos5420_core_save[] = {
	SAVE_ITEM(S3C_VA_SYS + 0x400),
	SAVE_ITEM(S3C_VA_SYS + 0x404),
};

/* For Cortex-A9 Diagnostic and Power control register */
static unsigned int save_arm_register[2];

static void exynos_clkgate_ctrl(bool on)
{
	unsigned int tmp;

	tmp = __raw_readl(EXYNOS5_CLKGATE_IP_GSCL0);
	tmp = on ? (tmp | EXYNOS5410_CLKGATE_GSCALER0_3) :
			(tmp & ~EXYNOS5410_CLKGATE_GSCALER0_3);

	__raw_writel(tmp, EXYNOS5_CLKGATE_IP_GSCL0);
}

static void exynos_show_wakeup_reason_eint(void)
{
	int bit;
	int reg_eintstart;
	long unsigned int ext_int_pend;
	unsigned long eint_wakeup_mask;
	bool found = 0;
	extern void __iomem *exynos_eint_base;

	eint_wakeup_mask = __raw_readl(EXYNOS_EINT_WAKEUP_MASK);

	for (reg_eintstart = 0; reg_eintstart <= 31; reg_eintstart += 8) {
		ext_int_pend =
			__raw_readl(EINT_PEND(exynos_eint_base,
					      IRQ_EINT(reg_eintstart)));

		for_each_set_bit(bit, &ext_int_pend, 8) {
			int irq = IRQ_EINT(reg_eintstart) + bit;
			struct irq_desc *desc = irq_to_desc(irq);

			if (eint_wakeup_mask & (1 << (reg_eintstart + bit)))
				continue;

			if (desc && desc->action && desc->action->name)
				pr_info("Resume caused by IRQ %d, %s\n", irq,
					desc->action->name);
			else
				pr_info("Resume caused by IRQ %d\n", irq);

			found = 1;
		}
	}

	if (!found)
		pr_info("Resume caused by unknown EINT\n");
}

static void exynos_show_wakeup_reason(void)
{
	unsigned long wakeup_stat;

	wakeup_stat = __raw_readl(EXYNOS_WAKEUP_STAT);

	if (wakeup_stat & EXYNOS_WAKEUP_STAT_RTC_ALARM)
		pr_info("Resume caused by RTC alarm\n");
	else if (wakeup_stat & EXYNOS_WAKEUP_STAT_EINT)
		exynos_show_wakeup_reason_eint();
	else
		pr_info("Resume caused by wakeup_stat=0x%08lx\n",
			wakeup_stat);
}

static int exynos_cpu_suspend(unsigned long arg)
{
	flush_cache_all();

#ifdef CONFIG_ARM_TRUSTZONE
	exynos_smc(SMC_CMD_SLEEP, 0, 0, 0);
#else
	/* issue the standby signal into the pm unit. */
	cpu_do_idle();
#endif
	pr_info("sleep resumed to originator?");
	return 1; /* abort suspend */
}

static int exynos_pm_suspend(void)
{
	unsigned long tmp;
	unsigned int cluster_id;

	s3c_pm_do_save(exynos5420_core_save, ARRAY_SIZE(exynos5420_core_save));

	s3c_pm_do_save(exynos_core_save, ARRAY_SIZE(exynos_core_save));

	cluster_id = read_cpuid(CPUID_MPIDR) >> 8 & 0xf;
	if(!cluster_id)
		__raw_writel(EXYNOS5410_ARM_USE_STANDBY_WFI0,
			     EXYNOS_CENTRAL_SEQ_OPTION);
	else
		__raw_writel(EXYNOS5410_KFC_USE_STANDBY_WFI0,
			     EXYNOS_CENTRAL_SEQ_OPTION);

	/* Setting Central Sequence Register for power down mode */
	tmp = __raw_readl(EXYNOS_CENTRAL_SEQ_CONFIGURATION);
	tmp &= ~EXYNOS_CENTRAL_LOWPWR_CFG;
	__raw_writel(tmp, EXYNOS_CENTRAL_SEQ_CONFIGURATION);

	return 0;
}

static void exynos_pm_prepare(void)
{
	unsigned int tmp;

	/* Set value of power down register for sleep mode */
	exynos_sys_powerdown_conf(SYS_SLEEP);

	if (soc_is_exynos5410() || soc_is_exynos5420()) {
		if (!(__raw_readl(EXYNOS_PMU_DEBUG) & 0x1))
			s3c_pm_do_restore_core(exynos54xx_enable_xxti,
					ARRAY_SIZE(exynos54xx_enable_xxti));
	}
	__raw_writel(EXYNOS_CHECK_SLEEP, REG_INFORM1);

	/* ensure at least INFORM0 has the resume address */
	__raw_writel(virt_to_phys(s3c_cpu_resume), REG_INFORM0);

	/*
	 * Before enter central sequence mode,
	 * clock src register have to set.
	 */

	s3c_pm_do_restore_core(exynos5420_set_clksrc, ARRAY_SIZE(exynos5420_set_clksrc));

	tmp = __raw_readl(EXYNOS5_ARM_L2_OPTION);
	tmp &= ~EXYNOS5_USE_RETENTION;
	__raw_writel(tmp, EXYNOS5_ARM_L2_OPTION);

	tmp = __raw_readl(EXYNOS5420_SFR_AXI_CGDIS1_REG);
	tmp |= (EXYNOS5420_UFS | EXYNOS5420_ACE_KFC | EXYNOS5420_ACE_EAGLE);
	__raw_writel(tmp, EXYNOS5420_SFR_AXI_CGDIS1_REG);

	tmp = __raw_readl(EXYNOS54XX_ARM_COMMON_OPTION);
	tmp &= ~(1<<3);
	__raw_writel(tmp, EXYNOS54XX_ARM_COMMON_OPTION);
}

static void exynos_pm_resume(void)
{
	unsigned long tmp;
#ifdef CONFIG_EXYNOS5_CLUSTER_POWER_CONTROL
	unsigned int cluster_id = !((read_cpuid_mpidr() >> 8) & 0xf);
#endif

	if (soc_is_exynos5410() || soc_is_exynos5420())
		__raw_writel(EXYNOS5410_USE_STANDBY_WFI_ALL,
			EXYNOS_CENTRAL_SEQ_OPTION);

	/*
	 * If PMU failed while entering sleep mode, WFI will be
	 * ignored by PMU and then exiting cpu_do_idle().
	 * S5P_CENTRAL_LOWPWR_CFG bit will not be set automatically
	 * in this situation.
	 */
	tmp = __raw_readl(EXYNOS_CENTRAL_SEQ_CONFIGURATION);
	if (!(tmp & EXYNOS_CENTRAL_LOWPWR_CFG)) {
		tmp |= EXYNOS_CENTRAL_LOWPWR_CFG;
		__raw_writel(tmp, EXYNOS_CENTRAL_SEQ_CONFIGURATION);
		/* No need to perform below restore code */
		goto early_wakeup;
	}

	/* For release retention */
	__raw_writel((1 << 28), EXYNOS_PAD_RET_DRAM_OPTION);
	__raw_writel((1 << 28), EXYNOS_PAD_RET_MAUDIO_OPTION);
	__raw_writel((1 << 28), EXYNOS_PAD_RET_JTAG_OPTION);
	__raw_writel((1 << 28), EXYNOS54XX_PAD_RET_GPIO_OPTION);
	__raw_writel((1 << 28), EXYNOS54XX_PAD_RET_UART_OPTION);
	__raw_writel((1 << 28), EXYNOS54XX_PAD_RET_MMCA_OPTION);
	__raw_writel((1 << 28), EXYNOS54XX_PAD_RET_MMCB_OPTION);
	__raw_writel((1 << 28), EXYNOS54XX_PAD_RET_MMCC_OPTION);
	__raw_writel((1 << 28), EXYNOS54XX_PAD_RET_HSI_OPTION);
	__raw_writel((1 << 28), EXYNOS_PAD_RET_EBIA_OPTION);
	__raw_writel((1 << 28), EXYNOS_PAD_RET_EBIB_OPTION);
	__raw_writel((1 << 28), EXYNOS5_PAD_RETENTION_SPI_OPTION);
	__raw_writel((1 << 28), EXYNOS5_PAD_RETENTION_GPIO_SYSMEM_OPTION);

	s3c_pm_do_restore_core(exynos_core_save, ARRAY_SIZE(exynos_core_save));

	s3c_pm_do_restore_core(exynos5420_core_save, ARRAY_SIZE(exynos5420_core_save));

	bts_initialize(NULL, true);

early_wakeup:
	exynos_show_wakeup_reason();
	return;
}

static int exynos_pm_add(struct device *dev, struct subsys_interface *sif)
{
	pm_cpu_prep = exynos_pm_prepare;
	pm_cpu_sleep = exynos_cpu_suspend;

	return 0;
}

static struct subsys_interface exynos4_pm_interface = {
	.name		= "exynos_pm",
	.subsys		= &exynos4_subsys,
	.add_dev	= exynos_pm_add,
};

static struct subsys_interface exynos5_pm_interface = {
	.name		= "exynos_pm",
	.subsys		= &exynos5_subsys,
	.add_dev	= exynos_pm_add,
};

static struct syscore_ops exynos_pm_syscore_ops = {
	.suspend	= exynos_pm_suspend,
	.resume		= exynos_pm_resume,
};

static __init int exynos_pm_drvinit(void)
{
	struct clk *pll_base;

	s3c_pm_init();

	return subsys_interface_register(&exynos5_pm_interface);
}
arch_initcall(exynos_pm_drvinit);

static __init int exynos_pm_syscore_init(void)
{
	register_syscore_ops(&exynos_pm_syscore_ops);
	return 0;
}
arch_initcall(exynos_pm_syscore_init);
