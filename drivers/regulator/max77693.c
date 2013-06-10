/*
 * max77693.c - Regulator driver for the Maxim 77693
 *
 * Copyright (C) 2011 Samsung Electronics
 * Sukdong Kim <sukdong.kim@smasung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * This driver is based on max8997.c
 */

#include <linux/bug.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/gpio.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/export.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/machine.h>
#include <linux/mfd/max77693.h>
#include <linux/mfd/max77693-private.h>
#include <linux/regulator/of_regulator.h>

struct max77693_data {
	struct device *dev;
	struct max77693_dev *iodev;
	int num_regulators;
	struct regulator_dev **rdev;

	u8 saved_states[MAX77693_REG_MAX];
};

struct voltage_map_desc {
	int min;
	int max;
	int step;
	unsigned int n_bits;
};

/* current map in mA */
static const struct voltage_map_desc charger_current_map_desc = {
	.min = 60, .max = 2580, .step = 20, .n_bits = 7,
};

static const struct voltage_map_desc topoff_current_map_desc = {
	.min = 50, .max = 200, .step = 10, .n_bits = 4,
};

static const struct voltage_map_desc *reg_voltage_map[] = {
	[MAX77693_ESAFEOUT1] = NULL,
	[MAX77693_ESAFEOUT2] = NULL,
	[MAX77693_CHARGER] = &charger_current_map_desc,
};

static inline int max77693_get_rid(struct regulator_dev *rdev)
{
	return rdev_get_id(rdev);
}

static int max77693_list_voltage_safeout(struct regulator_dev *rdev,
					 unsigned int selector)
{
	int rid = max77693_get_rid(rdev);

	if (rid == MAX77693_ESAFEOUT1 || rid == MAX77693_ESAFEOUT2) {
		switch (selector) {
		case 0:
			return 4850000;
		case 1:
			return 4900000;
		case 2:
			return 4950000;
		case 3:
			return 3300000;
		default:
			return -EINVAL;
		}
	}

	return -EINVAL;
}

static int max77693_get_enable_register(struct regulator_dev *rdev,
					int *reg, int *mask, int *pattern)
{
	int rid = max77693_get_rid(rdev);
	switch (rid) {
	case MAX77693_ESAFEOUT1...MAX77693_ESAFEOUT2:
		*reg = MAX77693_CHG_REG_SAFEOUT_CTRL;
		*mask = 0x40 << (rid - MAX77693_ESAFEOUT1);
		*pattern = 0x40 << (rid - MAX77693_ESAFEOUT1);
		break;
	case MAX77693_CHARGER:
		*reg = MAX77693_CHG_REG_CHG_CNFG_00;
		*mask = 0xf;
		*pattern = 0x5;
		break;
	default:
		/* Not controllable or not exists */
		return -EINVAL;
	}

	return 0;
}

static int max77693_get_disable_register(struct regulator_dev *rdev,
					int *reg, int *mask, int *pattern)
{
	int rid = max77693_get_rid(rdev);

	switch (rid) {
	case MAX77693_ESAFEOUT1...MAX77693_ESAFEOUT2:
		*reg = MAX77693_CHG_REG_SAFEOUT_CTRL;
		*mask = 0x40 << (rid - MAX77693_ESAFEOUT1);
		*pattern = 0x00;
		break;
	case MAX77693_CHARGER:
		*reg = MAX77693_CHG_REG_CHG_CNFG_00;
		*mask = 0xf;
		*pattern = 0x00;
		break;
	default:
		/* Not controllable or not exists */
		return -EINVAL;
	}

	return 0;
}

static int max77693_reg_is_enabled(struct regulator_dev *rdev)
{
	int ret, reg, mask, pattern;

	u8 val;
	ret = max77693_get_enable_register(rdev, &reg, &mask, &pattern);
	if (ret == -EINVAL)
		return 1;	/* "not controllable" */
	else if (ret)
		return ret;

	ret = max77693_read_reg(rdev->regmap, reg, &val);
	if (ret)
		return ret;

	return (val & mask) == pattern;
}

static int max77693_reg_enable(struct regulator_dev *rdev)
{
	int ret, reg, mask, pattern;

	ret = max77693_get_enable_register(rdev, &reg, &mask, &pattern);
	if (ret)
		return ret;

	return max77693_update_reg(rdev->regmap, reg, pattern, mask);
}

