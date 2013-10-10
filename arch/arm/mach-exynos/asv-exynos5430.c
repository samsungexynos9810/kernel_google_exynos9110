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

#define CHIP_ID_REG		(S5P_VA_CHIPID)
#define CHIP_MAIN_REV_SHIFT	(4)
#define CHIP_MAIN_REV_MASK	(0xF << CHIP_MAIN_REV_SHIFT)

#define TEMP_ASV_GROUP_BASE	(EXYNOS_PA_CHIPID + 0x4030)
#define ASV_TBL3_MASK		(0xF)
#define TEMP_ASV_GROUP_5	(0x5)
#define TEMP_ASV_GROUP_10	(0xA)
#define TEMP_ASV_GROUP_11	(0xB)
#define TEMP_ASV_GROUP_12	(0xC)

enum chip_main_rev {
	EXYNOS5430_EVT0,
	EXYNOS5430_EVT1,
};

static unsigned int temp_asv_group;

static bool is_speedgroup;
static unsigned special_lot_group;

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
early_param("g3d", get_g3d_volt);
#endif

#if 0
static void exynos5430_set_abb(struct asv_info *asv_inform)
{
	void __iomem *target_reg;
	unsigned int target_value;

	switch (asv_inform->asv_type) {
	case ID_ARM:
		target_reg = EXYNOS5430_BIAS_CON_ARM;
		target_value = arm_asv_abb_info[asv_inform->result_asv_grp];
		break;
	case ID_KFC:
		target_reg = EXYNOS5430_BIAS_CON_KFC;
		target_value = kfc_asv_abb_info[asv_inform->result_asv_grp];
		break;
	case ID_INT:
		target_reg = EXYNOS5430_BIAS_CON_INT;
		target_value = int_asv_abb_info[asv_inform->result_asv_grp];
		break;
	case ID_MIF:
		target_reg = EXYNOS5430_BIAS_CON_MIF;
		target_value = mif_asv_abb_info[asv_inform->result_asv_grp];
		break;
	case ID_G3D:
		target_reg = EXYNOS5430_BIAS_CON_G3D;
		target_value = g3d_asv_abb_info[asv_inform->result_asv_grp];
		break;
	case ID_ISP:
		target_reg = EXYNOS5430_BIAS_CON_ISP;
		target_value = isp_asv_abb_info[asv_inform->result_asv_grp];
		break;
	default:
		return;
	}

	set_abb(target_reg, target_value);
}
#endif

static unsigned int exynos5430_get_asv_group_arm(struct asv_common *asv_comm)
{
	unsigned int i;
	struct asv_info *target_asv_info = asv_get(ID_ARM);

	if (is_speedgroup)
		return special_lot_group;

	if (temp_asv_group)
		return temp_asv_group;

	for (i = 0; i < target_asv_info->asv_group_nr; i++) {
		if (refer_use_table_get_asv[0][i] &&
			asv_comm->ids_value <= refer_table_get_asv[0][i])
			return i;

		if (refer_use_table_get_asv[1][i] &&
			asv_comm->hpm_value <= refer_table_get_asv[1][i])
			return i;
	}

	return 0;
}

static void exynos5430_set_asv_info_arm(struct asv_info *asv_inform, bool show_value)
{
	unsigned int i;
	unsigned int target_asv_grp_nr = asv_inform->result_asv_grp;

#if 0
	exynos5430_set_abb(asv_inform);
#endif

	asv_inform->asv_volt = kmalloc((sizeof(struct asv_freq_table) * asv_inform->dvfs_level_nr), GFP_KERNEL);
	asv_inform->asv_abb  = kmalloc((sizeof(struct asv_freq_table) * asv_inform->dvfs_level_nr), GFP_KERNEL);

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
			pr_info("%s LV%d freq : %d volt : %d abb : %d\n",
					asv_inform->name, i,
					asv_inform->asv_volt[i].asv_freq,
					asv_inform->asv_volt[i].asv_value,
					asv_inform->asv_abb[i].asv_value);
	}
}

