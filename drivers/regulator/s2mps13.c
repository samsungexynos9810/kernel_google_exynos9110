/*
 * s2mps13.c
 *
 * Copyright (c) 2013 Samsung Electronics Co., Ltd
 *              http://www.samsung.com
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 *
 */

#include <linux/bug.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/gpio.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/machine.h>
#include <linux/regulator/of_regulator.h>
#include <linux/mfd/samsung/core.h>
#include <linux/mfd/samsung/s2mps13.h>

struct s2mps13_info {
	struct regulator_dev *rdev[S2MPS13_REGULATOR_MAX];

	int ramp_delay2;
	int ramp_delay3;
	int ramp_delay4;
	int ramp_delay6;
	int ramp_delay15;
	int ramp_delay710;
	int ramp_delay89;
	int ramp_delay_bb;

	bool buck6_ramp;
	bool buck2_ramp;
	bool buck3_ramp;
	bool buck4_ramp;
};

static int get_ramp_delay(int ramp_delay)
{
	unsigned char cnt = 0;

	ramp_delay /= 6;

	while (true) {
		ramp_delay = ramp_delay >> 1;
		if (ramp_delay == 0)
			break;
		cnt++;
	}
	return cnt;
}

static struct regulator_ops s2mps13_ldo_ops = {
	.list_voltage		= regulator_list_voltage_linear,
	.map_voltage		= regulator_map_voltage_linear,
	.is_enabled		= regulator_is_enabled_regmap,
	.enable			= regulator_enable_regmap,
	.disable		= regulator_disable_regmap,
	.get_voltage_sel	= regulator_get_voltage_sel_regmap,
	.set_voltage_sel	= regulator_set_voltage_sel_regmap,
	.set_voltage_time_sel	= regulator_set_voltage_time_sel,
};

static struct regulator_ops s2mps13_buck_ops = {
	.list_voltage		= regulator_list_voltage_linear,
	.map_voltage		= regulator_map_voltage_linear,
	.is_enabled		= regulator_is_enabled_regmap,
	.enable			= regulator_enable_regmap,
	.disable		= regulator_disable_regmap,
	.get_voltage_sel	= regulator_get_voltage_sel_regmap,
	.set_voltage_sel	= regulator_set_voltage_sel_regmap,
	.set_voltage_time_sel	= regulator_set_voltage_time_sel,
};

#define regulator_desc_ldo1(num)	{		\
	.name		= "LDO"#num,			\
	.id		= S2MPS13_LDO##num,		\
	.ops		= &s2mps13_ldo_ops,		\
	.type		= REGULATOR_VOLTAGE,		\
	.owner		= THIS_MODULE,			\
	.min_uV		= S2MPS13_LDO_MIN1,		\
	.uV_step	= S2MPS13_LDO_STEP2,		\
	.n_voltages	= S2MPS13_LDO_N_VOLTAGES,	\
	.vsel_reg	= S2MPS13_REG_L1CTRL + num - 1,	\
	.vsel_mask	= S2MPS13_LDO_VSEL_MASK,	\
	.enable_reg	= S2MPS13_REG_L1CTRL + num - 1,	\
	.enable_mask	= S2MPS13_ENABLE_MASK		\
}
#define regulator_desc_ldo2(num)	{		\
	.name		= "LDO"#num,			\
	.id		= S2MPS13_LDO##num,		\
	.ops		= &s2mps13_ldo_ops,		\
	.type		= REGULATOR_VOLTAGE,		\
	.owner		= THIS_MODULE,			\
	.min_uV		= S2MPS13_LDO_MIN2,		\
	.uV_step	= S2MPS13_LDO_STEP1,		\
	.n_voltages	= S2MPS13_LDO_N_VOLTAGES,	\
	.vsel_reg	= S2MPS13_REG_L1CTRL + num - 1,	\
	.vsel_mask	= S2MPS13_LDO_VSEL_MASK,	\
	.enable_reg	= S2MPS13_REG_L1CTRL + num - 1,	\
	.enable_mask	= S2MPS13_ENABLE_MASK		\
}
#define regulator_desc_ldo3(num)	{		\
	.name		= "LDO"#num,			\
	.id		= S2MPS13_LDO##num,		\
	.ops		= &s2mps13_ldo_ops,		\
	.type		= REGULATOR_VOLTAGE,		\
	.owner		= THIS_MODULE,			\
	.min_uV		= S2MPS13_LDO_MIN2,		\
	.uV_step	= S2MPS13_LDO_STEP2,		\
	.n_voltages	= S2MPS13_LDO_N_VOLTAGES,	\
	.vsel_reg	= S2MPS13_REG_L1CTRL + num - 1,	\
	.vsel_mask	= S2MPS13_LDO_VSEL_MASK,	\
	.enable_reg	= S2MPS13_REG_L1CTRL + num - 1,	\
	.enable_mask	= S2MPS13_ENABLE_MASK		\
}
#define regulator_desc_ldo4(num)	{		\
	.name		= "LDO"#num,			\
	.id		= S2MPS13_LDO##num,		\
	.ops		= &s2mps13_ldo_ops,		\
	.type		= REGULATOR_VOLTAGE,		\
	.owner		= THIS_MODULE,			\
	.min_uV		= S2MPS13_LDO_MIN2,		\
	.uV_step	= S2MPS13_LDO_STEP3,		\
	.n_voltages	= S2MPS13_LDO_N_VOLTAGES,	\
	.vsel_reg	= S2MPS13_REG_L1CTRL + num - 1,	\
	.vsel_mask	= S2MPS13_LDO_VSEL_MASK,	\
	.enable_reg	= S2MPS13_REG_L1CTRL + num - 1,	\
	.enable_mask	= S2MPS13_ENABLE_MASK		\
}

