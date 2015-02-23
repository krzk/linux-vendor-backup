/*
 * Fuel gauge driver for Maxim MAX77843
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

struct max77843_battery {
	struct device		*dev;
	struct max77843		*max77843;
	struct i2c_client	*client;
	struct regmap		*regmap;
	struct power_supply	psy;
};

static int max77843_battery_get_capacity(struct max77843_battery *battery)
{
	struct regmap *regmap = battery->regmap;
	int ret, val;
	unsigned int reg_data;

	ret = regmap_read(regmap, MAX77843_FG_REG_SOCREP, &reg_data);
	if (ret) {
		dev_err(battery->dev, "Failed to read fuelgauge register\n");
		return ret;
	}

	val = reg_data >> 8;

	return val;
}

static int max77843_battery_get_energy_prop(struct max77843_battery *battery,
		enum power_supply_property psp)
{
	struct regmap *regmap = battery->regmap;
	unsigned int reg;
	int ret, val;
	unsigned int reg_data;

	switch (psp) {
	case POWER_SUPPLY_PROP_ENERGY_FULL:
		reg = MAX77843_FG_REG_FULLCAP;
		break;
	case POWER_SUPPLY_PROP_ENERGY_NOW:
		reg = MAX77843_FG_REG_REMCAP_REP;
		break;
	case POWER_SUPPLY_PROP_ENERGY_AVG:
		reg = MAX77843_FG_REG_REMCAP_AV;
		break;
	default:
		return -EINVAL;
	}

	ret = regmap_read(regmap, reg, &reg_data);
	if (ret) {
		dev_err(battery->dev, "Failed to read fuelgauge register\n");
		return ret;
	}

	val = reg_data;

	return val;
}

static int max77843_battery_get_current_prop(struct max77843_battery *battery,
		enum power_supply_property psp)
{
	struct regmap *regmap = battery->regmap;
	unsigned int reg;
	int ret, val;
	unsigned int reg_data;

	switch (psp) {
	case POWER_SUPPLY_PROP_CURRENT_NOW:
		reg = MAX77843_FG_REG_CURRENT;
		break;
	case POWER_SUPPLY_PROP_CURRENT_AVG:
		reg = MAX77843_FG_REG_AVG_CURRENT;
		break;
	default:
		return -EINVAL;
	}

	ret = regmap_read(regmap, reg, &reg_data);
	if (ret) {
		dev_err(battery->dev, "Failed to read fuelgauge register\n");
		return ret;
	}

	val = reg_data;
	if (val & 0x8000) {
		/* Negative */
		val = ~val & 0xffff;
		val++;
		val *= -1;
	}
	/* Unit of current is mA */
	val =  val * 15625 / 100000;

	return val;
}

static int max77843_battery_get_voltage_prop(struct max77843_battery *battery,
		enum power_supply_property psp)
{
	struct regmap *regmap = battery->regmap;
	unsigned int reg;
	int ret, val;
	unsigned int reg_data;

	switch (psp) {
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		reg = MAX77843_FG_REG_VCELL;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_AVG:
		reg = MAX77843_FG_REG_AVG_VCELL;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_OCV:
		reg = MAX77843_FG_REG_OCV;
		break;
	default:
		return -EINVAL;
	}

	ret = regmap_read(regmap, reg, &reg_data);
	if (ret) {
		dev_err(battery->dev, "Failed to read fuelgauge register\n");
		return ret;
	}

	val = (reg_data >> 4)  * 125;
	val /= 100;

	return val;
}

static const char *model_name = "MAX77843";
static const char *manufacturer = "Maxim Integrated";

