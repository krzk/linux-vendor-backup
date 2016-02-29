/*
 * Copyright (C) 2013 Samsung Electronics Co.Ltd
 * Authors:
 *	Joong-Mock Shin <jmock.shin@samsung.com>
 *	Taeheon Kim <th908.kim@samsung.com>
 *	Sangmin Lee <lsmin.lee@samsung.com>
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 *
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/gpio.h>
#include <linux/platform_device.h>
#include <linux/regulator/consumer.h>
#include <linux/irq.h>
#include <linux/err.h>
#include <linux/interrupt.h>
#include <linux/suspend.h>
#include <linux/of_gpio.h>
#include <linux/wakelock.h>
#include <linux/input.h>
#include <linux/input/tizen_detent.h>
#include <plat/gpio-cfg.h>
#if defined(CONFIG_SYSTEM_LOAD_ANALYZER)
#include <linux/trm.h>
#endif
#ifdef CONFIG_SLEEP_MONITOR
#include <linux/power/sleep_monitor.h>
#endif
#ifdef CONFIG_DISPLAY_EARLY_DPMS
#include <drm/exynos_drm.h>
#endif

extern struct class *sec_class;

#define WAKELOCK_TIME		HZ/10

enum direction_patten {
	CounterClockwise = -2,
	Detent_Return = -1,
	Detent_Leave = 1,
	Clockwise = 2,
	Direction_MAX,
};

enum status_patten {
	Patten_S = 0,
	Patten_A = 1,
	Patten_B = 2,
	Patten_C = 4,
	Status_MAX,
};

static int hall_sensor_get_direction(struct hall_sensor_driverdata *ddata, int status)
{
	const int pattern[Status_MAX] = {-1, Patten_C, Patten_A, -1, Patten_B};


	if (!status)
		return Detent_Leave;
	else if (status == ddata->last_status)
		return Detent_Return;
	else  if (pattern[ddata->last_status] == status)
		return CounterClockwise;
	else
		return Clockwise;
}

static int hall_sensor_get_value(struct hall_sensor_driverdata *ddata)
{
	struct input_dev *input_dev = ddata->input_dev;
	int value;

	ddata->a_status = !gpio_get_value(ddata->gpio_a);
	ddata->b_status = !gpio_get_value(ddata->gpio_b);
	ddata->c_status = !gpio_get_value(ddata->gpio_c);

	value = (ddata->c_status << 2) |(ddata->b_status << 1) | ddata->a_status;

	dev_dbg(&input_dev->dev, "%s: a=[%u], b=[%u], c=[%u] value=[%u]\n",
		__func__, ddata->a_status, ddata->b_status, ddata->c_status, value);

	return value;
}

static int hall_sensor_check_validation(struct hall_sensor_driverdata *ddata, int status)
{
	if (status == ddata->last_value)
		return -1;
	else if (status == Patten_S)
		return 0;
	else if (status == Patten_A)
		return 0;
	else if (status == Patten_B)
		return 0;
	else if (status == Patten_C)
		return 0;
	else
		return -1;
}

static irqreturn_t hall_sensor_detect_handler(int irq, void *dev_id)
{
	struct hall_sensor_driverdata *ddata = dev_id;
	struct input_dev *input_dev = ddata->input_dev;
#ifdef CONFIG_DISPLAY_EARLY_DPMS
	struct display_early_dpms_nb_event event;
#endif
	int value, direction;

	if (!ddata->probe_done)
		goto out;

	value = hall_sensor_get_value(ddata);

	if (hall_sensor_check_validation(ddata, value)) {
		dev_err(&input_dev->dev, "%s: read invalid data [%d][%d]\n",
				__func__, ddata->last_value, value);
		goto out;
	}

	direction = hall_sensor_get_direction(ddata, value);

	wake_lock_timeout(&ddata->wake_lock, WAKELOCK_TIME);
	input_report_rel(ddata->input_dev, REL_WHEEL, direction);
	input_report_rel(ddata->input_dev, REL_X, (unsigned char)~value & 0x07);
	input_sync(ddata->input_dev);
#ifdef CONFIG_SLEEP_MONITOR
	if (ddata->event_cnt < 0xffff)
		ddata->event_cnt++;
#endif
#ifdef CONFIG_DISPLAY_EARLY_DPMS
	if (!ddata->resume_state) {
		event.id = DISPLAY_EARLY_DPMS_ID_PRIMARY;
		event.data = (void *)true;
		display_early_dpms_nb_send_event(DISPLAY_EARLY_DPMS_MODE_SET,
					(void *)&event);
	}
#endif

#if defined (ROTARY_BOOSTER)
	rotary_booster_turn_on();
#endif

	ddata->last_value = value;
	if (value)
		ddata->last_status = value;

	dev_info(&input_dev->dev, "%s: s=[%d], d=[%d]\n", __func__, value, direction);

out:
	return IRQ_HANDLED;
}

static ssize_t hall_sensor_show_raw_value(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct hall_sensor_driverdata *ddata = dev_get_drvdata(dev);
	struct input_dev *input_dev = ddata->input_dev;
	int hall_status;

	hall_status = hall_sensor_get_value(ddata);

	hall_status = (unsigned char)~hall_status & 0x07;

	dev_info(&input_dev->dev, "%s: hall_status=[%d]\n", __func__, hall_status);

	return sprintf(buf,"0x%1x\n", hall_status);
}

static struct device_attribute dev_attr_show_hall_status =
		__ATTR(raw_value, 0444,hall_sensor_show_raw_value, NULL);

static struct attribute *hall_detent_attributes[] = {
	&dev_attr_show_hall_status.attr,
	NULL,
};

static struct attribute_group hall_detent_attr_group = {
	.attrs = hall_detent_attributes,
};

#ifdef CONFIG_SLEEP_MONITOR
#define	PRETTY_MAX	14
#define	STATE_BIT	24
#define	CNT_MARK	0xffff
#define	STATE_MARK	0xff

static int hall_sensor_get_sleep_monitor_cb(void* priv, unsigned int *raw_val, int check_level, int caller_type);

static struct sleep_monitor_ops  hall_sensor_sleep_monitor_ops = {
	 .read_cb_func =  hall_sensor_get_sleep_monitor_cb,
};

static int hall_sensor_get_sleep_monitor_cb(void* priv, unsigned int *raw_val, int check_level, int caller_type)
{
	struct hall_sensor_driverdata *ddata = priv;
	struct input_dev *input_dev = ddata->input_dev;
	int state = DEVICE_UNKNOWN;
	int pretty = 0;
	*raw_val = -1;

	if ((check_level == SLEEP_MONITOR_CHECK_SOFT) ||\
	    (check_level == SLEEP_MONITOR_CHECK_HARD)){
		if (ddata->resume_state)
			state = DEVICE_ON_ACTIVE1;
		else
			state = DEVICE_POWER_OFF;
	}

	*raw_val = ((state & STATE_MARK) << STATE_BIT) |\
			(ddata->event_cnt & CNT_MARK);

	if (ddata->event_cnt > PRETTY_MAX)
		pretty = PRETTY_MAX;
	else
		pretty = ddata->event_cnt;

	ddata->event_cnt = 0;

	dev_dbg(&input_dev->dev, "%s: raw_val[0x%08x], check_level[%d], state[%d], pretty[%d]\n",
		__func__, *raw_val, check_level, state, pretty);

	return pretty;
}
#endif


#ifdef CONFIG_OF
static int hall_sensor_parse_dt(struct device *dev,
			struct hall_sensor_platform_data *pdata)
{
	struct device_node *np = dev->of_node;

	/* gpio info */
	pdata->gpio_a = of_get_named_gpio_flags(np, "hall_sensor,gpio_a",
				0, &pdata->gpio_a);
	pdata->gpio_b = of_get_named_gpio_flags(np, "hall_sensor,gpio_b",
				0, &pdata->gpio_b);
	pdata->gpio_c = of_get_named_gpio_flags(np, "hall_sensor,gpio_c",
				0, &pdata->gpio_c);

	return 0;
}
#else
static int hall_sensor_parse_dt(struct device *dev,
			struct hall_sensor_platform_data *pdata)
{
	return -ENODEV;
}
#endif

