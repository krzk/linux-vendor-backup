/*
 * Copyright (C) 2012 Samsung Electronics, Inc.
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
#include <linux/platform_device.h>
#include <linux/i2c-gpio.h>
#include <linux/i2c.h>
#include <linux/regulator/machine.h>
#include <linux/notifier.h>
#include <linux/reboot.h>
#include <linux/gpio.h>

#include <asm/io.h>

#include <plat/devs.h>
#include <plat/iic.h>
#include <plat/gpio-cfg.h>

#include <mach/regs-pmu.h>
#include <mach/irqs.h>

#include <linux/mfd/samsung/core.h>

#include "board-universal3250.h"

#include "common.h"

#if defined(CONFIG_S3C_ADC)
#include <plat/adc.h>
#endif

#include <linux/battery/sec_battery.h>
#include <linux/battery/sec_fuelgauge.h>
#include <linux/battery/sec_charger.h>
#include <linux/battery/charger/max14577_charger.h>
#include <linux/battery/bcm5935x.h>


#include <linux/usb/gadget.h>

int current_cable_type;
extern bool is_jig_attached;

extern unsigned int system_rev;

#if defined(CONFIG_BATTERY_DISCHG_IC)
extern unsigned short ssp_get_discharing_adc(void);
#endif

#define SEC_BATTERY_PMIC_NAME ""

/* temporally define here */
#define GPIO_FUEL_SCL		EXYNOS3_GPD1(3)
#define GPIO_FUEL_SDA		EXYNOS3_GPD1(2)
#define GPIO_FUEL_nALRT		EXYNOS3_GPX1(2)

#define GPIO_FUEL_SCL1		EXYNOS3_GPE1(3)
#define GPIO_FUEL_SDA1		EXYNOS3_GPE1(2)

//sec_battery_platform_data_t sec_battery_pdata;

static struct i2c_gpio_platform_data gpio_i2c_data_fgchg = {
	.sda_pin = GPIO_FUEL_SDA,
	.scl_pin = GPIO_FUEL_SCL,
};

static struct i2c_gpio_platform_data gpio_i2c_data_fgchg1 = {
	.sda_pin = GPIO_FUEL_SDA1,
	.scl_pin = GPIO_FUEL_SCL1,
};

#ifdef CONFIG_WPC_BCM5935X
#define GPIO_WPC_SCL		EXYNOS3_GPA1(5)
#define GPIO_WPC_SDA		EXYNOS3_GPA1(4)
#define	GPIO_WPC_INT		EXYNOS3_GPX3(7)

#define GPIO_WPC_SCL1		EXYNOS3_GPE0(6)
#define GPIO_WPC_SDA1		EXYNOS3_GPE0(5)

static struct i2c_gpio_platform_data gpio_i2c_data_wpc = {
	.sda_pin = GPIO_WPC_SDA,
	.scl_pin = GPIO_WPC_SCL,
};

static struct i2c_gpio_platform_data gpio_i2c_data_wpc1 = {
	.sda_pin = GPIO_WPC_SDA1,
	.scl_pin = GPIO_WPC_SCL1,
};

#endif

#if defined(CONFIG_BATTERY_DISCHG_IC)
#define GPIO_DISCHG_TEST	EXYNOS3_GPX3(6)
#endif

unsigned int lpcharge;
EXPORT_SYMBOL(lpcharge);

static sec_charging_current_t charging_current_table[] = {
	{125,	125,	35,	10},
	{0,	0,	0,	0},
	{125,	125,	35,	10},
	{125,	125,	35,	10},
	{125,	125,	35,	10},
	{125,	125,	35,	10},
	{125,	125,	35,	10},
	{125,	125,	35,	10},
	{125,	125,	35,	10},
	{0,	0,	0,	0},
	{125,	125,	35,	10},
	{125,	125,	35,	10},
	{0,	0,	0,	0},
	{0,	0,	0,	0},
};

static bool sec_bat_adc_none_init(
		struct platform_device *pdev) {return true; }
static bool sec_bat_adc_none_exit(void) {return true; }
static int sec_bat_adc_none_read(unsigned int channel) {return 0; }

