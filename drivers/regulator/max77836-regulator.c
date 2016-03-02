/*
 * Copyright (c) 2014, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/err.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/regulator/driver.h>
#include <linux/mfd/max14577.h>
#include <linux/mfd/max14577-private.h>
#include <linux/regulator/max77836-regulator.h>
#include <linux/module.h>
#include <linux/types.h>

/* Register */
#define REG_CNFG1_LDO1		0x51
#define REG_CNFG2_LDO1		0x52
#define REG_CNFG1_LDO2		0x53
#define REG_CNFG2_LDO2		0x54
#define REG_CNFG_LDO_BIAS	0x55

/* CNFG1_LDOX */
#define CNFG1_LDO_PWR_MD_L_SHIFT		0x6
#define CNFG1_LDO_TV_L_SHIFT			0x0
#define CNFG1_LDO_PWR_MD_L_MASK			(0x3 << CNFG1_LDO_PWR_MD_L_SHIFT)
#define CNFG1_LDO_TV_L_MASK				(0x3F << CNFG1_LDO_TV_L_SHIFT)

/* CNFG2_LDOX */
#define CNFG2_LDO_OVLMP_EN_L_SHIFT		0x7
#define CNFG2_LDO_ALPM_EN_L_SHIFT		0x6
#define CNFG2_LDO_COMP_L_SHIFT			0x4
#define CNFG2_LDO_POK_L_SHIFT			0x3
#define CNFG2_LDO_ADE_L_SHIFT			0x1
#define CNFG2_LDO_SS_L_SHIFT			0x0

/* CNFG_LDO_BIAS */
#define BIT_L_B_LEN			0x02
#define BIT_L_B_EN			0x01


#define MAX77836_LDO_MINUV	800000		/* 0.8V */
#define MAX77836_LDO_MAXUV	3950000		/* 3.95V */
#define MAX77836_LDO_UVSTEP 50000		/* 0.05V */

/* Undefined register address value */
#define REG_RESERVED 0xFF

struct max77836_reg
{
	struct device *dev;
	struct max14577_dev *mfd_dev;
	struct i2c_client *i2c;
	struct regulator_dev **rdev;
	int *enable_mode;
	int num_regulators;
};

struct max77836_reg *g_max77836_reg;
static atomic_t comp_ctrl;

struct voltage_map_desc {
	int min;
	int max;
	int step;
};

static struct voltage_map_desc reg_voltage_map[] = {
	{
		.min = MAX77836_LDO_MINUV,
		.max = MAX77836_LDO_MAXUV,
		.step = MAX77836_LDO_UVSTEP,
	},
	{
		.min = MAX77836_LDO_MINUV,
		.max = MAX77836_LDO_MAXUV,
		.step = MAX77836_LDO_UVSTEP,
	},
};

static inline int max77836_get_rid(struct regulator_dev *rdev)
{
	return rdev_get_id(rdev);
}

static int max77836_reg_list_voltage (struct regulator_dev *rdev,
	unsigned int selector)
{
	struct voltage_map_desc *vol_desc = NULL;
	int rid = max77836_get_rid(rdev);
	int val;

	if (rid >= ARRAY_SIZE(reg_voltage_map) || rid < 0)
		return -EINVAL;

	vol_desc = &reg_voltage_map[rid];
	if (!vol_desc)
		return -EINVAL;

	val = vol_desc->min + vol_desc->step * selector;
	if (val > vol_desc->max)
		return -EINVAL;

	return val;
}

static int max77836_reg_is_enabled (struct regulator_dev *rdev)
{
	const struct max77836_reg *reg_data = rdev_get_drvdata(rdev);
	u8 val, mask;
	int ret;

	ret = max14577_read_reg(reg_data->i2c, rdev->desc->enable_reg, &val);
	if (ret == -EINVAL)
		return 1; /* "not controllable" */
	else if (ret)
		return ret;

	mask = rdev->desc->enable_mask;
	return ((val & mask) == mask? 1 : 0);
}

static int max77836_reg_enable(struct regulator_dev *rdev)
{
	const struct max77836_reg *reg_data = rdev_get_drvdata(rdev);
	struct regulator_desc *desc = rdev->desc;
	int rid = max77836_get_rid(rdev);
	u8 val = 0;

	val = reg_data->enable_mode[rid] << CNFG1_LDO_PWR_MD_L_SHIFT;

	return max14577_update_reg(reg_data->i2c,
		desc->enable_reg, val, desc->enable_mask);
}

