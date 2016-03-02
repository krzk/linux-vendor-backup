/* linux/arch/arm/mach-mmp/board-golden-#endifmfd.c
 *
 * Copyright (c) 2013 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/


#include <linux/init.h>
#include <linux/export.h>
#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/i2c-gpio.h>
#include <linux/platform_device.h>
#include <linux/regulator/machine.h>
#include <linux/mfd/rt5033.h>
#include <plat/gpio-cfg.h>
#include "board-universal222ap.h"
#include "board-kmini.h"


#define GPIO_IF_PMIC_SDA	EXYNOS4_GPC1(3)
#define GPIO_IF_PMIC_SCL	EXYNOS4_GPC1(4)
#define GPIO_IF_PMIC_IRQ	EXYNOS4_GPX0(3)
#define IF_PMIC_ID			13

#ifdef CONFIG_MFD_RT5033

#ifdef CONFIG_REGULATOR_RT5033
extern struct rt5033_regulator_platform_data rv_pdata;
#endif

#ifdef CONFIG_RT5033_SADDR
#define RT5033FG_SLAVE_ADDR_MSB (0x40)
#else
#define RT5033FG_SLAVE_ADDR_MSB (0x00)
#endif

#define RT5033_SLAVE_ADDR   (0x34|RT5033FG_SLAVE_ADDR_MSB)


/* add regulator data */

/* add fled platform data */


/* Define mfd driver platform data*/
struct rt5033_mfd_platform_data exynos4_rt5033_info = {
	//.irq_base = NULL,	/* for swi*/
	.irq_gpio = GPIO_IF_PMIC_IRQ,
#ifdef CONFIG_CHARGER_RT5033
	.charger_data = &sec_battery_pdata,
#endif

#ifdef CONFIG_REGULATOR_RT5033
        .regulator_platform_data = &rv_pdata,
#endif
};

static struct i2c_board_info i2c_devs8_emul[] __initdata = {
	{
		I2C_BOARD_INFO("rt5033-mfd", RT5033_SLAVE_ADDR),
		.platform_data	= &exynos4_rt5033_info,
	}
};

static struct i2c_gpio_platform_data rt5033_i2c_data = {
	.sda_pin = GPIO_IF_PMIC_SDA,
	.scl_pin = GPIO_IF_PMIC_SCL,
	.udelay = 10,
	.timeout = 0,
};

static struct platform_device rt5033_i2c_device = {
	.name			= "i2c-gpio",
	.id			= IF_PMIC_ID,
	.dev.platform_data = &rt5033_i2c_data,
};

static struct platform_device *exynos4_mfd_devices[] __initdata = {
	&rt5033_i2c_device,
};

#endif /* CONFIG_MFD_RT5033 */

void __init exynos4_smdk4270_mfd_init(void)
{
	platform_add_devices(exynos4_mfd_devices,
			ARRAY_SIZE(exynos4_mfd_devices));
	i2c_register_board_info(IF_PMIC_ID, i2c_devs8_emul,
				ARRAY_SIZE(i2c_devs8_emul));
}
