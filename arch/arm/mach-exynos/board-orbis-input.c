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

#ifdef CONFIG_TOUCHSCREEN_CYTTSP5
#include <linux/i2c/cyttsp5_core.h>
#endif
#ifdef CONFIG_MOUSE_OFM_FINGER
#include <linux/i2c/partron_ofm.h>
#endif
#ifdef CONFIG_INPUT_SEC_ROTARY
#include <linux/i2c/sec_rotary.h>
#endif
#ifdef CONFIG_INPUT_TIZEN_DETENT
#include <linux/input/tizen_detent.h>
#endif
#ifdef CONFIG_INPUT_SECBRIDGE
#include <linux/input/sec-input-bridge.h>
#endif

struct class *sec_class;
EXPORT_SYMBOL(sec_class);

extern unsigned int system_rev;

#define GPIO_KEY_NAME	"gpio-keys"

#define WC1_S_MAIN_REV03A	0x04
#define WC1_S_MAIN_REV03B	0x05
#define WC1_S_MAIN_REV04	0x06
#define WC1_S_MAIN_REV15	0x0b
#define WC1_S_MAIN_REV16	0x0c

#define GPIO_TSP_INT		EXYNOS3_GPX3(5)
#define GPIO_TSP_INT_REV04	EXYNOS3_GPX1(3)
#define GPIO_TSP_SDA		EXYNOS3_GPA0(6)
#define GPIO_TSP_SCL		EXYNOS3_GPA0(7)

#define GPIO_POWER_BUTTON	EXYNOS3_GPX2(7)
#define GPIO_BACK_BUTTON	EXYNOS3_GPX3(3)

#ifdef CONFIG_INPUT_TIZEN_DETENT
#define GPIO_HALL_A_REV03	EXYNOS3_GPX2(1)
#define GPIO_HALL_B_REV03	EXYNOS3_GPX2(2)
#define GPIO_HALL_C_REV03	EXYNOS3_GPX2(3)

#define GPIO_HALL_A_REV04	EXYNOS3_GPX0(1)
#define GPIO_HALL_B_REV04	EXYNOS3_GPX1(0)
#define GPIO_HALL_C_REV04	EXYNOS3_GPX1(7)

static struct hall_sensor_platform_data tizen_detent_pdata = {
	.gpio_a = GPIO_HALL_A_REV04,
	.gpio_b = GPIO_HALL_B_REV04,
	.gpio_c = GPIO_HALL_C_REV04,
};

static struct platform_device tizen_detent = {
	.name	= HALL_NAME,
	.dev	= {
		.platform_data = &tizen_detent_pdata,
	},
};
#endif

#ifdef CONFIG_MOUSE_OFM_FINGER
#define GPIO_OFM_MOTION	EXYNOS3_GPX1(3)
#define GPIO_OFM_POWERDN	EXYNOS3_GPE0(6)
#define GPIO_OFM_STANBY	EXYNOS3_GPE0(5)
#define GPIO_OFM_SDA		EXYNOS3_GPA1(2)
#define GPIO_OFM_SCL		EXYNOS3_GPA1(3)
#define OFM_I2C_ADR		0x53
#endif

#ifdef CONFIG_INPUT_SEC_ROTARY
#define GPIO_HALL_A		EXYNOS3_GPX2(1)
#define GPIO_HALL_B		EXYNOS3_GPX2(2)
#define GPIO_OFM_MOTION	EXYNOS3_GPX1(3)
#define GPIO_OFM_POWERDN	EXYNOS3_GPE0(6)
#define GPIO_OFM_STANBY	EXYNOS3_GPE0(5)
#define GPIO_OFM_SDA		EXYNOS3_GPA1(2)
#define GPIO_OFM_SCL		EXYNOS3_GPA1(3)
#define OFM_I2C_ADR		0x53

static struct rotary_platform_data rotary_pdata = {
	.powerdown_pin = GPIO_OFM_POWERDN,
	.standby_pin = GPIO_OFM_STANBY,
	.motion_pin = GPIO_OFM_MOTION,
	.always_on = false,
	.hall_a_pin = GPIO_HALL_A,
	.hall_b_pin = GPIO_HALL_B,
};

static struct i2c_board_info i2c_devs3[] = {
	{
		I2C_BOARD_INFO(ROTARY_NAME, OFM_I2C_ADR),
		.platform_data = &rotary_pdata,
		.irq = GPIO_OFM_MOTION,
	},
};

