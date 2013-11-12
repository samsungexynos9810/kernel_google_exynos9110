/* linux arch/arm/mach-exynos4/hotplug.c
 *
 *  Cloned from linux/arch/arm/mach-realview/hotplug.c
 *
 *  Copyright (C) 2002 ARM Ltd.
 *  All Rights Reserved
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/smp.h>
#include <linux/io.h>

#include <asm/cacheflush.h>
#include <asm/cp15.h>
#include <asm/smp_plat.h>

#include <mach/regs-pmu.h>
#include <mach/pmu.h>
#include <mach/smc.h>
#include <plat/cpu.h>

#include "common.h"

#define L2_CCI_OFF (1 << 1)
#define CHECK_CCI_SNOOP (1 << 7)

static inline void cpu_enter_lowpower_a9(void)
{
	unsigned int v;

	asm volatile(
	"	mcr	p15, 0, %1, c7, c5, 0\n"
	"	mcr	p15, 0, %1, c7, c10, 4\n"
	/*
	 * Turn off coherency
	 */
	"	mrc	p15, 0, %0, c1, c0, 1\n"
	"	bic	%0, %0, %3\n"
	"	mcr	p15, 0, %0, c1, c0, 1\n"
	"	mrc	p15, 0, %0, c1, c0, 0\n"
	"	bic	%0, %0, %2\n"
	"	mcr	p15, 0, %0, c1, c0, 0\n"
	  : "=&r" (v)
	  : "r" (0), "Ir" (CR_C), "Ir" (0x40)
	  : "cc");
}

static inline void cpu_enter_lowpower_a15(void)
{
	unsigned int v;

	asm volatile(
	"	mrc	p15, 0, %0, c1, c0, 0\n"
	"	bic	%0, %0, %1\n"
	"	mcr	p15, 0, %0, c1, c0, 0\n"
	  : "=&r" (v)
	  : "Ir" (CR_C)
	  : "cc");

	flush_cache_louis();

	asm volatile(
	/*
	* Turn off coherency
	*/
	"       clrex\n"
	"	mrc	p15, 0, %0, c1, c0, 1\n"
	"	bic	%0, %0, %1\n"
	"	mcr	p15, 0, %0, c1, c0, 1\n"
	: "=&r" (v)
	: "Ir" (0x40)
	: "cc");

	isb();
	dsb();
}

static inline void cpu_leave_lowpower(void)
{
	unsigned int v;

	asm volatile(
	"mrc	p15, 0, %0, c1, c0, 0\n"
	"	orr	%0, %0, %1\n"
	"	mcr	p15, 0, %0, c1, c0, 0\n"
	"	mrc	p15, 0, %0, c1, c0, 1\n"
	"	orr	%0, %0, %2\n"
	"	mcr	p15, 0, %0, c1, c0, 1\n"
	  : "=&r" (v)
	  : "Ir" (CR_C), "Ir" (0x40)
	  : "cc");
}

void exynos_power_down_cpu(unsigned int cpu)
{
	if (soc_is_exynos5422()) {
		struct cpumask mask;
		int cluster_id = (read_cpuid_mpidr() >> 8) & 0xff;

		exynos_cpu.power_down(cpu);

		if (cluster_id == 0) {
			set_boot_flag(cpu, CHECK_CCI_SNOOP);
			if (!cpumask_and(&mask, cpu_online_mask, cpu_coregroup_mask(cpu))) {
				__raw_writel(0, EXYNOS_COMMON_CONFIGURATION(cluster_id));
				exynos_smc(SMC_CMD_SHUTDOWN, OP_TYPE_CLUSTER, SMC_POWERSTATE_IDLE, L2_CCI_OFF);
			}
		}
	} else {
		u32 non_boot_cluster = MPIDR_AFFINITY_LEVEL(cpu_logical_map(4), 1);
		unsigned int sif = non_boot_cluster ? 4 : 3;

		set_boot_flag(cpu, HOTPLUG);

		if (exynos_cpu.is_last_core(cpu)) {
			flush_cache_all();
			cci_snoop_disable(sif);
		}

		exynos_cpu.power_down(cpu);
	}
}

static inline void platform_do_lowpower(unsigned int cpu, int *spurious)
{
	for (;;) {

		/* make secondary cpus to be turned off at next WFI command */
		exynos_power_down_cpu(cpu);

		/*
		 * here's the WFI
		 */
		asm(".word	0xe320f003\n"
		    :
		    :
		    : "memory", "cc");

		if (pen_release == cpu_logical_map(cpu)) {
			/*
			 * OK, proper wakeup, we're done
			 */
			break;
		}

		/*
		 * Getting here, means that we have come out of WFI without
		 * having been woken up - this shouldn't happen
		 *
		 * Just note it happening - when we're woken, we can report
		 * its occurrence.
		 */
		(*spurious)++;
	}
}

/*
 * platform-specific code to shutdown a CPU
 *
 * Called with IRQs disabled
 */
void __ref exynos_cpu_die(unsigned int cpu)
{
	int spurious = 0;
	int primary_part = 0;

	/*
	 * we're ready for shutdown now, so do it.
	 * Exynos4 is A9 based while Exynos5 is A15; check the CPU part
	 * number by reading the Main ID register and then perform the
	 * appropriate sequence for entering low power.
	 */
	asm("mrc p15, 0, %0, c0, c0, 0" : "=r"(primary_part) : : "cc");
	primary_part &= 0xfff0;
	if ((primary_part == 0xc0f0) || (primary_part == 0xc070))
		cpu_enter_lowpower_a15();
	else
		cpu_enter_lowpower_a9();

	platform_do_lowpower(cpu, &spurious);

	/*
	 * bring this CPU back into the world of cache
	 * coherency, and then restore interrupts
	 */
	cpu_leave_lowpower();

	if (spurious)
		pr_warn("CPU%u: %u spurious wakeup calls\n", cpu, spurious);
}
