/*
 * s2mps11.c
 *
 * Copyright (c) 2012-2014 Samsung Electronics Co., Ltd
 *              http://www.samsung.com
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
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
#include <linux/mfd/samsung/s2mps11.h>
#include <linux/mfd/samsung/s2mps14.h>

struct s2mps11_info {
	struct regulator_dev **rdev;
	unsigned int rdev_num;
	struct sec_opmode_data *opmode;

	int ramp_delay2;
	int ramp_delay34;
	int ramp_delay5;
	int ramp_delay16;
	int ramp_delay7810;
	int ramp_delay9;

	bool buck6_ramp;
	bool buck2_ramp;
	bool buck3_ramp;
	bool buck4_ramp;
};

/* LDO_EN/BUCK_EN register values for enabling/disabling regulator */
static unsigned int s2mps14_opmode_reg[4] = {
	[S2MPS14_REGULATOR_OPMODE_OFF]		= 0x0,
	[S2MPS14_REGULATOR_OPMODE_ON]		= 0x3,
	[S2MPS14_REGULATOR_OPMODE_RESERVED]	= 0x2,
	[S2MPS14_REGULATOR_OPMODE_SUSPEND]	= 0x1,
};

static int s2mps14_get_opmode(struct regulator_dev *rdev)
{
	int i, reg_id = rdev_get_id(rdev);
	int mode = -EINVAL;
	struct s2mps11_info *s2mps11 = rdev_get_drvdata(rdev);

	for (i = 0; i < s2mps11->rdev_num; i++) {
		if (s2mps11->opmode[i].id == reg_id) {
			mode = s2mps11->opmode[i].mode;
			break;
		}
	}

	if (mode == -EINVAL) {
		dev_warn(rdev_get_dev(rdev),
				"No op_mode in the driver for regulator %s\n",
				rdev->desc->name);
		return mode;
	}

	return s2mps14_opmode_reg[mode] << S2MPS14_ENCTRL_SHIFT;
}

static int s2mps14_reg_enable(struct regulator_dev *rdev)
{
	int enable_ctrl = s2mps14_get_opmode(rdev);

	if (enable_ctrl < 0)
		return enable_ctrl;

	return sec_reg_update(rdev->regmap, rdev->desc->enable_reg,
			S2MPS14_ENCTRL_MASK, enable_ctrl);
}

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

static struct regulator_ops s2mps11_reg_ops = {
	.list_voltage		= regulator_list_voltage_linear,
	.map_voltage		= regulator_map_voltage_linear,
	.is_enabled		= regulator_is_enabled_regmap,
	.enable			= regulator_enable_regmap,
	.disable		= regulator_disable_regmap,
	.get_voltage_sel	= regulator_get_voltage_sel_regmap,
	.set_voltage_sel	= regulator_set_voltage_sel_regmap,
	.set_voltage_time_sel	= regulator_set_voltage_time_sel,
};

#define regulator_desc_s2mps11_ldo1(num) {		\
	.name		= "LDO"#num,			\
	.id		= S2MPS11_LDO##num,		\
	.ops		= &s2mps11_reg_ops,		\
	.type		= REGULATOR_VOLTAGE,		\
	.owner		= THIS_MODULE,			\
	.min_uV		= S2MPS11_LDO_MIN,		\
	.uV_step	= S2MPS11_LDO_STEP1,		\
	.n_voltages	= S2MPS11_LDO_N_VOLTAGES,	\
	.vsel_reg	= S2MPS11_REG_L1CTRL + num - 1,	\
	.vsel_mask	= S2MPS11_LDO_VSEL_MASK,	\
	.enable_reg	= S2MPS11_REG_L1CTRL + num - 1,	\
	.enable_mask	= S2MPS11_ENABLE_MASK		\
}
#define regulator_desc_s2mps11_ldo2(num) {		\
	.name		= "LDO"#num,			\
	.id		= S2MPS11_LDO##num,		\
	.ops		= &s2mps11_reg_ops,		\
	.type		= REGULATOR_VOLTAGE,		\
	.owner		= THIS_MODULE,			\
	.min_uV		= S2MPS11_LDO_MIN,		\
	.uV_step	= S2MPS11_LDO_STEP2,		\
	.n_voltages	= S2MPS11_LDO_N_VOLTAGES,	\
	.vsel_reg	= S2MPS11_REG_L1CTRL + num - 1,	\
	.vsel_mask	= S2MPS11_LDO_VSEL_MASK,	\
	.enable_reg	= S2MPS11_REG_L1CTRL + num - 1,	\
	.enable_mask	= S2MPS11_ENABLE_MASK		\
}