void __init sec_rotary_gpio_init(void)
{
	int ret;

	ret = gpio_request(GPIO_OFM_MOTION, "OFM_motion");
	if (ret)
		pr_err("failed to request gpio(OFM_motion)(%d)\n", ret);

	s3c_gpio_cfgpin(GPIO_OFM_MOTION, S3C_GPIO_SFN(0xf));
	s3c_gpio_setpull(GPIO_OFM_MOTION, S3C_GPIO_PULL_NONE);
	s3c_gpio_setpull(GPIO_OFM_SDA, S3C_GPIO_PULL_NONE);
	s3c_gpio_setpull(GPIO_OFM_SCL, S3C_GPIO_PULL_NONE);

	s5p_register_gpio_interrupt(GPIO_OFM_MOTION);
	i2c_devs3[0].irq = gpio_to_irq(GPIO_OFM_MOTION);
}
#endif
#ifdef CONFIG_INPUT_SECBRIDGE
static char* dev_name_str[] = {
	GPIO_KEY_NAME,
	HALL_NAME,
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

static const struct sec_input_bridge_mkey hall_log_map[] = {
	{ .type = EV_KEY , .code = KEY_POWER	},
	{ .type = EV_REL , .code = REL_WHEEL	},
	{ .type = EV_REL , .code = REL_WHEEL	},
	{ .type = EV_REL , .code = REL_WHEEL	},
	{ .type = EV_REL , .code = REL_WHEEL	},
	{ .type = EV_REL , .code = REL_WHEEL	},
	{ .type = EV_REL , .code = REL_WHEEL	},
	{ .type = EV_REL , .code = REL_WHEEL	},
	{ .type = EV_REL , .code = REL_WHEEL	},
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
	{
		.mkey_map = (struct sec_input_bridge_mkey*)hall_log_map,
		.num_mkey = ARRAY_SIZE(hall_log_map),
		.uevent_env_str = "APPS_LOG",
		.enable_uevent = 1,
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

struct tsp_callbacks *tsp_callbacks;
struct tsp_callbacks {
	void (*inform_charger) (struct tsp_callbacks *, bool);
};

#ifdef CONFIG_MOUSE_OFM_FINGER
static struct ofm_platform_data ofm_pdata = {
	.powerdown_pin = GPIO_OFM_POWERDN,
	.standby_pin = GPIO_OFM_STANBY,
	.motion_pin = GPIO_OFM_MOTION,
	.always_on = false,
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
	pr_info("%s OFM_motion:[%d]\n", __func__, i2c_devs3[0].irq);
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
	.report_rate = 90,
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
	.flags = 0,
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
	int ret;
	int irq_gpio;

	if (system_rev >= WC1_S_MAIN_REV04)
		irq_gpio = GPIO_TSP_INT_REV04;
	else
		irq_gpio = GPIO_TSP_INT;

	_cyttsp5_core_platform_data.irq_gpio = irq_gpio;
	ret = gpio_request(irq_gpio, "TSP_INT");
	if (ret)
		pr_err("failed to request gpio(TSP_INT)(%d)\n", ret);

	s3c_gpio_cfgpin(irq_gpio, S3C_GPIO_SFN(0xf));
	s3c_gpio_setpull(irq_gpio, S3C_GPIO_PULL_NONE);
	s3c_gpio_setpull(GPIO_TSP_SDA, S3C_GPIO_PULL_NONE);
	s3c_gpio_setpull(GPIO_TSP_SCL, S3C_GPIO_PULL_NONE);

	s5p_register_gpio_interrupt(irq_gpio);
	i2c_devs2[0].irq = gpio_to_irq(irq_gpio);

	pr_info("%s touch : %d\n", __func__, i2c_devs2[0].irq);
}
#endif

static void universal3250_gpio_keys_config_setup(void)
{
	int irq;

	s3c_gpio_cfgpin(GPIO_POWER_BUTTON, S3C_GPIO_SFN(0xf));
	s3c_gpio_setpull(GPIO_POWER_BUTTON, S3C_GPIO_PULL_NONE);

	irq = s5p_register_gpio_interrupt(GPIO_POWER_BUTTON);
	if (IS_ERR_VALUE(irq)) {
		pr_err("%s: Failed to configure POWER GPIO\n", __func__);
		return;
	}

	if (system_rev > WC1_S_MAIN_REV15) {
		s3c_gpio_cfgpin(GPIO_BACK_BUTTON, S3C_GPIO_SFN(0xf));
		s3c_gpio_setpull(GPIO_BACK_BUTTON, S3C_GPIO_PULL_UP);

		irq = s5p_register_gpio_interrupt(GPIO_BACK_BUTTON);
		if (IS_ERR_VALUE(irq)) {
			pr_err("%s: Failed to configure BACK GPIO\n", __func__);
			return;
		}
	}
}

static struct gpio_keys_button rev15_button[] = {
	{
		.code = KEY_POWER,
		.gpio = GPIO_POWER_BUTTON,
		.desc = "key_power",
		.active_low = 1,
		.wakeup = 1,
		.debounce_interval = 20,
	},
};

static struct gpio_keys_platform_data rev15_gpiokeys_pdata = {
	rev15_button,
	ARRAY_SIZE(rev15_button),
};

static struct platform_device rev15_gpio_keys = {
	.name	= GPIO_KEY_NAME,
	.dev	= {
		.platform_data = &rev15_gpiokeys_pdata,
	},
};

static struct gpio_keys_button rev16_button[] = {
	{
		.code = KEY_POWER,
		.gpio = GPIO_POWER_BUTTON,
		.desc = "key_power",
		.active_low = 1,
		.wakeup = 1,
		.debounce_interval = 20,
	},
	{
		.code = KEY_BACK,
		.gpio = GPIO_BACK_BUTTON,
		.desc = "key_back",
		.active_low = 1,
		.wakeup = 1,
		.debounce_interval = 20,
	},
};

static struct gpio_keys_platform_data rev16_gpiokeys_pdata = {
	rev16_button,
	ARRAY_SIZE(rev16_button),
};

static struct platform_device rev16_gpio_keys = {
	.name	= GPIO_KEY_NAME,
	.dev	= {
		.platform_data = &rev16_gpiokeys_pdata,
	},
};

static struct platform_device *universal3250_input_devices[] __initdata = {
#ifdef CONFIG_INPUT_SECBRIDGE
	&sec_input_bridge
#endif
};

void __init exynos3_universal3250_input_init(void)
{
	sec_class = class_create(THIS_MODULE, "sec");
	if (IS_ERR(sec_class))
		pr_err("Failed to create sec_class.\n");

#ifdef CONFIG_TOUCHSCREEN_CYTTSP5
	platform_device_register(&s3c_device_i2c2);
	cypress_tsp_init();
	s3c_i2c2_set_platdata(NULL);
	i2c_register_board_info(2, i2c_devs2, ARRAY_SIZE(i2c_devs2));
#endif
#ifdef CONFIG_MOUSE_OFM_FINGER
	platform_device_register(&s3c_device_i2c3);
	ofm_gpio_init();
	s3c_i2c3_set_platdata(NULL);
	i2c_register_board_info(3, i2c_devs3, ARRAY_SIZE(i2c_devs3));
#endif
	universal3250_gpio_keys_config_setup();

	if (system_rev > WC1_S_MAIN_REV15)
		platform_device_register(&rev16_gpio_keys);
	else
		platform_device_register(&rev15_gpio_keys);
	platform_add_devices(universal3250_input_devices,
			ARRAY_SIZE(universal3250_input_devices));

	if (system_rev >= WC1_S_MAIN_REV03A) {
#ifdef CONFIG_INPUT_TIZEN_DETENT
		if (system_rev < WC1_S_MAIN_REV04) {
			tizen_detent_pdata.gpio_a = GPIO_HALL_A_REV03;
			tizen_detent_pdata.gpio_b = GPIO_HALL_B_REV03;
			tizen_detent_pdata.gpio_c = GPIO_HALL_C_REV03;
		}
		platform_device_register(&tizen_detent);
#endif
	} else {
#ifdef CONFIG_INPUT_SEC_ROTARY
		platform_device_register(&s3c_device_i2c3);
		sec_rotary_gpio_init();
		s3c_i2c3_set_platdata(NULL);
		i2c_register_board_info(3, i2c_devs3, ARRAY_SIZE(i2c_devs3));
#endif
	}
}