static int max77693_reg_disable(struct regulator_dev *rdev)
{
	int ret, reg, mask, pattern;

	ret = max77693_get_disable_register(rdev, &reg, &mask, &pattern);
	if (ret)
		return ret;

	return max77693_update_reg(rdev->regmap, reg, pattern, mask);
}

static int max77693_get_voltage_register(struct regulator_dev *rdev,
					 int *_reg, int *_shift, int *_mask)
{
	int rid = max77693_get_rid(rdev);
	int reg, shift = 0, mask = 0x3f;

	switch (rid) {
	case MAX77693_ESAFEOUT1...MAX77693_ESAFEOUT2:
		reg = MAX77693_CHG_REG_SAFEOUT_CTRL;
		shift = (rid == MAX77693_ESAFEOUT2) ? 2 : 0;
		mask = 0x3;
		break;
	case MAX77693_CHARGER:
		reg = MAX77693_CHG_REG_CHG_CNFG_09;
		shift = 0;
		mask = 0x7f;
		break;
	default:
		return -EINVAL;
	}

	*_reg = reg;
	*_shift = shift;
	*_mask = mask;

	return 0;
}

static int max77693_list_voltage(struct regulator_dev *rdev,
				 unsigned int selector)
{
	const struct voltage_map_desc *desc;
	int rid = max77693_get_rid(rdev);
	int val;

	if (rid >= ARRAY_SIZE(reg_voltage_map) || rid < 0)
		return -EINVAL;

	desc = reg_voltage_map[rid];
	if (desc == NULL)
		return -EINVAL;

	/* the first four codes for charger current are all 60mA */
	if (rid == MAX77693_CHARGER) {
		if (selector <= 3)
			selector = 0;
		else
			selector -= 3;
	}

	val = desc->min + desc->step * selector;
	if (val > desc->max)
		return -EINVAL;

	return val * 1000;
}

static int max77693_get_voltage(struct regulator_dev *rdev)
{
	int reg, shift, mask, ret;

	u8 val;

	ret = max77693_get_voltage_register(rdev, &reg, &shift, &mask);
	if (ret)
		return ret;

	ret = max77693_read_reg(rdev->regmap, reg, &val);
	if (ret)
		return ret;

	val >>= shift;
	val &= mask;

	if (rdev->desc && rdev->desc->ops && rdev->desc->ops->list_voltage)
		return rdev->desc->ops->list_voltage(rdev, val);

	/*
	 * max77693_list_voltage returns value for any rdev with voltage_map,
	 * which works for "CHARGER" and "CHARGER TOPOFF" that do not have
	 * list_voltage ops (they are current regulators).
	 */
	return max77693_list_voltage(rdev, val);
}

static inline int max77693_get_voltage_proper_val(
		const struct voltage_map_desc *desc,
		int min_vol, int max_vol)
{
	int i = 0;

	if (desc == NULL)
		return -EINVAL;

	if (max_vol < desc->min || min_vol > desc->max)
		return -EINVAL;

	while (desc->min + desc->step * i < min_vol &&
			desc->min + desc->step * i < desc->max)
		i++;

	if (desc->min + desc->step * i > max_vol)
		return -EINVAL;

	if (i >= (1 << desc->n_bits))
		return -EINVAL;

	return i;
}

static int max77693_set_voltage(struct regulator_dev *rdev,
				int min_uV, int max_uV)
{
	int min_vol = min_uV / 1000, max_vol = max_uV / 1000;
	const struct voltage_map_desc *desc;
	int rid = max77693_get_rid(rdev);
	int reg, shift = 0, mask, ret;
	int i;
	u8 org;

	switch (rid) {
	case MAX77693_CHARGER:
		break;
	default:
		return -EINVAL;
	}

	desc = reg_voltage_map[rid];

	i = max77693_get_voltage_proper_val(desc, min_vol, max_vol);
	if (i < 0)
		return i;

	ret = max77693_get_voltage_register(rdev, &reg, &shift, &mask);
	if (ret)
		return ret;

	max77693_read_reg(rdev->regmap, reg, &org);
	org = (org & mask) >> shift;

	/* the first four codes for charger current are all 60mA */
	if (rid == MAX77693_CHARGER)
		i += 3;

	ret = max77693_update_reg(rdev->regmap, reg, i << shift, mask << shift);

	return ret;
}

static const int safeoutvolt[] = {
	3300000,
	4850000,
	4900000,
	4950000,
};