static struct of_device_id hall_sensor_of_match[] = {
	{ .compatible = HALL_NAME, },
	{ },
};
MODULE_DEVICE_TABLE(of, hall_sensor_of_match);

static int __devinit hall_sensor_probe(struct platform_device *pdev)
{
	struct hall_sensor_platform_data *pdata;
	struct hall_sensor_driverdata *ddata;
	unsigned long isr_flags;
	int ret = 0;

	if (pdev->dev.of_node) {
		pdata = kzalloc(sizeof(struct hall_sensor_platform_data), GFP_KERNEL);
		if (!pdata) {
			dev_err(&pdev->dev, "%s: Failed to allocate memory\n", __func__);
			return -ENOMEM;
		}

		ret = hall_sensor_parse_dt(&pdev->dev, pdata);
		if (ret) {
			dev_err(&pdev->dev, "%s: Error hall_sensor_parse_dt\n", __func__);
			return ret;
		}
	} else {
		pdata = pdev->dev.platform_data;
		if (!pdata) {
			dev_err(&pdev->dev, "%s: No hall_sensor platform data\n", __func__);
			return -EINVAL;
		}
	}

	ddata = kzalloc(sizeof(*ddata), GFP_KERNEL);
	if (!ddata) {
		dev_err(&pdev->dev, "%s: Failed ddata malloc.\n", __func__);
		goto err_kzalloc;
	}

	ddata->input_dev = input_allocate_device();
	if (!ddata->input_dev) {
		dev_err(&pdev->dev, "%s: Failed to allocate input device.\n", __func__);
		goto err_input_allocate;
	}

	ddata->gpio_a = pdata->gpio_a;
	ddata->gpio_b = pdata->gpio_b;
	ddata->gpio_c = pdata->gpio_c;
	ddata->dev = pdev;
	platform_set_drvdata(pdev, ddata);

	__set_bit(EV_REL, ddata->input_dev->evbit);
	__set_bit(EV_KEY, ddata->input_dev->evbit);
	__set_bit(REL_X, ddata->input_dev->relbit);
	__set_bit(REL_Y, ddata->input_dev->relbit);
	__set_bit(BTN_LEFT, ddata->input_dev->keybit);
	__set_bit(REL_WHEEL, ddata->input_dev->relbit);

	input_set_capability(ddata->input_dev, EV_REL, REL_X);
	input_set_capability(ddata->input_dev, EV_REL, REL_Y);
	input_set_capability(ddata->input_dev, EV_REL, REL_WHEEL);

	ddata->input_dev->name = HALL_NAME;
	ddata->input_dev->id.bustype = BUS_VIRTUAL;
	ddata->input_dev->dev.parent = &pdev->dev;
	ddata->input_dev->phys = HALL_NAME;
	ddata->input_dev->id.vendor = 0x0001;
	ddata->input_dev->id.product = 0x0001;
	ddata->input_dev->id.version = 0x0100;

	ret = gpio_request(ddata->gpio_a, "hall_sensor_a");
	if (ret) {
		dev_err(&pdev->dev, "%s:"\
			" unable to request hall_sensor_a [%d]\n",\
			__func__, ddata->gpio_a);
		goto err_gpio_request;
	}

	ret = s3c_gpio_cfgpin(ddata->gpio_a, S3C_GPIO_SFN(0xf));
	if (ret) {
		dev_err(&pdev->dev,
			"%s: unable to request input pin [%d]\n",
			__func__, ddata->gpio_a);
		goto err_gpio_direction_a;
	}

	ret = s3c_gpio_setpull(ddata->gpio_a, S3C_GPIO_PULL_NONE);
	if (ret) {
		dev_err(&pdev->dev,
			"%s: unable to set pull down [%d]\n",
			__func__, ddata->gpio_a);
		goto err_gpio_direction_a;
	}

	s5p_register_gpio_interrupt(ddata->gpio_a);
	ddata->hall_a_irq = gpio_to_irq(ddata->gpio_a);

	ret = gpio_request(ddata->gpio_b, "hall_sensor_b");
	if (ret) {
		dev_err(&pdev->dev, "%s:"\
			" unable to request hall_sensorr_b [%d]\n",\
			__func__, ddata->gpio_b);
		goto err_gpio_direction_a;
	}

	ret = s3c_gpio_cfgpin(ddata->gpio_b, S3C_GPIO_SFN(0xf));
	if (ret) {
		dev_err(&pdev->dev,
			"%s: unable to request input pin [%d]\n",
			__func__, ddata->gpio_b);
		goto err_gpio_direction_b;
	}

	s3c_gpio_setpull(ddata->gpio_b, S3C_GPIO_PULL_NONE);
	if (ret) {
		dev_err(&pdev->dev,
			"%s: unable to set pull down [%d]\n",
			__func__, ddata->gpio_b);
		goto err_gpio_direction_b;
	}

	s5p_register_gpio_interrupt(ddata->gpio_b);
	ddata->hall_b_irq = gpio_to_irq(ddata->gpio_b);

	ret = gpio_request(ddata->gpio_c, "hall_sensor_c");
	if (ret) {
		dev_err(&pdev->dev, "%s:"\
			" unable to request hall_sensorr_c [%d]\n",\
			__func__, ddata->gpio_c);
		goto err_gpio_direction_b;
	}

	ret = s3c_gpio_cfgpin(ddata->gpio_c, S3C_GPIO_SFN(0xf));
	if (ret) {
		dev_err(&pdev->dev,
			"%s: unable to request input pin [%d]\n",
			__func__, ddata->gpio_c);
		goto err_gpio_direction_c;
	}

	s3c_gpio_setpull(ddata->gpio_c, S3C_GPIO_PULL_NONE);
	if (ret) {
		dev_err(&pdev->dev,
			"%s: unable to set pull down [%d]\n",
			__func__, ddata->gpio_c);
		goto err_gpio_direction_c;
	}

	s5p_register_gpio_interrupt(ddata->gpio_c);
	ddata->hall_c_irq = gpio_to_irq(ddata->gpio_c);

	ddata->last_status = hall_sensor_get_value(ddata);

	isr_flags = IRQF_DISABLED |IRQF_TRIGGER_RISING|\
		IRQF_TRIGGER_FALLING | IRQF_ONESHOT;

	ret = request_threaded_irq(ddata->hall_a_irq , NULL,
		hall_sensor_detect_handler, isr_flags, "hall_a_status", ddata);
	if (ret < 0) {
		dev_err(&pdev->dev, "%s: failed to request hall_a_irq. irq[%d] gpio[%d]\n",
				__func__, ddata->hall_a_irq , ddata->gpio_a);
		goto err_threaded_irq;
	}

	ret = request_threaded_irq(ddata->hall_b_irq , NULL,
		hall_sensor_detect_handler, isr_flags, "hall_b_status", ddata);
	if (ret < 0) {
		dev_err(&pdev->dev, "%s: failed to request hall_b_irq. irq[%d] gpio[%d]\n",
				__func__, ddata->hall_b_irq , ddata->gpio_b);
		goto err_threaded_irq;
	}

	ret = request_threaded_irq(ddata->hall_c_irq , NULL,
		hall_sensor_detect_handler, isr_flags, "hall_c_status", ddata);
	if (ret < 0) {
		dev_err(&pdev->dev, "%s: failed to request hall_c_irq. irq[%d] gpio[%d]\n",
				__func__, ddata->hall_c_irq , ddata->gpio_c);
		goto err_threaded_irq;
	}

	if (!sec_class) {
		dev_err(&pdev->dev, "%s:sec_class is NULL.\n",  __func__);
		goto err_threaded_irq;
	}

	ddata->rotary_dev = device_create(sec_class, NULL, 0, NULL, HALL_NAME);
	if (IS_ERR(ddata->rotary_dev)) {
		dev_err(&pdev->dev, "%s:Unable to create rotary device.\n",  __func__);
		goto err_threaded_irq;
	}

	dev_set_drvdata(ddata->rotary_dev, ddata);

	device_init_wakeup(&pdev->dev, true);
	wake_lock_init(&ddata->wake_lock,
			WAKE_LOCK_SUSPEND, "hall_sensor_wake_lock");

	ret = input_register_device(ddata->input_dev);
	if (ret) {
		dev_err(&pdev->dev, "%s:Unable to register %s input device\n",\
			 __func__, ddata->input_dev->name);
		goto err_threaded_irq;
	}

	ret = sysfs_create_group(&ddata->rotary_dev->kobj, &hall_detent_attr_group);
	if (ret) {
		dev_err(&pdev->dev, "%s: Failed to create sysfs group\n", __func__);
		goto err_create_group;
	}

	ddata->resume_state = true;
	ddata->probe_done = true;
#ifdef CONFIG_DISPLAY_EARLY_DPMS
	device_set_early_complete(&pdev->dev, EARLY_COMP_SLAVE);
#endif
#ifdef CONFIG_SLEEP_MONITOR
	sleep_monitor_register_ops(ddata, &hall_sensor_sleep_monitor_ops,
			SLEEP_MONITOR_ROTARY);
#endif
	dev_info(&pdev->dev, "%s done successfully\n", __func__);

	return 0;

err_create_group:
	input_unregister_device(ddata->input_dev);
err_threaded_irq:
	wake_lock_destroy(&ddata->wake_lock);
err_gpio_direction_c:
	gpio_free(ddata->gpio_b);
err_gpio_direction_b:
	gpio_free(ddata->gpio_c);
err_gpio_direction_a:
	gpio_free(ddata->gpio_a);
err_gpio_request:
	input_free_device(ddata->input_dev);
	platform_set_drvdata(pdev, NULL);
err_input_allocate:
	kfree(ddata);
err_kzalloc:
	return ret;
}

