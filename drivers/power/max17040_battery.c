/*
 *  max17040_battery.c
 *  fuel-gauge systems for lithium-ion (Li+) batteries
 *
 *  Copyright (C) 2009 Samsung Electronics
 *  Minkyu Kang <mk7.kang@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/mutex.h>
#include <linux/err.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/power_supply.h>
#include <linux/max17040_battery.h>
#include <linux/slab.h>
#include <linux/regmap.h>
#include <linux/platform_device.h>

#define MAX17040_VCELL_MSB	0x02
#define MAX17040_VCELL_LSB	0x03
#define MAX17040_SOC_MSB	0x04
#define MAX17040_SOC_LSB	0x05
#define MAX17040_MODE_MSB	0x06
#define MAX17040_MODE_LSB	0x07
#define MAX17040_VER_MSB	0x08
#define MAX17040_VER_LSB	0x09
#define MAX17040_RCOMP_MSB	0x0C
#define MAX17040_RCOMP_LSB	0x0D
#define MAX17040_CMD_MSB	0xFE
#define MAX17040_CMD_LSB	0xFF

#define MAX17040_DELAY		1000
#define MAX17040_BATTERY_FULL	95

struct max17040_chip {
	struct i2c_client		*client;
	struct regmap			*regmap;
	struct power_supply		battery;
	struct max17040_platform_data	*pdata;

	/* battery voltage */
	int vcell;
	/* battery capacity */
	int soc;
	/* State Of Charge */
	int status;

	/* If true, SoC's shown in double */
	bool using_19_bits;
};

static void max17040_reset(struct i2c_client *client)
{
	struct max17040_chip *chip = i2c_get_clientdata(client);

	regmap_write(chip->regmap, MAX17040_CMD_MSB, 0x4000);
}

static void max17040_get_vcell(struct i2c_client *client)
{
	struct max17040_chip *chip = i2c_get_clientdata(client);
	u32 data;
	u8 msb, lsb;

	regmap_read(chip->regmap, MAX17040_VCELL_MSB, &data);

	msb = ((data & 0xFF00) >> 8);
	lsb = (data & 0xFF);

	chip->vcell = ((msb << 4) + (lsb >> 4)) * 1250 / 1000;
}

/* capacity is  0.1% unit */

static int max17040_capacity_max = 1000;
static int max17040_capacity_min = 10;

static void max17040_get_scaled_capacity(int *val)
{
	*val = (*val < max17040_capacity_min) ?
		0 : ((*val - max17040_capacity_min) * 1000 /
		(max17040_capacity_max - max17040_capacity_min));
}

static void max17040_get_soc(struct i2c_client *client)
{
	struct max17040_chip *chip = i2c_get_clientdata(client);
	u32 data;
	u8 msb, lsb;
	int soc;

	regmap_read(chip->regmap, MAX17040_SOC_MSB, &data);

	msb = ((data & 0xFF00) >> 8);
	lsb = (data & 0xFF);

	soc = (msb * 100) + (lsb * 100 / 256);
	soc /= 10;

	/* FIXME:
	 * No information about actual meaning of using 19 bits,
	 * it's just copied from vendor's code.
	 * However, It is clear that it represents doubled SoC
	 * if the chip is marked with 'using 19 bits'.
	 */
	if (chip->using_19_bits)
		soc /= 2;

	max17040_get_scaled_capacity(&soc);
	/* capacity should be between 0% and 100%
	  * (0.1% degree)
	  */
	if (soc > 1000)
		soc = 1000;
	if (soc < 0)
		soc = 0;

	/* get only integer part */
	soc /= 10;

	chip->soc = soc;
}

static void max17040_get_version(struct i2c_client *client)
{
	struct max17040_chip *chip = i2c_get_clientdata(client);
	u32 data;

	regmap_read(chip->regmap, MAX17040_VER_MSB, &data);

	dev_info(&client->dev, "MAX17040 Fuel-Gauge Ver %d\n", data);
}

static void max17040_get_status(struct i2c_client *client)
{
	struct max17040_chip *chip = i2c_get_clientdata(client);

	if (!chip->pdata->charger_online || !chip->pdata->charger_enable) {
		chip->status = POWER_SUPPLY_STATUS_UNKNOWN;
		return;
	}

	if (chip->pdata->charger_online()) {
		if (chip->pdata->charger_enable())
			chip->status = POWER_SUPPLY_STATUS_CHARGING;
		else
			chip->status = POWER_SUPPLY_STATUS_NOT_CHARGING;
	} else {
		chip->status = POWER_SUPPLY_STATUS_DISCHARGING;
	}

	if (chip->soc > MAX17040_BATTERY_FULL)
		chip->status = POWER_SUPPLY_STATUS_FULL;
}