#define regulator_desc_s2mps11_buck1_4(num) {			\
	.name		= "BUCK"#num,				\
	.id		= S2MPS11_BUCK##num,			\
	.ops		= &s2mps11_reg_ops,			\
	.type		= REGULATOR_VOLTAGE,			\
	.owner		= THIS_MODULE,				\
	.min_uV		= S2MPS11_BUCK_MIN1,			\
	.uV_step	= S2MPS11_BUCK_STEP1,			\
	.n_voltages	= S2MPS11_BUCK_N_VOLTAGES,		\
	.vsel_reg	= S2MPS11_REG_B1CTRL2 + (num - 1) * 2,	\
	.vsel_mask	= S2MPS11_BUCK_VSEL_MASK,		\
	.enable_reg	= S2MPS11_REG_B1CTRL1 + (num - 1) * 2,	\
	.enable_mask	= S2MPS11_ENABLE_MASK			\
}

#define regulator_desc_s2mps11_buck5 {				\
	.name		= "BUCK5",				\
	.id		= S2MPS11_BUCK5,			\
	.ops		= &s2mps11_reg_ops,			\
	.type		= REGULATOR_VOLTAGE,			\
	.owner		= THIS_MODULE,				\
	.min_uV		= S2MPS11_BUCK_MIN1,			\
	.uV_step	= S2MPS11_BUCK_STEP1,			\
	.n_voltages	= S2MPS11_BUCK_N_VOLTAGES,		\
	.vsel_reg	= S2MPS11_REG_B5CTRL2,			\
	.vsel_mask	= S2MPS11_BUCK_VSEL_MASK,		\
	.enable_reg	= S2MPS11_REG_B5CTRL1,			\
	.enable_mask	= S2MPS11_ENABLE_MASK			\
}

#define regulator_desc_s2mps11_buck6_8(num) {			\
	.name		= "BUCK"#num,				\
	.id		= S2MPS11_BUCK##num,			\
	.ops		= &s2mps11_reg_ops,			\
	.type		= REGULATOR_VOLTAGE,			\
	.owner		= THIS_MODULE,				\
	.min_uV		= S2MPS11_BUCK_MIN1,			\
	.uV_step	= S2MPS11_BUCK_STEP1,			\
	.n_voltages	= S2MPS11_BUCK_N_VOLTAGES,		\
	.vsel_reg	= S2MPS11_REG_B6CTRL2 + (num - 6) * 2,	\
	.vsel_mask	= S2MPS11_BUCK_VSEL_MASK,		\
	.enable_reg	= S2MPS11_REG_B6CTRL1 + (num - 6) * 2,	\
	.enable_mask	= S2MPS11_ENABLE_MASK			\
}

#define regulator_desc_s2mps11_buck9 {				\
	.name		= "BUCK9",				\
	.id		= S2MPS11_BUCK9,			\
	.ops		= &s2mps11_reg_ops,			\
	.type		= REGULATOR_VOLTAGE,			\
	.owner		= THIS_MODULE,				\
	.min_uV		= S2MPS11_BUCK_MIN3,			\
	.uV_step	= S2MPS11_BUCK_STEP3,			\
	.n_voltages	= S2MPS11_BUCK_N_VOLTAGES,		\
	.vsel_reg	= S2MPS11_REG_B9CTRL2,			\
	.vsel_mask	= S2MPS11_BUCK_VSEL_MASK,		\
	.enable_reg	= S2MPS11_REG_B9CTRL1,			\
	.enable_mask	= S2MPS11_ENABLE_MASK			\
}