/* For SAFEOUT1 and SAFEOUT2 */
static int max77693_set_voltage_safeout(struct regulator_dev *rdev,
					int min_uV, int max_uV,
					unsigned *selector)
{
	int rid = max77693_get_rid(rdev);
	int reg, shift = 0, mask, ret;
	int i = 0;
	u8 val;

	if (rid != MAX77693_ESAFEOUT1 && rid != MAX77693_ESAFEOUT2)
		return -EINVAL;

	for (i = 0; i < ARRAY_SIZE(safeoutvolt); i++) {
		if (min_uV <= safeoutvolt[i] && max_uV >= safeoutvolt[i])
			break;
	}

	if (i >= ARRAY_SIZE(safeoutvolt))
		return -EINVAL;

	if (i == 0)
		val = 0x3;
	else
		val = i - 1;

	ret = max77693_get_voltage_register(rdev, &reg, &shift, &mask);
	if (ret)
		return ret;

	ret = max77693_update_reg(rdev->regmap, reg, val << shift, mask << shift);
	*selector = val;

	return ret;
}

static int max77693_reg_enable_suspend(struct regulator_dev *rdev)
{
	return 0;
}

static int max77693_reg_disable_suspend(struct regulator_dev *rdev)
{
	struct max77693_data *max77693 = rdev_get_drvdata(rdev);
	int ret, reg, mask, pattern;
	int rid = max77693_get_rid(rdev);

	ret = max77693_get_disable_register(rdev, &reg, &mask, &pattern);
	if (ret)
		return ret;

	max77693_read_reg(rdev->regmap, reg, &max77693->saved_states[rid]);

	dev_dbg(&rdev->dev, "Full Power-Off for %s (%xh -> %xh)\n",
		rdev->desc->name, max77693->saved_states[rid] & mask,
		(~pattern) & mask);
	return max77693_update_reg(rdev->regmap, reg, pattern, mask);
}

/*
 * max77693_muic_is_enable_otg - Check the power state of otg
 * @rdev: the instance for voltage/current regulator class device
 */
static bool max77693_muic_is_enable_otg(struct regulator_dev *rdev)
{
	u8 chg_cnfg_00;
	u8 mask;
	u8 state;

	/* OTG on, boost on, DIS_MUIC_CTRL=1 */
	max77693_read_reg(rdev->regmap,
		MAX77693_CHG_REG_CHG_CNFG_00, &chg_cnfg_00);

	mask = (CHG_CNFG_00_OTG_MASK
			| CHG_CNFG_00_BOOST_MASK
			| CHG_CNFG_00_DIS_MUIC_CTRL_MASK);

	state = chg_cnfg_00 & mask;

	if (state == mask)
		return true;
	else
		return false;
}

/*
 * max77693_muic_is_enable_usb - Check the power state of usb
 * @rdev: the instance for voltage/current regulator class device
 */
static bool max77693_muic_is_enable_usb(struct regulator_dev *rdev)
{
	u8 chg_cnfg_00;
	bool state = false;

	max77693_read_reg(rdev->regmap,
		MAX77693_CHG_REG_CHG_CNFG_00, &chg_cnfg_00);

	if (chg_cnfg_00 == 0x05)
		state = true;
	else if (chg_cnfg_00 == 0x04)
		state = false;

	return state;
}

/*
 * max77693_muic_enable_otg - Enable/disable otg feature on max77693
 * @rdev: the instance for voltage/current regulator class device
 * @enable: the state of otg feature (true: ON, false: OFF)
 */