static struct asv_ops exynos5430_asv_ops_arm = {
	.get_asv_group	= exynos5430_get_asv_group_arm,
	.set_asv_info	= exynos5430_set_asv_info_arm,
};

static unsigned int exynos5430_get_asv_group_kfc(struct asv_common *asv_comm)
{
	unsigned int i;
	struct asv_info *target_asv_info = asv_get(ID_KFC);

	if (is_speedgroup)
		return special_lot_group;

	if (temp_asv_group)
		return temp_asv_group;

	for (i = 0; i < target_asv_info->asv_group_nr; i++) {
		if (refer_use_table_get_asv[0][i] &&
			asv_comm->ids_value <= refer_table_get_asv[0][i])
			return i;

		if (refer_use_table_get_asv[1][i] &&
			asv_comm->hpm_value <= refer_table_get_asv[1][i])
			return i;
	}

	return 0;
}

static void exynos5430_set_asv_info_kfc(struct asv_info *asv_inform, bool show_value)
{
	unsigned int i;
	unsigned int target_asv_grp_nr = asv_inform->result_asv_grp;

	asv_inform->asv_volt = kmalloc((sizeof(struct asv_freq_table) * asv_inform->dvfs_level_nr), GFP_KERNEL);
	asv_inform->asv_abb = kmalloc((sizeof(struct asv_freq_table) * asv_inform->dvfs_level_nr), GFP_KERNEL);

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
	unsigned int i;
	struct asv_info *target_asv_info = asv_get(ID_INT);

	if (is_speedgroup)
		return special_lot_group;

	if (temp_asv_group)
		return temp_asv_group;

	for (i = 0; i < target_asv_info->asv_group_nr; i++) {
		if (refer_use_table_get_asv[0][i] &&
			asv_comm->ids_value <= refer_table_get_asv[0][i])
			return i;

		if (refer_use_table_get_asv[1][i] &&
			asv_comm->hpm_value <= refer_table_get_asv[1][i])
			return i;
	}

	return 0;
}

static void exynos5430_set_asv_info_int(struct asv_info *asv_inform, bool show_value)
{
	unsigned int i;
	unsigned int target_asv_grp_nr = asv_inform->result_asv_grp;

	asv_inform->asv_volt = kmalloc((sizeof(struct asv_freq_table) * asv_inform->dvfs_level_nr), GFP_KERNEL);
	asv_inform->asv_abb = kmalloc((sizeof(struct asv_freq_table) * asv_inform->dvfs_level_nr), GFP_KERNEL);

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
	unsigned int i;
	struct asv_info *target_asv_info = asv_get(ID_MIF);

	if (is_speedgroup)
		return special_lot_group;

	if (temp_asv_group)
		return temp_asv_group;

	for (i = 0; i < target_asv_info->asv_group_nr; i++) {
		if (refer_use_table_get_asv[0][i] &&
			asv_comm->ids_value <= refer_table_get_asv[0][i])
			return i;

		if (refer_use_table_get_asv[1][i] &&
			asv_comm->hpm_value <= refer_table_get_asv[1][i])
			return i;
	}

	return 0;
}

static void exynos5430_set_asv_info_mif(struct asv_info *asv_inform, bool show_value)
{
	unsigned int i;
	unsigned int target_asv_grp_nr = asv_inform->result_asv_grp;

#if 0
	exynos5430_set_abb(asv_inform);
#endif

	asv_inform->asv_volt = kmalloc((sizeof(struct asv_freq_table) * asv_inform->dvfs_level_nr), GFP_KERNEL);
	asv_inform->asv_abb = kmalloc((sizeof(struct asv_freq_table) * asv_inform->dvfs_level_nr), GFP_KERNEL);

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
	unsigned int i;
	struct asv_info *target_asv_info = asv_get(ID_G3D);

	if (is_speedgroup)
		return special_lot_group;

	if (temp_asv_group)
		return temp_asv_group;

	for (i = 0; i < target_asv_info->asv_group_nr; i++) {
		if (refer_use_table_get_asv[0][i] &&
			asv_comm->ids_value <= refer_table_get_asv[0][i])
			return i;

		if (refer_use_table_get_asv[1][i] &&
			asv_comm->hpm_value <= refer_table_get_asv[1][i])
			return i;
	}

	return 0;
}