static int max77843_battery_get_property(struct power_supply *psy,
		enum power_supply_property psp,
		union power_supply_propval *val)
{
	struct max77843_battery *battery = container_of(psy,
				struct max77843_battery, psy);
	switch (psp) {
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
	case POWER_SUPPLY_PROP_VOLTAGE_AVG:
	case POWER_SUPPLY_PROP_VOLTAGE_OCV:
		val->intval = max77843_battery_get_voltage_prop(battery, psp);
		break;
	case POWER_SUPPLY_PROP_CURRENT_NOW:
	case POWER_SUPPLY_PROP_CURRENT_AVG:
		val->intval = max77843_battery_get_current_prop(battery, psp);
		break;
	case POWER_SUPPLY_PROP_ENERGY_FULL:
	case POWER_SUPPLY_PROP_ENERGY_NOW:
	case POWER_SUPPLY_PROP_ENERGY_AVG:
		val->intval = max77843_battery_get_energy_prop(battery, psp);
		break;
	case POWER_SUPPLY_PROP_CAPACITY:
		val->intval = max77843_battery_get_capacity(battery);
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

static enum power_supply_property max77843_battery_props[] = {
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_VOLTAGE_AVG,
	POWER_SUPPLY_PROP_VOLTAGE_OCV,
	POWER_SUPPLY_PROP_CURRENT_NOW,
	POWER_SUPPLY_PROP_CURRENT_AVG,
	POWER_SUPPLY_PROP_ENERGY_FULL,
	POWER_SUPPLY_PROP_ENERGY_NOW,
	POWER_SUPPLY_PROP_ENERGY_AVG,
	POWER_SUPPLY_PROP_CAPACITY,
	POWER_SUPPLY_PROP_MODEL_NAME,
	POWER_SUPPLY_PROP_MANUFACTURER,
};

static const struct regmap_config max77843_fuel_regmap_config = {
	.reg_bits	= 8,
	.val_bits	= 16,
	.val_format_endian = REGMAP_ENDIAN_NATIVE,
	.max_register	= MAX77843_FG_END,
};

static int max77843_battery_probe(struct platform_device *pdev)
{
	struct max77843 *max77843 = dev_get_drvdata(pdev->dev.parent);
	struct max77843_battery *battery;
	int ret;

	battery = devm_kzalloc(&pdev->dev, sizeof(*battery), GFP_KERNEL);
	if (!battery)
		return -ENOMEM;

	battery->dev = &pdev->dev;
	battery->max77843 = max77843;

	battery->client = i2c_new_dummy(max77843->i2c->adapter, I2C_ADDR_FG);
	if (!battery->client) {
		dev_err(&pdev->dev, "Failed to get dummy i2c client.\n");
		return PTR_ERR(battery->client);
	}
	i2c_set_clientdata(battery->client, max77843);

	battery->regmap = devm_regmap_init_i2c(battery->client,
			&max77843_fuel_regmap_config);
	if (IS_ERR(battery->regmap)) {
		ret = PTR_ERR(battery->regmap);
		goto err_i2c;
	}

	platform_set_drvdata(pdev, battery);

	battery->psy.name	= "max77843-fuelgauge";
	battery->psy.type	= POWER_SUPPLY_TYPE_BATTERY;
	battery->psy.get_property	= max77843_battery_get_property;
	battery->psy.properties		= max77843_battery_props;
	battery->psy.num_properties	= ARRAY_SIZE(max77843_battery_props);

	ret = power_supply_register(&pdev->dev, &battery->psy);
	if (ret) {
		dev_err(&pdev->dev, "Failed  to register power supply\n");
		goto err_i2c;
	}

	return 0;

err_i2c:
	i2c_unregister_device(battery->client);

	return ret;
}

static int max77843_battery_remove(struct platform_device *pdev)
{
	struct max77843_battery *battery = platform_get_drvdata(pdev);

	power_supply_unregister(&battery->psy);

	i2c_unregister_device(battery->client);

	return 0;
}

static const struct platform_device_id max77843_battery_id[] = {
	{ "max77843-fuelgauge", },
	{ }
};
MODULE_DEVICE_TABLE(platform, max77843_battery_id);

static struct platform_driver max77843_battery_driver = {
	.driver = {
		.name = "max77843-fuelgauge",
	},
	.probe = max77843_battery_probe,
	.remove = max77843_battery_remove,
	.id_table = max77843_battery_id,
};
module_platform_driver(max77843_battery_driver);

MODULE_DESCRIPTION("Maxim MAX77843 fuel gauge driver");
MODULE_AUTHOR("Beomho Seo <beomho.seo@samsung.com>");
MODULE_LICENSE("GPL");
