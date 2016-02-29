/*
 * Copyright (c) 2013 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/gpio.h>
#include <linux/mmc/host.h>
#include <linux/delay.h>
#include <linux/clk.h>

#include <plat/gpio-cfg.h>
#include <plat/cpu.h>
#include <plat/clock.h>
#include <plat/devs.h>
#include <plat/sdhci.h>

#include <mach/dwmci.h>

#include "board-universal3250.h"

static struct dw_mci_clk exynos_dwmci_clk_rates_for_mpll_pre_div[] = {
	{       400 * 1000,       1600 * 1000},
	{ 50 * 1000 * 1000, 200 * 1000 * 1000},
	{ 50 * 1000 * 1000, 200 * 1000 * 1000},
	{100 * 1000 * 1000, 400 * 1000 * 1000},
	{100 * 1000 * 1000, 400 * 1000 * 1000},
	{100 * 1000 * 1000, 400 * 1000 * 1000},
	{100 * 1000 * 1000, 400 * 1000 * 1000},
	{200 * 1000 * 1000, 400 * 1000 * 1000},

};

static int exynos_dwmci0_get_bus_wd(u32 slot_id)
{
	return 8;
}

static void exynos_dwmci_save_drv_st(void *data, u32 slot_id)
{
	struct dw_mci *host = (struct dw_mci *)data;
	struct drv_strength *drv_st = &host->pdata->__drv_st;

	drv_st->val = s5p_gpio_get_drvstr(drv_st->pin);
}

static void exynos_dwmci_restore_drv_st(void *data, u32 slot_id,
					int *compensation)
{
	struct dw_mci *host = (struct dw_mci *)data;
	struct drv_strength *drv_st = &host->pdata->__drv_st;

	*compensation = 0;

	s5p_gpio_set_drvstr(drv_st->pin, drv_st->val);
}

static void exynos_dwmci_tuning_drv_st(void *data, u32 slot_id)
{
	struct dw_mci *host = (struct dw_mci *)data;
	struct drv_strength *drv_st = &host->pdata->__drv_st;
	unsigned int gpio = drv_st->pin;
	s5p_gpio_drvstr_t next_ds[4] = {
					S5P_GPIO_DRVSTR_LV4,	/* LV1 -> LV4 */
					S5P_GPIO_DRVSTR_LV2,	/* LV3 -> LV2 */
					S5P_GPIO_DRVSTR_LV1,	/* LV2 -> LV1 */
					S5P_GPIO_DRVSTR_LV3};	/* LV4 -> LV3 */

	s5p_gpio_set_drvstr(gpio, next_ds[s5p_gpio_get_drvstr(gpio)]);
}

static void exynos_dwmci0_set_data_gpio_pullup(unsigned int pin)
{
#if defined(CONFIG_MACH_WC1)
	extern unsigned int system_rev;

	if (system_rev > 0x06)
		s3c_gpio_setpull(pin, S3C_GPIO_PULL_UP);
	else
		s3c_gpio_setpull(pin, S3C_GPIO_PULL_NONE);
#else
	s3c_gpio_setpull(pin, S3C_GPIO_PULL_NONE);
#endif
}

static void exynos_dwmci0_cfg_gpio(int width)
{
	unsigned int gpio;

	for (gpio = EXYNOS3_GPK0(0);
			gpio < EXYNOS3_GPK0(2); gpio++) {
		s3c_gpio_cfgpin(gpio, S3C_GPIO_SFN(2));
		s3c_gpio_setpull(gpio, S3C_GPIO_PULL_NONE);
		s5p_gpio_set_drvstr(gpio, S5P_GPIO_DRVSTR_LV4);
	}

	switch (width) {
	case 8:
		for (gpio = EXYNOS3_GPL0(0);
				gpio <= EXYNOS3_GPL0(3); gpio++) {
			s3c_gpio_cfgpin(gpio, S3C_GPIO_SFN(2));
			exynos_dwmci0_set_data_gpio_pullup(gpio);
			s5p_gpio_set_drvstr(gpio, S5P_GPIO_DRVSTR_LV4);
		}
	case 4:
		for (gpio = EXYNOS3_GPK0(3);
				gpio <= EXYNOS3_GPK0(6); gpio++) {
			s3c_gpio_cfgpin(gpio, S3C_GPIO_SFN(2));
			exynos_dwmci0_set_data_gpio_pullup(gpio);
			s5p_gpio_set_drvstr(gpio, S5P_GPIO_DRVSTR_LV4);
		}
		break;
	case 1:
		gpio = EXYNOS3_GPK0(3);
		s3c_gpio_cfgpin(gpio, S3C_GPIO_SFN(2));
		exynos_dwmci0_set_data_gpio_pullup(gpio);
		s5p_gpio_set_drvstr(gpio, S5P_GPIO_DRVSTR_LV4);
	default:
		break;
	}

	gpio = EXYNOS3_GPK0(7);
	s3c_gpio_cfgpin(gpio, S3C_GPIO_SFN(2));
	s3c_gpio_setpull(gpio, S3C_GPIO_PULL_DOWN);

	/* CDn PIN: Ouput / High */
	gpio = EXYNOS3_GPK0(2);
	s3c_gpio_cfgpin(gpio, S3C_GPIO_SFN(1));
	gpio_set_value(gpio, 1);
}