#define regulator_desc_s2mps11_buck10 {				\
	.name		= "BUCK10",				\
	.id		= S2MPS11_BUCK10,			\
	.ops		= &s2mps11_reg_ops,			\
	.type		= REGULATOR_VOLTAGE,			\
	.owner		= THIS_MODULE,				\
	.min_uV		= S2MPS11_BUCK_MIN2,			\
	.uV_step	= S2MPS11_BUCK_STEP2,			\
	.n_voltages	= S2MPS11_BUCK_N_VOLTAGES,		\
	.vsel_reg	= S2MPS11_REG_B10CTRL2,			\
	.vsel_mask	= S2MPS11_BUCK_VSEL_MASK,		\
	.enable_reg	= S2MPS11_REG_B10CTRL1,			\
	.enable_mask	= S2MPS11_ENABLE_MASK			\
}

static const struct regulator_desc s2mps11_regulators[] = {
	regulator_desc_s2mps11_ldo2(1),
	regulator_desc_s2mps11_ldo1(2),
	regulator_desc_s2mps11_ldo1(3),
	regulator_desc_s2mps11_ldo1(4),
	regulator_desc_s2mps11_ldo1(5),
	regulator_desc_s2mps11_ldo2(6),
	regulator_desc_s2mps11_ldo1(7),
	regulator_desc_s2mps11_ldo1(8),
	regulator_desc_s2mps11_ldo1(9),
	regulator_desc_s2mps11_ldo1(10),
	regulator_desc_s2mps11_ldo2(11),
	regulator_desc_s2mps11_ldo1(12),
	regulator_desc_s2mps11_ldo1(13),
	regulator_desc_s2mps11_ldo1(14),
	regulator_desc_s2mps11_ldo1(15),
	regulator_desc_s2mps11_ldo1(16),
	regulator_desc_s2mps11_ldo1(17),
	regulator_desc_s2mps11_ldo1(18),
	regulator_desc_s2mps11_ldo1(19),
	regulator_desc_s2mps11_ldo1(20),
	regulator_desc_s2mps11_ldo1(21),
	regulator_desc_s2mps11_ldo2(22),
	regulator_desc_s2mps11_ldo2(23),
	regulator_desc_s2mps11_ldo1(24),
	regulator_desc_s2mps11_ldo1(25),
	regulator_desc_s2mps11_ldo1(26),
	regulator_desc_s2mps11_ldo2(27),
	regulator_desc_s2mps11_ldo1(28),
	regulator_desc_s2mps11_ldo1(29),
	regulator_desc_s2mps11_ldo1(30),
	regulator_desc_s2mps11_ldo1(31),
	regulator_desc_s2mps11_ldo1(32),
	regulator_desc_s2mps11_ldo1(33),
	regulator_desc_s2mps11_ldo1(34),
	regulator_desc_s2mps11_ldo1(35),
	regulator_desc_s2mps11_ldo1(36),
	regulator_desc_s2mps11_ldo1(37),
	regulator_desc_s2mps11_ldo1(38),
	regulator_desc_s2mps11_buck1_4(1),
	regulator_desc_s2mps11_buck1_4(2),
	regulator_desc_s2mps11_buck1_4(3),
	regulator_desc_s2mps11_buck1_4(4),
	regulator_desc_s2mps11_buck5,
	regulator_desc_s2mps11_buck6_8(6),
	regulator_desc_s2mps11_buck6_8(7),
	regulator_desc_s2mps11_buck6_8(8),
	regulator_desc_s2mps11_buck9,
	regulator_desc_s2mps11_buck10,
};

static struct regulator_ops s2mps14_reg_ops = {
	.list_voltage		= regulator_list_voltage_linear,
	.map_voltage		= regulator_map_voltage_linear,
	.is_enabled		= regulator_is_enabled_regmap,
	.enable			= s2mps14_reg_enable,
	.disable		= regulator_disable_regmap,
	.get_voltage_sel	= regulator_get_voltage_sel_regmap,
	.set_voltage_sel	= regulator_set_voltage_sel_regmap,
	.set_voltage_time_sel	= regulator_set_voltage_time_sel,
};