static struct s3c_adc_client *adc_client;
static bool sec_bat_adc_ap_init(
		struct platform_device *pdev)
{
#if defined(CONFIG_S3C_ADC)
	if (!adc_client) {
		adc_client = s3c_adc_register(pdev, NULL, NULL, 0);
		if (IS_ERR(adc_client))
			pr_err("ADC READ ERROR");
		else
			pr_info("%s: sec_bat_adc_ap_init succeed\n", __func__);
	}
#endif
	return true;
}

static bool sec_bat_adc_ap_exit(void) {return true; }
static int sec_bat_adc_ap_read(unsigned int channel)
{
	int data = 1000;

	if (!adc_client)
		return data;

	switch (channel) {/* check ch # */
	case SEC_BAT_ADC_CHANNEL_TEMP:
#if defined(CONFIG_S3C_ADC)
		data = s3c_adc_read(adc_client, 0);
#endif
		break;
	case SEC_BAT_ADC_CHANNEL_TEMP_AMBIENT:
#if defined(CONFIG_S3C_ADC)
		data = s3c_adc_read(adc_client, 1);
#endif
		break;
	case SEC_BAT_ADC_CHANNEL_VOLTAGE_NOW:
		data = 2500;/*s3c_adc_read(adc_client, 0)*/
		break;
	}

	return data;
}

static bool sec_bat_adc_ic_init(
		struct platform_device *pdev) {return true; }
static bool sec_bat_adc_ic_exit(void) {return true; }
static int sec_bat_adc_ic_read(unsigned int channel)
{
#if defined(CONFIG_BATTERY_DISCHG_IC)
	int data =0;

	switch (channel) {
		case SEC_BAT_ADC_CHANNEL_BATT_DISCHG:
			data = (int)ssp_get_discharing_adc();
			break;
		default:
			data = -1;
			break;
	}

	return data;
#else
	return 0;
#endif
}

static bool sec_bat_gpio_init(void)
{
	return true;
}

static bool sec_fg_gpio_init(void)
{/* check this */
	s3c_gpio_cfgpin(GPIO_FUEL_nALRT, S3C_GPIO_INPUT);
	s3c_gpio_setpull(GPIO_FUEL_nALRT, S3C_GPIO_PULL_NONE);
	return true;
}

static bool sec_chg_gpio_init(void)
{
	return true;
}

#if 0/* check this */
static bool sec_bat_is_lpm(void) {
	u32 lpcharging;

#if 1
	lpcharging = __raw_readl(EXYNOS_INFORM2);
#else
	lpcharging = __raw_readl(S5P_INFORM2);
#endif

	if (lpcharging == 0x01)
		return true;
	else
		return false;
}
#else
static int sec_bat_is_lpm_check(char *str)
{
	if (strncmp(str, "charging-mode.target", 20) == 0)
		lpcharge = 1;

	pr_info("%s: Low power charging mode: %d\n", __func__, lpcharge);

	return lpcharge;
}
__setup("systemd.unit=", sec_bat_is_lpm_check);

static bool sec_bat_is_lpm(void)
{
	return lpcharge;
}
#endif

int extended_cable_type;

static void sec_bat_initial_check(void)
{
	union power_supply_propval value;

	psy_do_property("sec-charger", get,
		POWER_SUPPLY_PROP_ONLINE, value);
	if (value.intval != current_cable_type) {
		value.intval = current_cable_type;
		psy_do_property("battery", set,
			POWER_SUPPLY_PROP_ONLINE, value);
	}
}

static bool sec_bat_check_jig_status(void)
{
	return is_jig_attached;
}
static bool sec_bat_switch_to_check(void) {return true; }
static bool sec_bat_switch_to_normal(void) {return true; }

static int sec_bat_check_cable_callback(void)
{
#if 0
	if(current_cable_type == POWER_SUPPLY_TYPE_BATTERY)
		current_cable_type = POWER_SUPPLY_TYPE_MAINS;
	else
		current_cable_type = POWER_SUPPLY_TYPE_BATTERY;
#endif
	return current_cable_type;
}