static void exynos_dw_mci_control_power(unsigned int width, int enable)
{
	unsigned int gpio;

	if (!enable) {
		for (gpio = EXYNOS3_GPK0(0);
				gpio < EXYNOS3_GPK0(2); gpio++) {
			s3c_gpio_cfgpin(gpio, S3C_GPIO_INPUT);
			s3c_gpio_setpull(gpio, S3C_GPIO_PULL_NONE);
			s5p_gpio_set_drvstr(gpio, S5P_GPIO_DRVSTR_LV4);
		}

		switch (width) {
		case 8:
			for (gpio = EXYNOS3_GPL0(0);
					gpio <= EXYNOS3_GPL0(3); gpio++) {
				s3c_gpio_cfgpin(gpio, S3C_GPIO_INPUT);
				exynos_dwmci0_set_data_gpio_pullup(gpio);
				s5p_gpio_set_drvstr(gpio, S5P_GPIO_DRVSTR_LV4);
			}
		case 4:
			for (gpio = EXYNOS3_GPK0(3);
					gpio <= EXYNOS3_GPK0(6); gpio++) {
				s3c_gpio_cfgpin(gpio, S3C_GPIO_INPUT);
				exynos_dwmci0_set_data_gpio_pullup(gpio);
				s5p_gpio_set_drvstr(gpio, S5P_GPIO_DRVSTR_LV4);
			}
		}

		gpio = EXYNOS3_GPK0(7);
		s3c_gpio_cfgpin(gpio, S3C_GPIO_INPUT);
		s3c_gpio_setpull(gpio, S3C_GPIO_PULL_DOWN);

		gpio = EXYNOS3_GPK0(2);
		s3c_gpio_cfgpin(gpio, S3C_GPIO_SFN(1));
		gpio_set_value(gpio, 0);
	} else {
		exynos_dwmci0_cfg_gpio(width);
	}
}

static struct dw_mci_board universal3250_dwmci0_pdata __initdata = {
	.num_slots		= 1,
	.ch_num			= 0,
	.quirks			= DW_MCI_QUIRK_BROKEN_CARD_DETECTION |
				  DW_MCI_QUIRK_HIGHSPEED |
				  DW_MCI_QUIRK_NO_DETECT_EBIT |
				  DW_MMC_QUIRK_FIXED_VOLTAGE,
	.bus_hz			= 400 * 1000 * 1000 / 4,
	.caps			= MMC_CAP_CMD23 | MMC_CAP_8_BIT_DATA |
				  MMC_CAP_UHS_DDR50 | MMC_CAP_1_8V_DDR |
				  MMC_CAP_ERASE | MMC_CAP_NONREMOVABLE,
	.desc_sz		= 4,
	.qos_int_level		= 100 * 1000,
	.fifo_depth		= 0x80,
	.detect_delay_ms	= 200,
	.only_once_tune		= true,
	.hclk_name		= "dwmci",
	.cclk_name		= "sclk_dwmci",
	.cclk_name2		= "dout_sclk_mmc0",
	.cfg_gpio		= exynos_dwmci0_cfg_gpio,
	.get_bus_wd		= exynos_dwmci0_get_bus_wd,
	.save_drv_st		= exynos_dwmci_save_drv_st,
	.restore_drv_st		= exynos_dwmci_restore_drv_st,
	.tuning_drv_st		= exynos_dwmci_tuning_drv_st,
	.sdr_timing		= 0x03020001,
	.ddr_timing		= 0x03040002,
	.clk_drv		= 0x3,
	.ddr200_timing		= 0x01020002,
	.clk_tbl		= exynos_dwmci_clk_rates_for_mpll_pre_div,
	.__drv_st		= {
		.pin			= EXYNOS3_GPK0(0),
		.val			= S5P_GPIO_DRVSTR_LV4,
	},
	.poweroff_delay		= 50,
	.control_power		= exynos_dw_mci_control_power,
};

#ifdef CONFIG_WLAN
static int exynos_dwmci1_get_bus_wd(u32 slot_id)
{
	return 4;
}

