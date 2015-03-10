/*
 *  drivers/extcon/extcon-odroid-usbotg.c
 *
 *  USB cable extcon driver for Odroid U3, Odroid U3+ and Odroid X
 *
 * Copyright (C) 2014 Samsung Electronics
 * Author: Lukasz Stelmach <l.stelmach@samsung.com>
 *
 * Modified by Robert Baldyga <r.baldyga@samsung.com>
 *
 * based on drivers/extcon/extcon-gpio.c
 * Copyright (C) 2008 Google, Inc.
 * Author: Mike Lockwood <lockwood@android.com>
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/extcon.h>
#include <linux/extcon/extcon-gpio.h>
#include <linux/delay.h>

enum {
	EXTCON_CABLE_USB = 0,
	EXTCON_CABLE_USB_HOST,

	_EXTCON_CABLE_NUM,
};

static const char *odroid_usbotg_cable[] = {
	[EXTCON_CABLE_USB] = "USB",
	[EXTCON_CABLE_USB_HOST] = "USB-HOST",

	NULL,
};

struct odroid_usbotg_data {
	struct extcon_dev *edev;
	unsigned otg_id_gpio;
	unsigned vbus_det_gpio;
	int otg_id_irq;
	int vbus_det_irq;
	unsigned long debounce_ms;
};

/*
 * state    | VBUS_DET |  OTG_ID
 * -------------------------------
 * USB      |    H     |    H
 * USB-HOST |    H     |    L
 * disconn. |    L     |    H
 * USB-HOST |    L     |    L
 *
 * Only Odroid U3+ has OTG_ID line. U3 and X versions can detect only
 * USB slave cable.
 */

static void odroid_usbotg_detect_cable(struct odroid_usbotg_data *extcon_data)
{
	int state;

	mdelay(extcon_data->debounce_ms);

	if (extcon_data->otg_id_gpio)
		state = (gpio_get_value(extcon_data->vbus_det_gpio) << 1) |
			gpio_get_value(extcon_data->otg_id_gpio);
	else
		state = (gpio_get_value(extcon_data->vbus_det_gpio) << 1) | 1;

	dev_dbg(extcon_data->edev->dev, "cable state changed to %d\n", state);

	if (state & 0x1)
		extcon_set_cable_state_(extcon_data->edev,
			EXTCON_CABLE_USB_HOST, false);
	if (state != 0x3)
		extcon_set_cable_state_(extcon_data->edev,
			EXTCON_CABLE_USB, false);

	if (!(state & 0x1))
		extcon_set_cable_state_(extcon_data->edev,
			EXTCON_CABLE_USB_HOST, true);
	else if (state == 0x3)
		extcon_set_cable_state_(extcon_data->edev,
			EXTCON_CABLE_USB, true);
}

static irqreturn_t gpio_irq_handler(int irq, void *dev_id)
{
	struct odroid_usbotg_data *extcon_data = dev_id;

	odroid_usbotg_detect_cable(extcon_data);

	return IRQ_HANDLED;
}

static int odroid_usbotg_parse_dt(struct platform_device *pdev,
				    struct odroid_usbotg_data *extcon_data)
{
	struct device_node *np = pdev->dev.of_node;
	u32 val;

	if (!np)
		return -ENODEV;

	extcon_data->edev->name = np->name;
	extcon_data->vbus_det_gpio = of_get_named_gpio(np, "gpio-vbus-det", 0);
	if (of_property_read_bool(np, "gpio-otg-id"))
		extcon_data->otg_id_gpio =
			of_get_named_gpio(np, "gpio-otg-id", 0);

	if (of_property_read_u32(np, "debounce", &val) != 0)
		val = 50;
	extcon_data->debounce_ms = val;

	return 0;
}

