/*
 * Copyright (C) 2015 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <linux/gpio.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/rfkill.h>
#include <linux/platform_device.h>
#include <linux/clk.h>
#include <linux/slab.h>
#include <linux/gpio/consumer.h>
#include <linux/of_gpio.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/notifier.h>
#include <linux/hrtimer.h>
#include <net/bluetooth/bluetooth.h>
#include <net/bluetooth/hci_core.h>

#include <linux/rfkill-gpio.h>

struct bt_lpm_timer {
	struct hrtimer htimer;
	ktime_t sleep_delay;
};

#define BT_PM_WAKE_DELAY	(3)	/* 3 sec */

struct rfkill_bcm_data {
	const char		*name;
	const char		*clk_name;
	enum rfkill_type	type;
	struct gpio_desc	*reset_gpio;
	struct gpio_desc	*shutdown_gpio;
	struct gpio_desc	*wake_gpio;
	struct gpio_desc	*host_wake_gpio;

	struct rfkill		*rfkill_dev;
	struct clk		*clk;

	bool			clk_enabled;
	int			irq;
	struct device		*dev;
	struct bt_lpm_timer	timer;
};

static struct rfkill_bcm_data *bt_data;

static enum hrtimer_restart bt_lpm_sleep(struct hrtimer *htimer)
{
	struct bt_lpm_timer *timer = container_of(htimer, struct bt_lpm_timer,
				htimer);
	struct rfkill_bcm_data *rfkill = container_of(timer,
				struct rfkill_bcm_data, timer);

	gpiod_set_value(rfkill->wake_gpio, 0);
	pm_wakeup_event(rfkill->dev, HZ / 2);
	dev_dbg(rfkill->dev, "HCI Tx may be finished\n");

	return HRTIMER_NORESTART;
}

static void bt_lpm_wake(struct rfkill_bcm_data *rfkill)
{
	struct hrtimer *htimer = &rfkill->timer.htimer;

	hrtimer_try_to_cancel(htimer);
	pm_stay_awake(rfkill->dev);
	if (rfkill->clk_enabled)
		gpiod_set_value(rfkill->wake_gpio, 1);
	hrtimer_start(htimer, rfkill->timer.sleep_delay, HRTIMER_MODE_REL);
}