static void exynos_dwmci1_cfg_gpio(int width)
{
	unsigned int gpio;

	for (gpio = EXYNOS3_GPK1(0); gpio < EXYNOS3_GPK1(3); gpio++) {
		s3c_gpio_cfgpin(gpio, S3C_GPIO_SFN(2));
		if (gpio == EXYNOS3_GPK1(0))
			s3c_gpio_setpull(gpio, S3C_GPIO_PULL_NONE);
		else
			s3c_gpio_setpull(gpio, S3C_GPIO_PULL_UP);
		s5p_gpio_set_drvstr(gpio, S5P_GPIO_DRVSTR_LV4);
	}

	switch (width) {
	case 8:
	case 4:
		for (gpio = EXYNOS3_GPK1(3);
				gpio <= EXYNOS3_GPK1(6); gpio++) {
			s3c_gpio_cfgpin(gpio, S3C_GPIO_SFN(2));
			s3c_gpio_setpull(gpio, S3C_GPIO_PULL_UP);
			s5p_gpio_set_drvstr(gpio, S5P_GPIO_DRVSTR_LV4);
		}
		break;
	case 1:
		gpio = EXYNOS3_GPK1(3);
		s3c_gpio_cfgpin(gpio, S3C_GPIO_SFN(2));
		s3c_gpio_setpull(gpio, S3C_GPIO_PULL_UP);
		s5p_gpio_set_drvstr(gpio, S5P_GPIO_DRVSTR_LV4);
	default:
		break;
	}
}

static void (*wlan_notify_func)(struct platform_device *dev, int state);
static DEFINE_MUTEX(wlan_mutex_lock);

static int ext_cd_init_wlan(
	void (*notify_func)(struct platform_device *dev, int state))
{
	
	pr_err("%s: enter\n", __func__);
	mutex_lock(&wlan_mutex_lock);
	WARN_ON(wlan_notify_func);
	wlan_notify_func = notify_func;
	mutex_unlock(&wlan_mutex_lock);

	return 0;
}

static int ext_cd_cleanup_wlan(
	void (*notify_func)(struct platform_device *dev, int state))
{
	
	pr_err("%s: enter\n", __func__);
	mutex_lock(&wlan_mutex_lock);
	WARN_ON(wlan_notify_func);
	wlan_notify_func = NULL;
	mutex_unlock(&wlan_mutex_lock);

	return 0;
}

void mmc_force_presence_change(struct platform_device *pdev, int val)
{
	void (*notify_func)(struct platform_device *, int state) = NULL;

	mutex_lock(&wlan_mutex_lock);

	if (pdev == &exynos4_device_dwmci1) {
		pr_err("%s: called for device exynos4_device_dwmci1\n", __func__);
		notify_func = wlan_notify_func;
	} else
		pr_err("%s: called for device with no notifier, t\n", __func__);


	if (notify_func)
		notify_func(pdev, val);
	else
		pr_err("%s: called for device with no notifier\n", __func__);

	mutex_unlock(&wlan_mutex_lock);
}
EXPORT_SYMBOL_GPL(mmc_force_presence_change);

static struct dw_mci_board universal3250_dwmci1_pdata __initdata = {
	.num_slots		= 1,
	.ch_num			= 1,
	.quirks			= DW_MCI_QUIRK_HIGHSPEED,
	.bus_hz			= 50 * 1000 * 1000,
	.caps			= MMC_CAP_SD_HIGHSPEED | MMC_CAP_4_BIT_DATA,
	.caps2			= MMC_CAP2_BROKEN_VOLTAGE,
	.pm_caps		= MMC_PM_KEEP_POWER | MMC_PM_IGNORE_PM_NOTIFY,
	.fifo_depth		= 0x100,
	.detect_delay_ms	= 200,
	.only_once_tune		= true,
	.hclk_name		= "dwmci",
	.cclk_name		= "sclk_dwmci",
	.cfg_gpio		= exynos_dwmci1_cfg_gpio,
	.get_bus_wd		= exynos_dwmci1_get_bus_wd,
	.ext_cd_init		= ext_cd_init_wlan,
	.ext_cd_cleanup		= ext_cd_cleanup_wlan,
	.cd_type		= DW_MCI_CD_EXTERNAL,
	.sdr_timing		= 0x03020001,
	.ddr_timing		= 0x03030002,
	.clk_drv		= 0x3,
	.clk_tbl		= exynos_dwmci_clk_rates_for_mpll_pre_div,
};
#endif

static struct platform_device *universal3250_mmc_devices[] __initdata = {
	&exynos4_device_dwmci0,
#ifdef CONFIG_WLAN
	&exynos4_device_dwmci1,
#endif
};

void __init exynos3_universal3250_mmc_init(void)
{
	exynos_dwmci_set_platdata(&universal3250_dwmci0_pdata, 0);
#ifdef CONFIG_WLAN
	exynos_dwmci_set_platdata(&universal3250_dwmci1_pdata, 1);
#endif
	platform_add_devices(universal3250_mmc_devices,
			ARRAY_SIZE(universal3250_mmc_devices));

}