static int sec_bat_get_cable_from_extended_cable_type(
	int input_extended_cable_type)
{
	int cable_main, cable_sub, cable_power;
	int cable_type = POWER_SUPPLY_TYPE_UNKNOWN;
	//union power_supply_propval value;
	int charge_current_max = 0, charge_current = 0;

	cable_main = GET_MAIN_CABLE_TYPE(input_extended_cable_type);
	if (cable_main != POWER_SUPPLY_TYPE_UNKNOWN)
		extended_cable_type = (extended_cable_type &
			~(int)ONLINE_TYPE_MAIN_MASK) |
			(cable_main << ONLINE_TYPE_MAIN_SHIFT);
	cable_sub = GET_SUB_CABLE_TYPE(input_extended_cable_type);
	if (cable_sub != ONLINE_SUB_TYPE_UNKNOWN)
		extended_cable_type = (extended_cable_type &
			~(int)ONLINE_TYPE_SUB_MASK) |
			(cable_sub << ONLINE_TYPE_SUB_SHIFT);
	cable_power = GET_POWER_CABLE_TYPE(input_extended_cable_type);
	if (cable_power != ONLINE_POWER_TYPE_UNKNOWN)
		extended_cable_type = (extended_cable_type &
			~(int)ONLINE_TYPE_PWR_MASK) |
			(cable_power << ONLINE_TYPE_PWR_SHIFT);

	switch (cable_main) {
	case POWER_SUPPLY_TYPE_CARDOCK:
		switch (cable_power) {
		case ONLINE_POWER_TYPE_BATTERY:
			cable_type = POWER_SUPPLY_TYPE_BATTERY;
			break;
		case ONLINE_POWER_TYPE_TA:
			switch (cable_sub) {
			case ONLINE_SUB_TYPE_MHL:
				cable_type = POWER_SUPPLY_TYPE_USB;
				break;
			case ONLINE_SUB_TYPE_AUDIO:
			case ONLINE_SUB_TYPE_DESK:
			case ONLINE_SUB_TYPE_SMART_NOTG:
			case ONLINE_SUB_TYPE_KBD:
				cable_type = POWER_SUPPLY_TYPE_MAINS;
				break;
			case ONLINE_SUB_TYPE_SMART_OTG:
				cable_type = POWER_SUPPLY_TYPE_CARDOCK;
				break;
			}
			break;
		case ONLINE_POWER_TYPE_USB:
			cable_type = POWER_SUPPLY_TYPE_USB;
			break;
		default:
			cable_type = current_cable_type;
			break;
		}
		break;
	case POWER_SUPPLY_TYPE_MISC:
		switch (cable_sub) {
		case ONLINE_SUB_TYPE_MHL:
			switch (cable_power) {
			case ONLINE_POWER_TYPE_BATTERY:
				cable_type = POWER_SUPPLY_TYPE_BATTERY;
				break;
			case ONLINE_POWER_TYPE_TA:
				cable_type = POWER_SUPPLY_TYPE_MISC;
				charge_current_max = 400;
				charge_current = 400;
				break;
			case ONLINE_POWER_TYPE_USB:
				cable_type = POWER_SUPPLY_TYPE_USB;
				charge_current_max = 300;
				charge_current = 300;
				break;
			default:
				cable_type = cable_main;
			}
			break;
		default:
			cable_type = cable_main;
			break;
		}
		break;
	default:
		cable_type = cable_main;
		break;
	}
	return cable_type;
}

#if 0/*check this*/
extern bool is_cable_attached;
static bool sec_bat_check_cable_result_callback(
				int cable_type)
{
	struct usb_gadget *gadget =
		platform_get_drvdata(&s3c_device_usbgadget);


	static bool is_prev_usbtype = false;

	current_cable_type = cable_type;
	if (cable_type == POWER_SUPPLY_TYPE_BATTERY)
		is_cable_attached = false;
	else
		is_cable_attached = true;

	if (gadget) {
		pr_info("%s: cable_type=%d\n", __func__, cable_type);

		if(cable_type == POWER_SUPPLY_TYPE_USB ) {
			usb_gadget_vbus_connect(gadget);
			is_prev_usbtype = true;
		}
		else if ( is_prev_usbtype == true) {
			usb_gadget_vbus_disconnect(gadget);
			is_prev_usbtype = false;
		}
	}
	else {
		printk(KERN_ERR "Gadget is null\n");
	}

	return true;
}
#else
static bool sec_bat_check_cable_result_callback(
				int cable_type)
{
	current_cable_type = cable_type;

	switch (cable_type) {
	case POWER_SUPPLY_TYPE_USB:
		pr_info("%s set vbus applied\n",
				__func__);
		break;
	case POWER_SUPPLY_TYPE_BATTERY:
		pr_info("%s set vbus cut\n",
			__func__);
		break;
	case POWER_SUPPLY_TYPE_MAINS:
		break;
	default:
		pr_err("%s cable type (%d)\n",
			__func__, cable_type);
		return false;
	}
	return true;
}
#endif