#define regulator_desc_buck1_6(num)	{			\
	.name		= "BUCK"#num,				\
	.id		= S2MPS13_BUCK##num,			\
	.ops		= &s2mps13_buck_ops,			\
	.type		= REGULATOR_VOLTAGE,			\
	.owner		= THIS_MODULE,				\
	.min_uV		= S2MPS13_BUCK_MIN1,			\
	.uV_step	= S2MPS13_BUCK_STEP1,			\
	.n_voltages	= S2MPS13_BUCK_N_VOLTAGES,		\
	.vsel_reg	= S2MPS13_REG_B1CTRL2 + (num - 1) * 2,	\
	.vsel_mask	= S2MPS13_BUCK_VSEL_MASK,		\
	.enable_reg	= S2MPS13_REG_B1CTRL1 + (num - 1) * 2,	\
	.enable_mask	= S2MPS13_ENABLE_MASK			\
}

#define regulator_desc_buck7(num)	{			\
	.name		= "BUCK"#num,				\
	.id		= S2MPS13_BUCK##num,			\
	.ops		= &s2mps13_buck_ops,			\
	.type		= REGULATOR_VOLTAGE,			\
	.owner		= THIS_MODULE,				\
	.min_uV		= S2MPS13_BUCK_MIN1,			\
	.uV_step	= S2MPS13_BUCK_STEP1,			\
	.n_voltages	= S2MPS13_BUCK_N_VOLTAGES,		\
	.vsel_reg	= S2MPS13_REG_B7CTRL2,	\
	.vsel_mask	= S2MPS13_BUCK_VSEL_MASK,		\
	.enable_reg	= S2MPS13_REG_B7CTRL1 + (num - 1),	\
	.enable_mask	= (num == 1) ? S2MPS13_ENABLE_MASK : S2MPS13_SW_ENABLE_MASK			\
}

#define regulator_desc_buck10 {			\
	.name		= "BUCK10",				\
	.id		= S2MPS13_BUCK10,			\
	.ops		= &s2mps13_buck_ops,			\
	.type		= REGULATOR_VOLTAGE,			\
	.owner		= THIS_MODULE,				\
	.min_uV		= S2MPS13_BUCK_MIN1,			\
	.uV_step	= S2MPS13_BUCK_STEP1,			\
	.n_voltages	= S2MPS13_BUCK_N_VOLTAGES,		\
	.vsel_reg	= S2MPS13_REG_B10CTRL2,	\
	.vsel_mask	= S2MPS13_BUCK_VSEL_MASK,		\
	.enable_reg	= S2MPS13_REG_B10CTRL1,	\
	.enable_mask	= S2MPS13_ENABLE_MASK			\
}

