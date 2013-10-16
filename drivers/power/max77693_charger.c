/*
 * max77693_charger.c
 *
 * Copyright (C) 2011 Samsung Electronics
 * SangYoung Son <hello.son@samsung.com>
 *
 * Copyright (C) 2012 Samsung Electronics
 * MyungJoo Ham <myungjoo.ham@samsung.com>
 *
 * Simplified hello.son's driver removing unnecessary features for
 * charger-manager; supporting power-supply-class is the main role.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * "Input current" in the original driver is controlled by regulator
 * "CHARGER".
 * "Charge current" in the original driver is controlled by regulator
 * "CHARGER_CC".
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/err.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/power_supply.h>
#include <linux/gpio.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/slab.h>
#include <linux/mfd/max77693.h>
#include <linux/mfd/max77693-private.h>
#include <linux/power/charger-manager.h>
#include <linux/power/max77693_charger.h>
#include <linux/extcon.h>
#include <plat/gpio-cfg.h>

struct max77693_charger_data {
	struct max77693_dev	*max77693;

	struct power_supply	charger;

	/* mutex */
	struct mutex irq_lock;
	struct mutex ops_lock;

	unsigned int	charging_state;
	unsigned int	charging_type;
	unsigned int	battery_state;
	unsigned int	battery_present;
	unsigned int	cable_type;
	unsigned int	charging_current;
	unsigned int	vbus_state;

	int		irq_bypass;
	int		irq_therm;
	int		irq_battery;
	int		irq_charge;
	int		irq_chargin;

	/* software regulation */
	bool		soft_reg_state;
	int		soft_reg_current;
	bool		soft_reg_ing;

	/* unsufficient power */
	bool		reg_loop_deted;

	struct max77693_charger_platform_data	*charger_pdata;

	int		irq;
	u8		irq_reg;
	int		irq_cnt;
};

static int max77693_get_battery_present(struct max77693_charger_data *chg_data)
{
	struct regmap *rmap = chg_data->max77693->regmap;
	u8 reg_data;

	max77693_read_reg(rmap, MAX77693_CHG_REG_CHG_INT_OK, &reg_data);
	reg_data = ((reg_data & MAX77693_DETBAT) >> MAX77693_DETBAT_SHIFT);

	return !reg_data;
}

static int max77693_get_vbus_state(struct max77693_charger_data *chg_data)
{
	struct regmap *rmap = chg_data->max77693->regmap;
	int state;
	u8 reg_data;

	max77693_read_reg(rmap, MAX77693_CHG_REG_CHG_DETAILS_00, &reg_data);
	reg_data = ((reg_data & MAX77693_CHGIN_DTLS) >>
				MAX77693_CHGIN_DTLS_SHIFT);

	switch (reg_data) {
	case POWER_SUPPLY_VBUS_UVLO :
		/* V chgin < UVLO */
		state = POWER_SUPPLY_HEALTH_UNSPEC_FAILURE;
		break;
	case POWER_SUPPLY_VBUS_WEAK :
		/* V chgin < V batt + minimum threshold */
		state = POWER_SUPPLY_HEALTH_UNSPEC_FAILURE;
		break;
	case POWER_SUPPLY_VBUS_OVLO :
		/* V chgin > OVLO */
		state = POWER_SUPPLY_HEALTH_OVERVOLTAGE;
		break;
	case POWER_SUPPLY_VBUS_GOOD :
		state = POWER_SUPPLY_HEALTH_GOOD;
		break;
	default:
		state = POWER_SUPPLY_HEALTH_UNKNOWN;
		break;
	}

	chg_data->vbus_state = state;
	return state;
}

static int max77693_get_charger_type(struct max77693_charger_data *chg_data)
{
	struct regmap *rmap = chg_data->max77693->regmap;
	int state;
	u8 reg_data;

	max77693_read_reg(rmap, MAX77693_CHG_REG_CHG_DETAILS_01, &reg_data);
	reg_data = ((reg_data & MAX77693_CHG_DTLS) >> MAX77693_CHG_DTLS_SHIFT);

	switch (reg_data) {
	case 0x0:
		state = POWER_SUPPLY_CHARGE_TYPE_TRICKLE;
		break;
	case 0x1:
	case 0x2:
	case 0x3:
		state = POWER_SUPPLY_CHARGE_TYPE_FAST;
		break;
	case 0x4 ... 0x8:
	case 0xA:
	case 0xB:
		state = POWER_SUPPLY_CHARGE_TYPE_NONE;
		break;
	default:
		state = POWER_SUPPLY_CHARGE_TYPE_UNKNOWN;
		break;
	}

	chg_data->charging_type = state;
	return state;
}

