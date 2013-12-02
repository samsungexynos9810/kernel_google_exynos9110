/* linux/arch/arm/mach-exynos/asv-exynos5430.c
 *
 * Copyright (c) 2013 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com/
 *
 * EXYNOS5430 - ASV(Adoptive Support Voltage) driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/init.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/err.h>
#include <linux/clk.h>
#include <linux/io.h>
#include <linux/slab.h>

#include <mach/asv-exynos.h>
#include <mach/asv-exynos5430.h>
#include <mach/map.h>
#include <mach/regs-pmu.h>

#include <plat/cpu.h>

#define CHIP_ID2_REG			(S5P_VA_CHIPID)

#define ASV_ARM_SPEED_GRP_REG		(S5P_VA_CHIPID2 + 0x10)
#define ASV_ARM_MAIN_GRP_OFFSET		(0)

#define ASV_KFC_SPEED_GRP_REG		(S5P_VA_CHIPID2 + 0x14)
#define ASV_KFC_MAIN_GRP_OFFSET		(0)

#define ASV_G3D_MIF_SPEED_GRP_REG	(S5P_VA_CHIPID2 + 0x18)
#define ASV_G3D_MAIN_GRP_OFFSET		(0)
#define ASV_MIF_MAIN_GRP_OFFSET		(16)

#define ASV_INT_ISP_SPEED_GRP_REG	(S5P_VA_CHIPID2 + 0x1C)
#define ASV_INT_MAIN_GRP_OFFSET		(0)
#define ASV_CAM0_DISP_MAIN_GRP_OFFSET	(16)

#define ASV_SPEED_GRP_MASK		(0xFF)

static bool is_speedgroup;
static unsigned int speed_group[ASV_TYPE_END] = {0, };

#ifdef CONFIG_ASV_MARGIN_TEST
static int set_arm_volt = 0;
static int set_kfc_volt = 0;
static int set_int_volt = 0;
static int set_mif_volt = 0;
static int set_g3d_volt = 0;
static int set_isp_volt = 0;

static int __init get_arm_volt(char *str)
{
	get_option(&str, &set_arm_volt);
	return 0;
}
early_param("arm", get_arm_volt);

static int __init get_kfc_volt(char *str)
{
	get_option(&str, &set_kfc_volt);
	return 0;
}
early_param("kfc", get_kfc_volt);

static int __init get_int_volt(char *str)
{
	get_option(&str, &set_int_volt);
	return 0;
}
early_param("int", get_int_volt);

static int __init get_mif_volt(char *str)
{
	get_option(&str, &set_mif_volt);
	return 0;
}
early_param("mif", get_mif_volt);

static int __init get_g3d_volt(char *str)
{
	get_option(&str, &set_g3d_volt);
	return 0;
}
early_param("g3d", get_g3d_volt);

static int __init get_isp_volt(char *str)
{
	get_option(&str, &set_isp_volt);
	return 0;
}
early_param("isp", get_isp_volt);
#endif

static unsigned int exynos5430_get_asv_group_arm(struct asv_common *asv_comm)
{
	if (is_speedgroup)
		return speed_group[ID_ARM];

	return 0;
}

static void exynos5430_set_asv_info_arm(struct asv_info *asv_inform, bool show_value)
{
	unsigned int i;
	unsigned int target_asv_grp_nr = asv_inform->result_asv_grp;

	asv_inform->asv_volt = kmalloc((sizeof(struct asv_freq_table) * asv_inform->dvfs_level_nr), GFP_KERNEL);
	if (!asv_inform->asv_volt) {
		pr_err("%s: Memory allocation failed for asv voltage\n", __func__);
		return;
	}

	for (i = 0; i < asv_inform->dvfs_level_nr; i++) {
		asv_inform->asv_volt[i].asv_freq = arm_asv_volt_info[i][0];
#ifdef CONFIG_ASV_MARGIN_TEST
		asv_inform->asv_volt[i].asv_value =
			arm_asv_volt_info[i][target_asv_grp_nr + 1] + set_arm_volt;
#else
		asv_inform->asv_volt[i].asv_value = arm_asv_volt_info[i][target_asv_grp_nr + 1];
#endif
	}

	if (show_value) {
		for (i = 0; i < asv_inform->dvfs_level_nr; i++)
			pr_info("%s LV%d freq : %d volt : %d\n",
					asv_inform->name, i,
					asv_inform->asv_volt[i].asv_freq,
					asv_inform->asv_volt[i].asv_value);
	}
}

static struct asv_ops exynos5430_asv_ops_arm = {
	.get_asv_group	= exynos5430_get_asv_group_arm,
	.set_asv_info	= exynos5430_set_asv_info_arm,
};

static unsigned int exynos5430_get_asv_group_kfc(struct asv_common *asv_comm)
{
	if (is_speedgroup)
		return speed_group[ID_KFC];

	return 0;
}

static void exynos5430_set_asv_info_kfc(struct asv_info *asv_inform, bool show_value)
{
	unsigned int i;
	unsigned int target_asv_grp_nr = asv_inform->result_asv_grp;

	asv_inform->asv_volt = kmalloc((sizeof(struct asv_freq_table) * asv_inform->dvfs_level_nr), GFP_KERNEL);
	if (!asv_inform->asv_volt) {
		pr_err("%s: Memory allocation failed for asv voltage\n", __func__);
		return;
	}

	for (i = 0; i < asv_inform->dvfs_level_nr; i++) {
		asv_inform->asv_volt[i].asv_freq = kfc_asv_volt_info[i][0];
#ifdef CONFIG_ASV_MARGIN_TEST
		asv_inform->asv_volt[i].asv_value =
			kfc_asv_volt_info[i][target_asv_grp_nr + 1] + set_kfc_volt;
#else
		asv_inform->asv_volt[i].asv_value = kfc_asv_volt_info[i][target_asv_grp_nr + 1];
#endif
	}

	if (show_value) {
		for (i = 0; i < asv_inform->dvfs_level_nr; i++)
			pr_info("%s LV%d freq : %d volt : %d\n",
					asv_inform->name, i,
					asv_inform->asv_volt[i].asv_freq,
					asv_inform->asv_volt[i].asv_value);
	}
}

static struct asv_ops exynos5430_asv_ops_kfc = {
	.get_asv_group	= exynos5430_get_asv_group_kfc,
	.set_asv_info	= exynos5430_set_asv_info_kfc,
};

static unsigned int exynos5430_get_asv_group_int(struct asv_common *asv_comm)
{
	if (is_speedgroup)
		return speed_group[ID_INT];

	return 0;
}

static void exynos5430_set_asv_info_int(struct asv_info *asv_inform, bool show_value)
{
	unsigned int i;
	unsigned int target_asv_grp_nr = asv_inform->result_asv_grp;

	asv_inform->asv_volt = kmalloc((sizeof(struct asv_freq_table) * asv_inform->dvfs_level_nr), GFP_KERNEL);
	if (!asv_inform->asv_volt) {
		pr_err("%s: Memory allocation failed for asv voltage\n", __func__);
		return;
	}

	for (i = 0; i < asv_inform->dvfs_level_nr; i++) {
		asv_inform->asv_volt[i].asv_freq = int_asv_volt_info[i][0];
#ifdef CONFIG_ASV_MARGIN_TEST
		asv_inform->asv_volt[i].asv_value =
			int_asv_volt_info[i][target_asv_grp_nr + 1] + set_int_volt;
#else
		asv_inform->asv_volt[i].asv_value = int_asv_volt_info[i][target_asv_grp_nr + 1];
#endif
	}

	if (show_value) {
		for (i = 0; i < asv_inform->dvfs_level_nr; i++)
			pr_info("%s LV%d freq : %d volt : %d\n",
					asv_inform->name, i,
					asv_inform->asv_volt[i].asv_freq,
					asv_inform->asv_volt[i].asv_value);
	}
}

static struct asv_ops exynos5430_asv_ops_int = {
	.get_asv_group	= exynos5430_get_asv_group_int,
	.set_asv_info	= exynos5430_set_asv_info_int,
};

static unsigned int exynos5430_get_asv_group_mif(struct asv_common *asv_comm)
{
	if (is_speedgroup)
		return speed_group[ID_MIF];

	return 0;
}

static void exynos5430_set_asv_info_mif(struct asv_info *asv_inform, bool show_value)
{
	unsigned int i;
	unsigned int target_asv_grp_nr = asv_inform->result_asv_grp;

	asv_inform->asv_volt = kmalloc((sizeof(struct asv_freq_table) * asv_inform->dvfs_level_nr), GFP_KERNEL);
	if (!asv_inform->asv_volt) {
		pr_err("%s: Memory allocation failed for asv voltage\n", __func__);
		return;
	}

	for (i = 0; i < asv_inform->dvfs_level_nr; i++) {
		asv_inform->asv_volt[i].asv_freq = mif_asv_volt_info[i][0];
#ifdef CONFIG_ASV_MARGIN_TEST
		asv_inform->asv_volt[i].asv_value =
			mif_asv_volt_info[i][target_asv_grp_nr + 1] + set_mif_volt;
#else
		asv_inform->asv_volt[i].asv_value = mif_asv_volt_info[i][target_asv_grp_nr + 1];
#endif
	}

	if (show_value) {
		for (i = 0; i < asv_inform->dvfs_level_nr; i++)
			pr_info("%s LV%d freq : %d volt : %d\n",
					asv_inform->name, i,
					asv_inform->asv_volt[i].asv_freq,
					asv_inform->asv_volt[i].asv_value);
	}
}

static struct asv_ops exynos5430_asv_ops_mif = {
	.get_asv_group	= exynos5430_get_asv_group_mif,
	.set_asv_info	= exynos5430_set_asv_info_mif,
};

static unsigned int exynos5430_get_asv_group_g3d(struct asv_common *asv_comm)
{
	if (is_speedgroup)
		return speed_group[ID_G3D];

	return 0;
}

static void exynos5430_set_asv_info_g3d(struct asv_info *asv_inform, bool show_value)
{
	unsigned int i;
	unsigned int target_asv_grp_nr = asv_inform->result_asv_grp;

	asv_inform->asv_volt = kmalloc((sizeof(struct asv_freq_table) * asv_inform->dvfs_level_nr), GFP_KERNEL);
	if (!asv_inform->asv_volt) {
		pr_err("%s: Memory allocation failed for asv voltage\n", __func__);
		return;
	}

	for (i = 0; i < asv_inform->dvfs_level_nr; i++) {
		asv_inform->asv_volt[i].asv_freq = g3d_asv_volt_info[i][0];
#ifdef CONFIG_ASV_MARGIN_TEST
		asv_inform->asv_volt[i].asv_value =
			g3d_asv_volt_info[i][target_asv_grp_nr + 1] + set_g3d_volt;
#else
		asv_inform->asv_volt[i].asv_value = g3d_asv_volt_info[i][target_asv_grp_nr + 1];
#endif
	}

	if (show_value) {
		for (i = 0; i < asv_inform->dvfs_level_nr; i++)
			pr_info("%s LV%d freq : %d volt : %d\n",
					asv_inform->name, i,
					asv_inform->asv_volt[i].asv_freq,
					asv_inform->asv_volt[i].asv_value);
	}
}

static struct asv_ops exynos5430_asv_ops_g3d = {
	.get_asv_group	= exynos5430_get_asv_group_g3d,
	.set_asv_info	= exynos5430_set_asv_info_g3d,
};

static unsigned int exynos5430_get_asv_group_isp(struct asv_common *asv_comm)
{
	if (is_speedgroup)
		return speed_group[ID_ISP];

	return 0;
}

static void exynos5430_set_asv_info_isp(struct asv_info *asv_inform, bool show_value)
{
	unsigned int i;
	unsigned int target_asv_grp_nr = asv_inform->result_asv_grp;

	asv_inform->asv_volt = kmalloc((sizeof(struct asv_freq_table) * asv_inform->dvfs_level_nr), GFP_KERNEL);
	if (!asv_inform->asv_volt) {
		pr_err("%s: Memory allocation failed for asv voltage\n", __func__);
		return;
	}

	for (i = 0; i < asv_inform->dvfs_level_nr; i++) {
		asv_inform->asv_volt[i].asv_freq = isp_asv_volt_info[i][0];
#ifdef CONFIG_ASV_MARGIN_TEST
		asv_inform->asv_volt[i].asv_value =
			isp_asv_volt_info[i][target_asv_grp_nr + 1] + set_isp_volt;
#else
		asv_inform->asv_volt[i].asv_value = isp_asv_volt_info[i][target_asv_grp_nr + 1];
#endif
	}

	if (show_value) {
		for (i = 0; i < asv_inform->dvfs_level_nr; i++)
			pr_info("%s LV%d freq : %d volt : %d\n",
					asv_inform->name, i,
					asv_inform->asv_volt[i].asv_freq,
					asv_inform->asv_volt[i].asv_value);
	}
}

static struct asv_ops exynos5430_asv_ops_isp = {
	.get_asv_group	= exynos5430_get_asv_group_isp,
	.set_asv_info	= exynos5430_set_asv_info_isp,
};

struct asv_info exynos5430_asv_member[] = {
	{
		.asv_type	= ID_ARM,
		.name		= "VDD_ARM",
		.ops		= &exynos5430_asv_ops_arm,
		.asv_group_nr	= ASV_GRP_NR(ARM),
		.dvfs_level_nr	= DVFS_LEVEL_NR(ARM),
		.max_volt_value = MAX_VOLT(ARM),
	}, {
		.asv_type	= ID_KFC,
		.name		= "VDD_KFC",
		.ops		= &exynos5430_asv_ops_kfc,
		.asv_group_nr	= ASV_GRP_NR(KFC),
		.dvfs_level_nr	= DVFS_LEVEL_NR(KFC),
		.max_volt_value = MAX_VOLT(KFC),
	}, {
		.asv_type	= ID_INT,
		.name		= "VDD_INT",
		.ops		= &exynos5430_asv_ops_int,
		.asv_group_nr	= ASV_GRP_NR(INT),
		.dvfs_level_nr	= DVFS_LEVEL_NR(INT),
		.max_volt_value = MAX_VOLT(INT),
	}, {
		.asv_type	= ID_MIF,
		.name		= "VDD_MIF",
		.ops		= &exynos5430_asv_ops_mif,
		.asv_group_nr	= ASV_GRP_NR(MIF),
		.dvfs_level_nr	= DVFS_LEVEL_NR(MIF),
		.max_volt_value = MAX_VOLT(MIF),
	}, {
		.asv_type	= ID_G3D,
		.name		= "VDD_G3D",
		.ops		= &exynos5430_asv_ops_g3d,
		.asv_group_nr	= ASV_GRP_NR(G3D),
		.dvfs_level_nr	= DVFS_LEVEL_NR(G3D),
		.max_volt_value = MAX_VOLT(G3D),
	}, {
		.asv_type	= ID_ISP,
		.name		= "VDD_ISP",
		.ops		= &exynos5430_asv_ops_isp,
		.asv_group_nr	= ASV_GRP_NR(ISP),
		.dvfs_level_nr	= DVFS_LEVEL_NR(ISP),
		.max_volt_value = MAX_VOLT(ISP),
	},
};

unsigned int exynos5430_regist_asv_member(void)
{
	unsigned int i;

	/* Regist asv member into list */
	for (i = 0; i < ARRAY_SIZE(exynos5430_asv_member); i++)
		add_asv_member(&exynos5430_asv_member[i]);

	return 0;
}

