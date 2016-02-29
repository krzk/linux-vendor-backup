/*
 * Copyright (C) 2012 Samsung Electronics Co. Ltd. All Rights Reserved.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#ifdef CONFIG_MACH_WC1
#include <linux/i2c.h>
#include <linux/gpio.h>
#include <plat/gpio-cfg.h>
#include <plat/devs.h>
#include <linux/mfd/max77803.h>
#include <linux/mfd/max77803-private.h>
#include <mach/gpio-exynos.h>
#include <linux/isa1000a_vibrator.h>

#include "board-universal3250.h"

void __init exynos3_universal3250_vibrator_init(void)
{
	int ret;

	ret = gpio_request(GPIO_VIBTONE_PWM, "motor_pwm");
	if (ret) {
		printk(KERN_ERR "%s gpio_request error %d\n",
				__func__, ret);
		goto init_fail;
	}
	ret = s3c_gpio_cfgpin(GPIO_VIBTONE_PWM, S3C_GPIO_SFN(0x2));
	if (ret) {
		printk(KERN_ERR "%s s3c_gpio_cfgpin error %d\n",
				__func__, ret);
		goto init_fail;
	}

	ret = platform_device_register(&s3c_device_timer[0]);

	if (ret)
		printk(KERN_ERR "%s failed platform_device_register with error %d\n",
				__func__, ret);
init_fail:
	return;
}

#else
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/gpio_keys.h>
#include <linux/input.h>
#include <linux/i2c.h>
#include <linux/export.h>
#include <linux/interrupt.h>
#include <linux/regulator/consumer.h>

#include <plat/gpio-cfg.h>
#include <plat/devs.h>
#include <plat/iic.h>

#include <mach/irqs.h>
#include <mach/hs-iic.h>
#include <mach/regs-gpio.h>
#include <linux/dc_motor.h>

#include "board-universal3250.h"

#define ERROR_REGULATOR_NAME "error"

static int prev_val;
extern unsigned int system_rev;

enum {
        B2_BT_EMUL = 0x0,
        B2_BT_REV00 = 0x1,
        B2_NEO_REV00 = 0x8,
        B2_NEO_REV01 = 0x9,
};

static char * get_regulator_name(void) {
	switch (system_rev) {
#if 1
	case B2_BT_EMUL: return "vdd_ldo2";
#else
	case B2_BT_EMUL: return "regulator-haptic";
#endif
	case B2_BT_REV00:
	case B2_NEO_REV00:
	case B2_NEO_REV01:
#if 1
	default: return "vdd_ldo2";
#else
	default: return "vdd_ldo1";
#endif
        }
        return "error";
}

void dc_motor_power(bool on)
{
	struct regulator *regulator;
	static bool motor_enabled;
	char* regulator_name = get_regulator_name();

	if (motor_enabled == on)
		return ;

	if(!strcmp(ERROR_REGULATOR_NAME , regulator_name)){
		pr_info("%s : regulator_name error\n", __func__);
		return;
	}
	/* if the nforce is zero, do not enable the vdd */
	if (on && !prev_val)
		return ;

	regulator = regulator_get(NULL, regulator_name);
	motor_enabled = on;

#if defined(DC_MOTOR_LOG)
	pr_info("[VIB] %s\n", on ? "ON" : "OFF");
#endif

	if (on)
		regulator_enable(regulator);
	else {
		if (regulator_is_enabled(regulator))
			regulator_disable(regulator);
		else
			regulator_force_disable(regulator);
		prev_val = 0;
	}
	regulator_put(regulator);
}

void dc_motor_voltage(int val)
{
	struct regulator *regulator;
	char* regulator_name;

	regulator_name=get_regulator_name();
	if(!strcmp(ERROR_REGULATOR_NAME , regulator_name)){
		pr_info("%s : regulator_name error\n", __func__);
		return;
	}

	if (!val) {
		dc_motor_power(val);
		return ;
	} else if (DC_MOTOR_VOL_MAX < val) {
		val = DC_MOTOR_VOL_MAX;
	} else if (DC_MOTOR_VOL_MIN > val)
		val = DC_MOTOR_VOL_MIN;

	if (prev_val == val)
		return ;
	else
		prev_val = val;

	regulator = regulator_get(NULL, regulator_name);


	if (!IS_ERR(regulator)) {
	} else {
		pr_info("%s, fail!!!\n", __func__);
		regulator_put(regulator);
		return ;
	}


#if defined(DC_MOTOR_LOG)
	pr_info("[VIB] %s : %d\n", __func__, val);
#endif

	val *= 100000;

	if (val) {
		if (regulator_set_voltage(regulator, val, val))
			pr_err("[VIB] failed to set voltage: %duV\n", val);
	}

	regulator_put(regulator);
	dc_motor_power(!!val);
}

static struct dc_motor_platform_data dc_motor_pdata = {
	.power = dc_motor_power,
	.set_voltage = dc_motor_voltage,
	.max_timeout = 10000,
};

static struct platform_device dc_motor_device = {
	.name = "dc_motor",
	.id = -1,
	.dev = {
		.platform_data = &dc_motor_pdata,
	},
};

void __init exynos3_universal3250_vibrator_init(void)
{
	pr_info("[VIB] %s", __func__);

	platform_device_register(&dc_motor_device);
}
#endif