static int max77693_get_charger_state(struct max77693_charger_data *chg_data)
{
	struct regmap *rmap = chg_data->max77693->regmap;
	int state;
	u8 reg_data;

	max77693_read_reg(rmap, MAX77693_CHG_REG_CHG_DETAILS_01, &reg_data);
	reg_data = ((reg_data & MAX77693_CHG_DTLS) >> MAX77693_CHG_DTLS_SHIFT);
	switch (reg_data) {
	case 0x0:
	case 0x1:
	case 0x2:
		/*
		 * Note that whether to consider 0x3 as CHARGING or FULL
		 * is arguable.
		 * According to TN's standard 0x3 (TOP-OFF) should be
		 * "FULL".
		 * According to the strict semantics of "FULL", this is
		 * "CHARGING".
		 */
	case 0x3:
		state = POWER_SUPPLY_STATUS_CHARGING;
		break;
	case 0x4:
		state = POWER_SUPPLY_STATUS_FULL;
		break;
	case 0x5:
	case 0x6:
	case 0x7:
		state = POWER_SUPPLY_STATUS_NOT_CHARGING;
		break;
	case 0x8:
	case 0xA:
	case 0xB:
		state = POWER_SUPPLY_STATUS_DISCHARGING;
		break;
	default:
		state = POWER_SUPPLY_STATUS_UNKNOWN;
		break;
	}

	chg_data->charging_state = state;
	return state;
}

static int max77693_get_online(struct max77693_charger_data *chg_data)
{
	struct regmap *rmap = chg_data->max77693->regmap;
	u8 reg_data;

	max77693_read_reg(rmap, MAX77693_CHG_REG_CHG_INT_OK, &reg_data);
	return !!(reg_data & MAX77693_CHGIN_I);
}

static int max77693_get_battery_health(struct max77693_charger_data *chg_data)
{
	struct regmap *rmap = chg_data->max77693->regmap;
	int state;
	bool low_bat = false;
	u8 reg_data;

	max77693_read_reg(rmap, MAX77693_CHG_REG_CHG_DETAILS_01, &reg_data);
	reg_data = ((reg_data & MAX77693_BAT_DTLS) >> MAX77693_BAT_DTLS_SHIFT);
	switch (reg_data) {
	case 0x00: /* NO BATT */
		state = POWER_SUPPLY_HEALTH_DEAD;
		break;
	case 0x01: /* V Batt < Prequalification */
		low_bat = true;
		state = POWER_SUPPLY_HEALTH_GOOD;
		break;
	case 0x02: /* Takes too much time to charge. Damaged battery? */
		state = POWER_SUPPLY_HEALTH_DEAD;
		break;
	case 0x03: /* V Batt > Good > Prequal */
		state = POWER_SUPPLY_HEALTH_GOOD;
		break;
	case 0x04: /* Good > V Batt > Prequal. Not good enough */
		low_bat = true;
		state = POWER_SUPPLY_HEALTH_GOOD;
		break;
	case 0x05: /* V Batt > Overvoltage */
		state = POWER_SUPPLY_HEALTH_OVERVOLTAGE;
		break;
	case 0x06: /* I Batt > Overcurrent */
		state = POWER_SUPPLY_HEALTH_UNSPEC_FAILURE;
	default:
		state = POWER_SUPPLY_HEALTH_UNKNOWN;
		break;
	}

	if (state == POWER_SUPPLY_HEALTH_GOOD)
		state = max77693_get_vbus_state(chg_data);

	/* Battery is healthy and fully-charged, but has low voltage? */
	if (state == POWER_SUPPLY_HEALTH_GOOD && low_bat &&
	    max77693_get_charger_state(chg_data) == POWER_SUPPLY_STATUS_FULL)
		state = POWER_SUPPLY_HEALTH_UNSPEC_FAILURE;

	chg_data->battery_state = state;
	return state;
}

