/*
 * Copyright (c) 2011, NVIDIA Corporation.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include <linux/gpio.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/rfkill.h>
#include <linux/platform_device.h>
#include <linux/clk.h>
#include <linux/slab.h>
#include <linux/of_gpio.h>

#include <linux/rfkill-gpio.h>
#include <linux/irq.h>
#include <linux/interrupt.h>

struct rfkill_gpio_data {
	const char		*name;
	enum rfkill_type	type;
	int			reset_gpio;
	int			shutdown_gpio;
	int			wake_gpio;
	int			host_wake_gpio;

	struct rfkill		*rfkill_dev;
	char			*reset_name;
	char			*shutdown_name;
	char			*wake_name;
	char			*host_wake_name;
	struct clk		*clk;

	bool			clk_enabled;
	int			irq;
};

static int rfkill_gpio_set_power(void *data, bool blocked)
{
	struct rfkill_gpio_data *rfkill = data;

	if (blocked) {
		if (gpio_is_valid(rfkill->shutdown_gpio))
			gpio_set_value(rfkill->shutdown_gpio, 0);
		if (gpio_is_valid(rfkill->reset_gpio))
			gpio_set_value(rfkill->reset_gpio, 0);
		if (gpio_is_valid(rfkill->wake_gpio))
			gpio_set_value(rfkill->wake_gpio, 0);
		if (!IS_ERR(rfkill->clk) && rfkill->clk_enabled)
			clk_disable_unprepare(rfkill->clk);
	} else {
		if (!IS_ERR(rfkill->clk) && !rfkill->clk_enabled)
			clk_prepare_enable(rfkill->clk);
		if (gpio_is_valid(rfkill->wake_gpio))
			gpio_set_value(rfkill->wake_gpio, 1);
		if (gpio_is_valid(rfkill->reset_gpio))
			gpio_set_value(rfkill->reset_gpio, 1);
		if (gpio_is_valid(rfkill->shutdown_gpio))
			gpio_set_value(rfkill->shutdown_gpio, 1);
	}

	rfkill->clk_enabled = !blocked;

	return 0;
}

static const struct rfkill_ops rfkill_gpio_ops = {
	.set_block = rfkill_gpio_set_power,
};

static int rfkill_gpio_dt_probe(struct device *dev,
				struct rfkill_gpio_data *rfkill)
{
	struct device_node * np = dev->of_node;

	rfkill->name = np->name;
	of_property_read_string(np, "rfkill-name", &rfkill->name);
	of_property_read_u32(np, "rfkill-type", &rfkill->type);
	rfkill->shutdown_gpio = of_get_named_gpio(np, "shutdown-gpio", 0);
	rfkill->wake_gpio = of_get_named_gpio(np, "wake-gpio", 0);
	rfkill->host_wake_gpio = of_get_named_gpio(np, "host-wake-gpio", 0);
	rfkill->reset_gpio = of_get_named_gpio(np, "reset-gpio", 0);


	return 0;
}

static irqreturn_t rfkill_gpio_irq_handler(int irq, void *data)
{
	struct rfkill_gpio_data *rfkill = data;
	int host_wake;
	unsigned int type;

	host_wake = gpio_get_value(rfkill->host_wake_gpio);
	type = (host_wake ? IRQF_TRIGGER_LOW : IRQF_TRIGGER_HIGH) |
			IRQF_NO_SUSPEND;
	irq_set_irq_type(rfkill->irq, type);

	return IRQ_HANDLED;
}

static int rfkill_gpio_probe(struct platform_device *pdev)
{
	struct rfkill_gpio_platform_data *pdata = pdev->dev.platform_data;
	struct rfkill_gpio_data *rfkill;
	const char *clk_name = NULL;
	int ret = 0;
	int len = 0;

	rfkill = devm_kzalloc(&pdev->dev, sizeof(*rfkill), GFP_KERNEL);
	if (!rfkill)
		return -ENOMEM;

	if (&pdev->dev.of_node) {
		ret = rfkill_gpio_dt_probe(&pdev->dev, rfkill);
		if (ret)
			return ret;
	} else if (pdata) {
		clk_name = pdata->power_clk_name;
		rfkill->name = pdata->name;
		rfkill->type = pdata->type;
		rfkill->reset_gpio = pdata->reset_gpio;
		rfkill->shutdown_gpio = pdata->shutdown_gpio;
	} else {
		return -ENODEV;
	}

	/* make sure at-least one of the GPIO is defined and that
	 * a name is specified for this instance */
	if ((!gpio_is_valid(rfkill->reset_gpio) &&
	     !gpio_is_valid(rfkill->shutdown_gpio)) || !rfkill->name) {
		pr_warn("%s: invalid platform data\n", __func__);
		return -EINVAL;
	}

	if (pdata && pdata->gpio_runtime_setup) {
		ret = pdata->gpio_runtime_setup(pdev);
		if (ret) {
			pr_warn("%s: can't set up gpio\n", __func__);
			return ret;
		}
	}

	len = strlen(rfkill->name);
	rfkill->reset_name = devm_kzalloc(&pdev->dev, len + 7, GFP_KERNEL);
	if (!rfkill->reset_name)
		return -ENOMEM;

	rfkill->shutdown_name = devm_kzalloc(&pdev->dev, len + 10, GFP_KERNEL);
	if (!rfkill->shutdown_name)
		return -ENOMEM;

	snprintf(rfkill->reset_name, len + 6 , "%s_reset", rfkill->name);
	snprintf(rfkill->shutdown_name, len + 9, "%s_shutdown", rfkill->name);

	rfkill->clk = devm_clk_get(&pdev->dev, clk_name);

	if (gpio_is_valid(rfkill->reset_gpio)) {
		ret = devm_gpio_request_one(&pdev->dev, rfkill->reset_gpio,
					    0, rfkill->reset_name);
		if (ret) {
			pr_warn("%s: failed to get reset gpio.\n", __func__);
			return ret;
		}
	}

	if (gpio_is_valid(rfkill->shutdown_gpio)) {
		ret = devm_gpio_request_one(&pdev->dev, rfkill->shutdown_gpio,
					    0, rfkill->shutdown_name);
		if (ret) {
			pr_warn("%s: failed to get shutdown gpio.\n", __func__);
			return ret;
		}
	}

	if (gpio_is_valid(rfkill->wake_gpio)) {
		ret = devm_gpio_request_one(&pdev->dev, rfkill->wake_gpio,
				0, rfkill->wake_name);
		if (ret) {
			pr_warn("%s: failed to get wake gpio.\n", __func__);
			return ret;
		}
	}

	if (gpio_is_valid(rfkill->host_wake_gpio)) {
		ret = devm_gpio_request_one(&pdev->dev, rfkill->host_wake_gpio,
				1, rfkill->host_wake_name);
		if (ret) {
			pr_warn("%s: failed to get host wake gpio.\n",
					__func__);
			return ret;
		}

		rfkill->irq = gpio_to_irq(rfkill->host_wake_gpio);
		ret = devm_request_irq(&pdev->dev, rfkill->irq,
				rfkill_gpio_irq_handler,
				IRQF_TRIGGER_HIGH | IRQF_NO_SUSPEND,
				"rfkill-gpio-irq", rfkill);
		if (ret) {
			pr_warn("%s: failed to request IRQ(%d) ret(%d)\n",
					__func__, rfkill->irq, ret);
			return ret;
		}
		ret = irq_set_irq_wake(rfkill->irq, 1);
		if (ret)
			return ret;
	}

	rfkill->rfkill_dev = rfkill_alloc(rfkill->name, &pdev->dev,
					  rfkill->type, &rfkill_gpio_ops,
					  rfkill);
	if (!rfkill->rfkill_dev)
		return -ENOMEM;

	rfkill_init_sw_state(rfkill->rfkill_dev, false);

	ret = rfkill_register(rfkill->rfkill_dev);
	if (ret < 0)
		return ret;

	platform_set_drvdata(pdev, rfkill);

	rfkill_set_sw_state(rfkill->rfkill_dev, true);

	device_init_wakeup(&pdev->dev, true);

	dev_info(&pdev->dev, "%s device registered.\n", rfkill->name);

	return 0;
}