/* callback for battery check
 * return : bool
 * true - battery detected, false battery NOT detected
 */
static bool sec_bat_check_callback(void) {return true; }
static bool sec_bat_check_result_callback(void) {return true; }

/* callback for OVP/UVLO check
 * return : int
 * battery health
 */
static int sec_bat_ovp_uvlo_callback(void)
{
	int health;
	health = POWER_SUPPLY_HEALTH_GOOD;

	return health;
}

static bool sec_bat_ovp_uvlo_result_callback(int health) {return true; }

/*
 * val.intval : temperature
 */
static bool sec_bat_get_temperature_callback(
		enum power_supply_property psp,
		union power_supply_propval *val) {return true; }
static bool sec_fg_fuelalert_process(bool is_fuel_alerted) {return true; }

#ifdef CONFIG_MACH_WC1
static const sec_bat_adc_table_data_t temp_table[] = {
	{  195,  800 },
	{  220,  750 },
	{  256,  700 },
	{  297,  650 },
	{  327,  620 },
	{  340,  600 },
	{  371,  580 },
	{  397,  550 },
	{  461,  500 },
	{  480,  480 },
	{  534,  460 },
	{  560,  440 },
	{  632,  400 },
	{  737,  350 },
	{  846,  300 },
	{  960,  250 },
	{ 1067,  200 },
	{ 1183,  150 },
	{ 1296,  100 },
	{ 1401,   50 },
	{ 1502,    0 },
	{ 1539,  -20 },
	{ 1557,  -30 },
	{ 1592,  -50 },
	{ 1624,  -70 },
	{ 1656,  -90 },
	{ 1671, -100 },
	{ 1734, -150 },
	{ 1795, -200 },
};

static const sec_bat_adc_table_data_t temp_table1[] = {
	{  191,  800 },
	{  231,  750 },
	{  271,  700 },
	{  316,  650 },
	{  365,  600 },
	{  391,  580 },
	{  427,  550 },
	{  457,  530 },
	{  498,  500 },
	{  528,  480 },
	{  564,  460 },
	{  583,  450 },
	{  669,  400 },
	{  765,  350 },
	{  870,  300 },
	{  980,  250 },
	{ 1091,  200 },
	{ 1210,  150 },
	{ 1320,  100 },
	{ 1424,   50 },
	{ 1538,    0 },
	{ 1574,  -20 },
	{ 1628,  -50 },
	{ 1668,  -70 },
	{ 1717, -100 },
	{ 1755, -150 },
	{ 1795, -200 },
};

#else
static const sec_bat_adc_table_data_t temp_table[] = {
	{  196,  800 },
	{  221,  750 },
	{  259,  700 },
	{  309,  650 },
	{  327,  630 },
	{  350,  610 },
	{  346,  600 },
	{  459,  530 },
	{  498,  480 },
	{  648,  400 },
	{  846,  300 },
	{ 1076,  200 },
	{ 1305,  100 },
	{ 1541,    0 },
	{ 1594,  -30 },
	{ 1630,  -50 },
	{ 1661,  -70 },
	{ 1692,  -90 },
	{ 1718, -110 },
	{ 1770, -150 },
	{ 1835, -200 },
};
#endif

/* ADC region should be exclusive */
static sec_bat_adc_region_t cable_adc_value_table[] = {
	{0,	0},
	{0,	0},
	{0,	0},
	{0,	0},
	{0,	0},
	{0,	0},
	{0,	0},
	{0,	0},
	{0,	0},
	{0,	0},
	{0,	0},
};

static int polling_time_table[] = {
	10,	/* BASIC */
	30,	/* CHARGING */
	30,	/* DISCHARGING */
	30,	/* NOT_CHARGING */
	3600,	/* SLEEP */
};

/* For MAX17048 */
static struct battery_data_t exynos3250_battery_data[] = {
	/* SDI battery data (High voltage 4.35V) */
	{
		.RCOMP0 = 71,
		.RCOMP_charging = 73,
		.temp_cohot = -1600,	/* x 1000 */
		.temp_cocold = -9200,	/* x 1000 */
#if 1
		.is_using_model_data = true,	/* using 19 bits */
#else
		.is_using_model_data = false,	/* using 18 bits */
#endif
		.type_str = "SDI",
	}
};

