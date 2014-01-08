/*
 * Samsung Exynos5 SoC series FIMC-IS driver
 *
 *
 * Copyright (c) 2011 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef FIMC_IS_HW_H
#define FIMC_IS_HW_H

#define CSI_VIRTUAL_CH_0 0
#define CSI_VIRTUAL_CH_1 1
#define CSI_VIRTUAL_CH_2 2
#define CSI_VIRTUAL_CH_3 3

#define CSI_DATA_LANES_1 1
#define CSI_DATA_LANES_2 2
#define CSI_DATA_LANES_3 3
#define CSI_DATA_LANES_4 4

int csi_hw_reset(unsigned long __iomem *base_reg);
int csi_hw_s_settle(unsigned long __iomem *base_reg, u32 settle);
int csi_hw_s_datalane(unsigned long __iomem *base_reg, u32 lanes);
int csi_hw_s_config(unsigned long __iomem *base_reg, u32 channel, u32 pixelformat, u32 width, u32 height);
int csi_hw_s_interrupt(unsigned long __iomem *base_reg, bool on);
int csi_hw_enable(unsigned long __iomem *base_reg);
int csi_hw_disable(unsigned long __iomem *base_reg);

int fimc_is_runtime_suspend(struct device *dev);
int fimc_is_runtime_resume(struct device *dev);
#endif