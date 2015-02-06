/*
 * max77843.c - LED class driver for Maxim MAX77843
 *
 * Copyright (C) 2015 Samsung Electronics
 * Author: Jaewon Kim <jaewon02.kim@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <linux/err.h>
#include <linux/module.h>
#include <linux/leds.h>
#include <linux/regmap.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/mfd/max77843-private.h>

#define MAX77843_MAX_BRIGHTNESS		0xFF

enum max77843_leds {
	MAX77843_LED0 = 0,
	MAX77843_LED1,
	MAX77843_LED2,
	MAX77843_LED3,

	MAX77843_LED_NUM,
};

struct max77843_led_info {
	struct led_classdev cdev;
	const char *color;
	u8 channel;
	bool active;
};

struct max77843_led {
	struct max77843 *max77843;
	struct regmap *regmap_led;
	struct device *dev;
	struct max77843_led_info led_info[4];
};

static struct max77843_led *cdev_to_led(struct led_classdev *cdev)
{
	struct max77843_led_info *led_info = container_of(cdev,
			struct max77843_led_info, cdev);

	return container_of(led_info, struct max77843_led,
			led_info[led_info->channel]);
}

static void max77843_led_brightness_set(struct led_classdev *led_cdev,
		enum led_brightness value)
{

	struct max77843_led *led = cdev_to_led(led_cdev);
	struct max77843_led_info *led_info = container_of(led_cdev,
			struct max77843_led_info, cdev);
	u8 channel = led_info->channel;

	switch (channel) {
	case MAX77843_LED0:
		regmap_write(led->regmap_led,
				MAX77843_LED_REG_LED0BRT, value);
		if (value == 0)
			regmap_update_bits(led->regmap_led,
					MAX77843_LED_REG_LEDEN,
					MAX77843_LED_LED0EN_MASK,
					OFF << LED0EN_SHIFT);
		else
			regmap_update_bits(led->regmap_led,
					MAX77843_LED_REG_LEDEN,
					MAX77843_LED_LED0EN_MASK,
					CONSTANT << LED0EN_SHIFT);

		break;
	case MAX77843_LED1:
		regmap_write(led->regmap_led,
				MAX77843_LED_REG_LED1BRT, value);
		if (value == 0)
			regmap_update_bits(led->regmap_led,
					MAX77843_LED_REG_LEDEN,
					MAX77843_LED_LED1EN_MASK,
					OFF << LED1EN_SHIFT);
		else
			regmap_update_bits(led->regmap_led,
					MAX77843_LED_REG_LEDEN,
					MAX77843_LED_LED1EN_MASK,
					CONSTANT << LED1EN_SHIFT);
		break;
	case MAX77843_LED2:
		regmap_write(led->regmap_led,
				MAX77843_LED_REG_LED2BRT, value);
		if (value == 0)
			regmap_update_bits(led->regmap_led,
					MAX77843_LED_REG_LEDEN,
					MAX77843_LED_LED2EN_MASK,
					OFF << LED2EN_SHIFT);
		else
			regmap_update_bits(led->regmap_led,
					MAX77843_LED_REG_LEDEN,
					MAX77843_LED_LED2EN_MASK,
					CONSTANT << LED2EN_SHIFT);
		break;
	case MAX77843_LED3:
		regmap_write(led->regmap_led,
				MAX77843_LED_REG_LED3BRT, value);
		if (value == 0)
			regmap_update_bits(led->regmap_led,
					MAX77843_LED_REG_LEDEN,
					MAX77843_LED_LED3EN_MASK,
					OFF << LED3EN_SHIFT);
		else
			regmap_update_bits(led->regmap_led,
					MAX77843_LED_REG_LEDEN,
					MAX77843_LED_LED3EN_MASK,
					CONSTANT << LED3EN_SHIFT);
		break;
	}
}

static ssize_t max77843_led_show_color(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct led_classdev *cdev = dev_get_drvdata(dev);
	struct max77843_led_info *led_info = container_of(cdev,
			struct max77843_led_info, cdev);

	return sprintf(buf, "%s\n", led_info->color);
}

static DEVICE_ATTR(color, 0644, max77843_led_show_color, NULL);

static struct attribute *max77843_attrs[] = {
	&dev_attr_color.attr,
	NULL
};
ATTRIBUTE_GROUPS(max77843);

static int max77843_led_initialize(struct max77843_led_info *led_info,
		const char *name, const char *color, enum max77843_leds id)
{
	struct led_classdev *cdev = &led_info->cdev;
	struct max77843_led *led;
	int ret;

	led_info->channel = id;
	led_info->active = true;
	led_info->color = color;
	led = cdev_to_led(cdev);

	cdev->name = name;
	cdev->brightness = 0;
	cdev->max_brightness = MAX77843_MAX_BRIGHTNESS;
	cdev->brightness_set = max77843_led_brightness_set;
	cdev->groups = max77843_groups;

	ret = led_classdev_register(led->dev, cdev);
	if (ret < 0)
		return ret;

	return 0;
}

static int max77843_led_probe(struct platform_device *pdev)
{
	struct max77843 *max77843 = dev_get_drvdata(pdev->dev.parent);
	struct max77843_led *led;
	struct device_node *child;
	int i, ret;

	led = devm_kzalloc(&pdev->dev, sizeof(*led), GFP_KERNEL);
	if (!led)
		return -ENOMEM;

	led->regmap_led = max77843->regmap;
	led->dev = &pdev->dev;

	for_each_available_child_of_node(pdev->dev.of_node, child) {
		int channel;
		const char *label;
		const char *color;

		ret = of_property_read_u32(child, "channel", &channel);
		if (ret) {
			dev_err(&pdev->dev, "failed to parse channel\n");
			of_node_put(child);
			goto err_init;
		}

		ret = of_property_read_string(child, "label", &label);
		if (ret) {
			dev_err(&pdev->dev, "failed to parse lable\n");
			of_node_put(child);
			goto err_init;
		}

		ret = of_property_read_string(child, "color", &color);
		if (ret) {
			dev_err(&pdev->dev, "failed to parse color\n");
			of_node_put(child);
			goto err_init;
		}

		ret = max77843_led_initialize(&led->led_info[channel],
				label, color, channel);
		if (ret < 0) {
			dev_err(&pdev->dev, "failed to initialize leds\n");
			goto err_init;
		}
	}

	platform_set_drvdata(pdev, led);

	return 0;

err_init:
	for (i = 0; i < MAX77843_LED_NUM; i++)
		if (led->led_info[i].active)
			led_classdev_unregister(&led->led_info[i].cdev);

	return ret;
}

static int max77843_led_remove(struct platform_device *pdev)
{
	struct max77843_led *led = platform_get_drvdata(pdev);
	int i;

	for (i = 0; i < MAX77843_LED_NUM; i++)
		if (led->led_info[i].active)
			led_classdev_unregister(&led->led_info[i].cdev);

	return 0;
}

static struct platform_driver max77843_led_driver = {
	.driver = {
		.name	= "max77843-led",
	},
	.probe	= max77843_led_probe,
	.remove	= max77843_led_remove,
};

module_platform_driver(max77843_led_driver);

MODULE_AUTHOR("Jaewon Kim <jaewon02.kim@samsung.com>");
MODULE_DESCRIPTION("Maxim MAX77843 LED driver");
MODULE_LICENSE("GPL");
