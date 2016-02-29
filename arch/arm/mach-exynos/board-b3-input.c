/* linux/arch/arm/mach-exynos/board-universal3250-input.c
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

#ifdef CONFIG_TOUCHSCREEN_CYTTSP5
#include <linux/i2c/cyttsp5_core.h>
#endif
#ifdef CONFIG_MOUSE_OFM_FINGER
#include <linux/i2c/ofm_driver.h>
#endif

struct class *sec_class;
EXPORT_SYMBOL(sec_class);

#define GPIO_TSP_INT		EXYNOS3_GPX3(5)
#define GPIO_TSP_SDA		EXYNOS3_GPA0(6)
#define GPIO_TSP_SCL		EXYNOS3_GPA0(7)

#define GPIO_POWER_BUTTON	EXYNOS3_GPX2(7)
#ifdef CONFIG_MACH_BLINK
#define GPIO_MODE_BUTTON	EXYNOS3_GPX2(5)
#endif
#define GPIO_VOLUP_BUTTON	EXYNOS3_GPX3(2)
#define GPIO_VOLDOWN_BUTTON	EXYNOS3_GPX3(3)
#define GPIO_HOME_BUTTON	EXYNOS3_GPX3(4)

#ifdef CONFIG_MOUSE_OFM_FINGER
#define GPIO_OFM_MOTION	EXYNOS3_GPX1(3)
#define GPIO_OFM_POWERDN	EXYNOS3_GPE0(6)
#define GPIO_OFM_STANBY	EXYNOS3_GPE0(5)
#define GPIO_OFM_SDA		EXYNOS3_GPA1(2)
#define GPIO_OFM_SCL		EXYNOS3_GPA1(3)
#define OFM_I2C_ADR	0x53
#endif

#if defined(CONFIG_TOUCHSCREEN_MMS134S)
#include <linux/i2c/mms134s.h>
#elif defined(CONFIG_TOUCHSCREEN_MELFAS_W)
#include <linux/i2c/mms_ts_w.h>
#endif

struct tsp_callbacks *tsp_callbacks;
struct tsp_callbacks {
	void (*inform_charger) (struct tsp_callbacks *, bool);
};

#ifdef CONFIG_MOUSE_OFM_FINGER
static struct ofm_platform_data ofm_pdata = {
	.powerdown_pin = GPIO_OFM_POWERDN,
	.standby_pin = GPIO_OFM_STANBY,
};

static struct i2c_board_info i2c_devs3[] = {
	{
		I2C_BOARD_INFO(OFM_NAME, OFM_I2C_ADR),
		.platform_data = &ofm_pdata,
		.irq = GPIO_OFM_MOTION,
	},
};

void __init ofm_gpio_init(void)
{
	int motion_gpio;
	int ret;

	motion_gpio = GPIO_OFM_MOTION;
	ret = gpio_request(motion_gpio, "OFM_motion");
	if (ret)
		pr_err("failed to request gpio(OFM_motion)(%d)\n", ret);

	s3c_gpio_cfgpin(motion_gpio, S3C_GPIO_SFN(0xf));
	s3c_gpio_setpull(motion_gpio, S3C_GPIO_PULL_NONE);
	s3c_gpio_setpull(GPIO_OFM_SDA, S3C_GPIO_PULL_NONE);
	s3c_gpio_setpull(GPIO_OFM_SCL, S3C_GPIO_PULL_NONE);

	s5p_register_gpio_interrupt(motion_gpio);
	i2c_devs3[0].irq = gpio_to_irq(motion_gpio);
	ofm_pdata.motion_pin = i2c_devs3[0].irq;
	pr_info("%s OFM_motion:[%d]\n", __func__, i2c_devs3[0].irq);
}
#endif

#if defined(CONFIG_TOUCHSCREEN_MMS134S)
static bool tsp_power_enabled;

void tsp_charger_infom(bool en)
{
	if (tsp_callbacks && tsp_callbacks->inform_charger)
		tsp_callbacks->inform_charger(tsp_callbacks, en);
}
static void melfas_register_charger_callback(void *cb)
{
	tsp_callbacks = cb;
	pr_debug("[TSP] melfas_register_lcd_callback\n");
}

void melfas_vdd_pullup(bool on)
{
	struct regulator *regulator_vdd;

	regulator_vdd = regulator_get(NULL, "vtsp_1v8");
	if (IS_ERR(regulator_vdd))
		return;

	if (on) {
		regulator_enable(regulator_vdd);
	} else {
		if (regulator_is_enabled(regulator_vdd))
			regulator_disable(regulator_vdd);
		else
			regulator_force_disable(regulator_vdd);
	}
	regulator_put(regulator_vdd);
}

int melfas_power(bool on)
{
	struct regulator *regulator_vdd;

	if (tsp_power_enabled == on)
		return 0;

	printk(KERN_DEBUG "[TSP] %s %s\n",
		__func__, on ? "on" : "off");

	regulator_vdd = regulator_get(NULL, "vtsp_a3v0");
	if (IS_ERR(regulator_vdd))
		return PTR_ERR(regulator_vdd);

	if (on) {
		regulator_enable(regulator_vdd);
		usleep_range(2500, 3000);
	} else {
		if (regulator_is_enabled(regulator_vdd))
			regulator_disable(regulator_vdd);
		else
			regulator_force_disable(regulator_vdd);
	}
	regulator_put(regulator_vdd);

	melfas_vdd_pullup(on);

	tsp_power_enabled = on;

	return 0;
}

static bool tsp_keyled_enabled;
int key_led_control(bool on)
{
	struct regulator *regulator;

	if (tsp_keyled_enabled == on)
		return 0;

	printk(KERN_DEBUG "[TSP] %s %s\n",
		__func__, on ? "on" : "off");

	regulator = regulator_get(NULL, "key_led_3v3");
	if (IS_ERR(regulator))
		return PTR_ERR(regulator);

	if (on) {
		regulator_enable(regulator);
	} else {
		if (regulator_is_enabled(regulator))
			regulator_disable(regulator);
		else
			regulator_force_disable(regulator);
	}
	regulator_put(regulator);

	tsp_keyled_enabled = on;

	return 0;
}

int is_melfas_vdd_on(void)
{
	static struct regulator *regulator;
	int ret;

	if (!regulator) {
		regulator = regulator_get(NULL, "tsp_vdd_3.3v");
		if (IS_ERR(regulator)) {
			ret = PTR_ERR(regulator);
			pr_err("could not get touch, rc = %d\n", ret);
			return ret;
		}
	}

	if (regulator_is_enabled(regulator))
		return 1;
	else
		return 0;
}

static struct melfas_tsi_platform_data mms_ts_pdata = {
	.max_x = 480,
	.max_y = 800,
	.gpio_int = GPIO_TSP_INT,
	.gpio_scl = GPIO_TSP_SCL,
	.gpio_sda = GPIO_TSP_SDA,
	.power = melfas_power,
	.is_vdd_on = is_melfas_vdd_on,
	.touchkey = true,
	.keyled = key_led_control,
	.register_cb = melfas_register_charger_callback,
};

static struct i2c_board_info i2c_devs3[] = {
	{
		I2C_BOARD_INFO(MELFAS_TS_NAME, 0x48),
		.platform_data = &mms_ts_pdata
	},
};

void __init midas_tsp_set_platdata(struct melfas_tsi_platform_data *pdata)
{
	if (!pdata)
		pdata = &mms_ts_pdata;

	i2c_devs3[0].platform_data = pdata;
}
void __init garda_tsp_init(u32 system_rev)
{
	int gpio;
	int ret;

	printk(KERN_ERR "tsp:%s called\n", __func__);

	gpio = GPIO_TSP_INT;
	ret = gpio_request(gpio, "TSP_INT");
	if (ret)
		pr_err("failed to request gpio(TSP_INT)(%d)\n", ret);
	s3c_gpio_cfgpin(gpio, S3C_GPIO_SFN(0xf));
	s3c_gpio_setpull(gpio, S3C_GPIO_PULL_NONE);
	s3c_gpio_setpull(GPIO_TSP_SDA, S3C_GPIO_PULL_NONE);
	s3c_gpio_setpull(GPIO_TSP_SCL, S3C_GPIO_PULL_NONE);

	s5p_register_gpio_interrupt(gpio);

	i2c_devs3[0].irq = gpio_to_irq(gpio);
	printk(KERN_ERR "%s touch : %d\n", __func__, i2c_devs3[0].irq);
}

#endif

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

void __init midas_tsp_init(void)
{
	int gpio;
	int ret;
//	pr_info("melfas-ts : W TSP init() is called : [%d]", system_rev);
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

#ifdef CONFIG_TOUCHSCREEN_CYTTSP5
#define CYTTSP5_I2C_TCH_ADR 0x24

#define CYTTSP5_HID_DESC_REGISTER 1

#define CY_MAXX 360
#define CY_MAXY 480
#define CY_MINX 0
#define CY_MINY 0

#define CY_ABS_MIN_X CY_MINX
#define CY_ABS_MIN_Y CY_MINY
#define CY_ABS_MAX_X CY_MAXX
#define CY_ABS_MAX_Y CY_MAXY
#define CY_ABS_MIN_P 0
#define CY_ABS_MAX_P 255
#define CY_ABS_MIN_W 0
#define CY_ABS_MAX_W 255

#define CY_ABS_MIN_T 0

#define CY_ABS_MAX_T 15

#define CY_IGNORE_VALUE 0xFFFF


extern int cyttsp5_xres(struct cyttsp5_core_platform_data *pdata, struct device *dev);
extern int cyttsp5_power(struct cyttsp5_core_platform_data *pdata, int on, struct device *dev, atomic_t *ignore_irq);
extern int cyttsp5_irq_stat(struct cyttsp5_core_platform_data *pdata, struct device *dev);

static struct cyttsp5_core_platform_data _cyttsp5_core_platform_data = {
	.irq_gpio = GPIO_TSP_INT,
	.hid_desc_register = CYTTSP5_HID_DESC_REGISTER,
	.xres = cyttsp5_xres,
	.power = cyttsp5_power,
	.irq_stat = cyttsp5_irq_stat,
	.sett = {
		NULL,	/* Reserved */
		NULL,	/* Command Registers */
		NULL,	/* Touch Report */
		NULL,	/* Cypress Data Record */
		NULL,	/* Test Record */
		NULL,	/* Panel Configuration Record */
		NULL,	/* &cyttsp5_sett_param_regs, */
		NULL,	/* &cyttsp5_sett_param_size, */
		NULL,	/* Reserved */
		NULL,	/* Reserved */
		NULL,	/* Operational Configuration Record */
		NULL, /* &cyttsp5_sett_ddata, *//* Design Data Record */
		NULL, /* &cyttsp5_sett_mdata, *//* Manufacturing Data Record */
		NULL,	/* Config and Test Registers */
		NULL, /* &cyttsp5_sett_btn_keys, */	/* button-to-keycode table */
	},
};

