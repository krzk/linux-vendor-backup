/* linux/arch/arm/mach-exynos/board-orbis-input.c
 *
 * Copyright (c) 2013 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

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

#include "board-universal3250.h"

#ifdef CONFIG_INPUT_SECBRIDGE
#include <linux/input/sec-input-bridge.h>
#endif
#if defined(CONFIG_TOUCHSCREEN_MELFAS_W)
#include <linux/i2c/mms_ts_w.h>
#endif

struct class *sec_class;
EXPORT_SYMBOL(sec_class);

extern unsigned int system_rev;

#define GPIO_KEY_NAME	"gpio-keys"

#define WC1_S_MAIN_REV03A	0x04
#define WC1_S_MAIN_REV03B	0x05
#define WC1_S_MAIN_REV04	0x06

#define GPIO_TSP_INT		EXYNOS3_GPX3(5)
#define GPIO_TSP_SDA		EXYNOS3_GPA0(6)
#define GPIO_TSP_SCL		EXYNOS3_GPA0(7)

#define GPIO_POWER_BUTTON	EXYNOS3_GPX2(7)

#ifdef CONFIG_INPUT_SECBRIDGE
static char* dev_name_str[] = {
	GPIO_KEY_NAME,
};

static const struct sec_input_bridge_mkey apps_log_map[] = {
	{ .type = EV_KEY , .code = KEY_VOLUMEUP	},
	{ .type = EV_KEY , .code = KEY_VOLUMEDOWN	},
	{ .type = EV_KEY , .code = KEY_VOLUMEUP	},
	{ .type = EV_KEY , .code = KEY_VOLUMEDOWN	},
	{ .type = EV_KEY , .code = KEY_POWER	},
	{ .type = EV_KEY , .code = KEY_VOLUMEDOWN	},
	{ .type = EV_KEY , .code = KEY_VOLUMEUP	},
	{ .type = EV_KEY , .code = KEY_POWER	},
};

static const struct sec_input_bridge_mkey safemode_map[] = {
	{ .type = EV_KEY , .code = KEY_POWER	},
};

static const struct sec_input_bridge_mmap input_bridge_mmap[] = {
	{
		.mkey_map = (struct sec_input_bridge_mkey*)apps_log_map,
		.num_mkey = ARRAY_SIZE(apps_log_map),
		.uevent_env_str = "APPS_LOG",
		.enable_uevent = 1,
		.uevent_action = KOBJ_CHANGE,
		.uevent_env_value = "ON",
	},
	{
		.mkey_map = (struct sec_input_bridge_mkey*)safemode_map,
		.num_mkey = ARRAY_SIZE(safemode_map),
		.uevent_env_str = "SAFE_MODE",
		.enable_uevent = 0,
		.uevent_action = KOBJ_CHANGE,
		.uevent_env_value = "ON",
	},
};

static struct sec_input_bridge_platform_data input_bridge_pdata = {
	.mmap = (struct sec_input_bridge_mmap*)input_bridge_mmap,
	.num_map = ARRAY_SIZE(input_bridge_mmap),
	.support_dev_name = (char**)&dev_name_str,
	.support_dev_num = ARRAY_SIZE(dev_name_str),
};

static struct platform_device sec_input_bridge = {
	.name	= "samsung_input_bridge",
	.id	= -1,
	.dev	= {
		.platform_data = &input_bridge_pdata,
			},
};
#endif

static void universal3250_gpio_keys_config_setup(void)
{
	int irq;

	s3c_gpio_cfgpin(GPIO_POWER_BUTTON, S3C_GPIO_SFN(0xf));
	s3c_gpio_setpull(GPIO_POWER_BUTTON, S3C_GPIO_PULL_UP);

	irq = s5p_register_gpio_interrupt(GPIO_POWER_BUTTON);
	if (IS_ERR_VALUE(irq)) {
		pr_err("%s: Failed to configure POWER GPIO\n", __func__);
		return;
	}
}

#if defined(CONFIG_TOUCHSCREEN_MELFAS_W)
static bool enabled;
int melfas_power(int on)
{
	struct regulator *regulator_pwr;
	struct regulator *regulator_vdd;
	int ret = 0;

	if (enabled == on) {
		pr_err("melfas-ts : %s same state!", __func__);
		return 0;
	}

	regulator_pwr = regulator_get(NULL, "tsp_vdd_3.3v");
	regulator_vdd = regulator_get(NULL, "tsp_vdd_1.8v");

	if (IS_ERR(regulator_pwr)) {
		pr_err("melfas-ts : %s regulator_pwr error!", __func__);
		return PTR_ERR(regulator_pwr);
	}
	if (IS_ERR(regulator_vdd)) {
		pr_err("melfas-ts : %s regulator_vdd error!", __func__);
		return PTR_ERR(regulator_vdd);
	}

	if (on) {
		regulator_enable(regulator_vdd);
		usleep_range(2500, 3000);
		regulator_enable(regulator_pwr);
	} else {
		if (regulator_is_enabled(regulator_pwr))
			regulator_disable(regulator_pwr);
		else
			regulator_force_disable(regulator_pwr);

		if (regulator_is_enabled(regulator_vdd))
			regulator_disable(regulator_vdd);
		else
			regulator_force_disable(regulator_vdd);
	}

	if (regulator_is_enabled(regulator_pwr) == !!on &&
		regulator_is_enabled(regulator_vdd) == !!on) {
		pr_info("melfas-ts : %s %s", __func__, !!on ? "ON" : "OFF");
		enabled = on;
	} else {
		pr_err("melfas-ts : regulator_is_enabled value error!");
		ret = -1;
	}

	regulator_put(regulator_vdd);
	regulator_put(regulator_pwr);
	enabled = on;

	return ret;
}

int melfas_power_vdd(int on)
{
	struct regulator *regulator_vdd;
	int ret = 0;

	regulator_vdd = regulator_get(NULL, "tsp_vdd_1.8v");

	if (IS_ERR(regulator_vdd)) {
		pr_err("melfas-ts : %s regulator_vdd error!", __func__);
		return PTR_ERR(regulator_vdd);
	}

	if (on) {
		regulator_enable(regulator_vdd);
	} else {
		if (regulator_is_enabled(regulator_vdd))
			regulator_disable(regulator_vdd);
		else
			regulator_force_disable(regulator_vdd);
	}

	pr_info("melfas-ts : %s %s", __func__, !!on ? "ON" : "OFF");
	regulator_put(regulator_vdd);

	return ret;
}

struct tsp_callbacks *charger_callbacks;
struct tsp_callbacks {
	void (*inform_charger)(struct tsp_callbacks *, bool);
};

void tsp_charger_infom(bool en)
{
	if (charger_callbacks && charger_callbacks->inform_charger)
		charger_callbacks->inform_charger(charger_callbacks, en);
}

static void melfas_register_callback(void *cb)
{
	charger_callbacks = cb;
	pr_info("melfas-ts : melfas_register_callback");
}

static struct melfas_tsi_platform_data mms_ts_pdata = {
	.max_x = 320,
	.max_y = 320,
	.invert_x = 0,
	.invert_y = 0,
	.gpio_int = GPIO_TSP_INT,
	.gpio_scl = GPIO_TSP_SCL,
	.gpio_sda = GPIO_TSP_SDA,
	.power = melfas_power,
	.power_vdd = melfas_power_vdd,
	.tsp_vendor = "MELFAS",
	.tsp_ic	= "MMS128S",
	.tsp_tx = 7,	/* TX_NUM (Reg Addr : 0xEF) */
	.tsp_rx = 7,	/* RX_NUM (Reg Addr : 0xEE) */
	.config_fw_version = "ME",
	.register_cb = melfas_register_callback,
	.report_rate = 45,
};