static void max77693_muic_enable_otg(struct regulator_dev *rdev, int enable)
{
	struct max77693_data *max77693 = rdev_get_drvdata(rdev);
	struct regmap *regmap_muic = max77693->iodev->regmap_muic;
	static u8 chg_int_state; /* Restore charger interrupt states */
	u8 int_mask;
	u8 cdetctrl1;
	u8 chg_cnfg_00;

	dev_dbg(&rdev->dev, "enable(%d)\n", enable);

	/*
	 * FIXME: This function write/read directly the register of max77693
	 *	charger device. It has dependency between muic and charger
	 *	device. The dependency between two device have to be removed.
	 *	- MAX77693_CHG_REG_CHG_CNFG_00 : control charger/OTG/buck/boost
	 *	- MAX77693_CHG_REG_CHG_INT_MASK
	 */
	if (enable) {
		/* disable charger interrupt */
		max77693_read_reg(rdev->regmap,
			MAX77693_CHG_REG_CHG_INT_MASK, &int_mask);
		chg_int_state = int_mask;
		int_mask |= (1 << 4);	/* disable chgin intr */
		int_mask |= (1 << 6);	/* disable chg */
		int_mask &= ~(1 << 0);	/* enable byp intr */
		max77693_write_reg(rdev->regmap,
			MAX77693_CHG_REG_CHG_INT_MASK, int_mask);

		/* disable charger detection */
		max77693_read_reg(regmap_muic,
			MAX77693_MUIC_REG_CDETCTRL1, &cdetctrl1);
		cdetctrl1 &= ~(1 << 0);
		max77693_write_reg(regmap_muic,
			MAX77693_MUIC_REG_CDETCTRL1, cdetctrl1);

		/* OTG on, boost on, DIS_MUIC_CTRL=1 */
		max77693_read_reg(rdev->regmap,
			MAX77693_CHG_REG_CHG_CNFG_00, &chg_cnfg_00);
		chg_cnfg_00 &= ~(CHG_CNFG_00_CHG_MASK
				| CHG_CNFG_00_OTG_MASK
				| CHG_CNFG_00_BUCK_MASK
				| CHG_CNFG_00_BOOST_MASK
				| CHG_CNFG_00_DIS_MUIC_CTRL_MASK);
		chg_cnfg_00 |= (CHG_CNFG_00_OTG_MASK
				| CHG_CNFG_00_BOOST_MASK
				| CHG_CNFG_00_DIS_MUIC_CTRL_MASK);
		max77693_write_reg(rdev->regmap,
			MAX77693_CHG_REG_CHG_CNFG_00, chg_cnfg_00);
	} else {
		/* OTG off, boost off, (buck on),
		   DIS_MUIC_CTRL = 0 unless CHG_ENA = 1 */
		max77693_read_reg(rdev->regmap,
			MAX77693_CHG_REG_CHG_CNFG_00, &chg_cnfg_00);
		chg_cnfg_00 &= ~(CHG_CNFG_00_OTG_MASK
				| CHG_CNFG_00_BOOST_MASK
				| CHG_CNFG_00_DIS_MUIC_CTRL_MASK);
		chg_cnfg_00 |= CHG_CNFG_00_BUCK_MASK;
		max77693_write_reg(rdev->regmap,
			MAX77693_CHG_REG_CHG_CNFG_00, chg_cnfg_00);

		mdelay(50);

		/* enable charger detection */
		max77693_read_reg(regmap_muic,
			MAX77693_MUIC_REG_CDETCTRL1, &cdetctrl1);
		cdetctrl1 |= (1 << 0);
		max77693_write_reg(regmap_muic,
			MAX77693_MUIC_REG_CDETCTRL1, cdetctrl1);

		/* enable charger interrupt */
		max77693_write_reg(rdev->regmap,
			MAX77693_CHG_REG_CHG_INT_MASK, chg_int_state);
		max77693_read_reg(rdev->regmap,
			MAX77693_CHG_REG_CHG_INT_MASK, &int_mask);
	}

	dev_dbg(&rdev->dev,
		"INT_MASK(0x%x), CDETCTRL1(0x%x), CHG_CNFG_00(0x%x)\n",
		int_mask, cdetctrl1, chg_cnfg_00);
}

/*
 * max77693_muic_enable_powered_usb - Enable/disable usb feature on max77693
 * @rdev: the instance for voltage/current regulator class device
 * @enable: the state of USB feature (true: ON, false: OFF)
 */
static void max77693_muic_enable_usb(struct regulator_dev *rdev, int enable)
{
	dev_dbg(&rdev->dev, "enable(%d)\n", enable);

	if (enable) {
		/* OTG on, boost on */
		max77693_write_reg(rdev->regmap,
			MAX77693_CHG_REG_CHG_CNFG_00, 0x05);

		max77693_write_reg(rdev->regmap,
			MAX77693_CHG_REG_CHG_CNFG_02, 0x0E);
	} else {
		/* OTG off, boost off, (buck on) */
		max77693_write_reg(rdev->regmap,
			MAX77693_CHG_REG_CHG_CNFG_00, 0x04);
	}
}

static int max77693_usb_is_enabled(struct regulator_dev *rdev)
{
	struct regulator_desc *desc = rdev->desc;
	int ret;

	switch (desc->id) {
	case MAX77693_USBHOST:
		ret = max77693_muic_is_enable_otg(rdev);
		break;
	case MAX77693_USB:
		ret = max77693_muic_is_enable_usb(rdev);
		break;
	default:
		ret = -EINVAL;
	}

	return ret;
}