static int bt_lpm_init(struct rfkill_bcm_data *rfkill)
{
	struct hrtimer *timer = &rfkill->timer.htimer;

	hrtimer_init(timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	timer->function = bt_lpm_sleep;
	rfkill->timer.sleep_delay = ktime_set(BT_PM_WAKE_DELAY, 0);

	return 0;
}

static int rfkill_bcm_hci_event(struct notifier_block *this,
				 unsigned long event, void *data)
{
	struct hci_dev *hdev = (struct hci_dev *)data;
	struct rfkill_bcm_data *rfkill = bt_data;

	if (!hdev)
		return NOTIFY_DONE;

	switch (event) {
	case HCI_DEV_REG:
		dev_dbg(rfkill->dev, "HCI device is registered\n");
		break;
	case HCI_DEV_UNREG:
		dev_dbg(rfkill->dev, "HCI device is unregistered\n");
		break;
	case HCI_DEV_WRITE:
		dev_dbg(rfkill->dev, "HCI Tx is on going\n");
		bt_lpm_wake(rfkill);
		break;
	default:
		break;
	}

	return NOTIFY_DONE;
}

static struct notifier_block hci_event_nblock = {
	.notifier_call = rfkill_bcm_hci_event,
};

static int rfkill_bcm_set_power(void *data, bool blocked)
{
	struct rfkill_bcm_data *rfkill = data;

	if (!blocked && !IS_ERR(rfkill->clk) && !rfkill->clk_enabled)
		clk_prepare_enable(rfkill->clk);

	gpiod_set_value_cansleep(rfkill->shutdown_gpio, !blocked);
	gpiod_set_value_cansleep(rfkill->reset_gpio, !blocked);
	gpiod_set_value_cansleep(rfkill->wake_gpio, !blocked);

	if (blocked && !IS_ERR(rfkill->clk) && rfkill->clk_enabled)
		clk_disable_unprepare(rfkill->clk);

	rfkill->clk_enabled = !blocked;

	if (!blocked)
		msleep(100);

	return 0;
}

static const struct rfkill_ops rfkill_bcm_ops = {
	.set_block = rfkill_bcm_set_power,
};

static int rfkill_bcm_dt_probe(struct device *dev,
				struct rfkill_bcm_data *rfkill)
{
	struct device_node *np = dev->of_node;
	struct gpio_desc *gpio;
	int ret;

	rfkill->name = np->name;
	of_property_read_string(np, "rfkill-name", &rfkill->name);
	of_property_read_u32(np, "rfkill-type", &rfkill->type);
	of_property_read_string(np, "clock-names", &rfkill->clk_name);

	gpio = devm_gpiod_get_index(dev, "wake", 0);
	if (!IS_ERR(gpio)) {
		ret = gpiod_direction_output(gpio, 0);
		if (ret)
			return ret;
		rfkill->wake_gpio = gpio;
	}

	gpio = devm_gpiod_get_index(dev, "host-wake", 0);
	if (!IS_ERR(gpio)) {
		ret = gpiod_direction_input(gpio);
		if (ret)
			return ret;
		rfkill->host_wake_gpio = gpio;
	}

	return 0;
}

static irqreturn_t rfkill_bcm_irq_handler(int irq, void *data)
{
	struct rfkill_bcm_data *rfkill = data;
	int host_wake;
	unsigned int type;

	host_wake = gpiod_get_value(rfkill->host_wake_gpio);
	if (host_wake) {
		/* host is waked until Rx is finished */
		pm_stay_awake(rfkill->dev);
		dev_dbg(rfkill->dev, "HCI Rx is started\n");
	} else {
		struct hrtimer *htimer = &rfkill->timer.htimer;
		/* host is sleeped after guard time if there is no Tx*/
		if (!hrtimer_active(htimer))
			pm_wakeup_event(rfkill->dev, HZ / 2);
		dev_dbg(rfkill->dev, "HCI Rx is finished\n");
	}

	type = (host_wake ? IRQF_TRIGGER_FALLING : IRQF_TRIGGER_RISING) |
			IRQF_NO_SUSPEND;
	irq_set_irq_type(rfkill->irq, type);

	return IRQ_HANDLED;
}

static int rfkill_bcm_probe(struct platform_device *pdev)
{
	struct rfkill_gpio_platform_data *pdata = pdev->dev.platform_data;
	struct rfkill_bcm_data *rfkill;
	struct gpio_desc *gpio;
	int ret;

	rfkill = devm_kzalloc(&pdev->dev, sizeof(*rfkill), GFP_KERNEL);
	if (!rfkill)
		return -ENOMEM;

	rfkill->dev = &pdev->dev;

	if (&pdev->dev.of_node) {
		ret = rfkill_bcm_dt_probe(&pdev->dev, rfkill);
		if (ret)
			return ret;
	} else if (pdata) {
		rfkill->name = pdata->name;
		rfkill->type = pdata->type;
	} else {
		return -ENODEV;
	}

	rfkill->clk = devm_clk_get(&pdev->dev, rfkill->clk_name);

	gpio = devm_gpiod_get_index(&pdev->dev, "reset", 0);
	if (!IS_ERR(gpio)) {
		ret = gpiod_direction_output(gpio, 0);
		if (ret)
			return ret;
		rfkill->reset_gpio = gpio;
	}

	gpio = devm_gpiod_get_index(&pdev->dev, "shutdown", 0);
	if (!IS_ERR(gpio)) {
		ret = gpiod_direction_output(gpio, 0);
		if (ret)
			return ret;
		rfkill->shutdown_gpio = gpio;
	}

	/* Make sure at-least one of the GPIO is defined and that
	 * a name is specified for this instance
	 */
	if ((!rfkill->reset_gpio && !rfkill->shutdown_gpio) || !rfkill->name) {
		dev_err(&pdev->dev, "invalid platform data\n");
		return -EINVAL;
	}

	if (rfkill->host_wake_gpio) {
		rfkill->irq = gpiod_to_irq(rfkill->host_wake_gpio);
		ret = devm_request_irq(&pdev->dev, rfkill->irq,
				       rfkill_bcm_irq_handler,
				       IRQF_TRIGGER_HIGH | IRQF_NO_SUSPEND,
				       "rfkill-gpio-irq", rfkill);
		if (ret) {
			dev_err(&pdev->dev, "failed to request irq\n");
			return ret;
		}
		ret = irq_set_irq_wake(rfkill->irq, 1);
		if (ret)
			return ret;
	}

	rfkill->rfkill_dev = rfkill_alloc(rfkill->name, &pdev->dev,
					  rfkill->type, &rfkill_bcm_ops,
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

	/* bluetooth hci event registration */
	if (rfkill->type == 2) {
		bt_data = rfkill;
		bt_lpm_init(rfkill);
		hci_register_notifier(&hci_event_nblock);
	}

	dev_info(&pdev->dev, "%s device registered.\n", rfkill->name);

	return 0;
}

static int rfkill_bcm_remove(struct platform_device *pdev)
{
	struct rfkill_bcm_data *rfkill = platform_get_drvdata(pdev);

	/* bluetooth hci event unregistration */
	if (rfkill->type == 2) {
		hci_unregister_notifier(&hci_event_nblock);
		bt_data = NULL;
	}

	rfkill_unregister(rfkill->rfkill_dev);
	rfkill_destroy(rfkill->rfkill_dev);

	return 0;
}

#ifdef CONFIG_PM
static int rfkill_bcm_suspend(struct device *dev)
{
	struct platform_device *pdev = container_of(dev,
						struct platform_device,	dev);
	struct rfkill_bcm_data *rfkill = platform_get_drvdata(pdev);

	if (device_may_wakeup(dev))
		enable_irq_wake(rfkill->irq);

	return 0;
}

static int rfkill_bcm_resume(struct device *dev)
{
	struct platform_device *pdev = container_of(dev,
						struct platform_device,	dev);
	struct rfkill_bcm_data *rfkill = platform_get_drvdata(pdev);

	if (device_may_wakeup(dev))
		disable_irq_wake(rfkill->irq);

	return 0;
}
#else
#define rfkill_bcm_suspend	NULL
#define rfkill_bcm_resume	NULL
#endif /* CONFIG_PM */

const struct dev_pm_ops rfkill_bcm_pm = {
	.suspend = rfkill_bcm_suspend,
	.resume = rfkill_bcm_resume,
};

static const struct of_device_id rfkill_of_match[] = {
	{ .compatible = "rfkill-gpio", },
	{},
};

static struct platform_driver rfkill_bcm_driver = {
	.probe = rfkill_bcm_probe,
	.remove = rfkill_bcm_remove,
	.driver = {
		.name = "rfkill_bcm",
		.owner = THIS_MODULE,
		.pm = &rfkill_bcm_pm,
		.of_match_table = of_match_ptr(rfkill_of_match),
	},
};

module_platform_driver(rfkill_bcm_driver);

MODULE_DESCRIPTION("bcm rfkill");
MODULE_AUTHOR("beomho.seo@samsung.com");
MODULE_LICENSE("GPL");
