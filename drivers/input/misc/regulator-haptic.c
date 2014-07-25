/*
 * Regulator haptic driver
 *
 * Copyright (c) 2013 Samsung Electronics Co., Ltd.
 *
 * Author: Hyunhee Kim <hyunhee.kim@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/input.h>
#include <linux/slab.h>
#include <linux/regulator/consumer.h>
#include <linux/of.h>

#define MAX_MAGNITUDE		0xffff
#define DEFAULT_MIN_MICROVOLT	1100000
#define DEFAULT_MAX_MICROVOLT	2700000

/*
 * struct regulator
 *
 * One for each consumer device.
 */
struct regulator_haptic {
	struct device *dev;
	struct input_dev *input_dev;
	struct work_struct work;
	bool enabled;
	struct regulator *regulator;
	struct mutex mutex;
	int max_mV;
	int min_mV;
	int intensity; /* mV */
	int level;
};

static void regulator_haptic_enable(
			struct regulator_haptic *haptic, bool enable)
{
	int ret;

	mutex_lock(&haptic->mutex);
	if (enable && !haptic->enabled) {
		haptic->enabled = true;
		ret = regulator_enable(haptic->regulator);
		if (ret)
			dev_err(haptic->dev, "failed to enable regulator\n");
	} else if (!enable && haptic->enabled) {
		haptic->enabled = false;
		ret = regulator_disable(haptic->regulator);
		if (ret)
			dev_err(haptic->dev, "failed to disable regulator\n");
	}

	if (haptic->enabled)
		regulator_set_voltage(haptic->regulator,
			haptic->intensity * 1000, haptic->max_mV * 1000);

	mutex_unlock(&haptic->mutex);
}

static void regulator_haptic_work(struct work_struct *work)
{
	struct regulator_haptic *haptic = container_of(work,
						       struct regulator_haptic,
						       work);
	if (haptic->level)
		regulator_haptic_enable(haptic, true);
	else
		regulator_haptic_enable(haptic, false);

}

static int regulator_haptic_play(struct input_dev *input, void *data,
				struct ff_effect *effect)
{
	struct regulator_haptic *haptic = input_get_drvdata(input);

	haptic->level = effect->u.rumble.strong_magnitude;
	if (!haptic->level)
		haptic->level = effect->u.rumble.weak_magnitude;

	haptic->intensity =
		(haptic->max_mV - haptic->min_mV) * haptic->level /
		MAX_MAGNITUDE;
	haptic->intensity = haptic->intensity + haptic->min_mV;

	if (haptic->intensity > haptic->max_mV)
		haptic->intensity = haptic->max_mV;
	if (haptic->intensity < haptic->min_mV)
		haptic->intensity = haptic->min_mV;

	schedule_work(&haptic->work);

	return 0;
}

static void regulator_haptic_close(struct input_dev *input)
{
	struct regulator_haptic *haptic = input_get_drvdata(input);

	cancel_work_sync(&haptic->work);
	regulator_haptic_enable(haptic, false);
}

static void regulator_haptic_parse_dt(struct regulator_haptic *haptic,
				struct device *dev)
{
	struct device_node *node = dev->of_node;
	int max_uV, min_uV;

	if (of_property_read_u32(node, "max-microvolt", &max_uV)) {
		dev_err(haptic->dev, "unable to parse max-microvolt\n");
		max_uV = DEFAULT_MAX_MICROVOLT;
	}
	if (of_property_read_u32(node, "min-microvolt", &min_uV)) {
		dev_err(haptic->dev, "unable to parse min-microvolt\n");
		min_uV = DEFAULT_MIN_MICROVOLT;
	}

	haptic->max_mV = max_uV / 1000;
	haptic->min_mV = min_uV / 1000;

}

static int regulator_haptic_probe(struct platform_device *pdev)
{
	struct regulator_haptic *haptic;
	struct input_dev *input_dev;
	int error;

	haptic = devm_kzalloc(&pdev->dev, sizeof(*haptic), GFP_KERNEL);
	if (!haptic) {
		dev_err(&pdev->dev, "unable to allocate memory for haptic\n");
		return -ENOMEM;
	}

	input_dev = devm_input_allocate_device(&pdev->dev);
	if (!input_dev) {
		dev_err(&pdev->dev, "unable to allocate memory\n");
		return  -ENOMEM;
	}

	INIT_WORK(&haptic->work, regulator_haptic_work);
	mutex_init(&haptic->mutex);
	haptic->input_dev = input_dev;
	haptic->dev = &pdev->dev;
	haptic->regulator = devm_regulator_get(&pdev->dev, "haptic");
	if (IS_ERR(haptic->regulator)) {
		error = PTR_ERR(haptic->regulator);
		dev_err(&pdev->dev, "unable to get regulator, err: %d\n",
			error);
		return error;
	}

	regulator_haptic_parse_dt(haptic, haptic->dev);
	haptic->input_dev->name = "regulator:haptic";
	haptic->input_dev->dev.parent = &pdev->dev;
	haptic->input_dev->close = regulator_haptic_close;
	haptic->enabled = false;
	input_set_drvdata(haptic->input_dev, haptic);
	input_set_capability(haptic->input_dev, EV_FF, FF_RUMBLE);

	error = input_ff_create_memless(input_dev, NULL,
				      regulator_haptic_play);
	if (error) {
		dev_err(&pdev->dev,
			"input_ff_create_memless() failed: %d\n",
			error);
		goto err_put_regulator;
	}

	error = input_register_device(haptic->input_dev);
	if (error) {
		dev_err(&pdev->dev,
			"couldn't register input device: %d\n",
			error);
		goto err_destroy_ff;
	}

	platform_set_drvdata(pdev, haptic);

	return 0;

err_destroy_ff:
	input_ff_destroy(haptic->input_dev);
err_put_regulator:
	regulator_put(haptic->regulator);

	return error;
}

static int regulator_haptic_remove(struct platform_device *pdev)
{
	struct regulator_haptic *haptic = platform_get_drvdata(pdev);

	input_unregister_device(haptic->input_dev);
	devm_regulator_put(haptic->regulator);

	return 0;
}

static struct of_device_id regulator_haptic_dt_match[] = {
	{ .compatible = "linux,regulator-haptic" },
	{},
};

static struct platform_driver regulator_haptic_driver = {
	.driver		= {
		.name	= "regulator-haptic",
		.owner	= THIS_MODULE,
		.of_match_table = regulator_haptic_dt_match,
	},
	.probe		= regulator_haptic_probe,
	.remove		= regulator_haptic_remove,
};
module_platform_driver(regulator_haptic_driver);

MODULE_ALIAS("platform:regulator-haptic");
MODULE_DESCRIPTION("Regulator haptic driver");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Hyunhee Kim <hyunhee.kim@samsung.com>");
