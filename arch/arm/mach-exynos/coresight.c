/*
 * Copyright (c) 2013 Samsung Electronics Co., Ltd.
 *      http://www.samsung.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/device.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/kobject.h>
#include <linux/sysfs.h>
#include <linux/moduleparam.h>
#include <asm/cputype.h>
#include <asm/smp_plat.h>

#include "common.h"

#define CS_OSLOCK		0x300
#define CS_OSLOCK_CHECK	0x304
#define CS_PC_VAL		0xA0
#define CS_SJTAG_STAT		0x8004

struct exynos_cs {
	void __iomem	*base;
	void __iomem	*kfc_src_base;
	unsigned int	offset[2];
};

static struct exynos_cs cs;
static DEFINE_SPINLOCK(lock);

/*
 * This function will be removed after fixing
 * issue of coresight debug port.
 */
static void kfc_mux_changing(int en)
{
	unsigned int clk_src_kfc = readl(cs.kfc_src_base);

	if (!en)
		clk_src_kfc &= ~(0x1 << 0);
	else
		clk_src_kfc |= (0x1 << 0);

	writel(clk_src_kfc, cs.kfc_src_base);
}

void exynos_cs_show_pcval(void)
{
	unsigned int sjtag_stat;
	unsigned long flags;
	int cpu, val;
	int offset;
	int phys_cpu;

	if (!cs.base)
		return;

	spin_lock_irqsave(&lock, flags);

	/* To check whether SJTAG is fused or not. */
	sjtag_stat = readl(cs.base + CS_SJTAG_STAT);
	if (sjtag_stat & (0x1 << 2))
		return;

	kfc_mux_changing(0);

	for (cpu = 0; cpu < nr_cpu_ids; cpu++) {
		void __iomem *base;
		unsigned int core, cluster;

		phys_cpu = cpu_logical_map(cpu);
		core = MPIDR_AFFINITY_LEVEL(phys_cpu, 0);
		cluster = MPIDR_AFFINITY_LEVEL(phys_cpu, 1);

		if (cluster)
			offset = cs.offset[0] + core * 0x2000;
		else
			offset = cs.offset[1] + core * 0x2000;

		if (!exynos_cpu.power_state(cpu)) {
			pr_err("%s: CPU[%d] Power down.\n", __func__,
			cpu);
			continue;
		}

		base = cs.base + offset;

		/* Release OSlock */
		writel(0x1, base + CS_OSLOCK);

		/* Read current PC value */
		val = __raw_readl(base + CS_PC_VAL);

		if (!cluster) {
			/* The PCSR of A15 shoud be substracted 0x8 from
			 * curretnly PCSR value */
			val -= 0x8;
		}

		pr_err("%s: CPU[%d] PC Val 0x%x\n", __func__, cpu, val);
	}

	kfc_mux_changing(1);
	spin_unlock_irqrestore(&lock, flags);
}
EXPORT_SYMBOL(exynos_cs_show_pcval);

static int exynos_cs_init_dt(struct device *dev)
{
	struct device_node *node = NULL;
	const unsigned int *cs_reg;
	const unsigned int *kfc_src_reg;
	unsigned int cs_addr, kfc_src_addr;
	int len, i = 0;

	if (!dev->of_node) {
		pr_err("Couldn't get device node!\n");
		return -ENODEV;
	}

	cs_reg = of_get_property(dev->of_node, "reg", &len);
	if (!cs_reg)
		return -ESPIPE;

	cs_addr = be32_to_cpup(cs_reg);

	kfc_src_reg = of_get_property(dev->of_node, "reg1", &len);
	if (!kfc_src_reg)
		return -ESPIPE;

	kfc_src_addr = be32_to_cpup(kfc_src_reg);

	cs.base = devm_ioremap(dev, cs_addr, SZ_256K);
	if (!cs.base)
		return -ENOMEM;

	cs.kfc_src_base = devm_ioremap(dev, kfc_src_addr, SZ_4);
	if (!cs.kfc_src_base)
		return -ENOMEM;

	while ((node = of_find_node_by_type(node, "cs"))) {
		const unsigned int *offset;

		offset = of_get_property(node, "offset", &len);
		if (!offset)
			return -ESPIPE;

		cs.offset[i] = be32_to_cpup(offset);

		i++;
	}

	return 0;
}

static int exynos_cs_probe(struct platform_device *pdev)
{
	int ret = 0;

	pr_debug("%s\n", __func__);

	if (!pdev->dev.of_node) {
		pr_crit("[%s]No such device\n", __func__);
		return -ENODEV;
	}

	ret = exynos_cs_init_dt(&pdev->dev);
	if (ret) {
		pr_err("Error to init dt %d\n", ret);
		return -ENODEV;
	}

	return ret;
}

static const struct of_device_id of_exynos_cs_matches[] = {
	{.compatible = "exynos,coresight"},
	{},
};
MODULE_DEVICE_TABLE(of, of_exynos_cs_id);

static struct platform_driver exynos_cs_driver = {
	.driver = {
		.owner = THIS_MODULE,
		.name = "coresight",
		.of_match_table = of_match_ptr(of_exynos_cs_matches),
	},
	.probe = exynos_cs_probe,
};

module_platform_driver(exynos_cs_driver);

