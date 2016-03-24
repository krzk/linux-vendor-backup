/* drivers/motor/isa1000a_vibrator.c
* Copyright (C) 2014 Samsung Electronics Co. Ltd. All Rights Reserved.
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
#include <linux/timed_output.h>
#include <linux/hrtimer.h>
#include <linux/pwm.h>
#include <linux/platform_device.h>
#include <linux/err.h>
#include <linux/slab.h>
#include <linux/clk.h>
#include <linux/regulator/consumer.h>
#include <linux/isa1000a_vibrator.h>
#include <linux/gpio.h>
#include <linux/of_platform.h>
#include <linux/of_gpio.h>
#include <linux/of_device.h>
#include <linux/of_irq.h>
#include <linux/of.h>
#include <linux/delay.h>
#ifdef CONFIG_MACH_WC1
#include <plat/gpio-cfg.h>
#include <mach/regs-gpio.h>
#include <mach/gpio.h>
#endif

#ifdef CONFIG_MACH_WC1
struct isa1000a_vibrator_data *g_hap_data;

extern struct class *sec_class;
struct device *motor_device;
#endif

struct isa1000a_vibrator_data {
	struct device *dev;
	struct isa1000a_vibrator_platform_data *pdata;
	struct pwm_device *pwm;
	struct regulator *regulator;
	struct timed_output_dev tout_dev;
	struct hrtimer timer;
	unsigned int timeout;
	struct work_struct work;
	spinlock_t lock;
	bool running;
	bool resumed;
};

static int haptic_get_time(struct timed_output_dev *tout_dev)
{
	struct isa1000a_vibrator_data *hap_data
		= container_of(tout_dev, struct  isa1000a_vibrator_data, tout_dev);
	struct hrtimer *timer = &hap_data->timer;
	if (hrtimer_active(timer)) {
		ktime_t remain = hrtimer_get_remaining(timer);
		struct timeval t = ktime_to_timeval(remain);
		return t.tv_sec * 1000 + t.tv_usec / 1000;
	} else
		return 0;
}

static void haptic_enable(struct timed_output_dev *tout_dev, int value)
{
	struct isa1000a_vibrator_data *hap_data
		= container_of(tout_dev, struct isa1000a_vibrator_data, tout_dev);

	struct hrtimer *timer = &hap_data->timer;
	unsigned long flags;

	cancel_work_sync(&hap_data->work);
	hrtimer_cancel(timer);
	hap_data->timeout = value;
	schedule_work(&hap_data->work);
	spin_lock_irqsave(&hap_data->lock, flags);
	if (value > 0) {
		pr_debug("%s value %d\n", __func__, value);
		if (value > hap_data->pdata->max_timeout)
			value = hap_data->pdata->max_timeout;
		hrtimer_start(timer, ns_to_ktime((u64)value * NSEC_PER_MSEC),
			HRTIMER_MODE_REL);
	}
	spin_unlock_irqrestore(&hap_data->lock, flags);
}

static enum hrtimer_restart haptic_timer_func(struct hrtimer *timer)
{
	struct isa1000a_vibrator_data *hap_data
		= container_of(timer, struct isa1000a_vibrator_data, timer);

	hap_data->timeout = 0;

	schedule_work(&hap_data->work);
	return HRTIMER_NORESTART;
}

static int vibetonz_clk_on(struct device *dev, bool en)
{
	struct clk *vibetonz_clk = NULL;

#if defined(CONFIG_OF)
	struct device_node *np;
	np = of_find_node_by_name(NULL, "pwm");
	if (np == NULL) {
		printk("%s : pwm error to get dt node\n", __func__);
		return -EINVAL;
	}
	vibetonz_clk = of_clk_get_by_name(np, "gate_timers");
	if (!vibetonz_clk) {
		pr_info("%s fail to get the vibetonz_clk\n", __func__);
		return -EINVAL;
	}
#else
	vibetonz_clk = clk_get(dev, "timers");
#endif
	pr_debug("[VIB] DEV NAME %s %lu\n",
		 dev_name(dev), clk_get_rate(vibetonz_clk));

	if (IS_ERR(vibetonz_clk)) {
		pr_err("[VIB] failed to get clock for the motor\n");
		goto err_clk_get;
	}

	if (en)
		clk_enable(vibetonz_clk);
	else
		clk_disable(vibetonz_clk);

	clk_put(vibetonz_clk);
	return 0;

err_clk_get:
	clk_put(vibetonz_clk);
	return -EINVAL;
}

static void haptic_work(struct work_struct *work)
{
	struct isa1000a_vibrator_data *hap_data
		= container_of(work, struct isa1000a_vibrator_data, work);
	int ret;
	if (hap_data->timeout == 0) {
		if (!hap_data->running)
			return;
		hap_data->running = false;
		regulator_disable(hap_data->regulator);
		pwm_disable(hap_data->pwm);
	} else {
		if (hap_data->running)
			return;
		pwm_config(hap_data->pwm, hap_data->pdata->duty,
			   hap_data->pdata->period);
		pwm_enable(hap_data->pwm);
		ret = regulator_enable(hap_data->regulator);
		if (ret)
			goto error_reg_enable;
		hap_data->running = true;
	}
	return;
error_reg_enable:
	printk(KERN_ERR "%s: Failed to enable vdd.\n", __func__);
}

#ifdef CONFIG_VIBETONZ
void vibtonz_en(bool en)
{
	int ret;

	pr_info("[VIB] %s %s\n", __func__, en ? "on" : "off");

	if (g_hap_data == NULL) {
		printk(KERN_ERR "[VIB] the motor is not ready!!!");
		return ;
	}

	if (en) {
#ifdef CONFIG_MACH_WC1
		regulator_set_voltage(g_hap_data->regulator, MOTOR_VDD, MOTOR_VDD);
		regulator_enable(g_hap_data->regulator);

		s3c_gpio_cfgpin(GPIO_MOTOR_EN, S3C_GPIO_OUTPUT);
		s3c_gpio_setpull(GPIO_MOTOR_EN, S3C_GPIO_PULL_NONE);
#ifndef CONFIG_MACH_WC1
		gpio_direction_output(GPIO_MOTOR_EN, 0);
#endif
		/* msleep(1); */
		gpio_set_value(GPIO_MOTOR_EN, 1);
		/* vibtonz_pwm(127); */