#define regulator_desc_buck89(num)	{				\
	.name		= "BUCK"#num,				\
	.id		= S2MPS13_BUCK##num,			\
	.ops		= &s2mps13_buck_ops,			\
	.type		= REGULATOR_VOLTAGE,			\
	.owner		= THIS_MODULE,				\
	.min_uV		= S2MPS13_BUCK_MIN1,			\
	.uV_step	= S2MPS13_BUCK_STEP2,			\
	.n_voltages	= S2MPS13_BUCK_N_VOLTAGES,		\
	.vsel_reg	= S2MPS13_REG_B8CTRL2 + (num - 8) * 2,	\
	.vsel_mask	= S2MPS13_BUCK_VSEL_MASK,		\
	.enable_reg	= S2MPS13_REG_B8CTRL1 + (num - 8) * 2,	\
	.enable_mask	= S2MPS13_ENABLE_MASK			\
}

#define regulator_desc_bb	{				\
	.name		= "BUCKBOOST",				\
	.id		= S2MPS13_BB1,			\
	.ops		= &s2mps13_buck_ops,			\
	.type		= REGULATOR_VOLTAGE,			\
	.owner		= THIS_MODULE,				\
	.min_uV		= S2MPS13_BUCK_MIN2,			\
	.uV_step	= S2MPS13_BUCK_STEP2,			\
	.n_voltages	= S2MPS13_BUCK_N_VOLTAGES,		\
	.vsel_reg	= S2MPS13_REG_BB1CTRL2,			\
	.vsel_mask	= S2MPS13_BUCK_VSEL_MASK,		\
	.enable_reg	= S2MPS13_REG_BB1CTRL1,			\
	.enable_mask	= S2MPS13_ENABLE_MASK			\
}

static struct regulator_desc regulators[] = {
	regulator_desc_ldo2(1),
	regulator_desc_ldo4(2),
	regulator_desc_ldo3(3),
	regulator_desc_ldo2(4),
	regulator_desc_ldo2(5),
	regulator_desc_ldo2(6),
	regulator_desc_ldo3(7),
	regulator_desc_ldo3(8),
	regulator_desc_ldo3(9),
	regulator_desc_ldo4(10),
	regulator_desc_ldo1(11),
	regulator_desc_ldo1(12),
	regulator_desc_ldo1(13),
	regulator_desc_ldo2(14),
	regulator_desc_ldo2(15),
	regulator_desc_ldo4(16),
	regulator_desc_ldo4(17),
	regulator_desc_ldo3(18),
	regulator_desc_ldo3(19),
	regulator_desc_ldo4(20),
	regulator_desc_ldo3(21),
	regulator_desc_ldo3(22),
	regulator_desc_ldo2(23),
	regulator_desc_ldo2(24),
	regulator_desc_ldo4(25),
	regulator_desc_ldo4(26),
	regulator_desc_ldo4(27),
	regulator_desc_ldo3(28),
	regulator_desc_ldo4(29),
	regulator_desc_ldo4(30),
	regulator_desc_ldo3(31),
	regulator_desc_ldo3(32),
	regulator_desc_ldo4(33),
	regulator_desc_ldo3(34),
	regulator_desc_ldo4(35),
	regulator_desc_ldo2(36),
	regulator_desc_ldo3(37),
	regulator_desc_ldo4(38),
	regulator_desc_ldo3(39),
	regulator_desc_ldo4(40),
	regulator_desc_buck1_6(1),
	regulator_desc_buck1_6(2),
	regulator_desc_buck1_6(3),
	regulator_desc_buck1_6(4),
	regulator_desc_buck1_6(5),
	regulator_desc_buck1_6(6),
	regulator_desc_buck7(1),
	regulator_desc_buck7(2),
	regulator_desc_buck89(8),
	regulator_desc_buck89(9),
	regulator_desc_buck10,
	regulator_desc_bb,
};

