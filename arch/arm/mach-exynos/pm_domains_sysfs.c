/* linux/arch/arm/mach-exynos/dev-runtime_pm.c
 *
 * Copyright (c) 2013 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com
 *
 * EXYNOS - Runtime PM Test Driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of_address.h>
#include <linux/of_platform.h>
#include <linux/sched.h>

#include <mach/pm_domains.h>

struct platform_device exynos_device_runtime_pm = {
	.name	= "runtime_pm_test",
	.id	= -1,
};

static ssize_t show_power_domain(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct device_node *np;
	struct platform_device *pdev;
	int ret = 0;

	for_each_compatible_node(np, NULL, "samsung,exynos5430-pd") {
		struct exynos_pm_domain *pd;

		/* skip unmanaged power domain */
		if (!of_device_is_available(np))
			continue;

		pdev = of_find_device_by_node(np);
		if (!pdev)
			continue;
		pd = platform_get_drvdata(pdev);
		ret += snprintf(buf+ret, PAGE_SIZE-ret, "%-8s - %-3s, ", pd->genpd.name, pd->check_status(pd) ? "on" : "off");
		ret += snprintf(buf+ret, PAGE_SIZE-ret, "%08x %08x %08x\n",
					__raw_readl(pd->base+0x0),
					__raw_readl(pd->base+0x4),
					__raw_readl(pd->base+0x8));
	}

	return ret;
}

static ssize_t store_power_domain_test(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	struct platform_device *pdev;
	struct device_node *np;
	char device_name[32];
	int ret;

	sscanf(buf, "%s", device_name);

	for_each_compatible_node(np, NULL, "samsung,exynos5430-pd") {
		struct exynos_pm_domain *pd;
		int i;

		/* skip unmanaged power domain */
		if (!of_device_is_available(np))
			continue;

		pdev = of_find_device_by_node(np);
		if (!pdev)
			continue;
		pd = platform_get_drvdata(pdev);

		if (strcmp(pd->genpd.name, device_name)) {
			continue;
		}

		if (pd->check_status(pd)) {
			pr_err("PM DOMAIN: %s is working. Stop testing\n", pd->genpd.name);
			break;
		}

		while (1) {
			ret = pm_genpd_add_device(&pd->genpd, dev);
			if (ret != -EAGAIN)
				break;
			cond_resched();
		}
		if (!ret) {
			pm_genpd_dev_need_restore(dev, true);
			pr_info("PM DOMAIN: %s, Device : %s Registered\n", pd->genpd.name, dev_name(dev));
		} else
			pr_err("PM DOMAIN: %s cannot add device %s\n", pd->genpd.name, dev_name(dev));

		pr_info("%s: test start.\n", pd->genpd.name);
		pm_runtime_enable(dev);
		for (i=0; i<100; i++) {
			pm_runtime_get_sync(dev);
			mdelay(50);
			pm_runtime_put_sync(dev);
			mdelay(50);
		}
		pr_info("%s: test done.\n", pd->genpd.name);
		pm_runtime_disable(dev);

		while (1) {
			ret = pm_genpd_remove_device(&pd->genpd, dev);
			if (ret != -EAGAIN)
				break;
			cond_resched();
		}
		if (ret)
			pr_err("PM DOMAIN: %s cannot remove device %s\n", pd->genpd.name, dev_name(dev));
	}

	return count;
}

static DEVICE_ATTR(control, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH, show_power_domain, store_power_domain_test);

static struct attribute *control_device_attrs[] = {
	&dev_attr_control.attr,
	NULL,
};

static const struct attribute_group control_device_attr_group = {
	.attrs = control_device_attrs,
};

static int runtime_pm_test_probe(struct platform_device *pdev)
{
	struct class *runtime_pm_class;
	struct device *runtime_pm_dev;
	int ret;

	runtime_pm_class = class_create(THIS_MODULE, "runtime_pm");
	runtime_pm_dev = device_create(runtime_pm_class, NULL, 0, NULL, "test");
	ret = sysfs_create_group(&runtime_pm_dev->kobj, &control_device_attr_group);
	if (ret) {
		pr_err("Runtime PM Test : error to create sysfs\n");
		return -EINVAL;
	}

	pm_runtime_enable(&pdev->dev);

	return 0;
}

static int runtime_pm_test_runtime_suspend(struct device *dev)
{
	pr_info("Runtime PM Test : Runtime_Suspend\n");
	return 0;
}

static int runtime_pm_test_runtime_resume(struct device *dev)
{
	pr_info("Runtime PM Test : Runtime_Resume\n");
	return 0;
}

static struct dev_pm_ops pm_ops = {
	.runtime_suspend = runtime_pm_test_runtime_suspend,
	.runtime_resume = runtime_pm_test_runtime_resume,
};

static struct platform_driver runtime_pm_test_driver = {
	.probe		= runtime_pm_test_probe,
	.driver		= {
		.name	= "runtime_pm_test",
		.owner	= THIS_MODULE,
		.pm	= &pm_ops,
	},
};

static int __init runtime_pm_test_driver_init(void)
{
	platform_device_register(&exynos_device_runtime_pm);

	return platform_driver_register(&runtime_pm_test_driver);
}
arch_initcall_sync(runtime_pm_test_driver_init);
