/*
 * Charger driver for Maxim MAX77843
 *
 * Copyright (C) 2015 Samsung Electronics, Co., Ltd.
 * Author: Beomho Seo <beomho.seo@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published bythe Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/power_supply.h>
#include <linux/mfd/max77843-private.h>

struct max77843_charger_info {
	u32 fast_charge_uamp;
	u32 top_off_uamp;
	u32 input_uamp_limit;
};

struct max77843_charger {
	struct device		*dev;
	struct max77843		*max77843;
	struct i2c_client	*client;
	struct regmap		*regmap;
	struct power_supply	psy;

	struct max77843_charger_info	*info;
};

static int max77843_charger_get_max_current(struct max77843_charger *charger)
{
	struct regmap *regmap = charger->regmap;
	int ret, val = 0;
	unsigned int reg_data;

	ret = regmap_read(regmap, MAX77843_CHG_REG_CHG_CNFG_09, &reg_data);
	if (ret) {
		dev_err(charger->dev,
			"Failed to read max current register: %d\n", ret);
		return ret;
	}

	if (reg_data <= 0x03) {
		val = MAX77843_CHG_INPUT_CURRENT_LIMIT_MIN;
	} else if (reg_data >= 0x78) {
		val = MAX77843_CHG_INPUT_CURRENT_LIMIT_MAX;
	} else {
		val = reg_data / 3;
		if (reg_data % 3 == 0)
			val *= 100000;
		else if (reg_data % 3 == 1)
			val = val * 100000 + 33000;
		else
			val = val * 100000 + 67000;
	}

	return val;
}

static int max77843_charger_get_now_current(struct max77843_charger *charger)
{
	struct regmap *regmap = charger->regmap;
	int ret, val = 0;
	unsigned int reg_data;

	ret = regmap_read(regmap, MAX77843_CHG_REG_CHG_CNFG_02, &reg_data);
	if (ret) {
		dev_err(charger->dev,
			"Failed to read charge current register: %d\n", ret);
		return ret;
	}

	reg_data &= MAX77843_CHG_FAST_CHG_CURRENT_MASK;

	if (reg_data <= 0x02)
		val = MAX77843_CHG_FAST_CHG_CURRENT_MIN;
	else if (reg_data >= 0x3f)
		val = MAX77843_CHG_FAST_CHG_CURRENT_MAX;
	else
		val = reg_data * MAX77843_CHG_FAST_CHG_CURRENT_STEP;

	return val;
}

static int max77843_charger_get_online(struct max77843_charger *charger)
{
	struct regmap *regmap = charger->regmap;
	int ret, val = 0;
	unsigned int reg_data;

	ret = regmap_read(regmap, MAX77843_CHG_REG_CHG_INT_OK, &reg_data);
	if (ret) {
		dev_err(charger->dev,
				"Failed to read charger status: %d\n", ret);
		return ret;
	}

	if (reg_data & MAX77843_CHG_CHGIN_OK)
		val = true;
	else
		val = false;

	return val;
}

static int max77843_charger_get_present(struct max77843_charger *charger)
{
	struct regmap *regmap = charger->regmap;
	int ret, val = 0;
	unsigned int reg_data;

	ret = regmap_read(regmap, MAX77843_CHG_REG_CHG_DTLS_00,	&reg_data);
	if (ret) {
		dev_err(charger->dev,
				"Failed to read battery present: %d\n", ret);
		return ret;
	}

	if (reg_data & MAX77843_CHG_BAT_DTLS)
		val = false;
	else
		val = true;

	return val;
}

static int max77843_charger_get_health(struct max77843_charger *charger)
{
	struct regmap *regmap = charger->regmap;
	int ret, val = POWER_SUPPLY_HEALTH_UNKNOWN;
	unsigned int reg_data;

	ret = regmap_read(regmap, MAX77843_CHG_REG_CHG_DTLS_01,	&reg_data);
	if (ret) {
		dev_err(charger->dev,
				"Failed to read battery health: %d\n", ret);
		return ret;
	}

	reg_data &= MAX77843_CHG_BAT_DTLS_MASK;

	switch (reg_data) {
	case MAX77843_CHG_NO_BAT:
		val = POWER_SUPPLY_HEALTH_UNSPEC_FAILURE;
		break;
	case MAX77843_CHG_LOW_VOLT_BAT:
	case MAX77843_CHG_OK_BAT:
	case MAX77843_CHG_OK_LOW_VOLT_BAT:
		val = POWER_SUPPLY_HEALTH_GOOD;
		break;
	case MAX77843_CHG_LONG_BAT_TIME:
		val = POWER_SUPPLY_HEALTH_DEAD;
		break;
	case MAX77843_CHG_OVER_VOLT_BAT:
	case MAX77843_CHG_OVER_CURRENT_BAT:
		val = POWER_SUPPLY_HEALTH_OVERVOLTAGE;
		break;
	default:
		val = POWER_SUPPLY_HEALTH_UNKNOWN;
		break;
	}

	return val;
}

static int max77843_charger_get_status(struct max77843_charger *charger)
{
	struct regmap *regmap = charger->regmap;
	int ret, val = 0;
	unsigned int reg_data;

	ret = regmap_read(regmap, MAX77843_CHG_REG_CHG_DTLS_01,	&reg_data);
	if (ret) {
		dev_err(charger->dev,
				"Failed to read charger status: %d\n", ret);
		return ret;
	}

	reg_data &= MAX77843_CHG_DTLS_MASK;

	switch (reg_data) {
	case MAX77843_CHG_PQ_MODE:
	case MAX77843_CHG_CC_MODE:
	case MAX77843_CHG_CV_MODE:
		val = POWER_SUPPLY_STATUS_CHARGING;
		break;
	case MAX77843_CHG_TO_MODE:
	case MAX77843_CHG_DO_MODE:
		val = POWER_SUPPLY_STATUS_FULL;
		break;
	case MAX77843_CHG_HT_MODE:
	case MAX77843_CHG_TF_MODE:
	case MAX77843_CHG_TS_MODE:
		val = POWER_SUPPLY_STATUS_NOT_CHARGING;
		break;
	case MAX77843_CHG_OFF_MODE:
		val = POWER_SUPPLY_STATUS_DISCHARGING;
		break;
	default:
		val = POWER_SUPPLY_STATUS_UNKNOWN;
		break;
	}

	return val;
}

static const char *model_name = "MAX77843";
static const char *manufacturer = "Maxim Integrated";

static int max77843_charger_get_property(struct power_supply *psy,
		enum power_supply_property psp,
		union power_supply_propval *val)
{
	struct max77843_charger *charger = container_of(psy,
				struct max77843_charger, psy);

	switch (psp) {
	case POWER_SUPPLY_PROP_STATUS:
		val->intval = max77843_charger_get_status(charger);
		break;
	case POWER_SUPPLY_PROP_HEALTH:
		val->intval = max77843_charger_get_health(charger);
		break;
	case POWER_SUPPLY_PROP_PRESENT:
		val->intval = max77843_charger_get_present(charger);
		break;
	case POWER_SUPPLY_PROP_ONLINE:
		val->intval = max77843_charger_get_online(charger);
		break;
	case POWER_SUPPLY_PROP_CURRENT_NOW:
		val->intval = max77843_charger_get_now_current(charger);
		break;
	case POWER_SUPPLY_PROP_CURRENT_MAX:
		val->intval = max77843_charger_get_max_current(charger);
		break;
	case POWER_SUPPLY_PROP_MODEL_NAME:
		val->strval =  model_name;
		break;
	case POWER_SUPPLY_PROP_MANUFACTURER:
		val->strval = manufacturer;
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static enum power_supply_property max77843_charger_props[] = {
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_HEALTH,
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_ONLINE,
	POWER_SUPPLY_PROP_CURRENT_NOW,
	POWER_SUPPLY_PROP_CURRENT_MAX,
	POWER_SUPPLY_PROP_MODEL_NAME,
	POWER_SUPPLY_PROP_MANUFACTURER,
};

static int max77843_charger_init_current_limit(struct max77843_charger *charger)
{
	struct regmap *regmap = charger->regmap;
	struct max77843_charger_info *info = charger->info;
	unsigned int input_uamp_limit = info->input_uamp_limit;
	int ret;
	unsigned int reg_data, val;

	ret = regmap_update_bits(regmap, MAX77843_CHG_REG_CHG_CNFG_02,
			MAX77843_CHG_OTG_ILIMIT_MASK,
			MAX77843_CHG_OTG_ILIMIT_900);
	if (ret) {
		dev_err(charger->dev,
			"Failed to write OTG current limit: %d\n", ret);
		return ret;
	}

	if (input_uamp_limit == MAX77843_CHG_INPUT_CURRENT_LIMIT_MIN) {
		reg_data = 0x03;
	} else if (input_uamp_limit == MAX77843_CHG_INPUT_CURRENT_LIMIT_MAX) {
		reg_data = 0x78;
	} else {
		if (input_uamp_limit < MAX77843_CHG_INPUT_CURRENT_LIMIT_REF)
			val = 0x03;
		else
			val = 0x02;

		input_uamp_limit -= MAX77843_CHG_INPUT_CURRENT_LIMIT_MIN;
		input_uamp_limit /= MAX77843_CHG_INPUT_CURRENT_LIMIT_STEP;
		reg_data = val + input_uamp_limit;
	}

	ret = regmap_write(regmap, MAX77843_CHG_REG_CHG_CNFG_09, reg_data);
	if (ret)
		dev_err(charger->dev,
			"Failed to write charge current limit: %d\n", ret);

	return ret;
}

static int max77843_charger_init_top_off(struct max77843_charger *charger)
{
	struct regmap *regmap = charger->regmap;
	struct max77843_charger_info *info = charger->info;
	unsigned int top_off_uamp = info->top_off_uamp;
	int ret;
	unsigned int reg_data;

	if (top_off_uamp == MAX77843_CHG_TOP_OFF_CURRENT_MIN) {
		reg_data = 0x00;
	} else if (top_off_uamp == MAX77843_CHG_TOP_OFF_CURRENT_MAX) {
		reg_data = 0x07;
	} else {
		top_off_uamp -= MAX77843_CHG_TOP_OFF_CURRENT_MIN;
		top_off_uamp /= MAX77843_CHG_TOP_OFF_CURRENT_STEP;
		reg_data = top_off_uamp;
	}

	ret = regmap_update_bits(regmap, MAX77843_CHG_REG_CHG_CNFG_03,
			MAX77843_CHG_TOP_OFF_CURRENT_MASK, reg_data);
	if (ret)
		dev_err(charger->dev,
				"Failed to write top off current: %d\n", ret);

	return ret;
}

static int max77843_charger_init_fast_charge(struct max77843_charger *charger)
{
	struct regmap *regmap = charger->regmap;
	struct max77843_charger_info *info = charger->info;
	unsigned int fast_charge_uamp = info->fast_charge_uamp;
	int ret;
	unsigned int reg_data;

	if (fast_charge_uamp < info->input_uamp_limit) {
		reg_data = 0x09;
	} else if (fast_charge_uamp == MAX77843_CHG_FAST_CHG_CURRENT_MIN) {
		reg_data = 0x02;
	} else if (fast_charge_uamp == MAX77843_CHG_FAST_CHG_CURRENT_MAX) {
		reg_data = 0x3f;
	} else {
		fast_charge_uamp -= MAX77843_CHG_FAST_CHG_CURRENT_MIN;
		fast_charge_uamp /= MAX77843_CHG_FAST_CHG_CURRENT_STEP;
		reg_data = 0x02 + fast_charge_uamp;
	}

	ret = regmap_update_bits(regmap, MAX77843_CHG_REG_CHG_CNFG_02,
			MAX77843_CHG_FAST_CHG_CURRENT_MASK, reg_data);
	if (ret)
		dev_err(charger->dev,
			"Failed to write fast charge current: %d\n", ret);

	return ret;
}

static int max77843_charger_init(struct max77843_charger *charger)
{
	struct regmap *regmap = charger->regmap;
	int ret;

	ret = regmap_write(regmap, MAX77843_CHG_REG_CHG_CNFG_06,
			MAX77843_CHG_WRITE_CAP_UNBLOCK);
	if (ret) {
		dev_err(charger->dev,
			"Failed to unblock write capability: %d\n", ret);
		return ret;
	}

	ret = regmap_write(regmap, MAX77843_CHG_REG_CHG_CNFG_01,
			MAX77843_CHG_RESTART_THRESHOLD_DISABLE);
	if (ret) {
		dev_err(charger->dev,
			"Failed to write charger restart threshold: %d\n", ret);
		return ret;
	}

	ret = max77843_charger_init_fast_charge(charger);
	if (ret) {
		dev_err(charger->dev,
				"Failed to set fast charge mode: %d\n", ret);
		return ret;
	}

	ret = max77843_charger_init_top_off(charger);
	if (ret) {
		dev_err(charger->dev, "Failed to set top off charge mode.\n");
		return ret;
	}

	ret = max77843_charger_init_current_limit(charger);
	if (ret)
		dev_err(charger->dev, "Faied to set current limit.\n");

	return 0;
}

static struct max77843_charger_info *max77843_charger_dt_init(
		struct platform_device *pdev)
{
	struct max77843_charger_info *info;
	struct device_node *np = pdev->dev.of_node;
	int ret;

	if (!np) {
		dev_err(&pdev->dev, "No charger OF node\n");
		return ERR_PTR(-EINVAL);
	}

	info = devm_kzalloc(&pdev->dev, sizeof(*info), GFP_KERNEL);
	if (!info)
		return ERR_PTR(-ENOMEM);

	ret = of_property_read_u32(np, "maxim,fast-charge-uamp",
			&info->fast_charge_uamp);
	if (ret) {
		dev_err(&pdev->dev, "Cannot parse fast charge current.\n");
		return ERR_PTR(ret);
	}

	ret = of_property_read_u32(np, "maxim,top-off-uamp",
			&info->top_off_uamp);
	if (ret) {
		dev_err(&pdev->dev,
			"Cannot parse primary charger termination voltage.\n");
		return ERR_PTR(ret);
	}

	ret = of_property_read_u32(np, "maxim,input-uamp-limit",
			&info->input_uamp_limit);
	if (ret) {
		dev_err(&pdev->dev, "Cannot parse input current limit value\n");
		return ERR_PTR(ret);
	}

	return info;
}

static int max77843_charger_probe(struct platform_device *pdev)
{
	struct max77843 *max77843 = dev_get_drvdata(pdev->dev.parent);
	struct max77843_charger *charger;
	int ret;

	charger = devm_kzalloc(&pdev->dev, sizeof(*charger), GFP_KERNEL);
	if (!charger)
		return -ENOMEM;

	platform_set_drvdata(pdev, charger);
	charger->dev = &pdev->dev;
	charger->max77843 = max77843;
	charger->client = max77843->i2c_chg;
	charger->regmap = max77843->regmap_chg;

	charger->info = max77843_charger_dt_init(pdev);
	if (IS_ERR_OR_NULL(charger->info))
		return PTR_ERR(charger->info);

	charger->psy.name	= "max77843-charger";
	charger->psy.type	= POWER_SUPPLY_TYPE_MAINS;
	charger->psy.get_property	= max77843_charger_get_property;
	charger->psy.properties		= max77843_charger_props;
	charger->psy.num_properties	= ARRAY_SIZE(max77843_charger_props);

	ret = max77843_charger_init(charger);
	if (ret)
		return ret;

	ret = power_supply_register(&pdev->dev, &charger->psy);
	if (ret) {
		dev_err(&pdev->dev,
			"Failed to register power supply %d\n", ret);
		return ret;
	}

	return 0;
}

static int max77843_charger_remove(struct platform_device *pdev)
{
	struct max77843_charger *charger = platform_get_drvdata(pdev);

	power_supply_unregister(&charger->psy);

	return 0;
}

static const struct platform_device_id max77843_charger_id[] = {
	{ "max77843-charger", },
	{ }
};
MODULE_DEVICE_TABLE(platform, max77843_charger_id);

static struct platform_driver max77843_charger_driver = {
	.driver = {
		.name = "max77843-charger",
	},
	.probe = max77843_charger_probe,
	.remove = max77843_charger_remove,
	.id_table = max77843_charger_id,
};
module_platform_driver(max77843_charger_driver);

MODULE_DESCRIPTION("Maxim MAX77843 charger driver");
MODULE_AUTHOR("Beomho Seo <beomho.seo@samsung.com>");
MODULE_LICENSE("GPL");
