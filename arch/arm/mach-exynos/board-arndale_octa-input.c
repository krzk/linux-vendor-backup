/*
 * Copyright (c) 2013 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/gpio.h>
#include <linux/gpio_keys.h>
#include <linux/input.h>
#include <linux/i2c.h>
#if defined(CONFIG_TOUCHSCREEN_MXT540E)
#include <linux/i2c/mxt540e.h>
#include <linux/export.h>
#include <linux/interrupt.h>
#endif

#include <plat/gpio-cfg.h>
#include <plat/devs.h>
#include <plat/iic.h>

#include <mach/irqs.h>
#include <mach/hs-iic.h>
#include <mach/regs-gpio.h>

#include "board-arndale_octa.h"

#define GPIO_POWER_BUTTON	EXYNOS5420_GPX1(3)
#define GPIO_MENU_BUTTON	EXYNOS5420_GPX2(7)

static void arndale_octa_gpio_keys_config_setup(void)
{
	int irq;

	/* set to GND the row of the keypad */
	gpio_request_one(EXYNOS5420_GPG1(5), GPIOF_OUT_INIT_LOW, "GPG1");
	/* set pull up the column of the keypad */
	s3c_gpio_setpull(GPIO_MENU_BUTTON, S3C_GPIO_PULL_UP);

	irq = s5p_register_gpio_interrupt(GPIO_MENU_BUTTON);	/* MENU */
	if (IS_ERR_VALUE(irq)) {
		pr_err("%s: Failed to configure MENU GPIO\n", __func__);
		return;
	}
}

static struct gpio_keys_button arndale_octa_button[] = {
	{
		.code = KEY_POWER,
		.gpio = GPIO_POWER_BUTTON,
		.desc = "gpio-keys: KEY_POWER",
		.active_low = 1,
		.wakeup = 1,
	}, {
		.code = KEY_MENU,
		.gpio = GPIO_MENU_BUTTON,
		.desc = "gpio-keys: KEY_MENU",
		.active_low = 1,
	},
};

static struct gpio_keys_platform_data arndale_octa_gpiokeys_platform_data = {
	arndale_octa_button,
	ARRAY_SIZE(arndale_octa_button),
};


static struct platform_device arndale_octa_gpio_keys = {
	.name	= "gpio-keys",
	.dev	= {
		.platform_data = &arndale_octa_gpiokeys_platform_data,
	},
};

static struct i2c_board_info i2c_devs0[] __initdata = { 
#ifdef CONFIG_TOUCHSCREEN_GT9XX
	{
		I2C_BOARD_INFO("Goodix-TS", (0xBA >> 1)),
	},
#endif
};

static struct platform_device *arndale_octa_input_devices[] __initdata = {
	&s3c_device_i2c0,
	&arndale_octa_gpio_keys,
};

void __init exynos5_arndale_octa_input_init(void)
{
	arndale_octa_gpio_keys_config_setup();

	s3c_i2c0_set_platdata(NULL);
	i2c_register_board_info(0, i2c_devs0, ARRAY_SIZE(i2c_devs0));

	platform_add_devices(arndale_octa_input_devices,
			ARRAY_SIZE(arndale_octa_input_devices));
}
