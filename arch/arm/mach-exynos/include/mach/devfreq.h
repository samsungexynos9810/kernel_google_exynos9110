/* linux/arch/arm/mach-exynos/include/mach/exynos-devfreq.h
 *
 * Copyright (c) 2012 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/
#ifndef __EXYNOS_DEVFREQ_H_
#define __EXYNOS_DEVFREQ_H_
enum devfreq_media_type {
	TYPE_FIMC_LITE,
	TYPE_MIXER,
	TYPE_DECON,
	TYPE_TV,
	TYPE_GSCL_LOCAL,
	TYPE_RESOLUTION,
};

enum devfreq_media_resolution {
	RESOLUTION_FULLHD,
	RESOLUTION_WQHD
};

enum devfreq_layer_count {
	NUM_LAYER_0,
	NUM_LAYER_1,
	NUM_LAYER_2,
	NUM_LAYER_3,
	NUM_LAYER_4,
	NUM_LAYER_5,
};

#ifdef CONFIG_ARM_EXYNOS5430_BUS_DEVFREQ
void exynos5_update_media_layers(enum devfreq_media_type media_type, unsigned int value);
#endif

#endif
