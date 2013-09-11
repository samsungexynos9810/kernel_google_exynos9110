/*
 * Copyright (c) 2013 Samsung Electronics Co., Ltd.
 *      http://www.samsung.com
 *
 * EXYNOS - support to view information of big.LITTLE switcher
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/device.h>
#include <linux/fs.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/kobject.h>
#include <linux/sysfs.h>
#include <linux/moduleparam.h>
#include <mach/regs-pmu.h>
#include "common.h"

static struct bus_type core_subsys = {
	.name = "b.L",
	.dev_name = "b.L",
};

unsigned int (*cpu_state)(unsigned int cpu_id);

static ssize_t exynos5_core_status_show(struct kobject *kobj,
			struct kobj_attribute *attr, char *buf)
{
	ssize_t n = 0;
	int cpu;

	for_each_possible_cpu(cpu) {
		unsigned int v = cpu_state(cpu);
		n += scnprintf(buf + n, 11, "cpu %d : %d\n", cpu,
				v == EXYNOS_CORE_PWR_EN ? 1 : 0);
	}

	return n;
}

static struct kobj_attribute exynos5_core_status_attr =
	__ATTR(core_status, 0644, exynos5_core_status_show, NULL);

static struct attribute *exynos5_core_sysfs_attrs[] = {
	&exynos5_core_status_attr.attr,
	NULL,
};

static struct attribute_group exynos5_core_sysfs_group = {
	.attrs = exynos5_core_sysfs_attrs,
};

static const struct attribute_group *exynos5_core_sysfs_groups[] = {
	&exynos5_core_sysfs_group,
	NULL,
};

static int __init exynos5_core_sysfs_init(void)
{
	int ret = 0;

	ret = subsys_system_register(&core_subsys, exynos5_core_sysfs_groups);
	if (ret)
		pr_err("Fail to register exynos5 core subsys\n");

	if (of_machine_is_compatible("samsung,exynos5430"))
		cpu_state = exynos5430_cpu_state;

	return ret;
}

late_initcall(exynos5_core_sysfs_init);