static int max77836_reg_disable (struct regulator_dev *rdev)
{
	const struct max77836_reg *reg_data = rdev_get_drvdata(rdev);
	struct regulator_desc *desc = rdev->desc;

	return max14577_update_reg(reg_data->i2c,
		desc->enable_reg, REG_OUTPUT_DISABLE, desc->enable_mask);
}

static int max77836_reg_get_voltage_sel (struct regulator_dev *rdev)
{
	const struct max77836_reg *reg_data = rdev_get_drvdata(rdev);
	struct regulator_desc *desc = rdev->desc;
	u8 val = 0;
	int ret;

	ret = max14577_read_reg(reg_data->i2c, desc->enable_reg, &val);
	if (ret)
		return ret;

	val = val & desc->vsel_mask;
	return val;
}

static int max77836_reg_set_voltage_sel (struct regulator_dev *rdev,
	unsigned int selector)
{
	struct max77836_reg *reg_data = rdev_get_drvdata(rdev);
	struct regulator_desc *desc = rdev->desc;

	return max14577_update_reg(reg_data->i2c,
		desc->vsel_reg, selector, desc->vsel_mask);
}

static struct regulator_ops max77836_ldo_ops = {
	.list_voltage		= max77836_reg_list_voltage,
	.is_enabled			= max77836_reg_is_enabled,
	.enable				= max77836_reg_enable,
	.disable			= max77836_reg_disable,
	.get_voltage_sel	= max77836_reg_get_voltage_sel,
	.set_voltage_sel	= max77836_reg_set_voltage_sel,
};

#define regulator_desc_ldo(num)		{			\
	.name			= "LDO"#num,				\
	.id				= MAX77836_LDO##num,		\
	.ops			= &max77836_ldo_ops,		\
	.type			= REGULATOR_VOLTAGE,		\
	.owner			= THIS_MODULE,				\
	.min_uV			= MAX77836_LDO_MINUV,		\
	.uV_step		= MAX77836_LDO_UVSTEP,		\
	.n_voltages		= CNFG1_LDO_TV_L_MASK +1,	\
	.vsel_reg		= REG_CNFG1_LDO##num,		\
	.vsel_mask		= CNFG1_LDO_TV_L_MASK,		\
	.enable_reg		= REG_CNFG1_LDO##num,		\
	.enable_mask	= CNFG1_LDO_PWR_MD_L_MASK,	\
}

static struct regulator_desc regulators[] = {
	regulator_desc_ldo(1),
	regulator_desc_ldo(2),
};

int max77836_ldo2_set(bool enable)
{
	u8 val;
	int ret;

	if (!g_max77836_reg || !atomic_read(&comp_ctrl)) {
		pr_info("%s: can't control ldo2, en:%d\n", __func__, enable);
		return -ENODEV;
	}

	if (enable) {
		ret = max14577_read_reg(g_max77836_reg->i2c, REG_CNFG_LDO_BIAS, &val);
		if (ret) {
			pr_err("%s: failed to read ldo bias reg : %d\n", __func__, ret);
			return ret;
		}

		/* enable ldo bias */
		val |= 0x03;
		ret = max14577_write_reg(g_max77836_reg->i2c, REG_CNFG_LDO_BIAS, val);
		if (ret) {
			pr_err("%s: failed to set ldo bias reg : %d\n", __func__, ret);
			return ret;
		}

		/* ldo2 on with 1.8V */
		val = 0xD4;
		ret = max14577_write_reg(g_max77836_reg->i2c, REG_CNFG1_LDO2, val);
		if (ret) {
			pr_err("%s: failed to set ldo2 cfg1 reg : %d\n", __func__, ret);
			return ret;
		}
	} else {
		/* ldo2 off with 1.8V */
		val = 0x14;
		ret = max14577_write_reg(g_max77836_reg->i2c, REG_CNFG1_LDO2, val);
		if (ret) {
			pr_err("%s: failed to set ldo2 cfg1 reg : %d\n", __func__, ret);
			return ret;
		}

		ret = max14577_read_reg(g_max77836_reg->i2c, REG_CNFG2_LDO2, &val);
		if (ret) {
			pr_err("%s: failed to read ldo2 cfg2 reg : %d\n", __func__, ret);
			return ret;
		}

		/* enable active discharge */
		val |= 0x02;
		ret = max14577_write_reg(g_max77836_reg->i2c, REG_CNFG2_LDO2, val);
		if (ret) {
			pr_err("%s: failed to set ldo2 cfg2 reg : %d\n", __func__, ret);
			return ret;
		}

	}

	pr_info("%s: enable[%d], val[0x%x]\n", __func__, enable, val);
	return 0;
}
EXPORT_SYMBOL(max77836_ldo2_set);

