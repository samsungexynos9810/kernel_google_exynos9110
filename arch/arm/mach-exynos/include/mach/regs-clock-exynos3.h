/* arch/arm/mach-exynos/include/mach/regs-clock-exynos3.h
 *
 * Copyright (c) 2014 Samsung Electronics Co., Ltd.
 *                                                http://www.samsung.com
 *
 * EXYNOS3 - Clock register definitions
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __ASM_ARCH_REGS_CLOCK_EXYNOS3_H
#define __ASM_ARCH_REGS_CLOCK_EXYNOS3_H __FILE__

#include <plat/cpu.h>
#include <mach/map.h>

#define EXYNOS3_CLK_BUS_TOP_REG(x)      (EXYNOS3_VA_CMU_BUS_TOP + (x)) /* 0x1003_0000*/
#define EXYNOS3_CPU_ISP_REG(x)          (EXYNOS3_VA_CMU_CPU_ISP + (x))     /* 0x1004_0000 */
#define EXYNOS3_MIF_R_REG(x)            (EXYNOS3_VA_CMU_ACP + (x)) 	    /* 0x1045_0000 */
#define EXYNOS3_MIF_L_REG(x)            (EXYNOS3_VA_CMU_DMC + (x))         /* 0x105c_0000 */

/* RIGHT_BUS */
#define EXYNOS3_CLKSRC_RIGHTBUS                 EXYNOS3_CLK_BUS_TOP_REG(0x8200)
#define EXYNOS3_CLKMUX_STAT_RIGHTBUS            EXYNOS3_CLK_BUS_TOP_REG(0x8400)
#define EXYNOS3_CLKDIV_RIGHTBUS                 EXYNOS3_CLK_BUS_TOP_REG(0x8500)
#define EXYNOS3_CLKDIV_STAT_RIGHTBUS            EXYNOS3_CLK_BUS_TOP_REG(0x8600)
#define EXYNOS3_CLKGATE_BUS_RIGHTBUS            EXYNOS3_CLK_BUS_TOP_REG(0x8700)
#define EXYNOS3_CLKGATE_BUS_PERIR               EXYNOS3_CLK_BUS_TOP_REG(0x8760)
#define EXYNOS3_CLKGATE_IP_RIGHTBUS             EXYNOS3_CLK_BUS_TOP_REG(0x8800)
#define EXYNOS3_CLKGATE_IP_PERIR                EXYNOS3_CLK_BUS_TOP_REG(0x8960)
#define EXYNOS3_CLKOUT_CMU_RIGHTBUS             EXYNOS3_CLK_BUS_TOP_REG(0x8A00)
#define EXYNOS3_CLKOUT_CMU_RIGHTBUS_DIV_STAT    EXYNOS3_CLK_BUS_TOP_REG(0x8A04)

#endif
