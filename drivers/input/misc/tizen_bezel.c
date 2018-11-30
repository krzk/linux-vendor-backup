/*
 * Copyright (C) 2018 Samsung Electronics Co.Ltd
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
#include <linux/input.h>
#include <linux/input/tizen_bezel.h>
#include <linux/trm.h>
#ifdef CONFIG_SLEEP_MONITOR
#include <linux/power/sleep_monitor.h>
#endif
#ifdef CONFIG_DISPLAY_EARLY_DPMS
#include <drm/tgm_drm.h>
#endif
#ifdef CONFIG_KNOX_GEARPAY
#include <linux/knox_gearlock.h>
#endif
#ifdef CONFIG_SEC_SYSFS
#include <linux/sec_sysfs.h>
#else
extern struct class *sec_class;
#endif

enum direction_patten {
	BZ_CC	= -2,
	BZ_RT	= -1,
	BZ_NA	= 0,
	BZ_LV	= 1,
	BZ_CW	= 2,
	BZ_MAX,
};

enum status_patten {
	Status_S = 0,
	Status_A = 1,
	Status_B = 2,
	Status_C = 4,
	Status_MAX,
};

#define	REL_X_EVENT_MASK 0x07

static int bezel_open(struct input_dev *input);
static void bezel_close(struct input_dev *input);

static int bezel_get_direction(struct bezel_ddata *ddata, int value)
{
	const int magic_pattern[Status_MAX][Status_MAX] = {
		{BZ_RT, BZ_LV, BZ_LV, BZ_NA, BZ_LV,},
		{BZ_LV, BZ_RT, BZ_CC, BZ_NA, BZ_CW,},
		{BZ_LV, BZ_CW, BZ_RT, BZ_NA, BZ_CC,},
		{BZ_NA, BZ_NA, BZ_NA, BZ_NA, BZ_NA,},
		{BZ_LV, BZ_CC, BZ_CW, BZ_NA, BZ_RT,} };

	if ((ddata->last_status >= Status_MAX) ||
	     (ddata->last_status <= Status_S))
		return BZ_RT;

	if ((value >= Status_MAX) || (value < Status_S))
		return BZ_RT;

	return magic_pattern[value][ddata->last_status];
}

static int bezel_get_status(struct bezel_ddata *ddata)
{
	struct platform_device *pdev = ddata->pdev;
	int value;

	ddata->a_status = !gpio_get_value(ddata->gpio_a);
	ddata->b_status = !gpio_get_value(ddata->gpio_b);
	ddata->c_status = !gpio_get_value(ddata->gpio_c);

	value = (ddata->c_status << 2) |(ddata->b_status << 1) | ddata->a_status;

	dev_dbg(&pdev->dev, "%s: a=[%u], b=[%u], c=[%u] value=[%u]\n",
		__func__, ddata->a_status, ddata->b_status, ddata->c_status, value);

	return value;
}

static int bezel_check_validation(struct bezel_ddata *ddata, int status)
{
	struct platform_device *pdev = ddata->pdev;

	if (status == ddata->last_value) {
		if (ddata->factory_mode) {
			dev_info(&pdev->dev,
				"%s: factory_mode. status =[%d]\n",
				__func__, status);
			return 0;
		} else
			return -1;
	}

	if (status == Status_S)
		return 0;
	else if (status == Status_A)
		return 0;
	else if (status == Status_B)
		return 0;
	else if (status == Status_C)
		return 0;
	else {
		if (ddata->factory_mode) {
			dev_info(&pdev->dev,
				"%s: factory_mode. status =[%d]\n",
				__func__, status);
			return 0;
		} else
			return -1;
	}
}

static void bezel_work_func(struct work_struct *work)
{
	struct bezel_ddata *ddata =
		container_of(work, struct bezel_ddata, work);
	struct platform_device *pdev = ddata->pdev;
#ifdef CONFIG_DISPLAY_EARLY_DPMS
	struct display_early_dpms_nb_event event;
#endif
#ifdef CONFIG_KNOX_GEARPAY
	int ret;
#endif
	int value, direction;

	mutex_lock(&ddata->hall_lock);

	value = bezel_get_status(ddata);

	if (bezel_check_validation(ddata, value)) {
		dev_err(&pdev->dev, "%s:Invalid data. old:[%d], new:[%d]\n",
				__func__, ddata->last_value, value);
		mutex_unlock(&ddata->hall_lock);
		goto out;
	}

	direction = bezel_get_direction(ddata, value);

	ddata->last_value = value;
	if (value)
		ddata->last_status = value;

#if defined(ROTARY_BOOSTER)
	rotary_booster_turn_on();
#endif

	input_report_rel(ddata->input_dev, REL_WHEEL, direction);
	input_report_rel(ddata->input_dev, REL_X, (unsigned char)~value & REL_X_EVENT_MASK);
	input_sync(ddata->input_dev);

	mutex_unlock(&ddata->hall_lock);

#ifdef CONFIG_INPUT_ASSISTANT
	dev_dbg(&pdev->dev, "%s: v:[%d], d:[%d]\n", __func__, value, direction);
#else
	dev_info(&pdev->dev, "%s: v:[%d], d:[%d]\n", __func__, value, direction);
#endif

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
#ifdef CONFIG_KNOX_GEARPAY
	/*
	 * call exynos_smc_gearpay for every touch event.
	 * g_lock_layout_active is set in the ioctl handler once
	 * Privacy Lock sends an event that it is active and
	 * it has drawn the Lock Screen layout.
	 */
	if (g_lock_layout_active) {
		/* send smc only once */
		g_lock_layout_active = false;
		ret = exynos_smc_gearpay(TOUCH, true);
		if (ret)
			dev_err(&pdev->dev,
				"%s:Bezel Event SMC failure %u\n",
				__func__, ret);
		else
			dev_dbg(&pdev->dev,
				"%s:Bezel Event SMC success\n", __func__);
	}