int max77836_comparator_set(bool enable)
{
	u8 en, val;
	int ret;

	if (!g_max77836_reg || !atomic_read(&comp_ctrl)) {
		pr_info("%s: can't control comp, en:%d\n", __func__, enable);
		return -ENODEV;
	}

	ret = max14577_read_reg(g_max77836_reg->i2c, MAX77836_PMIC_REG_COMP, &val);
	if (ret) {
		pr_err("%s: failed to read comparator reg :%d\n", __func__, ret);
		return ret;
	}
	pr_info("%s: old comp:0x%02x, en:%d\n", __func__, val, enable);

	en = !!enable;
	val = ((MAX77836_PMIC_REG_COMP_REF_900mV << MAX77836_PMIC_REG_COMP_REF_SHIFT)
			| (0x1 << MAX77836_PMIC_REG_COMP_POLARITY_SHIFT)
			| (en << MAX77836_PMIC_REG_COMP_EN_SHIFT));
	ret = max14577_write_reg(g_max77836_reg->i2c, MAX77836_PMIC_REG_COMP, val);
	if (ret) {
		pr_err("%s: failed to set comparator reg :%d\n", __func__, ret);
		return ret;
	}

	ret = max14577_read_reg(g_max77836_reg->i2c, MAX77836_PMIC_REG_COMP, &val);
	if (ret) {
		pr_err("%s: failed to read comparator reg :%d\n", __func__, ret);
		return ret;
	}

	pr_info("%s: new comp:0x%02x\n", __func__, val);
	return 0;
}
EXPORT_SYMBOL(max77836_comparator_set);

static int max77836_reg_hw_init(struct max77836_reg *max77836_reg,
		struct max77836_reg_platform_data *pdata)
{
	int reg_cnfg2_data, reg_cnfg2_addr;

	reg_cnfg2_data = pdata->overvoltage_clamp_enable << CNFG2_LDO_OVLMP_EN_L_SHIFT |
					 pdata->auto_low_power_mode_enable << CNFG2_LDO_ALPM_EN_L_SHIFT |
					 pdata->compensation_ldo << CNFG2_LDO_COMP_L_SHIFT |
					 pdata->active_discharge_enable << CNFG2_LDO_ADE_L_SHIFT |
					 pdata->soft_start_slew_rate << CNFG2_LDO_SS_L_SHIFT;

	switch (pdata->reg_id) {
		case MAX77836_LDO1:
			reg_cnfg2_addr = REG_CNFG2_LDO1;
			break;
		case MAX77836_LDO2:
			reg_cnfg2_addr = REG_CNFG2_LDO2;
			break;
		default:
			return -ENODEV;
	}
	return max14577_write_reg(max77836_reg->i2c, reg_cnfg2_addr, reg_cnfg2_data);
}

