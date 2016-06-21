/* drivers/gpu/mali400/mali/platform/exynos4270/exynos4.c
 *
 * Copyright 2011 by S.LSI. Samsung Electronics Inc.
 * San#24, Nongseo-Dong, Giheung-Gu, Yongin, Korea
 *
 * Samsung SoC Mali400 DVFS driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software FoundatIon.
 */

/**
 * @file exynos4.c
 * Platform specific Mali driver functions for the exynos 4XXX based platforms
 */

#ifdef CONFIG_PM_RUNTIME
#include <linux/pm_runtime.h>
#endif
#include <linux/mali/mali_utgard.h>
#include "mali_kernel_common.h"


#include <linux/irq.h>
#ifndef CONFIG_MALI_DT
#include <mach/irqs.h>
#include <plat/devs.h>
#endif
#include "mali_kernel_common.h"
#include "exynos3_pmm.h"
#include <linux/dma-mapping.h>

#ifndef CONFIG_MALI_DT
#define MALI_GP_IRQ       EXYNOS3_IRQ_GP_3D
#define MALI_PP0_IRQ      EXYNOS3_IRQ_PP0_3D
#define MALI_PP1_IRQ      EXYNOS3_IRQ_PP1_3D
#define MALI_PP2_IRQ      EXYNOS3_IRQ_PP2_3D
#define MALI_PP3_IRQ      EXYNOS3_IRQ_PP3_3D
#define MALI_GP_MMU_IRQ   EXYNOS3_IRQ_GPMMU_3D
#define MALI_PP0_MMU_IRQ  EXYNOS3_IRQ_PPMMU0_3D
#define MALI_PP1_MMU_IRQ  EXYNOS3_IRQ_PPMMU1_3D
#define MALI_PP2_MMU_IRQ  EXYNOS3_IRQ_PPMMU2_3D
#define MALI_PP3_MMU_IRQ  EXYNOS3_IRQ_PPMMU3_3D

static struct resource mali_gpu_resources[] =
{
	MALI_GPU_RESOURCES_MALI400_MP2(0x13000000,
	                               MALI_GP_IRQ, MALI_GP_MMU_IRQ,
	                               MALI_PP0_IRQ, MALI_PP0_MMU_IRQ,
	                               MALI_PP1_IRQ, MALI_PP1_MMU_IRQ)
};
#endif

//extern struct platform_device mali_gpu_device;

static struct mali_gpu_device_data mali_gpu_data =
{
	.shared_mem_size = 256 * 1024 * 1024, /* 256MB */
	.fb_start = 0x40000000,
	.fb_size = 0xb1000000,
	.utilization_interval = 1000, /* 1000ms */
	.utilization_callback = mali_gpu_utilization_handler,
};

#ifndef CONFIG_MALI_DT
int mali_platform_device_register(struct platform_device *exynos4_device_g3d)
{
	int err;

	MALI_DEBUG_PRINT(4, ("mali_platform_device_register() called\n"));

	/* Connect resources to the device */
	err = platform_device_add_resources(exynos4_device_g3d, mali_gpu_resources, sizeof(mali_gpu_resources) / sizeof(mali_gpu_resources[0]));
	if (0 == err)
	{
		err = platform_device_add_data(exynos4_device_g3d, &mali_gpu_data, sizeof(mali_gpu_data));
		if (0 == err)
		{
			mali_platform_init(exynos4_device_g3d);

#ifdef CONFIG_PM_RUNTIME
			pm_runtime_set_autosuspend_delay(&exynos4_device_g3d->dev, 1000);
			pm_runtime_use_autosuspend(&exynos4_device_g3d->dev);
			pm_runtime_enable(&exynos4_device_g3d->dev);
#endif
#ifdef CONFIG_MALI400_DEBUG_SYS
	if (mali400_create_sysfs_file(&exynos4_device_g3d->dev))
		return -ENOENT;
#endif /* CONFIG_MALI_T6XX_DEBUG_SYS */
			return 0;
		}

	}
	return err;
}

void mali_platform_device_unregister(struct platform_device *exynos4_device_g3d)
{
	MALI_DEBUG_PRINT(4, ("mali_platform_device_unregister() called\n"));

	mali_platform_deinit(exynos4_device_g3d);
#ifdef CONFIG_MALI400_DEBUG_SYS
	mali400_remove_sysfs_file(&exynos4_device_g3d->dev);
#endif /* CONFIG_MALI_T6XX_DEBUG_SYS */
}

#else /* CONFIG_MALI_DT */
int mali_platform_device_init(struct platform_device *device)
{
	//int num_pp_cores = 0;
	int err = -1;
	struct dma_map_ops *dma_ops = get_dma_ops(&device->dev);

	/* After kernel 3.15 device tree will default set dev
	 * related parameters in of_platform_device_create_pdata.
	 * But kernel changes from version to version,
	 * For example 3.10 didn't include device->dev.dma_mask parameter setting,
	 * if we didn't include here will cause dma_mapping error,
	 * but in kernel 3.15 it include  device->dev.dma_mask parameter setting,
	 * so it's better to set must need paramter by DDK itself.
	 */
	if (!device->dev.dma_mask)
		device->dev.dma_mask = &device->dev.coherent_dma_mask;
	device->dev.archdata.dma_ops = dma_ops;

	err = platform_device_add_data(device, &mali_gpu_data, sizeof(mali_gpu_data));

	if (0 == err) {
#ifdef CONFIG_PM_RUNTIME
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 37))
		pm_runtime_set_autosuspend_delay(&(device->dev), 1000);
		pm_runtime_use_autosuspend(&(device->dev));
#endif
		pm_runtime_enable(&(device->dev));
#endif
		MALI_DEBUG_ASSERT(0 < num_pp_cores);
	//	mali_core_scaling_init(num_pp_cores);
	}

	return err;
}

int mali_platform_device_deinit(struct platform_device *device)
{
	MALI_IGNORE(device);

	MALI_DEBUG_PRINT(4, ("mali_platform_device_deinit() called\n"));

	//mali_core_scaling_term();

	return 0;
}

#endif /* CONFIG_MALI_DT */