static struct i2c_board_info i2c_devs3[] = {
	{
	 I2C_BOARD_INFO(MELFAS_TS_NAME, 0x48),
	 .platform_data = &mms_ts_pdata},
};

void __init midas_tsp_set_platdata(struct melfas_tsi_platform_data *pdata)
{
	if (!pdata)
		pdata = &mms_ts_pdata;

	i2c_devs3[0].platform_data = pdata;
}

void __init melfas_w_tsp_init(void)
{
	int gpio;
	int ret;
	pr_info("melfas-ts : W TSP init() is called!");

	gpio = mms_ts_pdata.gpio_int;
	ret = gpio_request(gpio, "TSP_INT");
	if (ret)
		pr_err("melfas-ts : failed to request gpio(TSP_INT)");

	s3c_gpio_cfgpin(gpio, S3C_GPIO_SFN(0xf));
	s3c_gpio_setpull(gpio, S3C_GPIO_PULL_NONE);
	s3c_gpio_setpull(GPIO_TSP_SDA, S3C_GPIO_PULL_NONE);
	s3c_gpio_setpull(GPIO_TSP_SCL, S3C_GPIO_PULL_NONE);

	s5p_register_gpio_interrupt(gpio);
	i2c_devs3[0].irq = gpio_to_irq(gpio);

	pr_info("melfas-ts : %s touch : %d\n", __func__, i2c_devs3[0].irq);
}
#endif


static struct gpio_keys_button universal3250_button[] = {
	{
		.code = KEY_POWER,
		.gpio = GPIO_POWER_BUTTON,
		.desc = "key_power",
		.active_low = 1,
		.wakeup = 1,
		.debounce_interval = 20,
	},
};

static struct gpio_keys_platform_data gpiokeys_platform_data = {
	universal3250_button,
	ARRAY_SIZE(universal3250_button),
};


static struct platform_device universal3250_gpio_keys = {
	.name	= GPIO_KEY_NAME,
	.dev	= {
		.platform_data = &gpiokeys_platform_data,
	},
};

static struct platform_device *universal3250_input_devices[] __initdata = {
	&s3c_device_i2c2,
	&universal3250_gpio_keys,
#ifdef CONFIG_INPUT_SECBRIDGE
	&sec_input_bridge
#endif
};

void __init exynos3_universal3250_input_init(void)
{
	sec_class = class_create(THIS_MODULE, "sec");
	if (IS_ERR(sec_class))
		pr_err("Failed to create sec_class.\n");
#if defined(CONFIG_TOUCHSCREEN_MELFAS_W)
	melfas_w_tsp_init();

	s3c_i2c2_set_platdata(NULL);
	i2c_register_board_info(2, i2c_devs3, ARRAY_SIZE(i2c_devs3));
#endif
	universal3250_gpio_keys_config_setup();
	platform_add_devices(universal3250_input_devices,
			ARRAY_SIZE(universal3250_input_devices));
}