#endif
		if (g_hap_data->running)
			return;
		/* must set pwm after resume. this may be workaround.. */
		if (g_hap_data->resumed) {
			pwm_config(g_hap_data->pwm, g_hap_data->pdata->period/2, g_hap_data->pdata->period);
			g_hap_data->resumed = false;
		}

		pwm_enable(g_hap_data->pwm);
		ret = regulator_enable(g_hap_data->regulator);
		if (ret)
			goto error_reg_enable;
		g_hap_data->running = true;
		} else {
#ifdef CONFIG_MACH_WC1
		regulator_set_voltage(g_hap_data->regulator, MOTOR_VDD, MOTOR_VDD);
		regulator_disable(g_hap_data->regulator);

		s3c_gpio_cfgpin(GPIO_MOTOR_EN, S3C_GPIO_OUTPUT);
		s3c_gpio_setpull(GPIO_MOTOR_EN, S3C_GPIO_PULL_NONE);
#ifndef CONFIG_MACH_WC1
		gpio_direction_output(GPIO_MOTOR_EN, 0);
#endif
		/* msleep(1); */
		gpio_set_value(GPIO_MOTOR_EN, 0);
		/* vibtonz_pwm(127); */
#endif
		if (!g_hap_data->running)
			return;
		regulator_disable(g_hap_data->regulator);
		pwm_disable(g_hap_data->pwm);
		g_hap_data->running = false;
	}
	return;
error_reg_enable:
	printk(KERN_ERR "%s: Failed to enable vdd.\n", __func__);
}
EXPORT_SYMBOL(vibtonz_en);

void vibtonz_pwm(int nForce)
{
	/* add to avoid the glitch issue */
	static int prev_duty;
	int pwm_period = 0, pwm_duty = 0;

	if (g_hap_data == NULL) {
		printk(KERN_ERR "[VIB] the motor is not ready!!!");
		return ;
	}
	pwm_period = g_hap_data->pdata->period;
	pwm_duty = pwm_period / 2 + ((pwm_period / 2 - 2) * nForce) / 127;

	if (pwm_duty > g_hap_data->pdata->duty)
		pwm_duty = g_hap_data->pdata->duty;
	else if (pwm_period - pwm_duty > g_hap_data->pdata->duty)
		pwm_duty = pwm_period - g_hap_data->pdata->duty;

	/* add to avoid the glitch issue */
	if (prev_duty != pwm_duty) {
		prev_duty = pwm_duty;
		pwm_config(g_hap_data->pwm, pwm_duty, pwm_period);
	}
}
EXPORT_SYMBOL(vibtonz_pwm);
#endif

#ifdef CONFIG_MACH_WC1
static ssize_t motor_control_show_motor_on(struct device *dev, struct device_attribute *attr, char *buf)
{
	vibtonz_en(1);
	vibtonz_pwm(127);
	return 0;
}