static void exynos5430_set_asv_info_g3d(struct asv_info *asv_inform, bool show_value)
{
	unsigned int i;
	unsigned int target_asv_grp_nr = asv_inform->result_asv_grp;

	asv_inform->asv_volt = kmalloc((sizeof(struct asv_freq_table) * asv_inform->dvfs_level_nr), GFP_KERNEL);
	asv_inform->asv_abb = kmalloc((sizeof(struct asv_freq_table) * asv_inform->dvfs_level_nr), GFP_KERNEL);

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
	unsigned int i;
	struct asv_info *target_asv_info = asv_get(ID_ISP);

	if (is_speedgroup)
		return special_lot_group;

	if (temp_asv_group)
		return temp_asv_group;

	for (i = 0; i < target_asv_info->asv_group_nr; i++) {
		if (refer_use_table_get_asv[0][i] &&
			asv_comm->ids_value <= refer_table_get_asv[0][i])
			return i;

		if (refer_use_table_get_asv[1][i] &&
			asv_comm->hpm_value <= refer_table_get_asv[1][i])
			return i;
	}

	return 0;
}

static void exynos5430_set_asv_info_isp(struct asv_info *asv_inform, bool show_value)
{
	unsigned int i;
	unsigned int target_asv_grp_nr = asv_inform->result_asv_grp;

	asv_inform->asv_volt = kmalloc((sizeof(struct asv_freq_table) * asv_inform->dvfs_level_nr), GFP_KERNEL);
	asv_inform->asv_abb = kmalloc((sizeof(struct asv_freq_table) * asv_inform->dvfs_level_nr), GFP_KERNEL);

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
#if 0
	struct clk *clk_abb;
#endif
	unsigned int chip_id_value;
	unsigned int chip_rev = 0;
	void __iomem *temp_asv_base;
	unsigned int temp_asv_value = 0;

	special_lot_group = 0;
	is_speedgroup = false;

#if 0
	/* enable abb clock */
	clk_abb = clk_get(NULL, "clk_abb_apbif");
	if (IS_ERR(clk_abb)) {
		pr_err("EXYNOS5430 ASV : cannot find abb clock!\n");
		return -EINVAL;
	}
	clk_enable(clk_abb);
#endif

	chip_id_value = __raw_readl(CHIP_ID_REG);
	chip_rev = (chip_id_value & CHIP_MAIN_REV_MASK) >> CHIP_MAIN_REV_SHIFT;

	pr_info("EXYNOS5430 Rev is %u\n", chip_rev);

	if (chip_rev == EXYNOS5430_EVT0) {
		temp_asv_base = ioremap(TEMP_ASV_GROUP_BASE, SZ_1K);
		if (!temp_asv_base) {
			pr_err("EXYNOS5430 ASV : cannot allocation memory\n");
			return -ENOMEM;
		}

		temp_asv_value = __raw_readl(temp_asv_base);
		temp_asv_value &= ASV_TBL3_MASK;

		pr_info("EXYNOS5430 ASV : temp ASV value is %u\n", temp_asv_value);

		if (temp_asv_value == TEMP_ASV_GROUP_10)
			temp_asv_group = TEMP_ASV_GROUP_10;
		else if (temp_asv_value == TEMP_ASV_GROUP_11)
			temp_asv_group = TEMP_ASV_GROUP_11;
		else if (temp_asv_value == TEMP_ASV_GROUP_12)
			temp_asv_group = TEMP_ASV_GROUP_12;
		else
			temp_asv_group = TEMP_ASV_GROUP_5;

		iounmap(temp_asv_base);
	} else {
		pr_err("EXYNOS5430 ASV : cannot support evt%u\n", chip_rev);
		return -EINVAL;
	}

	asv_info->regist_asv_member = exynos5430_regist_asv_member;

	return 0;
}