static int max77836_regulator_probe(struct platform_device *pdev)
{
	struct max14577_dev *max14577 = dev_get_drvdata(pdev->dev.parent);
	struct max14577_platform_data *pdata = dev_get_platdata(max14577->dev);
	struct max77836_reg *max77836_reg = NULL;
	struct regulator_dev **rdev;
	u8 val = 0;
	int rc, size;
	int i, ret;

	max77836_reg = devm_kzalloc(&pdev->dev, sizeof(*max77836_reg), GFP_KERNEL);
	if (unlikely(max77836_reg == NULL))
		return -ENOMEM;

	dev_info(&pdev->dev, "func: %s\n", __func__);

	max77836_reg->dev = &pdev->dev;
	max77836_reg->mfd_dev = max14577;
	max77836_reg->i2c = max14577->i2c_pmic;
	max77836_reg->enable_mode = pdata->opmode_data;
	max77836_reg->num_regulators = pdata->num_regulators;
	platform_set_drvdata(pdev, max77836_reg);

	g_max77836_reg = max77836_reg;
	atomic_set(&comp_ctrl, 1);

	size = sizeof(struct regulator_dev *) * pdata->num_regulators;
	max77836_reg->rdev = kzalloc(size, GFP_KERNEL);
	if (!max77836_reg->rdev) {
		dev_err(&pdev->dev, "%s: Failed to allocate rdev memory\n", __func__);
		kfree(max77836_reg);
		return -ENOMEM;
	}

	rdev = max77836_reg->rdev;
	for (i = 0; i < max77836_reg->num_regulators; i++) {
		rc = max77836_reg_hw_init(max77836_reg, &pdata->regulators[i]);
		if (IS_ERR_VALUE(rc))
			goto err;

		rdev[i] = regulator_register(&regulators[i], max77836_reg->dev,
			pdata->regulators[i].init_data, max77836_reg, NULL);
		if (IS_ERR_OR_NULL(rdev)) {
			dev_err(&pdev->dev,	"%s: regulator-%d register failed\n", __func__, i);
			goto err;
		}
	}

	/* Set comparator reg */
	val = ((MAX77836_PMIC_REG_COMP_REF_900mV << MAX77836_PMIC_REG_COMP_REF_SHIFT)
			| (0x1 << MAX77836_PMIC_REG_COMP_POLARITY_SHIFT)
			| (0x1 << MAX77836_PMIC_REG_COMP_EN_SHIFT));
	rc = max14577_write_reg(max77836_reg->i2c, MAX77836_PMIC_REG_COMP, val);
	if (rc) {
		pr_err("%s: failed to set comparator reg :%d\n", __func__, rc);
		goto err;
	}

	/* disable ldo2 to supply the power on wpc ic */
	ret = max77836_ldo2_set(false);
	if (ret < 0)
		pr_err("%s: ldo2 set failed.\n", __func__);

	return 0;
err:
	dev_err(&pdev->dev, "%s: returns %d\n", __func__, rc);
	while(--i >= 0)
		if (rdev[i])
			regulator_unregister(rdev[i]);

	kfree(rdev);
	kfree(max77836_reg);
	return rc;
}

static int max77836_regulator_remove(struct platform_device *pdev)
{
	struct max77836_reg *max77836_reg = platform_get_drvdata(pdev);
	int i;

	for (i = 0; i < max77836_reg->num_regulators; i++)
		if (max77836_reg->rdev[i])
			regulator_unregister(max77836_reg->rdev[i]);

	kfree(max77836_reg->rdev);
	kfree(max77836_reg);
	return 0;
}

static void max77836_regulator_shutdown(struct device *dev)
{
	struct max77836_reg *max77836_reg = dev_get_drvdata(dev);
	u8 val = 0;
	int rc = 0;

	/* disable ldo2 to supply the power on wpc ic */
	rc = max77836_ldo2_set(false);
	if (rc < 0)
		pr_err("%s: ldo2 set failed.\n", __func__);

	atomic_set(&comp_ctrl, 0);

	val = 0x0 << MAX77836_PMIC_REG_COMP_EN_SHIFT;
	rc = max14577_write_reg(max77836_reg->i2c, MAX77836_PMIC_REG_COMP, val);
	pr_info("%s: Disable comparator_en: %d\n", __func__, rc);

	/* Disable all ldo bias for power-off current leakage reducing */
	val = 0x00;
	rc = max14577_write_reg(g_max77836_reg->i2c, REG_CNFG_LDO_BIAS, val);
	pr_info("%s: Disable LDO_BIAS: %d\n", __func__, rc);

	return;
}

static const struct platform_device_id max77836_regulator_id[] =
{
	{ "max77836-regulator", 0},
	{ },
};

MODULE_DEVICE_TABLE(platform, max77836_regulator_id);

static struct platform_driver max77836_regulator_driver =
{
	.driver =
	{
		.name = "max77836-regulator",
		.owner = THIS_MODULE,
		.shutdown = max77836_regulator_shutdown,
	},
	.probe = max77836_regulator_probe,
	.remove = max77836_regulator_remove,
	.id_table = max77836_regulator_id,
};

static int __init max77836_regulator_init(void)
{
	return platform_driver_register(&max77836_regulator_driver);
}

module_init(max77836_regulator_init);

static void __exit max77836_regulator_exit(void)
{
	platform_driver_unregister(&max77836_regulator_driver);
}

module_exit(max77836_regulator_exit);

MODULE_DESCRIPTION("MAXIM 77836 Regulator Driver");
MODULE_AUTHOR("Clark Kim <clark.kim@maximintegrated.com>");
MODULE_VERSION("1.0");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:max77836-regulator");