static int max77693_usb_enable(struct regulator_dev *rdev)
{
	struct regulator_desc *desc = rdev->desc;

	switch (desc->id) {
	case MAX77693_USBHOST:
		max77693_muic_enable_otg(rdev, true);
		break;
	case MAX77693_USB:
		max77693_muic_enable_usb(rdev, true);
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int max77693_usb_disable(struct regulator_dev *rdev)
{
	struct regulator_desc *desc = rdev->desc;

	switch (desc->id) {
	case MAX77693_USBHOST:
		max77693_muic_enable_otg(rdev, false);
		break;
	case MAX77693_USB:
		max77693_muic_enable_usb(rdev, false);
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static struct regulator_ops max77693_safeout_ops = {
	.list_voltage = max77693_list_voltage_safeout,
	.is_enabled = max77693_reg_is_enabled,
	.enable = max77693_reg_enable,
	.disable = max77693_reg_disable,
	.get_voltage = max77693_get_voltage,
	.set_voltage = max77693_set_voltage_safeout,
	.set_suspend_enable = max77693_reg_enable_suspend,
	.set_suspend_disable = max77693_reg_disable_suspend,
};

static struct regulator_ops max77693_charger_ops = {
	.is_enabled		= max77693_reg_is_enabled,
	.enable			= max77693_reg_enable,
	.disable		= max77693_reg_disable,
	.get_current_limit	= max77693_get_voltage,
	.set_current_limit	= max77693_set_voltage,
};

static struct regulator_ops max77693_usb_ops = {
	.is_enabled		= max77693_usb_is_enabled,
	.enable			= max77693_usb_enable,
	.disable		= max77693_usb_disable,
};

static struct regulator_desc regulators[] = {
	{
		.name = "ESAFEOUT1",
		.id = MAX77693_ESAFEOUT1,
		.ops = &max77693_safeout_ops,
		.type = REGULATOR_VOLTAGE,
		.owner = THIS_MODULE,
		.n_voltages = 4,
	}, {
		.name = "ESAFEOUT2",
		.id = MAX77693_ESAFEOUT2,
		.ops = &max77693_safeout_ops,
		.type = REGULATOR_VOLTAGE,
		.owner = THIS_MODULE,
		.n_voltages = 4,
	}, {
		.name = "CHARGER",
		.id = MAX77693_CHARGER,
		.ops = &max77693_charger_ops,
		.type = REGULATOR_CURRENT,
		.owner = THIS_MODULE,
	}, {
		.name = "USBHOST",
		.id = MAX77693_USBHOST,
		.ops = &max77693_usb_ops,
		.type = REGULATOR_CURRENT,
		.owner = THIS_MODULE,
	}, {
		.name = "USB",
		.id = MAX77693_USB,
		.ops = &max77693_usb_ops,
		.type = REGULATOR_CURRENT,
		.owner = THIS_MODULE,
	},
};
#ifdef CONFIG_OF
static int max77693_pmic_dt_parse_pdata(struct platform_device *pdev,
					struct max77693_platform_data *pdata)
{
	struct max77693_dev *iodev = dev_get_drvdata(pdev->dev.parent);
	struct device_node *np;
	struct max77693_regulator_data *rdata;
	struct of_regulator_match rmatch;
	int i, ret, cnt = 0;

	np = of_find_node_by_name(iodev->dev->of_node, "regulators");
	if (!np)
		return -EINVAL;

	of_property_read_u32(np, "number-of-regulators", &pdata->num_regulators);

	/* Keep going whether the number of regulators prdefined or not*/
	if (!pdata->num_regulators) {
		dev_err(&pdev->dev,
			"failed to get the number of regulators supported\n");
		pdata->num_regulators = ARRAY_SIZE(regulators);
	}

	rdata = devm_kzalloc(&pdev->dev, sizeof(*rdata) *
			     pdata->num_regulators, GFP_KERNEL);
	if (!rdata)
		return -ENOMEM;

	for (i = 0; i < pdata->num_regulators; i++) {
		rmatch.name = regulators[i].name;
		ret = of_regulator_match(&pdev->dev, np, &rmatch, 1);
		if (ret == 1)
			rdata[cnt++].id = regulators[i].id;
		else
			continue;
		rdata[i].initdata = rmatch.init_data;
		rdata[i].of_node = rmatch.of_node;
	}

	pdata->regulators = rdata;
	pdata->num_regulators = cnt;

	return 0;
}
#else
static int max77693_pmic_dt_parse_pdata(struct platform_device *pdev,
					struct max77693_platform_data *pdata)
{
	return 0;
}
#endif
static int max77693_pmic_probe(struct platform_device *pdev)
{
	struct max77693_dev *iodev = dev_get_drvdata(pdev->dev.parent);
	struct max77693_platform_data *pdata = dev_get_platdata(iodev->dev);
	struct max77693_data *max77693;
	int i, ret, size;
	struct regulator_config config = {};

	if (!pdata) {
		pr_info("[%s:%d] !pdata\n", __FILE__, __LINE__);
		dev_err(pdev->dev.parent, "No platform init data supplied.\n");
		return -ENODEV;
	}

	if (iodev->dev->of_node) {
		ret = max77693_pmic_dt_parse_pdata(pdev, pdata);
		if (ret)
			return ret;
	}

	max77693 = devm_kzalloc(&pdev->dev, sizeof(struct max77693_data),
				GFP_KERNEL);
	if (!max77693) {
		pr_info("[%s:%d] if (!max77693)\n", __FILE__, __LINE__);
		return -ENOMEM;
	}
	size = sizeof(struct regulator_dev *) * pdata->num_regulators;
	max77693->rdev =devm_kzalloc(&pdev->dev, size, GFP_KERNEL);
	if (!max77693->rdev) {
		pr_info("[%s:%d] if (!max77693->rdev)\n", __FILE__, __LINE__);
		return -ENOMEM;
	}

	max77693->dev = &pdev->dev;
	max77693->iodev = iodev;
	max77693->num_regulators = pdata->num_regulators;

	config.dev = &pdev->dev;
	config.regmap = iodev->regmap;
	config.driver_data = max77693;
	platform_set_drvdata(pdev, max77693);

	pr_debug("[%s:%d] pdata->num_regulators:%d\n", __FILE__, __LINE__,
		pdata->num_regulators);
	for (i = 0; i < pdata->num_regulators; i++) {
		const struct voltage_map_desc *desc;
		int id = pdata->regulators[i].id;

		config.init_data = pdata->regulators[i].initdata;
		config.of_node = pdata->regulators[i].of_node;

		pr_debug("[%s:%d] for in pdata->num_regulators:%d\n", __FILE__,
			__LINE__, pdata->num_regulators);
		desc = reg_voltage_map[id];
		if (id == MAX77693_ESAFEOUT1 || id == MAX77693_ESAFEOUT2)
			regulators[id].n_voltages = 4;

		max77693->rdev[i] = regulator_register(&regulators[id], &config);
		if (IS_ERR(max77693->rdev[i])) {
			ret = PTR_ERR(max77693->rdev[i]);
			dev_err(max77693->dev, "regulator init failed for %d\n",
				id);
			max77693->rdev[i] = NULL;
			goto err;
		}
	}

	return 0;
 err:
	pr_info("[%s:%d] err:\n", __FILE__, __LINE__);
	while (--i >= 0)
		regulator_unregister(max77693->rdev[i]);

	return ret;
}

static int max77693_pmic_remove(struct platform_device *pdev)
{
	struct max77693_data *max77693 = platform_get_drvdata(pdev);
	struct regulator_dev **rdev = max77693->rdev;
	int i;

	for (i = 0; i < max77693->num_regulators; i++)
		if (rdev[i])
			regulator_unregister(rdev[i]);

	return 0;
}

static const struct platform_device_id max77693_pmic_id[] = {
	{"max77693-pmic", 0},
	{},
};

MODULE_DEVICE_TABLE(platform, max77693_pmic_id);

static struct platform_driver max77693_pmic_driver = {
	.driver = {
		   .name = "max77693-pmic",
		   .owner = THIS_MODULE,
		   },
	.probe = max77693_pmic_probe,
	.remove = max77693_pmic_remove,
	.id_table = max77693_pmic_id,
};

static int __init max77693_pmic_init(void)
{
	return platform_driver_register(&max77693_pmic_driver);
}
#ifdef CONFIG_FAST_RESUME
beforeresume_initcall(max77693_pmic_init);
#else
subsys_initcall(max77693_pmic_init);
#endif

static void __exit max77693_pmic_cleanup(void)
{
	platform_driver_unregister(&max77693_pmic_driver);
}

module_exit(max77693_pmic_cleanup);

MODULE_DESCRIPTION("MAXIM 77693 Regulator Driver");
MODULE_AUTHOR("Sukdong Kim <Sukdong.Kim@samsung.com>");
MODULE_LICENSE("GPL");