static ssize_t motor_control_show_motor_off(struct device *dev, struct device_attribute *attr, char *buf)
{
	vibtonz_pwm(0);
	vibtonz_en(0);
	return 0;
}
#endif

#ifdef CONFIG_MACH_WC1
static DEVICE_ATTR(motor_on, S_IRUGO, motor_control_show_motor_on, NULL);
static DEVICE_ATTR(motor_off, S_IRUGO, motor_control_show_motor_off, NULL);

static struct attribute *motor_control_attributes[] = {
	&dev_attr_motor_on.attr,
	&dev_attr_motor_off.attr,
	NULL
};
static const struct attribute_group motor_control_group = {
	.attrs = motor_control_attributes,
};
#endif

#if defined(CONFIG_OF)
static int of_isa1000a_vibrator_dt(struct isa1000a_vibrator_platform_data *pdata)
{
	struct device_node *np_haptic;
	int temp;
	const char *temp_str;

	pr_info("[VIB] ++ %s\n", __func__);

	np_haptic = of_find_node_by_path("/isa1000a-vibrator");
	if (np_haptic == NULL) {
		printk("%s : error to get dt node\n", __func__);
		return -EINVAL;
	}

	of_property_read_u32(np_haptic, "haptic,max_timeout", &temp);
	pdata->max_timeout = temp;

	of_property_read_u32(np_haptic, "haptic,duty", &temp);
	pdata->duty = temp;

	of_property_read_u32(np_haptic, "haptic,period", &temp);
	pdata->period = temp;

	of_property_read_u32(np_haptic, "haptic,pwm_id", &temp);
	pdata->pwm_id = temp;

	of_property_read_string(np_haptic, "haptic,regulator_name", &temp_str);
	pdata->regulator_name = (char *)temp_str;

/* debugging */
	printk("%s : max_timeout = %d\n", __func__, pdata->max_timeout);
	printk("%s : duty = %d\n", __func__, pdata->duty);
	printk("%s : period = %d\n", __func__, pdata->period);
	printk("%s : pwm_id = %d\n", __func__, pdata->pwm_id);
	printk("%s : regulator_name = %s\n", __func__, pdata->regulator_name);

	return 0;
}
#endif /* CONFIG_OF */