static int __devexit hall_sensor_remove(struct platform_device *pdev)
{
	struct hall_sensor_driverdata *ddata = platform_get_drvdata(pdev);

#ifdef CONFIG_SLEEP_MONITOR
	sleep_monitor_unregister_ops(SLEEP_MONITOR_ROTARY);
#endif
	device_destroy(sec_class, ddata->rotary_dev->devt);
	input_unregister_device(ddata->input_dev);
	wake_lock_destroy(&ddata->wake_lock);
	gpio_free(ddata->gpio_c);
	gpio_free(ddata->gpio_a);
	wake_lock_destroy(&ddata->wake_lock);
	input_free_device(ddata->input_dev);
	platform_set_drvdata(pdev, NULL);
	kfree(ddata);
	return 0;
}

static int hall_sensor_resume(struct device *dev)
{
	struct hall_sensor_driverdata *ddata = dev_get_drvdata(dev);

	if (device_may_wakeup(dev)) {
		ddata->resume_state = true;
		disable_irq_wake(ddata->hall_a_irq);
		disable_irq_wake(ddata->hall_b_irq);
		disable_irq_wake(ddata->hall_c_irq);
	} else {
		ddata->last_status = hall_sensor_get_value(ddata);
		enable_irq(ddata->hall_a_irq);
		enable_irq(ddata->hall_b_irq);
		enable_irq(ddata->hall_c_irq);
	}

	return 0;
}