#define regulator_desc_s2mps14_ldo1(num) {		\
	.name		= "LDO"#num,			\
	.id		= S2MPS14_LDO##num,		\
	.ops		= &s2mps14_reg_ops,		\
	.type		= REGULATOR_VOLTAGE,		\
	.owner		= THIS_MODULE,			\
	.min_uV		= S2MPS14_LDO_MIN_800MV,	\
	.uV_step	= S2MPS14_LDO_STEP_25MV,	\
	.n_voltages	= S2MPS14_LDO_N_VOLTAGES,	\
	.vsel_reg	= S2MPS14_REG_L1CTRL + num - 1,	\
	.vsel_mask	= S2MPS14_LDO_VSEL_MASK,	\
	.enable_reg	= S2MPS14_REG_L1CTRL + num - 1,	\
	.enable_mask	= S2MPS14_ENABLE_MASK		\
}
#define regulator_desc_s2mps14_ldo2(num) {		\
	.name		= "LDO"#num,			\
	.id		= S2MPS14_LDO##num,		\
	.ops		= &s2mps14_reg_ops,		\
	.type		= REGULATOR_VOLTAGE,		\
	.owner		= THIS_MODULE,			\
	.min_uV		= S2MPS14_LDO_MIN_1800MV,	\
	.uV_step	= S2MPS14_LDO_STEP_25MV,	\
	.n_voltages	= S2MPS14_LDO_N_VOLTAGES,	\
	.vsel_reg	= S2MPS14_REG_L1CTRL + num - 1,	\
	.vsel_mask	= S2MPS14_LDO_VSEL_MASK,	\
	.enable_reg	= S2MPS14_REG_L1CTRL + num - 1,	\
	.enable_mask	= S2MPS14_ENABLE_MASK		\
}
#define regulator_desc_s2mps14_ldo3(num) {		\
	.name		= "LDO"#num,			\
	.id		= S2MPS14_LDO##num,		\
	.ops		= &s2mps14_reg_ops,		\
	.type		= REGULATOR_VOLTAGE,		\
	.owner		= THIS_MODULE,			\
	.min_uV		= S2MPS14_LDO_MIN_800MV,	\
	.uV_step	= S2MPS14_LDO_STEP_12_5MV,	\
	.n_voltages	= S2MPS14_LDO_N_VOLTAGES,	\
	.vsel_reg	= S2MPS14_REG_L1CTRL + num - 1,	\
	.vsel_mask	= S2MPS14_LDO_VSEL_MASK,	\
	.enable_reg	= S2MPS14_REG_L1CTRL + num - 1,	\
	.enable_mask	= S2MPS14_ENABLE_MASK		\
}
#define regulator_desc_s2mps14_buck1235(num) {			\
	.name		= "BUCK"#num,				\
	.id		= S2MPS14_BUCK##num,			\
	.ops		= &s2mps14_reg_ops,			\
	.type		= REGULATOR_VOLTAGE,			\
	.owner		= THIS_MODULE,				\
	.min_uV		= S2MPS14_BUCK1235_MIN_600MV,		\
	.uV_step	= S2MPS14_BUCK1235_STEP_6_25MV,		\
	.n_voltages	= S2MPS14_BUCK_N_VOLTAGES,		\
	.linear_min_sel = S2MPS14_BUCK1235_START_SEL,		\
	.ramp_delay	= S2MPS14_BUCK_RAMP_DELAY,		\
	.vsel_reg	= S2MPS14_REG_B1CTRL2 + (num - 1) * 2,	\
	.vsel_mask	= S2MPS14_BUCK_VSEL_MASK,		\
	.enable_reg	= S2MPS14_REG_B1CTRL1 + (num - 1) * 2,	\
	.enable_mask	= S2MPS14_ENABLE_MASK			\
}
#define regulator_desc_s2mps14_buck4(num) {			\
	.name		= "BUCK"#num,				\
	.id		= S2MPS14_BUCK##num,			\
	.ops		= &s2mps14_reg_ops,			\
	.type		= REGULATOR_VOLTAGE,			\
	.owner		= THIS_MODULE,				\
	.min_uV		= S2MPS14_BUCK4_MIN_1400MV,		\
	.uV_step	= S2MPS14_BUCK4_STEP_12_5MV,		\
	.n_voltages	= S2MPS14_BUCK_N_VOLTAGES,		\
	.linear_min_sel = S2MPS14_BUCK4_START_SEL,		\
	.ramp_delay	= S2MPS14_BUCK_RAMP_DELAY,		\
	.vsel_reg	= S2MPS14_REG_B1CTRL2 + (num - 1) * 2,	\
	.vsel_mask	= S2MPS14_BUCK_VSEL_MASK,		\
	.enable_reg	= S2MPS14_REG_B1CTRL1 + (num - 1) * 2,	\
	.enable_mask	= S2MPS14_ENABLE_MASK			\
}
static const struct regulator_desc s2mps14_regulators[] = {
	regulator_desc_s2mps14_ldo3(1),
	regulator_desc_s2mps14_ldo3(2),
	regulator_desc_s2mps14_ldo1(3),
	regulator_desc_s2mps14_ldo1(4),
	regulator_desc_s2mps14_ldo3(5),
	regulator_desc_s2mps14_ldo3(6),
	regulator_desc_s2mps14_ldo1(7),
	regulator_desc_s2mps14_ldo2(8),
	regulator_desc_s2mps14_ldo3(9),
	regulator_desc_s2mps14_ldo3(10),
	regulator_desc_s2mps14_ldo1(11),
	regulator_desc_s2mps14_ldo2(12),
	regulator_desc_s2mps14_ldo2(13),
	regulator_desc_s2mps14_ldo2(14),
	regulator_desc_s2mps14_ldo2(15),
	regulator_desc_s2mps14_ldo2(16),
	regulator_desc_s2mps14_ldo2(17),
	regulator_desc_s2mps14_ldo2(18),
	regulator_desc_s2mps14_ldo1(19),
	regulator_desc_s2mps14_ldo1(20),
	regulator_desc_s2mps14_ldo1(21),
	regulator_desc_s2mps14_ldo3(22),
	regulator_desc_s2mps14_ldo1(23),
	regulator_desc_s2mps14_ldo2(24),
	regulator_desc_s2mps14_ldo2(25),
	regulator_desc_s2mps14_buck1235(1),
	regulator_desc_s2mps14_buck1235(2),
	regulator_desc_s2mps14_buck1235(3),
	regulator_desc_s2mps14_buck4(4),
	regulator_desc_s2mps14_buck1235(5),
};