sec_battery_platform_data_t sec_battery_pdata = {
	/* NO NEED TO BE CHANGED */
	.initial_check = sec_bat_initial_check,
	.bat_gpio_init = sec_bat_gpio_init,
	.fg_gpio_init = sec_fg_gpio_init,
	.chg_gpio_init = sec_chg_gpio_init,

	.is_lpm = sec_bat_is_lpm,
	.check_jig_status = sec_bat_check_jig_status,
	.check_cable_callback =
		sec_bat_check_cable_callback,
	.get_cable_from_extended_cable_type =
		sec_bat_get_cable_from_extended_cable_type,
	.cable_switch_check = sec_bat_switch_to_check,
	.cable_switch_normal = sec_bat_switch_to_normal,
	.check_cable_result_callback =
		sec_bat_check_cable_result_callback,
	.check_battery_callback =
		sec_bat_check_callback,
	.check_battery_result_callback =
		sec_bat_check_result_callback,
	.ovp_uvlo_callback = sec_bat_ovp_uvlo_callback,
	.ovp_uvlo_result_callback =
		sec_bat_ovp_uvlo_result_callback,
	.fuelalert_process = sec_fg_fuelalert_process,
	.get_temperature_callback =
		sec_bat_get_temperature_callback,

	.adc_api[SEC_BATTERY_ADC_TYPE_NONE] = {
		.init = sec_bat_adc_none_init,
		.exit = sec_bat_adc_none_exit,
		.read = sec_bat_adc_none_read
		},
	.adc_api[SEC_BATTERY_ADC_TYPE_AP] = {
		.init = sec_bat_adc_ap_init,
		.exit = sec_bat_adc_ap_exit,
		.read = sec_bat_adc_ap_read
		},
	.adc_api[SEC_BATTERY_ADC_TYPE_IC] = {
		.init = sec_bat_adc_ic_init,
		.exit = sec_bat_adc_ic_exit,
		.read = sec_bat_adc_ic_read
		},
	.cable_adc_value = cable_adc_value_table,
	.charging_current = charging_current_table,
	.polling_time = polling_time_table,
	/* NO NEED TO BE CHANGED */

	.pmic_name = SEC_BATTERY_PMIC_NAME,

	.adc_check_count = 6,
	.adc_type = {
		SEC_BATTERY_ADC_TYPE_NONE,	/* CABLE_CHECK */
		SEC_BATTERY_ADC_TYPE_AP,	/* BAT_CHECK */
		SEC_BATTERY_ADC_TYPE_AP,	/* TEMP */
		SEC_BATTERY_ADC_TYPE_AP,	/* TEMP_AMB */
		SEC_BATTERY_ADC_TYPE_AP,	/* FULL_CHECK */
	},

	/* Battery */
	.vendor = "SDI SDI",
	.technology = POWER_SUPPLY_TECHNOLOGY_LION,
	.battery_data = (void *)exynos3250_battery_data,
	.bat_gpio_ta_nconnected = 0,
	.bat_polarity_ta_nconnected = 0,
	.bat_irq = 0,
	.bat_irq_attr = 0,
	.cable_check_type =
		SEC_BATTERY_CABLE_CHECK_CHGINT,
	.cable_source_type =
		SEC_BATTERY_CABLE_SOURCE_EXTERNAL,

	.event_check = true,
	.event_waiting_time = 180,

	/* Monitor setting */
	.polling_type = SEC_BATTERY_MONITOR_ALARM,
	.monitor_initial_count = 3,

	/* Battery check */
	.battery_check_type = SEC_BATTERY_CHECK_NONE,
	.check_count = 0,
	/* Battery check by ADC */
	.check_adc_max = 1440,
	.check_adc_min = 0,

	/* OVP/UVLO check */
	.ovp_uvlo_check_type = SEC_BATTERY_OVP_UVLO_CHGPOLLING,

	/* Temperature check */
	.thermal_source = SEC_BATTERY_THERMAL_SOURCE_ADC,
	.temp_adc_table = temp_table,
	.temp_adc_table_size =
		sizeof(temp_table)/sizeof(sec_bat_adc_table_data_t),
	.temp_amb_adc_table = temp_table,
	.temp_amb_adc_table_size =
		sizeof(temp_table)/sizeof(sec_bat_adc_table_data_t),

	.temp_check_type = SEC_BATTERY_TEMP_CHECK_TEMP,
	.temp_check_count = 1,

	.temp_high_threshold_event = 580,
	.temp_high_recovery_event = 570,
	.temp_low_threshold_event = -50,
	.temp_low_recovery_event = 15,

	.temp_high_threshold_normal = 580,
	.temp_high_recovery_normal = 570,
	.temp_low_threshold_normal = -50,
	.temp_low_recovery_normal = 15,

	.temp_high_threshold_lpm = 580,
	.temp_high_recovery_lpm = 545,
	.temp_low_threshold_lpm = -50,
	.temp_low_recovery_lpm = 0,

	.full_check_type = SEC_BATTERY_FULLCHARGED_CHGPSY,
	.full_check_type_2nd = SEC_BATTERY_FULLCHARGED_CHGPSY,
	.full_check_count = 1,
	.chg_gpio_full_check = 0,
	.chg_polarity_full_check = 1,
	.full_condition_type = SEC_BATTERY_FULL_CONDITION_SOC |
		SEC_BATTERY_FULL_CONDITION_NOTIMEFULL |
		SEC_BATTERY_FULL_CONDITION_VCELL,
	.full_condition_soc = 97,
	.full_condition_vcell = 4250,

	.recharge_check_count = 2,
	.recharge_condition_type =
		SEC_BATTERY_RECHARGE_CONDITION_VCELL,
	.recharge_condition_soc = 90,
	.recharge_condition_vcell = 4300,

	.charging_total_time = 6 * 60 * 60,
	.recharging_total_time = 90 * 60,
	.charging_reset_time = 0,

	/* Fuel Gauge */
	.fg_irq = GPIO_FUEL_nALRT,
	.fg_irq_attr = IRQF_TRIGGER_RISING,
	.fuel_alert_soc = 2,
	.repeated_fuelalert = false,
	.capacity_calculation_type =
		SEC_FUELGAUGE_CAPACITY_TYPE_RAW |
		SEC_FUELGAUGE_CAPACITY_TYPE_SCALE |
		SEC_FUELGAUGE_CAPACITY_TYPE_DYNAMIC_SCALE |
		SEC_FUELGAUGE_CAPACITY_TYPE_SKIP_ABNORMAL,
	.capacity_max = 1000,
	.capacity_max_margin = 50,
	.capacity_min = -8,

	/* Charger */
	.charger_name = "sec-charger",
	.chg_gpio_en = 0,
	.chg_polarity_en = 0,
	.chg_gpio_status = 0,
	.chg_polarity_status = 0,
	.chg_irq = 0,
	.chg_irq_attr = 0,
	.chg_float_voltage = 4350,

#if defined(CONFIG_BATTERY_DISCHG_IC)
	.dischg_test = GPIO_DISCHG_TEST,
#endif
};