#ifdef CONFIG_OF
static int s2mps13_pmic_dt_parse_pdata(struct sec_pmic_dev *iodev,
					struct sec_platform_data *pdata)
{
	struct device_node *pmic_np, *regulators_np, *reg_np;
	struct sec_regulator_data *rdata;
	unsigned int i;

	pmic_np = iodev->dev->of_node;
	if (!pmic_np) {
		dev_err(iodev->dev, "could not find pmic sub-node\n");
		return -ENODEV;
	}

	regulators_np = of_find_node_by_name(pmic_np, "regulators");
	if (!regulators_np) {
		dev_err(iodev->dev, "could not find regulators sub-node\n");
		return -EINVAL;
	}

	/* count the number of regulators to be supported in pmic */
	pdata->num_regulators = 0;
	for_each_child_of_node(regulators_np, reg_np) {
		pdata->num_regulators++;
	}

	rdata = devm_kzalloc(iodev->dev, sizeof(*rdata) *
				pdata->num_regulators, GFP_KERNEL);
	if (!rdata) {
		dev_err(iodev->dev,
			"could not allocate memory for regulator data\n");
		return -ENOMEM;
	}

	pdata->regulators = rdata;
	for_each_child_of_node(regulators_np, reg_np) {
		for (i = 0; i < ARRAY_SIZE(regulators); i++)
			if (!of_node_cmp(reg_np->name, regulators[i].name))
				break;

		if (i == ARRAY_SIZE(regulators)) {
			dev_warn(iodev->dev,
			"don't know how to configure regulator %s\n",
			reg_np->name);
			continue;
		}

		rdata->id = i;
		rdata->initdata = of_get_regulator_init_data(
						iodev->dev, reg_np);
		rdata->reg_node = reg_np;
		rdata++;
	}

	if (!of_property_read_u32(pmic_np, "s2mps13,buck2-ramp-delay", &i))
		pdata->buck2_ramp_delay = i;
	if (!of_property_read_u32(pmic_np, "s2mps13,buck34-ramp-delay", &i))
		pdata->buck34_ramp_delay = i;
	if (!of_property_read_u32(pmic_np, "s2mps13,buck5-ramp-delay", &i))
		pdata->buck5_ramp_delay = i;
	if (!of_property_read_u32(pmic_np, "s2mps13,buck16-ramp-delay", &i))
		pdata->buck16_ramp_delay = i;
	if (!of_property_read_u32(pmic_np, "s2mps13,buck7810-ramp-delay", &i))
		pdata->buck7810_ramp_delay = i;
	if (!of_property_read_u32(pmic_np, "s2mps13,buck9-ramp-delay", &i))
		pdata->buck9_ramp_delay = i;
	if (!of_property_read_u32(pmic_np, "s2mps13,buck6-ramp-enable", &i))
		pdata->buck6_ramp_enable = i;
	if (!of_property_read_u32(pmic_np, "s2mps13,buck2-ramp-enable", &i))
		pdata->buck2_ramp_enable = i;
	if (!of_property_read_u32(pmic_np, "s2mps13,buck3-ramp-enable", &i))
		pdata->buck3_ramp_enable = i;
	if (!of_property_read_u32(pmic_np, "s2mps13,buck4-ramp-enable", &i))
		pdata->buck4_ramp_enable = i;

	return 0;
}
#else
static int s5m8767_pmic_dt_parse_pdata(struct sec_pmic_dev *iodev,
					struct sec_platform_data *pdata)
{
	return 0;
}
#endif /* CONFIG_OF */