#ifdef CONFIG_OF
static int s2mps11_pmic_dt_parse_pdata(struct platform_device *pdev,
					struct sec_platform_data *pdata)
{
	struct sec_pmic_dev *iodev = dev_get_drvdata(pdev->dev.parent);
	struct device_node *pmic_np, *regulators_np, *reg_np;
	struct sec_regulator_data *rdata;
	struct sec_opmode_data *rmode;
	const struct regulator_desc *regulators;
	int i, num_regulators;

	switch(iodev->device_type) {
	case S2MPS11X:
		regulators = s2mps11_regulators;
		num_regulators = ARRAY_SIZE(s2mps11_regulators);
		break;
	case S2MPS14X:
		regulators = s2mps14_regulators;
		num_regulators = ARRAY_SIZE(s2mps14_regulators);
		break;
	default:
		dev_err(&pdev->dev, "Invalid device type: %u\n",
					iodev->device_type);
		return -EINVAL;
	};

	pmic_np = iodev->dev->of_node;
	if (!pmic_np) {
		dev_err(iodev->dev, "couldn't find pmic sub-node\n");
		return -ENODEV;
	}

	regulators_np = of_find_node_by_name(pmic_np, "regulators");
	if (!regulators_np) {
		dev_err(iodev->dev, "couldn't find regulators sub-node\n");
		return -EINVAL;
	}

