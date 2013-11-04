/* linux/arch/arm/mach-exynos/pmu-exynos5422.c
 *
 * Copyright (c) 2013 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com/
 *
 * EXYNOS5422 - EXYNOS PMU(Power Management Unit) support
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/io.h>
#include <linux/cpumask.h>
#include <linux/kernel.h>
#include <linux/bug.h>

#include <mach/regs-clock.h>
#include <mach/regs-pmu.h>
#include <mach/pmu.h>
#include <asm/cputype.h>
#include <asm/smp_plat.h>
#include <asm/topology.h>
#include "common.h"

#define REG_CPU_STATE_ADDR     (S5P_VA_SYSRAM_NS + 0x28)

static struct exynos_pmu_conf *exynos_pmu_config;

void set_boot_flag(unsigned int cpu, unsigned int mode)
{
	unsigned int phys_cpu = cpu_logical_map(cpu);
	unsigned int tmp, core, cluster;
	void __iomem *addr;

	core = MPIDR_AFFINITY_LEVEL(phys_cpu, 0);
	cluster = MPIDR_AFFINITY_LEVEL(phys_cpu, 1);
	addr = REG_CPU_STATE_ADDR + 4 * (core + cluster * 4);

	tmp = __raw_readl(addr);

	if (mode & BOOT_MODE_MASK)
		tmp &= ~BOOT_MODE_MASK;
	else
		BUG_ON(mode == 0);

	tmp |= mode;
	__raw_writel(tmp, addr);
}

void clear_boot_flag(unsigned int cpu, unsigned int mode)
{
	unsigned int phys_cpu = cpu_logical_map(cpu);
	unsigned int tmp, core, cluster;
	void __iomem *addr;

	BUG_ON(mode == 0);

	core = MPIDR_AFFINITY_LEVEL(phys_cpu, 0);
	cluster = MPIDR_AFFINITY_LEVEL(phys_cpu, 1);
	addr = REG_CPU_STATE_ADDR + 4 * (core + cluster * 4);

	tmp = __raw_readl(addr);
	tmp &= ~mode;
	__raw_writel(tmp, addr);
}

void exynos5422_secondary_up(unsigned int cpu_id)
{
	unsigned int phys_cpu = cpu_logical_map(cpu_id);
	unsigned int tmp, core, cluster;
	void __iomem *addr;

	core = MPIDR_AFFINITY_LEVEL(phys_cpu, 0);
	cluster = MPIDR_AFFINITY_LEVEL(phys_cpu, 1);

	addr = EXYNOS_ARM_CORE_CONFIGURATION(core + (4 * cluster));

	tmp = __raw_readl(addr);
	tmp |= EXYNOS_CORE_LOCAL_PWR_EN;

	__raw_writel(tmp, addr);
}

void exynos5422_cpu_up(unsigned int cpu_id)
{
	unsigned int phys_cpu = cpu_logical_map(cpu_id);
	unsigned int tmp, core, cluster;
	void __iomem *addr;

	core = MPIDR_AFFINITY_LEVEL(phys_cpu, 0);
	cluster = MPIDR_AFFINITY_LEVEL(phys_cpu, 1);

	addr = EXYNOS_ARM_CORE_CONFIGURATION(core + (4 * cluster));

	tmp = __raw_readl(addr);
	tmp |= EXYNOS_CORE_LOCAL_PWR_EN;
	__raw_writel(tmp, addr);
}

void exynos5422_cpu_down(unsigned int cpu_id)
{
	unsigned int phys_cpu = cpu_logical_map(cpu_id);
	unsigned int tmp, core, cluster;
	void __iomem *addr;

	core = MPIDR_AFFINITY_LEVEL(phys_cpu, 0);
	cluster = MPIDR_AFFINITY_LEVEL(phys_cpu, 1);

	addr = EXYNOS_ARM_CORE_CONFIGURATION(core + (4 * cluster));

	tmp = __raw_readl(addr);
	tmp &= ~(EXYNOS_CORE_LOCAL_PWR_EN);
	__raw_writel(tmp, addr);
}

unsigned int exynos5422_cpu_state(unsigned int cpu_id)
{
	unsigned int phys_cpu = cpu_logical_map(cpu_id);
	unsigned int core, cluster, val;

	core = MPIDR_AFFINITY_LEVEL(phys_cpu, 0);
	cluster = MPIDR_AFFINITY_LEVEL(phys_cpu, 1);

	val = __raw_readl(EXYNOS_ARM_CORE_STATUS(core + (4 * cluster)))
						& EXYNOS_CORE_LOCAL_PWR_EN;

	return val == EXYNOS_CORE_LOCAL_PWR_EN;
}

extern struct cpumask hmp_slow_cpu_mask;
extern struct cpumask hmp_fast_cpu_mask;

#define cpu_online_hmp(cpu, mask)      cpumask_test_cpu((cpu), mask)

bool exynos5422_is_last_core(unsigned int cpu)
{
	unsigned int cluster = MPIDR_AFFINITY_LEVEL(cpu_logical_map(cpu), 1);
	unsigned int cpu_id;
	struct cpumask mask, mask_and_online;

	if (cluster)
		cpumask_copy(&mask, &hmp_slow_cpu_mask);
	else
		cpumask_copy(&mask, &hmp_fast_cpu_mask);

	cpumask_and(&mask_and_online, &mask, cpu_online_mask);

	for_each_cpu(cpu_id, &mask) {
		if (cpu_id == cpu)
			continue;
		if (cpu_online_hmp(cpu_id, &mask_and_online))
			return false;
	}

	return true;
}

int __init exynos5422_pmu_init(void)
{
	exynos_cpu.power_up = exynos5422_secondary_up;
	exynos_cpu.power_state = exynos5422_cpu_state;
	exynos_cpu.power_down = exynos5422_cpu_down;
	exynos_cpu.is_last_core = exynos5422_is_last_core;

	if (exynos_pmu_config != NULL)
		pr_info("EXYNOS5422 PMU Initialize\n");
	else
		pr_info("EXYNOS: PMU not supported\n");

	return 0;
}