static int s2mps13_pmic_probe(struct platform_device *pdev)
{
	struct sec_pmic_dev *iodev = dev_get_drvdata(pdev->dev.parent);
	struct sec_platform_data *pdata = iodev->pdata;
	struct regulator_config config = { };
	struct s2mps13_info *s2mps13;
	int i, ret;
	unsigned int ramp_reg = 0;

	if (iodev->dev->of_node) {
		ret = s2mps13_pmic_dt_parse_pdata(iodev, pdata);
		if (ret)
			return ret;
	}

	if (!pdata) {
		dev_err(pdev->dev.parent, "Platform data not supplied\n");
		return -ENODEV;
	}

	s2mps13 = devm_kzalloc(&pdev->dev, sizeof(struct s2mps13_info),
				GFP_KERNEL);
	if (!s2mps13)
		return -ENOMEM;

	platform_set_drvdata(pdev, s2mps13);

	s2mps13->ramp_delay2 = pdata->buck2_ramp_delay;
	s2mps13->ramp_delay3 = pdata->buck3_ramp_delay;
	s2mps13->ramp_delay4 = pdata->buck4_ramp_delay;
	s2mps13->ramp_delay6 = pdata->buck6_ramp_delay;
	s2mps13->ramp_delay15 = pdata->buck15_ramp_delay;
	s2mps13->ramp_delay710 = pdata->buck710_ramp_delay;
	s2mps13->ramp_delay89 = pdata->buck89_ramp_delay;
	s2mps13->ramp_delay_bb = pdata->bb_ramp_delay;

	ramp_reg &= 0x00;
	ramp_reg |= get_ramp_delay(s2mps13->ramp_delay2) << 6;
	ramp_reg |= get_ramp_delay(s2mps13->ramp_delay3) << 4;
	ramp_reg |= get_ramp_delay(s2mps13->ramp_delay4) << 2;
	ramp_reg |= get_ramp_delay(s2mps13->ramp_delay6);
	sec_reg_write(iodev, S2MPS13_REG_BUCK_RAMP1, ramp_reg);

	ramp_reg &= 0x00;
	ramp_reg |= get_ramp_delay(s2mps13->ramp_delay710) << 6;
	ramp_reg |= get_ramp_delay(s2mps13->ramp_delay89) << 4;
	ramp_reg |= get_ramp_delay(s2mps13->ramp_delay_bb) << 2;
	ramp_reg |= get_ramp_delay(s2mps13->ramp_delay15);
	sec_reg_write(iodev, S2MPS13_REG_BUCK_RAMP2, ramp_reg);

	for (i = 0; i < pdata->num_regulators; i++) {
		int id = pdata->regulators[i].id;
		config.dev = &pdev->dev;
		config.regmap = iodev->regmap;
		config.init_data = pdata->regulators[i].initdata;
		config.driver_data = s2mps13;
		config.of_node = pdata->regulators[i].reg_node;

		s2mps13->rdev[i] = regulator_register(&regulators[id], &config);
		if (IS_ERR(s2mps13->rdev[i])) {
			ret = PTR_ERR(s2mps13->rdev[i]);
			dev_err(&pdev->dev, "regulator init failed for %d\n",
				i);
			s2mps13->rdev[i] = NULL;
			goto err;
		}
	}

	return 0;
err:
	for (i = 0; i < S2MPS13_REGULATOR_MAX; i++)
		regulator_unregister(s2mps13->rdev[i]);

	return ret;
}

static int s2mps13_pmic_remove(struct platform_device *pdev)
{
	struct s2mps13_info *s2mps13 = platform_get_drvdata(pdev);
	int i;

	for (i = 0; i < S2MPS13_REGULATOR_MAX; i++)
		regulator_unregister(s2mps13->rdev[i]);

	return 0;
}

static const struct platform_device_id s2mps13_pmic_id[] = {
	{ "s2mps13-pmic", 0},
	{ },
};
MODULE_DEVICE_TABLE(platform, s2mps13_pmic_id);

static struct platform_driver s2mps13_pmic_driver = {
	.driver = {
		.name = "s2mps13-pmic",
		.owner = THIS_MODULE,
	},
	.probe = s2mps13_pmic_probe,
	.remove = s2mps13_pmic_remove,
	.id_table = s2mps13_pmic_id,
};

static int __init s2mps13_pmic_init(void)
{
	return platform_driver_register(&s2mps13_pmic_driver);
}
subsys_initcall(s2mps13_pmic_init);

static void __exit s2mps13_pmic_exit(void)
{
	platform_driver_unregister(&s2mps13_pmic_driver);
}
module_exit(s2mps13_pmic_exit);

/* Module information */
MODULE_AUTHOR("Sangbeom Kim <sbkim73@samsung.com>");
MODULE_DESCRIPTION("SAMSUNG S2MPS13 Regulator Driver");
MODULE_LICENSE("GPL");
