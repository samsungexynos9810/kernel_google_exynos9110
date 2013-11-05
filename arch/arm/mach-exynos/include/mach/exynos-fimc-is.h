/* linux/arch/arm/plat-s5p/include/plat/fimc_is.h
 *
 * Copyright (C) 2011 Samsung Electronics, Co. Ltd
 *
 * Exynos 4 series FIMC-IS slave device support
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#ifndef EXYNOS_FIMC_IS_H_
#define EXYNOS_FIMC_IS_H_ __FILE__

#define FIMC_IS_MAKE_QOS_IDX_NM(__LV) __LV ## _IDX
#define FIMC_IS_DECLARE_QOS_ENUM(__TYPE) enum FIMC_IS_DVFS_ ## __TYPE ## _LV_IDX

#include <linux/videodev2.h>
#include <linux/platform_device.h>

#define FIMC_IS_DEV_NAME		"exynos-fimc-is"

/* FIMC-IS DVFS LEVEL enum (INT, MIF, I2C) */
enum FIMC_IS_INT_LV {
	FIMC_IS_INT_L0,
	FIMC_IS_INT_L1,
	FIMC_IS_INT_L1_1,
	FIMC_IS_INT_L1_2,
	FIMC_IS_INT_L1_3,
};

enum FIMC_IS_MIF_LV {
	FIMC_IS_MIF_L0,
	FIMC_IS_MIF_L1,
	FIMC_IS_MIF_L2,
	FIMC_IS_MIF_L3,
	FIMC_IS_MIF_L4,
};

/*
 * On some soc, It needs to notify change of INT clock to F/W.
 * Because I2C clock can be take affect from other clock change(like INT)
 */
enum FIMC_IS_I2C_LV {
	FIMC_IS_I2C_L0,
	FIMC_IS_I2C_L1,
	FIMC_IS_I2C_L1_1,
	FIMC_IS_I2C_L1_3,
};

/* FIMC-IS DVFS SCENARIO enum */
enum FIMC_IS_SCENARIO_ID {
	FIMC_IS_SN_DEFAULT,
	FIMC_IS_SN_FRONT_PREVIEW,
	FIMC_IS_SN_FRONT_CAPTURE,
	FIMC_IS_SN_FRONT_CAMCORDING,
	FIMC_IS_SN_FRONT_VT1,
	FIMC_IS_SN_FRONT_VT2,
	FIMC_IS_SN_REAR_PREVIEW,
	FIMC_IS_SN_REAR_CAPTURE,
	FIMC_IS_SN_REAR_CAMCORDING,
	FIMC_IS_SN_DUAL_PREVIEW,
	FIMC_IS_SN_DUAL_CAPTURE,
	FIMC_IS_SN_DUAL_CAMCORDING,
	FIMC_IS_SN_HIGH_SPEED_FPS,
	FIMC_IS_SN_DIS_ENABLE,
	FIMC_IS_SN_MAX,
};

/**
* struct exynos_platform_fimc_is - camera host interface platform data
*
* @isp_info: properties of camera sensor required for host interface setup
*/
struct exynos_platform_fimc_is {
	int	hw_ver;
	int	(*cfg_gpio)(struct platform_device *pdev, int channel, bool flag_on);
	int	(*clk_cfg)(struct platform_device *pdev);
	int	(*clk_on)(struct platform_device *pdev);
	int	(*clk_off)(struct platform_device *pdev);
	int	(*print_cfg)(struct platform_device *pdev, u32 channel);

	/* These fields are to return qos value for dvfs scenario */
	u32	*int_qos_table;
	u32	*mif_qos_table;
	u32	*i2c_qos_table;
	int	(*get_int_qos)(int scenario_id);
	int	(*get_mif_qos)(int scenario_id);
	int	(*get_i2c_qos)(int scenario_id);
};

extern void exynos5_fimc_is_set_platdata(struct exynos_platform_fimc_is *pd);

int fimc_is_set_parent_dt(struct platform_device *pdev,
	const char *child, const char *parent);
struct clk *fimc_is_get_parent_dt(struct platform_device *pdev,
	const char *child);
int fimc_is_set_rate_dt(struct platform_device *pdev,
	const char *conid, unsigned int rate);
unsigned int  fimc_is_get_rate_dt(struct platform_device *pdev,
	const char *conid);


/* platform specific clock functions */
#if defined(CONFIG_SOC_EXYNOS5250)
extern int exynos5250_fimc_is_cfg_clk(struct platform_device *pdev);
extern int exynos5250_fimc_is_clk_on(struct platform_device *pdev);
extern int exynos5250_fimc_is_clk_off(struct platform_device *pdev);
#elif defined(CONFIG_SOC_EXYNOS5410)
extern int exynos5410_fimc_is_cfg_clk(struct platform_device *pdev);
extern int exynos5410_fimc_is_clk_on(struct platform_device *pdev);
extern int exynos5410_fimc_is_clk_off(struct platform_device *pdev);
extern int exynos5410_fimc_is_sensor_clk_on(struct platform_device *pdev, u32 source);
extern int exynos5410_fimc_is_sensor_clk_off(struct platform_device *pdev, u32 source);
#elif defined(CONFIG_SOC_EXYNOS5420)
extern int exynos5420_fimc_is_cfg_clk(struct platform_device *pdev);
extern int exynos5420_fimc_is_clk_on(struct platform_device *pdev);
extern int exynos5420_fimc_is_clk_off(struct platform_device *pdev);
extern int exynos5420_fimc_is_sensor_clk_on(struct platform_device *pdev, u32 source);
extern int exynos5420_fimc_is_sensor_clk_off(struct platform_device *pdev, u32 source);
#elif defined(CONFIG_SOC_EXYNOS5430)
extern int exynos5430_fimc_is_cfg_clk(struct platform_device *pdev);
extern int exynos5430_fimc_is_clk_on(struct platform_device *pdev);
extern int exynos5430_fimc_is_clk_off(struct platform_device *pdev);
extern int exynos5430_fimc_is_sensor_clk_on(struct platform_device *pdev, u32 source);
extern int exynos5430_fimc_is_sensor_clk_off(struct platform_device *pdev, u32 source);
#endif
extern int exynos5_fimc_is_sensor_power_on(struct platform_device *pdev, int sensor_id);
extern int exynos5_fimc_is_sensor_power_off(struct platform_device *pdev, int sensor_id);
extern int exynos5_fimc_is_print_cfg(struct platform_device *pdev, u32 channel);

#endif /* EXYNOS_FIMC_IS_H_ */