static int isa1000a_vibrator_probe(struct platform_device *pdev)
{
#ifdef CONFIG_MACH_WC1
	int ret;
	int error = 0;
#else
	int error = 0;
#endif

#if !defined(CONFIG_OF)
	struct isa1000a_vibrator_platform_data *isa1000a_pdata
		= dev_get_platdata(&pdev->dev);
#endif
	struct isa1000a_vibrator_data *hap_data;

	pr_info("[VIB] ++ %s\n", __func__);

	hap_data = kzalloc(sizeof(struct isa1000a_vibrator_data), GFP_KERNEL);
	if (!hap_data)
		return -ENOMEM;

#if defined(CONFIG_OF)
	hap_data->pdata = kzalloc(sizeof(struct isa1000a_vibrator_data), GFP_KERNEL);
	if (!hap_data->pdata) {
		kfree(hap_data);
		return -ENOMEM;
	}

	ret = of_isa1000a_vibrator_dt(hap_data->pdata);
	if (ret < 0) {
		pr_info("isa1000a_vibrator : %s not found haptic dt! ret[%d]\n",
				 __func__, ret);
		kfree(hap_data->pdata);
		kfree(hap_data);
		return -1;
	}
#else
	hap_data->pdata = isa1000a_pdata;
#ifdef CONFIG_MACH_WC1
#if 0
	if (pdata == NULL) {
#else
	if (hap_data == NULL) {
#endif
		pr_err("%s: no pdata\n", __func__);
		kfree(hap_data);
		return -ENODEV;
	}
#endif
#endif /* CONFIG_OF */
	platform_set_drvdata(pdev, hap_data);
	g_hap_data = hap_data;
	hap_data->dev = &pdev->dev;
	INIT_WORK(&(hap_data->work), haptic_work);
	spin_lock_init(&(hap_data->lock));
	hap_data->pwm = pwm_request(hap_data->pdata->pwm_id, "vibrator");
	if (IS_ERR(hap_data->pwm)) {
		pr_err("[VIB] Failed to request pwm\n");
		error = -EFAULT;
		goto err_pwm_request;
	}

	pwm_config(hap_data->pwm, hap_data->pdata->period / 2, hap_data->pdata->period);

	vibetonz_clk_on(&pdev->dev, true);

	hap_data->regulator
			= regulator_get(NULL, hap_data->pdata->regulator_name);

	if (IS_ERR(hap_data->regulator)) {
		pr_err("[VIB] Failed to get vmoter regulator.\n");
		error = -EFAULT;
		goto err_regulator_get;
	}
	/* hrtimer init */
	hrtimer_init(&hap_data->timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	hap_data->timer.function = haptic_timer_func;

	/* timed_output_dev init*/
	hap_data->tout_dev.name = "vibrator";
	hap_data->tout_dev.get_time = haptic_get_time;
	hap_data->tout_dev.enable = haptic_enable;

	hap_data->resumed = false;

	error = timed_output_dev_register(&hap_data->tout_dev);
	if (error < 0) {
		pr_err("[VIB] Failed to register timed_output : %d\n", error);
		error = -EFAULT;
		goto err_to_dev_reg;
	}

	if (IS_ERR(hap_data->pwm)) {
		pr_err("[VIB] Failed to request pwm\n");
		error = -EFAULT;
		goto err_pwm_request;
	}

#ifdef CONFIG_MACH_WC1
	/* create sysfs group */
	dev_set_drvdata(motor_device, hap_data);
	ret = sysfs_create_group(&motor_device->kobj, &motor_control_group);
	if (ret) {
		pr_err("%s: failed to create motor control attribute group\n", __func__);
		goto
			fail;
	}
#endif

	pr_info("[VIB] -- %s\n", __func__);
	return error;
err_to_dev_reg:
	regulator_put(hap_data->regulator);
err_regulator_get:
	pwm_free(hap_data->pwm);
err_pwm_request:
#if defined(CONFIG_OF)
	kfree(hap_data->pdata);
#endif
	kfree(hap_data);
	g_hap_data = NULL;
	return error;
#ifdef CONFIG_MACH_WC1
fail:
	return error;
#endif
}

static int __devexit isa1000a_vibrator_remove(struct platform_device *pdev)
{
	struct isa1000a_vibrator_data *data = platform_get_drvdata(pdev);

	timed_output_dev_unregister(&data->tout_dev);
	regulator_put(data->regulator);
	pwm_free(data->pwm);
	kfree(data->pdata);
	kfree(data);
	g_hap_data = NULL;
	return 0;
}

#if defined(CONFIG_OF)
static struct of_device_id haptic_dt_ids[] = {
	{ .compatible = "isa1000a-vibrator" },
	{ },
};
MODULE_DEVICE_TABLE(of, haptic_dt_ids);
#endif /* CONFIG_OF */

static int isa1000a_vibrator_suspend(struct platform_device *pdev,
			pm_message_t state)
{
	pr_debug("[VIB] %s\n", __func__);
	if (g_hap_data != NULL) {
		cancel_work_sync(&g_hap_data->work);
		hrtimer_cancel(&g_hap_data->timer);
	}
	vibetonz_clk_on(&pdev->dev, false);
	return 0;
}

static int isa1000a_vibrator_resume(struct platform_device *pdev)
{
	pr_debug("[VIB] %s\n", __func__);
	vibetonz_clk_on(&pdev->dev, true);
	g_hap_data->resumed = true;
	return 0;
}

static struct platform_driver isa1000a_vibrator_driver = {
	.probe		= isa1000a_vibrator_probe,
	.remove		= isa1000a_vibrator_remove,
	.suspend	= isa1000a_vibrator_suspend,
	.resume		= isa1000a_vibrator_resume,
	.driver = {
		.name	= "isa1000a-vibrator",
		.owner	= THIS_MODULE,
#if defined(CONFIG_OF)
		.of_match_table = haptic_dt_ids,
#endif /* CONFIG_OF */
	},
};

static int __init isa1000a_vibrator_init(void)
{
#ifdef CONFIG_MACH_WC1
	motor_device = device_create(sec_class, NULL, 0, NULL, "motor");
	if (IS_ERR(motor_device)) {
		pr_err("%s Failed to create device(motor)!\n", __func__);
		return -ENODEV;
	}
#endif
	/* pr_debug("[VIB] %s\n", __func__);	pr_info("[VIB] %s\n", __func__); */
	return platform_driver_register(&isa1000a_vibrator_driver);
}
module_init(isa1000a_vibrator_init);

static void __exit isa1000a_vibrator_exit(void)
{
	platform_driver_unregister(&isa1000a_vibrator_driver);
}
module_exit(isa1000a_vibrator_exit);

MODULE_AUTHOR("Samsung Electronics");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("ISA1000A motor driver");