#endif

out:
	return;
}

static irqreturn_t bezel_detect_handler(int irq, void *dev_id)
{
	struct bezel_ddata *ddata = dev_id;

	if (!ddata->probe_done)
		goto out;

	if (!ddata->open_state)
		goto out;

	schedule_work(&ddata->work);

out:
	return IRQ_HANDLED;
}

static ssize_t bezel_show_raw_value(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct bezel_ddata *ddata = dev_get_drvdata(dev);
	struct platform_device *pdev = ddata->pdev;
	int hall_status;

	hall_status = bezel_get_status(ddata);

	hall_status = (unsigned char)~hall_status & 0x07;

	dev_info(&pdev->dev, "%s: hall_status=[%d]\n", __func__, hall_status);

	return sprintf(buf,"0x%1x\n", hall_status);
}

#ifdef CONFIG_TIZEN_SEC_KERNEL_ENG
static ssize_t bezel_show_factory_mode(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct bezel_ddata *ddata = dev_get_drvdata(dev);
	struct platform_device *pdev = ddata->pdev;

	dev_info(&pdev->dev, "%s: factory_mode=[%d]\n", __func__, ddata->factory_mode);

	return sprintf(buf,"%d\n", ddata->factory_mode);
}

static ssize_t bezel_store_factory_mode(struct device *dev, struct device_attribute
		*devattr, const char *buf, size_t count)
{
	struct bezel_ddata *ddata = dev_get_drvdata(dev);
	struct platform_device *pdev = ddata->pdev;
	int factory_mode;

	sscanf(buf, "%d", &factory_mode);

	ddata->factory_mode = !!factory_mode;

	dev_info(&pdev->dev, "%s: factory_mode=[%d]\n", __func__, ddata->factory_mode);

	return count;
}
#endif

static struct device_attribute dev_attr_show_hall_status =
		__ATTR(raw_value, S_IRUGO,bezel_show_raw_value, NULL);
#ifdef CONFIG_TIZEN_SEC_KERNEL_ENG
static struct device_attribute dev_attr_show_hall_debug =
		__ATTR(factory_mode, S_IRUGO | S_IWUSR | S_IWGRP,
		bezel_show_factory_mode, bezel_store_factory_mode);
#endif