static int hall_sensor_suspend(struct device *dev)
{
	struct hall_sensor_driverdata *ddata = dev_get_drvdata(dev);

	if (device_may_wakeup(dev)) {
		ddata->resume_state = false;
		enable_irq_wake(ddata->hall_a_irq);
		enable_irq_wake(ddata->hall_b_irq);
		enable_irq_wake(ddata->hall_c_irq);
	} else {
		disable_irq(ddata->hall_a_irq);
		disable_irq(ddata->hall_b_irq);
		disable_irq(ddata->hall_c_irq);
	}

	return 0;
}

static const struct dev_pm_ops hall_pm_ops = {
	.suspend = hall_sensor_suspend,
	.resume = hall_sensor_resume,
};

static struct platform_driver hall_sensor_driver = {
	.probe		= hall_sensor_probe,
	.remove		= __devexit_p(hall_sensor_remove),
	.driver		= {
		.name	= HALL_NAME,
		.owner	= THIS_MODULE,
		.pm	= &hall_pm_ops,
		.of_match_table = hall_sensor_of_match
	},
};

static int __init hall_sensor_init(void)
{
	return platform_driver_register(&hall_sensor_driver);
}

static void __exit hall_sensor_exit(void)
{
	platform_driver_unregister(&hall_sensor_driver);
}

late_initcall(hall_sensor_init);
module_exit(hall_sensor_exit);

/* Module information */
MODULE_AUTHOR("Joong-Mock Shin <jmock.shin@samsung.com>");
MODULE_DESCRIPTION("Hall sensor driver");
MODULE_LICENSE("GPL");