#define SEC_FUELGAUGE_I2C_ID	14
#define SEC_WPC_I2C_ID			15

static struct platform_device sec_device_battery = {
	.name = "sec-battery",
	.id = -1,
	.dev.platform_data = &sec_battery_pdata,
};

struct platform_device sec_device_fgchg = {
	.name = "i2c-gpio",
	.id = SEC_FUELGAUGE_I2C_ID,
	.dev.platform_data = &gpio_i2c_data_fgchg,
};

struct platform_device sec_device_fgchg1 = {
	.name = "i2c-gpio",
	.id = SEC_FUELGAUGE_I2C_ID,
	.dev.platform_data = &gpio_i2c_data_fgchg1,
};

static struct i2c_board_info sec_brdinfo_fgchg[] __initdata = {
	{
		I2C_BOARD_INFO("sec-fuelgauge",
			SEC_FUELGAUGE_I2C_SLAVEADDR),
		.platform_data	= &sec_battery_pdata,
	},
};

#ifdef CONFIG_WPC_BCM5935X
static struct bcm5935x_platform_data bcm5935x_mfd_pdata = {
	.irq_gpio		= GPIO_WPC_INT,
	.detect_type	= SEC_WPC_DETECT_MUIC_INT,
};

struct platform_device sec_device_wpc = {
	.name = "i2c-gpio",
	.id = SEC_WPC_I2C_ID,
	.dev.platform_data = &gpio_i2c_data_wpc,
};