static bool max77693_charger_unlock(struct max77693_charger_data *chg_data)
{
	struct regmap *rmap = chg_data->max77693->regmap;
	u8 reg_data;
	u8 chgprot;
	int retry_cnt = 0;
	bool need_init = false;
	pr_debug("%s\n", __func__);

	max77693_read_reg(rmap, MAX77693_CHG_REG_CHG_CNFG_06, &reg_data);
	chgprot = ((reg_data & MAX77693_CHG_CHGPROT) >>
				MAX77693_CHG_CHGPROT_SHIFT);

	if (chgprot == MAX77693_CHG_CHGPROT_UNLOCK) {
		pr_debug("%s: unlocked state, return\n", __func__);
		need_init = false;
		goto unlock_finish;
	}

	do {
		max77693_write_reg(rmap, MAX77693_CHG_REG_CHG_CNFG_06,
					(MAX77693_CHG_CHGPROT_UNLOCK <<
					MAX77693_CHG_CHGPROT_SHIFT));

		max77693_read_reg(rmap, MAX77693_CHG_REG_CHG_CNFG_06, &reg_data);
		chgprot = ((reg_data & MAX77693_CHG_CHGPROT) >>
					MAX77693_CHG_CHGPROT_SHIFT);

		if (chgprot != MAX77693_CHG_CHGPROT_UNLOCK) {
			pr_err("%s: unlock err, chgprot(0x%x), retry(%d)\n",
					__func__, chgprot, retry_cnt);
			msleep(CHG_UNLOCK_DELAY);
		} else {
			pr_info("%s: unlock success, chgprot(0x%x)\n",
							__func__, chgprot);
			need_init = true;
			break;
		}
	} while ((chgprot != MAX77693_CHG_CHGPROT_UNLOCK) &&
				(++retry_cnt < CHG_UNLOCK_RETRY));

unlock_finish:
	return need_init;
}

static void max77693_charger_reg_init(struct max77693_charger_data *chg_data)
{
	struct regmap *rmap = chg_data->max77693->regmap;

	/*
	 * fast charge timer 10hrs
	 * restart threshold disable
	 * pre-qual charge enable(default)
	 */
	max77693_update_reg(rmap, MAX77693_CHG_REG_CHG_CNFG_01,
			MAX77693_FCHGTIME_10HRS | MAX77693_CHG_RSTRT_MASK,
			MAX77693_FCHGTIME_MASK | MAX77693_CHG_RSTRT_MASK);

	/*
	 * charge current 466mA(default)
	 * otg current limit 900mA
	 */
	max77693_update_reg(rmap, MAX77693_CHG_REG_CHG_CNFG_02,
			MAX77693_OTG_ILIM_MASK, MAX77693_OTG_ILIM_MASK);
	/*
	 * top off current 100mA
	 * top off timer 0min
	 */
	max77693_update_reg(rmap, MAX77693_CHG_REG_CHG_CNFG_03,
			0x0, MAX77693_CHG_TO_ITHM | MAX77693_CHG_TO_TIMEM);

	/*
	 * cv voltage 4.35V
	 * MINVSYS 3.6V(default)
	 */
	max77693_update_reg(rmap, MAX77693_CHG_REG_CHG_CNFG_04,
			MAX77693_CHG_CV_PRM_4_35V, MAX77693_CHG_CV_PRM_MASK);

	/* VBYPSET 5V */
	max77693_write_reg(rmap, MAX77693_CHG_REG_CHG_CNFG_11,
					MAX77693_CHG_VBYPSET_5V);
}

/* Support property from charger */
static enum power_supply_property max77693_charger_props[] = {
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_CHARGE_TYPE,
	POWER_SUPPLY_PROP_HEALTH,
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_ONLINE,
	POWER_SUPPLY_PROP_MODEL_NAME,
	POWER_SUPPLY_PROP_MANUFACTURER,
};

static const char *model_name = "MAX77693";
static const char *manufacturer = "Maxim Semiconductor";
static int max77693_charger_get_property(struct power_supply *psy,
			    enum power_supply_property psp,
			    union power_supply_propval *val)
{
	struct max77693_charger_data *chg_data = container_of(psy,
						  struct max77693_charger_data,
						  charger);
	int ret = 0;

	switch (psp) {
	case POWER_SUPPLY_PROP_STATUS:
		val->intval = max77693_get_charger_state(chg_data);
		break;
	case POWER_SUPPLY_PROP_CHARGE_TYPE:
		val->intval = max77693_get_charger_type(chg_data);
		break;
	case POWER_SUPPLY_PROP_HEALTH:
		val->intval = max77693_get_battery_health(chg_data);
		break;
	case POWER_SUPPLY_PROP_PRESENT:
		val->intval = max77693_get_battery_present(chg_data);
		break;
	case POWER_SUPPLY_PROP_ONLINE:
		val->intval = max77693_get_online(chg_data);
		break;
	case POWER_SUPPLY_PROP_MODEL_NAME:
		val->strval = model_name;
		break;
	case POWER_SUPPLY_PROP_MANUFACTURER:
		val->strval = manufacturer;
		break;
	default:
		return -EINVAL;
	}
	/*
	 * TODO: support chaging type...
		val->intval = max77693_get_cable_type(chg_data);
	*/

	return ret;
}

