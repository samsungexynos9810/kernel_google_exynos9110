/*
 * Copyright (c) 2010-2011 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <mach/map.h>

	.macro addruart, rp, rv, tmp
#ifdef CONFIG_SOC_EXYNOS5430
		ldr	\rp, =EXYNOS5430_PA_UART
#else
		mrc	p15, 0, \tmp, c0, c0, 0
		and	\tmp, \tmp, #0xf0
		teq	\tmp, #0xf0		@@ A15
		ldreq	\rp, =EXYNOS5_PA_UART
		movne	\rp, #EXYNOS4_PA_UART	@@ EXYNOS4
#endif
		ldr	\rv, =S3C_VA_UART
#if CONFIG_DEBUG_S3C_UART != 0
		add	\rp, \rp, #(0x10000 * CONFIG_DEBUG_S3C_UART)
		add	\rv, \rv, #(0x10000 * CONFIG_DEBUG_S3C_UART)
#endif
	.endm

#define fifo_full fifo_full_s5pv210
#define fifo_level fifo_level_s5pv210

#include <debug/samsung.S>