struct platform_device sec_device_wpc1 = {
	.name = "i2c-gpio",
	.id = SEC_WPC_I2C_ID,
	.dev.platform_data = &gpio_i2c_data_wpc1,
};

static struct i2c_board_info sec_brdinfo_wpc[] __initdata = {
	{
		I2C_BOARD_INFO("bcm5935x",
			SEC_WPC_I2C_SLAVEADDR),
		.platform_data	= &bcm5935x_mfd_pdata,
	},
};
#endif

#ifdef CONFIG_MACH_WC1
static struct platform_device *sec_battery_devices[] __initdata = {
	&sec_device_fgchg,
	&sec_device_battery,
};

static struct platform_device *sec_battery_devices1[] __initdata = {
	&sec_device_fgchg1,
	&sec_device_battery,
#ifdef CONFIG_WPC_BCM5935X	
	&sec_device_wpc,
#endif	
};

static struct platform_device *sec_battery_devices2[] __initdata = {
	&sec_device_fgchg1,
	&sec_device_battery,
#ifdef CONFIG_WPC_BCM5935X
	&sec_device_wpc1,
#endif
};
#elif defined CONFIG_MACH_HWP
static struct platform_device *sec_battery_devices[] __initdata = {
	&sec_device_fgchg1,
	&sec_device_battery,
};
#else
static struct platform_device *sec_battery_devices[] __initdata = {
	&sec_device_fgchg,
	&sec_device_battery,
};
#endif

void __init exynos3_universal3250_battery_init(void)
{
	pr_err("%s: watch charger init!\n", __func__);

	if (system_rev <= 7)
		exynos3250_battery_data->RCOMP_charging = 73;	/*R380*/
	else
		exynos3250_battery_data->RCOMP_charging = 88;	/*R381*/

#ifdef CONFIG_MACH_WC1
	pr_info("%s: system_rev[%d]\n", __func__, system_rev);
	if (system_rev == 0) {
		platform_add_devices(sec_battery_devices,
			ARRAY_SIZE(sec_battery_devices));
	} else if (system_rev >= 6) {
		platform_add_devices(sec_battery_devices2,
			ARRAY_SIZE(sec_battery_devices2));
	} else {
		platform_add_devices(sec_battery_devices1,
			ARRAY_SIZE(sec_battery_devices1));
	}

	if (system_rev >= 8) {
		sec_battery_pdata.temp_adc_table = temp_table1;
		sec_battery_pdata.temp_adc_table_size =
			sizeof(temp_table1)/sizeof(sec_bat_adc_table_data_t);
		sec_battery_pdata.temp_amb_adc_table = temp_table1;
		sec_battery_pdata.temp_amb_adc_table_size =
			sizeof(temp_table1)/sizeof(sec_bat_adc_table_data_t);
	}

	if (system_rev >= 0x0C)
		bcm5935x_mfd_pdata.detect_type = SEC_WPC_DETECT_WPC_INT;

#elif defined CONFIG_MACH_HWP
	platform_add_devices(sec_battery_devices,
		ARRAY_SIZE(sec_battery_devices));
#else
	platform_add_devices(sec_battery_devices,
			ARRAY_SIZE(sec_battery_devices));
#endif
	i2c_register_board_info(SEC_FUELGAUGE_I2C_ID, sec_brdinfo_fgchg,
			ARRAY_SIZE(sec_brdinfo_fgchg));

#ifdef CONFIG_WPC_BCM5935X
	s3c_gpio_setpull(GPIO_WPC_SDA, S3C_GPIO_PULL_NONE);
	s3c_gpio_setpull(GPIO_WPC_SCL, S3C_GPIO_PULL_NONE);

	i2c_register_board_info(SEC_WPC_I2C_ID, sec_brdinfo_wpc,
			ARRAY_SIZE(sec_brdinfo_wpc));
#endif	

#if defined(CONFIG_BATTERY_DISCHG_IC)
	s3c_gpio_cfgpin(GPIO_DISCHG_TEST, S3C_GPIO_OUTPUT);
	s3c_gpio_setpull(GPIO_DISCHG_TEST, S3C_GPIO_PULL_NONE);

	gpio_direction_output(GPIO_DISCHG_TEST, 0);
#endif
}