static struct attribute *hall_detent_attributes[] = {
	&dev_attr_show_hall_status.attr,
#ifdef CONFIG_TIZEN_SEC_KERNEL_ENG
	&dev_attr_show_hall_debug.attr,
#endif
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

static int bezel_get_sleep_monitor_cb(void* priv, unsigned int *raw_val, int check_level, int caller_type);

static struct sleep_monitor_ops  bezel_sleep_monitor_ops = {
	 .read_cb_func =  bezel_get_sleep_monitor_cb,
};

static int bezel_get_sleep_monitor_cb(void* priv, unsigned int *raw_val, int check_level, int caller_type)
{
	struct bezel_ddata *ddata = priv;
	struct platform_device *pdev = ddata->pdev;
	int state = DEVICE_UNKNOWN;
	int pretty = 0;
	*raw_val = -1;

	if ((check_level == SLEEP_MONITOR_CHECK_SOFT) ||\
	    (check_level == SLEEP_MONITOR_CHECK_HARD)){
		if (ddata->open_state)
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

	dev_dbg(&pdev->dev, "%s: raw_val[0x%08x], check_level[%d], state[%d], pretty[%d]\n",
		__func__, *raw_val, check_level, state, pretty);

	return pretty;
}
#endif

static int bezel_sysfs_init(struct bezel_ddata *ddata)
{
	struct platform_device *pdev = ddata->pdev;
	int ret;

#ifdef CONFIG_SEC_SYSFS
	ddata->rotary_dev = sec_device_create(ddata, BEZEL_NAME);
	if (IS_ERR(ddata->rotary_dev)) {
		dev_err(&pdev->dev, "Failed to create sec_device_create\n");
		return -1;
	}
#else
	if (sec_class) {
		ddata->rotary_dev = device_create(sec_class, NULL, 0, NULL, BEZEL_NAME);
		if (IS_ERR(ddata->rotary_dev)) {
			dev_err(&pdev->dev, "%s:Unable to create rotary device.\n",  __func__);
			return -1;
		}
	} else {
		dev_err(&pdev->dev, "%s: sec_class is NULL\n", __func__);
		return -1;
	}
#endif

	dev_set_drvdata(ddata->rotary_dev, ddata);
	ret = sysfs_create_group(&ddata->rotary_dev->kobj, &hall_detent_attr_group);
	if (ret) {
		dev_err(&pdev->dev, "%s: Failed to create sysfs group\n", __func__);
#ifdef CONFIG_SEC_SYSFS
		sec_device_destroy(ddata->rotary_dev->devt);
#else
		device_destroy(sec_class, ddata->rotary_dev->devt);
#endif
		return -1;
	}

	return 0;
}

static int bezel_gpio_init(struct bezel_ddata *ddata)
{
	struct platform_device *pdev = ddata->pdev;
	struct bezel_pdata *pdata = ddata->pdata;
	unsigned long isr_flags;
	int ret;

	ddata->gpio_a = pdata->gpio_a;
	ddata->gpio_b = pdata->gpio_b;
	ddata->gpio_c = pdata->gpio_c;

	ret = gpio_request(ddata->gpio_a, "bezel_a");
	if (ret) {
		dev_err(&pdev->dev, "%s:"\
			" unable to request bezel_a [%d]\n",\
			__func__, ddata->gpio_a);
		goto err_gpio_request;
	}

	ret = gpio_direction_input(ddata->gpio_a);
	if (ret) {
		dev_err(&pdev->dev,
			"%s: unable to request input pin [%d]\n",
			__func__, ddata->gpio_a);
		goto err_gpio_direction_a;
	}

	ddata->hall_a_irq = gpio_to_irq(ddata->gpio_a);

	ret = gpio_request(ddata->gpio_b, "bezel_b");
	if (ret) {
		dev_err(&pdev->dev, "%s:"\
			" unable to request bezel_b [%d]\n",\
			__func__, ddata->gpio_b);
		goto err_gpio_direction_a;
	}

	ret = gpio_direction_input(ddata->gpio_b);
	if (ret) {
		dev_err(&pdev->dev,
			"%s: unable to request input pin [%d]\n",
			__func__, ddata->gpio_b);
		goto err_gpio_direction_b;
	}
	ddata->hall_b_irq = gpio_to_irq(ddata->gpio_b);

	ret = gpio_request(ddata->gpio_c, "bezel_c");
	if (ret) {
		dev_err(&pdev->dev, "%s:"\
			" unable to request bezel_c [%d]\n",\
			__func__, ddata->gpio_c);
		goto err_gpio_direction_b;
	}

	ret = gpio_direction_input(ddata->gpio_c);
	if (ret) {
		dev_err(&pdev->dev,
			"%s: unable to request input pin [%d]\n",
			__func__, ddata->gpio_c);
		goto err_gpio_direction_c;
	}
	ddata->hall_c_irq = gpio_to_irq(ddata->gpio_c);

	ddata->last_status = bezel_get_status(ddata);

	isr_flags = IRQF_TRIGGER_RISING|IRQF_TRIGGER_FALLING | IRQF_ONESHOT;

	ret = request_threaded_irq(ddata->hall_a_irq , NULL,
		bezel_detect_handler, isr_flags, "hall_a_status", ddata);
	if (ret < 0) {
		dev_err(&pdev->dev, "%s: failed to request hall_a_irq. irq[%d] gpio[%d]\n",
				__func__, ddata->hall_a_irq , ddata->gpio_a);
		goto err_threaded_irq;
	}

	ret = request_threaded_irq(ddata->hall_b_irq , NULL,
		bezel_detect_handler, isr_flags, "hall_b_status", ddata);
	if (ret < 0) {
		dev_err(&pdev->dev, "%s: failed to request hall_b_irq. irq[%d] gpio[%d]\n",
				__func__, ddata->hall_b_irq , ddata->gpio_b);
		goto err_threaded_irq;
	}

	ret = request_threaded_irq(ddata->hall_c_irq , NULL,
		bezel_detect_handler, isr_flags, "hall_c_status", ddata);
	if (ret < 0) {
		dev_err(&pdev->dev, "%s: failed to request hall_c_irq. irq[%d] gpio[%d]\n",
				__func__, ddata->hall_c_irq , ddata->gpio_c);
		goto err_threaded_irq;
	}

	ret = enable_irq_wake(ddata->hall_a_irq);
	if (ret < 0) {
		dev_err(&pdev->dev, "%s: failed to enable irq_wake hall_a_irq\n", __func__);
		goto err_threaded_irq;
	}

	ret = enable_irq_wake(ddata->hall_b_irq);
	if (ret < 0) {
		dev_err(&pdev->dev, "%s: failed to enable irq_wake hall_b_irq\n", __func__);
		goto err_threaded_irq;
	}

	ret = enable_irq_wake(ddata->hall_c_irq);
	if (ret < 0) {
		dev_err(&pdev->dev, "%s: failed to enable irq_wake hall_c_irq\n", __func__);
		goto err_threaded_irq;
	}

	ddata->pinctrl = devm_pinctrl_get(&pdev->dev);
	if (IS_ERR(ddata->pinctrl)) {
		dev_err(&pdev->dev, "%s: failed to get %s pinctrl\n", __func__, BEZEL_NAME);
		ddata->pinctrl = NULL;
	} else {
#ifdef CONFIG_TIZEN_SEC_KERNEL_ENG
		ddata->gpio_config = pinctrl_lookup_state(ddata->pinctrl, "eng");
		if (IS_ERR(ddata->gpio_config)) {
			dev_err(&pdev->dev, "%s: failed to get gpio_eng pin state\n", __func__);
			ddata->gpio_config = NULL;
		} else
			dev_err(&pdev->dev, "%s: success to get gpio_eng pin state\n", __func__);
#else
		ddata->gpio_config = pinctrl_lookup_state(ddata->pinctrl, "usr");
		if (IS_ERR(ddata->gpio_config)) {
			dev_err(&pdev->dev, "%s: failed to get gpio_usr pin state\n", __func__);
			ddata->gpio_config = NULL;
		} else
			dev_err(&pdev->dev, "%s: success to get gpio_usr pin state\n", __func__);
#endif
	}

	if (ddata->pinctrl && ddata->gpio_config) {
		if (pinctrl_select_state(ddata->pinctrl, ddata->gpio_config))
			dev_err(&pdev->dev, "%s: failed to turn on gpio_config\n", __func__);
	} else
		dev_err(&pdev->dev, "%s: pinctrl or gpio_config is NULL\n", __func__);

	return 0;

err_threaded_irq:
err_gpio_direction_c:
	gpio_free(ddata->gpio_c);
err_gpio_direction_b:
	gpio_free(ddata->gpio_b);
err_gpio_direction_a:
	gpio_free(ddata->gpio_a);
err_gpio_request:

	return -1;
}

static int bezel_setup_input_dev(struct bezel_ddata *ddata)
{
	struct platform_device *pdev = ddata->pdev;
	int ret;

	ddata->input_dev = input_allocate_device();
	if (!ddata->input_dev) {
		dev_err(&pdev->dev, "%s: Failed to allocate input device.\n", __func__);
		return -EPERM;
	}

	__set_bit(EV_REL, ddata->input_dev->evbit);
	__set_bit(EV_KEY, ddata->input_dev->evbit);
	__set_bit(REL_X, ddata->input_dev->relbit);
	__set_bit(REL_Y, ddata->input_dev->relbit);
	__set_bit(BTN_LEFT, ddata->input_dev->keybit);
	__set_bit(REL_WHEEL, ddata->input_dev->relbit);

	input_set_capability(ddata->input_dev, EV_REL, REL_X);
	input_set_capability(ddata->input_dev, EV_REL, REL_Y);
	input_set_capability(ddata->input_dev, EV_REL, REL_WHEEL);

	ddata->input_dev->name = BEZEL_NAME;
	ddata->input_dev->id.bustype = BUS_VIRTUAL;
	ddata->input_dev->dev.parent = &pdev->dev;
	ddata->input_dev->phys = BEZEL_NAME;
	ddata->input_dev->id.vendor = 0x0001;
	ddata->input_dev->id.product = 0x0001;
	ddata->input_dev->id.version = 0x0100;
	ddata->input_dev->open = bezel_open;
	ddata->input_dev->close = bezel_close;

	ret = input_register_device(ddata->input_dev);
	if (ret) {
		dev_err(&pdev->dev, "%s:Unable to register %s input device\n",\
			 __func__, ddata->input_dev->name);
		return -EPERM;
	}

	return 0;
}

static int bezel_regulator_init(struct bezel_ddata *ddata)
{
	struct bezel_pdata *pdata = ddata->pdata;
	struct platform_device *pdev = ddata->pdev;
	int rc;

	if (!pdata->ldo_name)
		return 0;

	ddata->power = regulator_get(NULL, pdata->ldo_name);
	if (IS_ERR(ddata->power)) {
		dev_err(&pdev->dev, "%s: could not get regulator %s, rc = %ld\n",
			__func__, pdata->ldo_name, PTR_ERR(ddata->power));
		return -EINVAL;
	}

	rc = regulator_set_voltage(ddata->power, 1800000, 1800000);
	if (rc) {
		dev_err(&pdev->dev, "%s: could not set regulator %s, rc = %d\n",
			__func__, pdata->ldo_name, rc);
		return -EINVAL;
	}

	return 0;
}

static int bezel_power_on(struct bezel_ddata *ddata)
{
	struct bezel_pdata *pdata = ddata->pdata;
	struct platform_device *pdev = ddata->pdev;
	int ret;

	if (!pdata->ldo_name)
		return 0;

	ret = regulator_enable(ddata->power);
	if (ret) {
		dev_err(&pdev->dev, "%s: %s enable failed (%d)\n",
			__func__, pdata->ldo_name, ret);
		return -EINVAL;
	}
	dev_info(&pdev->dev, "%s: %s power [%s]", __func__,
		pdata->ldo_name, regulator_is_enabled(ddata->power) ? "on":"off");

	return 0;
}

static int bezel_power_off(struct bezel_ddata *ddata)
{
	struct bezel_pdata *pdata = ddata->pdata;
	struct platform_device *pdev = ddata->pdev;
	int ret;

	if (!pdata->ldo_name)
		return 0;

	ret = regulator_disable(ddata->power);
	if (ret) {
		dev_err(&pdev->dev, "%s: %s disable failed (%d)\n",
			__func__, pdata->ldo_name, ret);
		return -EINVAL;
	}
	dev_info(&pdev->dev, "%s: %s power [%s]", __func__,
		pdata->ldo_name, regulator_is_enabled(ddata->power) ? "on":"off");

	return 0;

}

static int bezel_open(struct input_dev *input)
{
	struct device *dev = input->dev.parent;
	struct bezel_ddata *ddata = dev_get_drvdata(dev);

	if (!ddata->probe_done)
		return 0;

	bezel_power_on(ddata);

	enable_irq(ddata->hall_a_irq);
	enable_irq(ddata->hall_b_irq);
	enable_irq(ddata->hall_c_irq);
	enable_irq_wake(ddata->hall_a_irq);
	enable_irq_wake(ddata->hall_b_irq);
	enable_irq_wake(ddata->hall_c_irq);

	ddata->last_status = bezel_get_status(ddata);
	ddata->open_state = true;

	dev_info(dev, "%s\n", __func__);

	return 0;
}

static void bezel_close(struct input_dev *input)
{
	struct device *dev = input->dev.parent;
	struct bezel_ddata *ddata = dev_get_drvdata(dev);

	if (!ddata->probe_done)
		return;

	ddata->open_state = false;

	disable_irq_wake(ddata->hall_a_irq);
	disable_irq_wake(ddata->hall_b_irq);
	disable_irq_wake(ddata->hall_c_irq);
	disable_irq(ddata->hall_a_irq);
	disable_irq(ddata->hall_b_irq);
	disable_irq(ddata->hall_c_irq);

	bezel_power_off(ddata);

	dev_info(dev, "%s\n", __func__);
}

static int bezel_resume(struct device *dev)
{
	struct bezel_ddata *ddata = dev_get_drvdata(dev);

	dev_dbg(dev, "%s\n", __func__);

	ddata->resume_state = true;

	return 0;
}

static int bezel_suspend(struct device *dev)
{
	struct bezel_ddata *ddata = dev_get_drvdata(dev);

	dev_dbg(dev, "%s\n", __func__);

	ddata->resume_state = false;

	return 0;
}



#ifdef CONFIG_OF
static int bezel_parse_dt(struct device *dev,
			struct bezel_pdata *pdata)
{
	struct device_node *np = dev->of_node;
	int ret;

	/* gpio info */
	pdata->gpio_a = of_get_named_gpio(np, "hall_sensor,gpio_a", 0);
	if (!gpio_is_valid(pdata->gpio_a)) {
		pdata->gpio_a = -1;
		dev_err(dev, "failed to get gpio_a property\n");
	} else
		dev_dbg(dev, "%s: pdata->gpio_a: %d.\n",
			__func__, pdata->gpio_a);

	pdata->gpio_b = of_get_named_gpio(np, "hall_sensor,gpio_b", 0);
	if (!gpio_is_valid(pdata->gpio_a)) {
		pdata->gpio_b = -1;
		dev_err(dev, "failed to get gpio_b property\n");
	} else
		dev_dbg(dev, "%s: pdata->gpio_b: %d.\n",
			__func__, pdata->gpio_b);

	pdata->gpio_c = of_get_named_gpio(np, "hall_sensor,gpio_c", 0);
	if (!gpio_is_valid(pdata->gpio_a)) {
		pdata->gpio_c = -1;
		dev_err(dev, "failed to get gpio_c property\n");
	} else
		dev_dbg(dev, "%s: pdata->gpio_c: %d.\n",
			__func__, pdata->gpio_c);

	ret = of_property_read_string(np, "regulator_name", &pdata->ldo_name);
	if (ret < 0)
		dev_err(dev, "%s: failed to get  TSP ldo name %d\n", __func__, ret);

	return 0;
}
#else
static int bezel_parse_dt(struct device *dev,
			struct bezel_pdata *pdata)
{
	return -ENODEV;
}
#endif

static int bezel_probe(struct platform_device *pdev)
{
	struct bezel_pdata *pdata;
	struct bezel_ddata *ddata;
	int ret = 0;

	if (pdev->dev.of_node) {
		pdata = kzalloc(sizeof(struct bezel_pdata), GFP_KERNEL);
		if (!pdata) {
			dev_err(&pdev->dev, "%s: Failed to allocate memory\n", __func__);
			return -ENOMEM;
		}

		ret = bezel_parse_dt(&pdev->dev, pdata);
		if (ret) {
			dev_err(&pdev->dev, "%s: Error bezel_parse_dt\n", __func__);
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

	ddata->pdata = pdata;
	ddata->pdev = pdev;
	platform_set_drvdata(ddata->pdev, ddata);
	mutex_init(&ddata->hall_lock);
	device_init_wakeup(&pdev->dev, true);
	INIT_WORK(&ddata->work, bezel_work_func);

	ret = bezel_setup_input_dev(ddata);
	if (ret) {
		dev_err(&pdev->dev, "%s: Failed input device setup.\n", __func__);
		goto err_input_allocate;
	}

	ret = bezel_regulator_init(ddata);
	if (ret) {
		dev_err(&pdev->dev, "%s: unable to init regulator [%d]\n",
			__func__, ret);
		goto err_regulator_init;
	}

	ret = bezel_power_on(ddata);
	if (ret) {
		dev_err(&pdev->dev, "%s: unable to power on [%d]\n",
			__func__, ret);
		goto err_regulator_init;
	}

	ret = bezel_gpio_init(ddata);
	if (ret) {
		dev_err(&pdev->dev, "%s: Failed gpio init.\n", __func__);
		goto err_gpio_request;
	}

	ret = bezel_sysfs_init(ddata);
	if (ret) {
		dev_err(&pdev->dev, "%s:Failed init sysfs.\n", __func__);
		goto err_create_device;
	}

#ifdef CONFIG_DISPLAY_EARLY_DPMS
	device_set_early_complete(&pdev->dev, EARLY_COMP_SLAVE);
#endif
#ifdef CONFIG_SLEEP_MONITOR
	sleep_monitor_register_ops(ddata, &bezel_sleep_monitor_ops,
			SLEEP_MONITOR_ROTARY);
#endif

	ddata->open_state = true;
	ddata->resume_state = true;
	ddata->probe_done = true;
	ddata->last_value = bezel_get_status(ddata);
	dev_info(&pdev->dev, "%s Init value:[%d]\n", __func__, ddata->last_value);
	dev_info(&pdev->dev, "%s done successfully\n", __func__);

	return 0;

err_create_device:
	gpio_free(ddata->gpio_c);
	gpio_free(ddata->gpio_b);
	gpio_free(ddata->gpio_a);
err_gpio_request:
	bezel_power_off(ddata);
err_regulator_init:
	input_unregister_device(ddata->input_dev);
	input_free_device(ddata->input_dev);
	platform_set_drvdata(pdev, NULL);
err_input_allocate:
	mutex_destroy(&ddata->hall_lock);
	kfree(ddata);
err_kzalloc:
	dev_err(&pdev->dev, "%s failed\n", __func__);
	return ret;
}

static int bezel_remove(struct platform_device *pdev)
{
	struct bezel_ddata *ddata = platform_get_drvdata(pdev);

#ifdef CONFIG_SLEEP_MONITOR
	sleep_monitor_unregister_ops(SLEEP_MONITOR_ROTARY);
#endif
#ifdef CONFIG_SEC_SYSFS
	sec_device_destroy(ddata->rotary_dev->devt);
#else
	device_destroy(sec_class, ddata->rotary_dev->devt);
#endif
	input_unregister_device(ddata->input_dev);
	gpio_free(ddata->gpio_c);
	gpio_free(ddata->gpio_b);
	gpio_free(ddata->gpio_a);
	bezel_power_off(ddata);
	input_free_device(ddata->input_dev);
	platform_set_drvdata(pdev, NULL);
	kfree(ddata);
	return 0;
}

static const struct dev_pm_ops hall_pm_ops = {
	.suspend = bezel_suspend,
	.resume = bezel_resume,
};

static struct of_device_id bezel_of_match[] = {
	{ .compatible = BEZEL_NAME, },
	{ },
};
MODULE_DEVICE_TABLE(of, bezel_of_match);

static struct platform_driver bezel_driver = {
	.probe		= bezel_probe,
	.remove		= bezel_remove,
	.driver		= {
		.name	= BEZEL_NAME,
		.owner	= THIS_MODULE,
		.pm	= &hall_pm_ops,
		.of_match_table = bezel_of_match
	},
};

static int __init bezel_init(void)
{
	return platform_driver_register(&bezel_driver);
}

static void __exit bezel_exit(void)
{
	platform_driver_unregister(&bezel_driver);
}

late_initcall(bezel_init);
module_exit(bezel_exit);

/* Module information */
MODULE_AUTHOR("Sang-Min Lee <lsmin.lee@samsung.com>");
MODULE_DESCRIPTION("tizen bezel driver");
MODULE_LICENSE("GPL");