static int rfkill_gpio_remove(struct platform_device *pdev)
{
	struct rfkill_gpio_data *rfkill = platform_get_drvdata(pdev);
	struct rfkill_gpio_platform_data *pdata = pdev->dev.platform_data;

	if (pdata && pdata->gpio_runtime_close)
		pdata->gpio_runtime_close(pdev);
	rfkill_unregister(rfkill->rfkill_dev);
	rfkill_destroy(rfkill->rfkill_dev);

	return 0;
}

#ifdef CONFIG_PM
static int rfkill_gpio_suspend(struct device *dev)
{
	struct platform_device *pdev = container_of(dev,
						struct platform_device,	dev);
	struct rfkill_gpio_data *rfkill = platform_get_drvdata(pdev);

	if (device_may_wakeup(dev))
		enable_irq_wake(rfkill->irq);

	return 0;
}

static int rfkill_gpio_resume(struct device *dev)
{
	struct platform_device *pdev = container_of(dev,
						struct platform_device,	dev);
	struct rfkill_gpio_data *rfkill = platform_get_drvdata(pdev);

	if (device_may_wakeup(dev))
		disable_irq_wake(rfkill->irq);

	return 0;
}
#else
#define rfkill_gpio_suspend	NULL
#define rfkill_gpio_resume		NULL
#endif /* CONFIG_PM */

const struct dev_pm_ops rfkill_gpio_pm = {
	.suspend = rfkill_gpio_suspend,
	.resume = rfkill_gpio_resume,
};

static const struct of_device_id rfkill_of_match[] = {
	{ .compatible = "rfkill-gpio", },
	{},
};

static struct platform_driver rfkill_gpio_driver = {
	.probe = rfkill_gpio_probe,
	.remove = rfkill_gpio_remove,
	.driver = {
		.name = "rfkill_gpio",
		.owner = THIS_MODULE,
		.pm = &rfkill_gpio_pm,
		.of_match_table = of_match_ptr(rfkill_of_match),
	},
};

module_platform_driver(rfkill_gpio_driver);

MODULE_DESCRIPTION("gpio rfkill");
MODULE_AUTHOR("NVIDIA");
MODULE_LICENSE("GPL");