static int max17040_get_property(struct power_supply *psy,
			    enum power_supply_property psp,
			    union power_supply_propval *val)
{
	struct max17040_chip *chip = container_of(psy,
				struct max17040_chip, battery);

	switch (psp) {
	case POWER_SUPPLY_PROP_STATUS:
		max17040_get_status(chip->client);
		val->intval = chip->status;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		max17040_get_vcell(chip->client);
		val->intval = chip->vcell;
		break;
	case POWER_SUPPLY_PROP_CAPACITY:
		max17040_get_soc(chip->client);
		val->intval = chip->soc;
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

static enum power_supply_property max17040_battery_props[] = {
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_CAPACITY,
};

static struct regmap_config max17040_regmap_config = {
	.reg_bits = 8,
	.val_bits = 16,
	.val_format_endian = REGMAP_ENDIAN_BIG,
};

static int max17040_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	struct i2c_adapter *adapter = to_i2c_adapter(client->dev.parent);
	struct max17040_chip *chip;
	u32 ret;

	if (!i2c_check_functionality(adapter, I2C_FUNC_SMBUS_BYTE))
		return -EIO;

	chip = devm_kzalloc(&client->dev, sizeof(*chip), GFP_KERNEL);
	if (!chip)
		return -ENOMEM;

	chip->client = client;
	chip->regmap = devm_regmap_init_i2c(client, &max17040_regmap_config);
	if (IS_ERR(chip->regmap)) {
		dev_err(&client->dev, "Failed to initialize regmap\n");
		return -EINVAL;
	}

	/* A flag for value of SoC in double or not */
	if (client->dev.of_node)
		chip->using_19_bits = of_property_read_bool(client->dev.of_node,
							"using-19-bits");

	chip->pdata = client->dev.platform_data;

	if (!chip->pdata) {
		chip->pdata = devm_kzalloc(&client->dev,
					sizeof(*chip->pdata), GFP_KERNEL);
		if (!chip->pdata)
			return -ENOMEM;
	}

	i2c_set_clientdata(client, chip);

	chip->battery.name		= "max17040";
	chip->battery.type		= POWER_SUPPLY_TYPE_BATTERY;
	chip->battery.get_property	= max17040_get_property;
	chip->battery.properties	= max17040_battery_props;
	chip->battery.num_properties	= ARRAY_SIZE(max17040_battery_props);

	ret = power_supply_register(&client->dev, &chip->battery);
	if (ret) {
		dev_err(&client->dev, "failed: power supply register\n");
		return ret;
	}

	max17040_reset(client);
	max17040_get_version(client);

	return 0;
}

static int max17040_remove(struct i2c_client *client)
{
	struct max17040_chip *chip = i2c_get_clientdata(client);

	power_supply_unregister(&chip->battery);
	return 0;
}

#ifdef CONFIG_PM_SLEEP

static int max17040_suspend(struct device *dev)
{
	return 0;
}

static int max17040_resume(struct device *dev)
{
	return 0;
}

static SIMPLE_DEV_PM_OPS(max17040_pm_ops, max17040_suspend, max17040_resume);
#define MAX17040_PM_OPS (&max17040_pm_ops)

#else

#define MAX17040_PM_OPS NULL

#endif /* CONFIG_PM_SLEEP */

static const struct i2c_device_id max17040_id[] = {
	{ "max17040", 0 },
	{ "max17048", 0 },
	{ "max77836-battery", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, max17040_id);

static struct i2c_driver max17040_i2c_driver = {
	.driver	= {
		.name	= "max17040",
		.pm	= MAX17040_PM_OPS,
	},
	.probe		= max17040_probe,
	.remove		= max17040_remove,
	.id_table	= max17040_id,
};
module_i2c_driver(max17040_i2c_driver);

MODULE_AUTHOR("Minkyu Kang <mk7.kang@samsung.com>");
MODULE_DESCRIPTION("MAX17040 Fuel Gauge");
MODULE_LICENSE("GPL");