int exynos5430_init_asv(struct asv_common *asv_info)
{
	unsigned int arm_speed_grp, kfc_speed_grp;
	unsigned int g3d_mif_speed_grp, int_isp_speed_grp;

	is_speedgroup = true;

	if (samsung_rev() == EXYNOS5430_REV_0) {
		pr_err("EXYNOS5430 ASV : cannot support Rev0\n");
		return -EINVAL;
	}

	arm_speed_grp = readl(ASV_ARM_SPEED_GRP_REG);
	kfc_speed_grp = readl(ASV_KFC_SPEED_GRP_REG);
	g3d_mif_speed_grp = readl(ASV_G3D_MIF_SPEED_GRP_REG);
	int_isp_speed_grp = readl(ASV_INT_ISP_SPEED_GRP_REG);

	speed_group[ID_ARM] = (arm_speed_grp >> ASV_ARM_MAIN_GRP_OFFSET) & ASV_SPEED_GRP_MASK;
	speed_group[ID_KFC] = (kfc_speed_grp >> ASV_KFC_MAIN_GRP_OFFSET) & ASV_SPEED_GRP_MASK;
	speed_group[ID_G3D] = (g3d_mif_speed_grp >> ASV_G3D_MAIN_GRP_OFFSET) & ASV_SPEED_GRP_MASK;
	speed_group[ID_MIF] = (g3d_mif_speed_grp >> ASV_MIF_MAIN_GRP_OFFSET) & ASV_SPEED_GRP_MASK;
	speed_group[ID_INT] = (int_isp_speed_grp >> ASV_INT_MAIN_GRP_OFFSET) & ASV_SPEED_GRP_MASK;
	speed_group[ID_ISP] = (int_isp_speed_grp >> ASV_CAM0_DISP_MAIN_GRP_OFFSET) & ASV_SPEED_GRP_MASK;

	if (speed_group[ID_ISP] == 0)
		speed_group[ID_ISP] = speed_group[ID_INT];

	pr_info("EXYNOS5430 ASV : ARM(%u), KFC(%u), G3D(%u), MIF(%u), INT(%u), ISP(%u)\n",
			speed_group[ID_ARM], speed_group[ID_KFC], speed_group[ID_G3D],
			speed_group[ID_MIF], speed_group[ID_INT], speed_group[ID_ISP]);

	asv_info->regist_asv_member = exynos5430_regist_asv_member;

	return 0;
}