static void max77693_charger_initialize(struct max77693_charger_data *chg_data)
{
	struct max77693_charger_platform_data *charger_pdata =
					chg_data->charger_pdata;
	struct regmap *rmap = chg_data->max77693->regmap;
	int i;

	for (i = 0; i < charger_pdata->num_init_data; i++)
		max77693_write_reg(rmap, charger_pdata->init_data[i].addr,
				charger_pdata->init_data[i].data);
}

static int max77693_charger_probe(struct platform_device *pdev)
{
	struct max77693_charger_data *chg_data;
	struct max77693_dev *max77693 = dev_get_drvdata(pdev->dev.parent);
	struct max77693_platform_data *pdata = dev_get_platdata(max77693->dev);
	int ret;

	pr_info("%s: charger init start\n", __func__);

	chg_data = devm_kzalloc(&pdev->dev, sizeof(struct max77693_charger_data), GFP_KERNEL);
	if (!chg_data)
		return -ENOMEM;

	platform_set_drvdata(pdev, chg_data);
	chg_data->max77693 = max77693;

	mutex_init(&chg_data->irq_lock);
	mutex_init(&chg_data->ops_lock);

	/* unlock charger setting protect */
	max77693_charger_unlock(chg_data);

	chg_data->charger_pdata = pdata->charger_data;
	if (pdata->charger_data && pdata->charger_data->init_data)
		max77693_charger_initialize(chg_data);
	else
		max77693_charger_reg_init(chg_data);

	chg_data->charger.name = "max77693-charger",
	chg_data->charger.type = POWER_SUPPLY_TYPE_BATTERY,
	chg_data->charger.properties = max77693_charger_props,
	chg_data->charger.num_properties = ARRAY_SIZE(max77693_charger_props),
	chg_data->charger.get_property = max77693_charger_get_property,

	ret = power_supply_register(&pdev->dev, &chg_data->charger);
	if (ret) {
		pr_err("%s: failed: power supply register\n", __func__);
		goto err_kfree;
	}

	return 0;

err_kfree:
	mutex_destroy(&chg_data->ops_lock);
	mutex_destroy(&chg_data->irq_lock);
	return ret;
}

static int max77693_charger_remove(struct platform_device *pdev)
{
	struct max77693_charger_data *chg_data = platform_get_drvdata(pdev);

	mutex_destroy(&chg_data->ops_lock);
	mutex_destroy(&chg_data->irq_lock);

	power_supply_unregister(&chg_data->charger);

	return 0;
}

/*
 * WORKAROUND: (test and remove w/ later MAX77693 chips)
 * TODO: read chip revision and bypass this code if revision > ?
 * Several interrupts occur while charging through TA.
 * Suspended state cannot be maintained by the interrupts.
 */
static u8 saved_int_mask;
static int max77693_charger_suspend(struct device *dev)
{
	struct max77693_dev *max77693 = dev_get_drvdata(dev->parent);
	u8 int_mask;

	/* Save the masking value */
	max77693_read_reg(max77693->regmap,
			MAX77693_CHG_REG_CHG_INT_MASK,
			&saved_int_mask);

	/* Mask all the interrupts related to charger */
	int_mask = 0xff;
	max77693_write_reg(max77693->regmap,
			MAX77693_CHG_REG_CHG_INT_MASK,
			int_mask);
	return 0;
}

static int max77693_charger_resume(struct device *dev)
{
	struct max77693_dev *max77693 = dev_get_drvdata(dev->parent);

	/* Restore the saved masking value */
	max77693_write_reg(max77693->regmap,
			MAX77693_CHG_REG_CHG_INT_MASK,
			saved_int_mask);
	return 0;
}

static SIMPLE_DEV_PM_OPS(max77693_charger_pm_ops, max77693_charger_suspend,
			max77693_charger_resume);

#ifdef CONFIG_OF
static struct of_device_id max77693_charger_of_match[] = {
	{ .compatible = "samsung,max77693-charger", },
	{ },
};
#endif

static struct platform_driver max77693_charger_driver = {
	.driver = {
		.owner	= THIS_MODULE,
		.name	= "max77693-charger",
		.pm	= &max77693_charger_pm_ops,
		.of_match_table = of_match_ptr(max77693_charger_of_match),
	},
	.probe		= max77693_charger_probe,
	.remove		= max77693_charger_remove,
};

static int __init max77693_charger_init(void)
{
	return platform_driver_register(&max77693_charger_driver);
}

static void __exit max77693_charger_exit(void)
{
	platform_driver_unregister(&max77693_charger_driver);
}

module_init(max77693_charger_init);
module_exit(max77693_charger_exit);

MODULE_AUTHOR("SangYoung Son <hello.son@samsung.com>");
MODULE_DESCRIPTION("max77693 Charger driver");
MODULE_LICENSE("GPL");