	pdata->num_regulators = of_get_child_count(regulators_np);
	rdata = devm_kzalloc(&pdev->dev, sizeof(*rdata) * pdata->num_regulators,
			     GFP_KERNEL);
	if (!rdata) {
		dev_err(iodev->dev, "couldn't allocate memory for rdata\n");
		return -ENOMEM;
	}

	rmode = devm_kzalloc(&pdev->dev, sizeof(*rmode) * pdata->num_regulators,
			     GFP_KERNEL);
	if (!rmode) {
		dev_err(iodev->dev, "couldn't allocate memory for opmode\n");
		return -ENOMEM;
	}

	pdata->regulators = rdata;
	pdata->opmode = rmode;
	for_each_child_of_node(regulators_np, reg_np) {
		for (i = 0; i < num_regulators; i++)
			if (!of_node_cmp(reg_np->name, regulators[i].name))
				break;

		if (i == num_regulators) {
			dev_warn(iodev->dev,
				"don't know how to configure regulator %s\n",
				reg_np->name);
			continue;
		}

		rdata->id = i;
		rdata->initdata = of_get_regulator_init_data(&pdev->dev,
							     reg_np);
		rdata->reg_node = reg_np;
		rdata++;
		rmode->id = i;
		if (of_property_read_u32(reg_np, "op_mode", &rmode->mode)) {
			dev_warn(iodev->dev,
				"no op_mode property property at %s\n",
				reg_np->full_name);
			rmode->mode = S2MPS14_REGULATOR_OPMODE_ON;
		} else if (rmode->mode >= S2MPS14_REGULATOR_OPMODE_MAX) {
			dev_warn(iodev->dev, "wrong op_mode value at %s\n",
					reg_np->full_name);
			rmode->mode = S2MPS14_REGULATOR_OPMODE_ON;
		}
		rmode++;
	}

	return 0;
}
#else
static int s2mps11_pmic_dt_parse_pdata(struct platform_device *pdev,
					struct sec_platform_data *pdata)
{
	return 0;
}
#endif /* CONFIG_OF */