static const uint16_t cyttsp5_abs[] = {
	ABS_MT_POSITION_X, CY_ABS_MIN_X, CY_ABS_MAX_X, 0, 0,
	ABS_MT_POSITION_Y, CY_ABS_MIN_Y, CY_ABS_MAX_Y, 0, 0,
	ABS_MT_PRESSURE, CY_ABS_MIN_P, CY_ABS_MAX_P, 0, 0,
	CY_IGNORE_VALUE, CY_ABS_MIN_W, CY_ABS_MAX_W, 0, 0,
	ABS_MT_TRACKING_ID, CY_ABS_MIN_T, CY_ABS_MAX_T, 0, 0,
	ABS_MT_TOUCH_MAJOR, 0, 255, 0, 0,
	ABS_MT_TOUCH_MINOR, 0, 255, 0, 0,
	ABS_MT_ORIENTATION, -128, 127, 0, 0,
	ABS_MT_TOOL_TYPE, 0, MT_TOOL_MAX, 0, 0,
};

struct touch_framework cyttsp5_framework = {
	.abs = (uint16_t *)&cyttsp5_abs[0],
	.size = ARRAY_SIZE(cyttsp5_abs),
	.enable_vkeys = 0,
};

static struct cyttsp5_mt_platform_data _cyttsp5_mt_platform_data = {
	.frmwrk = &cyttsp5_framework,
	.flags = CY_MT_FLAG_NONE,
	.inp_dev_name = "sec_touchscreen",
};