static int odroid_usbotg_probe(struct platform_device *pdev)
{
	struct odroid_usbotg_data *extcon_data;
	int ret = 0;

	extcon_data = devm_kzalloc(&pdev->dev, sizeof(struct odroid_usbotg_data),
				   GFP_KERNEL);
	if (!extcon_data)
		return -ENOMEM;

	extcon_data->edev = devm_kzalloc(&pdev->dev, sizeof(struct extcon_dev),
					 GFP_KERNEL);
	if (IS_ERR(extcon_data->edev)) {
		dev_err(&pdev->dev, "failed to allocate extcon device\n");
		return -ENOMEM;
	}
	extcon_data->edev->supported_cable = odroid_usbotg_cable;

	ret = odroid_usbotg_parse_dt(pdev, extcon_data);
	if (IS_ERR_VALUE(ret)) {
		dev_err(&pdev->dev, "failed to get data from device tree\n");
		return ret;
	}

	/* gpios */
	ret = devm_gpio_request_one(&pdev->dev, extcon_data->vbus_det_gpio,
				    GPIOF_DIR_IN, "usbotg_vbus_det");
	if (ret < 0) {
		dev_err(&pdev->dev, "failed to get vbus_det gpio\n");
		return ret;
	}
	ret = devm_gpio_request_one(&pdev->dev, extcon_data->otg_id_gpio,
				    GPIOF_DIR_IN, "usbotg_otg_id");
	if (ret < 0)
		extcon_data->otg_id_gpio = 0;

	ret = extcon_dev_register(extcon_data->edev, &pdev->dev);
	if (ret < 0)
		return ret;

	/* irq */
	extcon_data->vbus_det_irq = gpio_to_irq(extcon_data->vbus_det_gpio);
	if (extcon_data->vbus_det_irq < 0) {
		dev_err(&pdev->dev, "failed to get irq from vbus_det\n");
		ret = extcon_data->vbus_det_irq;
		goto err;
	}
	ret = request_threaded_irq(extcon_data->vbus_det_irq, NULL,
				   gpio_irq_handler, IRQF_ONESHOT |
				   IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING,
				   pdev->name, extcon_data);
	if (ret < 0) {
		dev_err(&pdev->dev, "failed to request vbus_det irq\n");
		goto err;
	}

	if (extcon_data->otg_id_gpio) {
		extcon_data->otg_id_irq = gpio_to_irq(extcon_data->otg_id_gpio);
		if (extcon_data->otg_id_irq < 0) {
			dev_err(&pdev->dev, "failed to get irq from otg_id\n");
			ret = extcon_data->otg_id_irq;
			goto err;
		}
		ret = request_threaded_irq(extcon_data->otg_id_irq, NULL,
				gpio_irq_handler, IRQF_ONESHOT |
				IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING,
				pdev->name, extcon_data);
		if (ret < 0) {
			dev_err(&pdev->dev, "failed to request otg_id irq\n");
			goto err;
		}
	}

	platform_set_drvdata(pdev, extcon_data);
	/* Perform initial detection */
	odroid_usbotg_detect_cable(extcon_data);

	dev_dbg(&pdev->dev, "probe: success\n");

	return 0;

err:
	extcon_dev_unregister(extcon_data->edev);

	return ret;
}

static int odroid_usbotg_remove(struct platform_device *pdev)
{
	struct odroid_usbotg_data *extcon_data = platform_get_drvdata(pdev);

	free_irq(extcon_data->vbus_det_irq, extcon_data);
	if (extcon_data->otg_id_gpio)
		free_irq(extcon_data->otg_id_irq, extcon_data);
	extcon_dev_unregister(extcon_data->edev);

	return 0;
}

static const struct of_device_id odroid_usbotg_of_match[] = {
	{ .compatible = "extcon-odroid-usbotg" },
	{ },
};
MODULE_DEVICE_TABLE(of, odroid_usbotg_of_match);

static struct platform_driver odroid_usbotg_driver = {
	.probe		= odroid_usbotg_probe,
	.remove		= odroid_usbotg_remove,
	.driver		= {
		.name	= "odroid-usbotg",
		.owner	= THIS_MODULE,
		.of_match_table = of_match_ptr(odroid_usbotg_of_match)
	},
};

static int __init odroid_usbotg_init(void)
{
	return platform_driver_register(&odroid_usbotg_driver);
}

subsys_initcall(odroid_usbotg_init);

static void __exit odroid_usbotg_cleanup(void)
{
	platform_driver_unregister(&odroid_usbotg_driver);
}

module_exit(odroid_usbotg_cleanup);

MODULE_AUTHOR("Lukasz Stelmach <l.stelmach@samsung.com>");
MODULE_DESCRIPTION("USB OTG extcon driver for Odroid U3, U3+ and X");
MODULE_LICENSE("GPL");