static int s2mps11_pmic_probe(struct platform_device *pdev)
{
	struct sec_pmic_dev *iodev = dev_get_drvdata(pdev->dev.parent);
	struct sec_platform_data *pdata = iodev->pdata;
	struct regulator_config config = { };
	struct s2mps11_info *s2mps11;
	int i, ret;
	unsigned char ramp_enable, ramp_reg = 0;
	const struct regulator_desc *regulators;
	enum sec_device_type dev_type;

	if (!pdata) {
		dev_err(pdev->dev.parent, "Platform data not supplied\n");
		return -ENODEV;
	}

	if (iodev->dev->of_node) {
		ret = s2mps11_pmic_dt_parse_pdata(pdev, pdata);
		if (ret)
			return ret;
	}

	s2mps11 = devm_kzalloc(&pdev->dev, sizeof(struct s2mps11_info),
				GFP_KERNEL);
	if (!s2mps11)
		return -ENOMEM;

	dev_type = platform_get_device_id(pdev)->driver_data;

	switch(dev_type) {
	case S2MPS11X:
		regulators = s2mps11_regulators;
		break;
	case S2MPS14X:
		regulators = s2mps14_regulators;
		break;
	default:
		dev_err(&pdev->dev, "Invalid device type: %u\n", dev_type);
		return -EINVAL;
	};

	s2mps11->rdev_num = pdata->num_regulators;
	s2mps11->rdev = devm_kzalloc(&pdev->dev,
			sizeof(*s2mps11->rdev)*s2mps11->rdev_num, GFP_KERNEL);
	if (!s2mps11->rdev)
		return -ENOMEM;

	platform_set_drvdata(pdev, s2mps11);

	s2mps11->ramp_delay2 = pdata->buck2_ramp_delay;
	s2mps11->ramp_delay34 = pdata->buck34_ramp_delay;
	s2mps11->ramp_delay5 = pdata->buck5_ramp_delay;
	s2mps11->ramp_delay16 = pdata->buck16_ramp_delay;
	s2mps11->ramp_delay7810 = pdata->buck7810_ramp_delay;
	s2mps11->ramp_delay9 = pdata->buck9_ramp_delay;

	s2mps11->buck6_ramp = pdata->buck6_ramp_enable;
	s2mps11->buck2_ramp = pdata->buck2_ramp_enable;
	s2mps11->buck3_ramp = pdata->buck3_ramp_enable;
	s2mps11->buck4_ramp = pdata->buck4_ramp_enable;
	s2mps11->opmode = pdata->opmode;

	ramp_enable = (s2mps11->buck2_ramp << 3) | (s2mps11->buck3_ramp << 2) |
		(s2mps11->buck4_ramp << 1) | s2mps11->buck6_ramp ;

	if (ramp_enable) {
		if (s2mps11->buck2_ramp)
			ramp_reg |= get_ramp_delay(s2mps11->ramp_delay2) << 6;
		if (s2mps11->buck3_ramp || s2mps11->buck4_ramp)
			ramp_reg |= get_ramp_delay(s2mps11->ramp_delay34) << 4;
		sec_reg_write(iodev->regmap_pmic, S2MPS11_REG_RAMP, ramp_reg | ramp_enable);
	}

	if (dev_type == S2MPS11X) {
		ramp_reg &= 0x00;
		ramp_reg |= get_ramp_delay(s2mps11->ramp_delay5) << 6;
		ramp_reg |= get_ramp_delay(s2mps11->ramp_delay16) << 4;
		ramp_reg |= get_ramp_delay(s2mps11->ramp_delay7810) << 2;
		ramp_reg |= get_ramp_delay(s2mps11->ramp_delay9);
		sec_reg_write(iodev->regmap_pmic, S2MPS11_REG_RAMP_BUCK, ramp_reg);
	}

	for (i = 0; i < s2mps11->rdev_num; i++) {
		int id = pdata->regulators[i].id;

		config.dev = &pdev->dev;
		config.regmap = iodev->regmap_pmic;
		config.init_data = pdata->regulators[i].initdata;
		config.driver_data = s2mps11;
		config.of_node = pdata->regulators[i].reg_node;

		s2mps11->rdev[i] = regulator_register(&regulators[id], &config);
		if (IS_ERR(s2mps11->rdev[i])) {
			ret = PTR_ERR(s2mps11->rdev[i]);
			dev_err(&pdev->dev, "regulator init failed for %d\n",
				i);
			s2mps11->rdev[i] = NULL;
			goto err;
		}
	}

	return 0;
err:
	for (i = 0; i < s2mps11->rdev_num; i++)
		regulator_unregister(s2mps11->rdev[i]);

	return ret;
}

static int s2mps11_pmic_remove(struct platform_device *pdev)
{
	struct s2mps11_info *s2mps11 = platform_get_drvdata(pdev);
	int i;

	for (i = 0; i < s2mps11->rdev_num; i++)
		regulator_unregister(s2mps11->rdev[i]);

	return 0;
}

static const struct platform_device_id s2mps11_pmic_id[] = {
	{ "s2mps11-pmic", S2MPS11X},
	{ "s2mps14-pmic", S2MPS14X},
	{ },
};
MODULE_DEVICE_TABLE(platform, s2mps11_pmic_id);

static struct platform_driver s2mps11_pmic_driver = {
	.driver = {
		.name = "s2mps11-pmic",
		.owner = THIS_MODULE,
	},
	.probe = s2mps11_pmic_probe,
	.remove = s2mps11_pmic_remove,
	.id_table = s2mps11_pmic_id,
};

static int __init s2mps11_pmic_init(void)
{
	return platform_driver_register(&s2mps11_pmic_driver);
}
subsys_initcall(s2mps11_pmic_init);

static void __exit s2mps11_pmic_exit(void)
{
	platform_driver_unregister(&s2mps11_pmic_driver);
}
module_exit(s2mps11_pmic_exit);

/* Module information */
MODULE_AUTHOR("Sangbeom Kim <sbkim73@samsung.com>");
MODULE_DESCRIPTION("SAMSUNG S2MPS11/S2MPS14 Regulator Driver");
MODULE_LICENSE("GPL");