extern struct cyttsp5_loader_platform_data _cyttsp5_loader_platform_data;
static struct cyttsp5_platform_data _cyttsp5_platform_data = {
	.core_pdata = &_cyttsp5_core_platform_data,
	.mt_pdata = &_cyttsp5_mt_platform_data,
	.loader_pdata = &_cyttsp5_loader_platform_data,
};

static struct i2c_board_info i2c_devs2[] = {
	{
		I2C_BOARD_INFO("cyttsp5_i2c_adapter", CYTTSP5_I2C_TCH_ADR),
		.platform_data = &_cyttsp5_platform_data,
	},
};

void __init cypress_tsp_set_platdata(struct cyttsp5_platform_data *pdata)
{
	if (!pdata)
		pdata = &_cyttsp5_platform_data;

	i2c_devs2[0].platform_data = pdata;
}

void __init cypress_tsp_init(void)
{
	int gpio;
	int ret;

	gpio = GPIO_TSP_INT;
	ret = gpio_request(gpio, "TSP_INT");
	if (ret)
		pr_err("failed to request gpio(TSP_INT)(%d)\n", ret);

	s3c_gpio_cfgpin(gpio, S3C_GPIO_SFN(0xf));
	s3c_gpio_setpull(gpio, S3C_GPIO_PULL_NONE);
	s3c_gpio_setpull(GPIO_TSP_SDA, S3C_GPIO_PULL_NONE);
	s3c_gpio_setpull(GPIO_TSP_SCL, S3C_GPIO_PULL_NONE);

	s5p_register_gpio_interrupt(gpio);
	i2c_devs2[0].irq = gpio_to_irq(gpio);

	pr_info("%s touch : %d\n", __func__, i2c_devs2[0].irq);
}
#endif /* CONFIG_TOUCHSCREEN_CYTTSP5 */


static void universal3250_gpio_keys_config_setup(void)
{
	int irq;

	s3c_gpio_cfgpin(GPIO_VOLUP_BUTTON, S3C_GPIO_SFN(0xf));
	s3c_gpio_cfgpin(GPIO_VOLDOWN_BUTTON, S3C_GPIO_SFN(0xf));
	s3c_gpio_cfgpin(GPIO_HOME_BUTTON, S3C_GPIO_SFN(0xf));
	s3c_gpio_cfgpin(GPIO_POWER_BUTTON, S3C_GPIO_SFN(0xf));

	s3c_gpio_setpull(GPIO_VOLUP_BUTTON, S3C_GPIO_PULL_UP);
	s3c_gpio_setpull(GPIO_VOLDOWN_BUTTON, S3C_GPIO_PULL_UP);
	s3c_gpio_setpull(GPIO_HOME_BUTTON, S3C_GPIO_PULL_UP);
	s3c_gpio_setpull(GPIO_POWER_BUTTON, S3C_GPIO_PULL_UP);

	irq = s5p_register_gpio_interrupt(GPIO_VOLUP_BUTTON);
	if (IS_ERR_VALUE(irq)) {
		pr_err("%s: Failed to configure VOL UP GPIO\n", __func__);
		return;
	}
	irq = s5p_register_gpio_interrupt(GPIO_VOLDOWN_BUTTON);
	if (IS_ERR_VALUE(irq)) {
		pr_err("%s: Failed to configure VOL DOWN GPIO\n", __func__);
		return;
	}
	irq = s5p_register_gpio_interrupt(GPIO_HOME_BUTTON);
	if (IS_ERR_VALUE(irq)) {
		pr_err("%s: Failed to configure HOME GPIO\n", __func__);
		return;
	}
	irq = s5p_register_gpio_interrupt(GPIO_POWER_BUTTON);
	if (IS_ERR_VALUE(irq)) {
		pr_err("%s: Failed to configure POWER GPIO\n", __func__);
		return;
	}
}

static struct gpio_keys_button universal3250_button[] = {
	{
		.code = KEY_POWER,
		.gpio = GPIO_POWER_BUTTON,
		.desc = "power_key",
		.active_low = 1,
		.wakeup = 1,
		.debounce_interval = 20,
	},
#ifdef CONFIG_MACH_BLINK
	{
		.code = KEY_CAMERA,
		.gpio = GPIO_MODE_BUTTON,
		.desc = "mode_key",
		.active_low = 0,
		.wakeup = 0,
		.debounce_interval = 20,
	}
#endif
};

static struct gpio_keys_platform_data universal3250_gpiokeys_platform_data = {
	universal3250_button,
	ARRAY_SIZE(universal3250_button),
};


static struct platform_device universal3250_gpio_keys = {
	.name	= "gpio-keys",
	.dev	= {
		.platform_data = &universal3250_gpiokeys_platform_data,
	},
};

static struct platform_device *universal3250_input_devices[] __initdata = {
	&s3c_device_i2c2,
	&s3c_device_i2c3,
	&universal3250_gpio_keys,
};


void __init exynos3_universal3250_input_init(void)
{
	pr_info("%s\n", __func__);

	sec_class = class_create(THIS_MODULE, "sec");

#if defined(CONFIG_TOUCHSCREEN_MELFAS_W)
	midas_tsp_init();

	s3c_i2c2_set_platdata(NULL);
	i2c_register_board_info(2, i2c_devs2, ARRAY_SIZE(i2c_devs2));
#elif defined(CONFIG_TOUCHSCREEN_MMS134S)
	garda_tsp_init(0);

	s3c_i2c2_set_platdata(NULL);
	i2c_register_board_info(2, i2c_devs2, ARRAY_SIZE(i2c_devs2));
#elif defined(CONFIG_TOUCHSCREEN_CYTTSP5)
	cypress_tsp_init();

	s3c_i2c2_set_platdata(NULL);
	i2c_register_board_info(2, i2c_devs2, ARRAY_SIZE(i2c_devs2));
#endif
#ifdef CONFIG_MOUSE_OFM_FINGER
	ofm_gpio_init();
	s3c_i2c3_set_platdata(NULL);
	i2c_register_board_info(3, i2c_devs3, ARRAY_SIZE(i2c_devs3));
#endif
	universal3250_gpio_keys_config_setup();
	platform_add_devices(universal3250_input_devices,
			ARRAY_SIZE(universal3250_input_devices));
}
