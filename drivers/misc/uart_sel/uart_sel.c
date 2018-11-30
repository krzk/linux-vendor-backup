/*
 * Copyright (C) 2014 Samsung Electronics Co.Ltd
 * http://www.samsung.com
 *
 * UART Switch driver
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
*/

#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/miscdevice.h>
#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/delay.h>
#include <soc/samsung/pmu-cp.h>
#include <soc/samsung/exynos-pmu.h>
#ifdef CONFIG_SEC_SYSFS
#include <linux/sec_sysfs.h>
#endif

#ifdef CONFIG_MUIC_NOTIFIER
#include <linux/muic/muic.h>
#include <linux/muic/muic_notifier.h>
#endif

#include <linux/mcu_ipc.h>
#include <linux/uart_sel.h>

enum uart_direction_t uart_dir = AP;
struct uart_sel_data *switch_data = NULL;

static void uart_dir_work(void);
#ifdef __UART_SEL_DEBUG_
int test_cp_uart_set_read(void)
{
#define CP_UART_CFG_ADDR	0x11860760

	int ret = 0;
	void __iomem *CP_UART_CFG_AKQ;

	CP_UART_CFG_AKQ = devm_ioremap(switch_data->dev, CP_UART_CFG_ADDR, SZ_64);

	usel_err("__raw_readl(CP_UART_CFG_AKQ): 0x%08x\n", __raw_readl(CP_UART_CFG_AKQ));

	devm_iounmap(switch_data->dev, CP_UART_CFG_AKQ);

	return ret;
}
#endif

#ifdef CONFIG_SOC_EXYNOS7570
void config_pmu_sel(int path)
{
	unsigned int func_iso_en, sel_uart_dbg_gpio, usbdev_phy_enable;
	int ret	= 0;

	pr_info("%s: Changing path to %s\n", __func__, path ? "CP" : "AP");

	if (path == AP) {	/* AP */
		func_iso_en = 0;
		sel_uart_dbg_gpio = SEL_UART_DBG_GPIO;
		usbdev_phy_enable = 0;
	} else {		/* CP */
		func_iso_en = FUNC_ISO_EN;
		sel_uart_dbg_gpio = 0;
		usbdev_phy_enable = USBDEV_PHY_ENABLE;
	}

	ret = exynos_pmu_update(EXYNOS_PMU_UART_IO_SHARE_CTRL,
				FUNC_ISO_EN, func_iso_en);
	if (ret < 0)
		usel_err("%s: ERR(%d) set FUNC_ISO_EN\n", __func__, ret);

	ret = exynos_pmu_update(EXYNOS_PMU_UART_IO_SHARE_CTRL,
				SEL_UART_DBG_GPIO, sel_uart_dbg_gpio);
	if (ret < 0)
		usel_err("%s: ERR(%d) set SEL_UART_DBG_GPIO\n", __func__, ret);

	ret = exynos_pmu_update(PMU_USBDEV_PHY_CONTROL,
				USBDEV_PHY_ENABLE, usbdev_phy_enable);
	if (ret < 0)
		usel_err("%s: ERR(%d) set USBDEV_PHY_ENABLE\n", __func__, ret);
}
#elif defined (CONFIG_SOC_EXYNOS9110)
int config_pmu_uart_sel(int path)
{
	unsigned int sel_txd_usb_phy, sel_txd_gpio_0, sel_txd_gpio_1, sel_rxd_ap_uart, sel_rxd_cp_uart;
	unsigned int func_ios_en, enable_usb20;
	int ret	= 0;

	usel_err("%s: Changing path to %s\n", __func__, path ? "CP" : "AP");

	if (path == AP) {	/* AP 0x00123000 | 0x00113001 */
		sel_txd_usb_phy = 0x0;
		sel_txd_gpio_0 = 0x0;
		sel_txd_gpio_1 = 0x1;
		sel_rxd_ap_uart = (switch_data->use_usb_phy) ? 0x1 : 0x2;
		sel_rxd_cp_uart = (switch_data->use_usb_phy) ? 0x0 : 0x0;
	} else {		/* CP 0x11032000 | 0x11031001 */
		sel_txd_usb_phy = 0x1;
		sel_txd_gpio_0 = 0x1;
		sel_txd_gpio_1 = 0x0;
		sel_rxd_ap_uart = (switch_data->use_usb_phy) ? 0x0 : 0x0;
		sel_rxd_cp_uart = (switch_data->use_usb_phy) ? 0x1 : 0x2;
	}
	func_ios_en = (switch_data->use_usb_phy) ? 0x1 : 0x0;

	/* SEL_TXD_USB_PHY */
	ret = exynos_pmu_update(EXYNOS_PMU_UART_IO_SHARE_CTRL,
				SEL_TXD_USB_PHY_MASK, sel_txd_usb_phy << SEL_TXD_USB_PHY_POS);
	if (ret < 0) {
		usel_err("%s: ERR(%d) set SEL_TXD_USB_PHY\n", __func__, ret);
		return ret;
	}
	/* SEL_TXD_GPIO_0 */
	ret = exynos_pmu_update(EXYNOS_PMU_UART_IO_SHARE_CTRL,
				SEL_TXD_GPIO_0_MASK, sel_txd_gpio_0 << SEL_TXD_GPIO_0_POS);
	if (ret < 0) {
		usel_err("%s: ERR(%d) set SEL_TXD_GPIO_0\n", __func__, ret);
		return ret;
	}

	/* SEL_TXD_GPIO_1 */
	ret = exynos_pmu_update(EXYNOS_PMU_UART_IO_SHARE_CTRL,
				SEL_TXD_GPIO_1_MASK, sel_txd_gpio_1 << SEL_TXD_GPIO_1_POS);
	if (ret < 0) {
		usel_err("%s: ERR(%d) set SEL_TXD_GPIO_1\n", __func__, ret);
		return ret;
	}

	/* SEL_RXD_AP_UART */
	ret = exynos_pmu_update(EXYNOS_PMU_UART_IO_SHARE_CTRL,
				SEL_RXD_AP_UART_MASK, sel_rxd_ap_uart << SEL_RXD_AP_UART_POS);
	if (ret < 0) {
		usel_err("%s: ERR(%d) set SEL_RXD_AP_UART\n", __func__, ret);
		return ret;
	}

	/* SEL_RXD_CP_UART */
	ret = exynos_pmu_update(EXYNOS_PMU_UART_IO_SHARE_CTRL,
				SEL_RXD_CP_UART_MASK, sel_rxd_cp_uart << SEL_RXD_CP_UART_POS);
	if (ret < 0) {
		usel_err("%s: ERR(%d) set SEL_RXD_CP_UART\n", __func__, ret);
		return ret;
	}

	/* FUNC_ISO_EN */
	ret = exynos_pmu_update(EXYNOS_PMU_UART_IO_SHARE_CTRL,
				FUNC_ISO_EN_MASK, func_ios_en << FUNC_ISO_EN_POS);
	if (ret < 0) {
		usel_err("%s: ERR(%d) set FUNC_ISO_EN\n", __func__, ret);
		return ret;
	}

	if (switch_data->use_usb_phy) {
		/* ENABLE_USB20 */
		enable_usb20 = 0x1;
		ret = exynos_pmu_update(EXYNOS_PMU_USBDEV_PHY_CONTROL,
					ENABLE_USB20_MASK, enable_usb20 << ENABLE_USB20_POS);
		if (ret < 0) {
			usel_err("%s: ERR(%d) set ENABLE_USB20\n", __func__, ret);
			return ret;
		}
	}

#ifdef __UART_SEL_DEBUG_
	//check value
	test_cp_uart_set_read();
#endif

	return ret;
}

#endif

static int set_uart_sel(void)
{
	int ret;

	if (switch_data->uart_switch_sel == CP) {
		usel_err("Change Uart to CP\n");
#ifdef CONFIG_SOC_EXYNOS7570
		ret = exynos_pmu_update(EXYNOS_PMU_UART_IO_SHARE_CTRL,
			SEL_CP_UART_DBG, 0);
#elif defined CONFIG_SOC_EXYNOS9110
		ret = config_pmu_uart_sel(CP);
#endif
		if (ret < 0)
			usel_err("%s: ERR! write Fail: %d\n", __func__, ret);
	} else {
		usel_err("Change UART to AP\n");
#ifdef CONFIG_SOC_EXYNOS7570
		ret = exynos_pmu_update(EXYNOS_PMU_UART_IO_SHARE_CTRL,
			SEL_CP_UART_DBG, SEL_CP_UART_DBG);
#elif defined CONFIG_SOC_EXYNOS9110
		ret = config_pmu_uart_sel(AP);
#endif
		if (ret < 0)
			usel_err("%s: ERR! write Fail: %d\n", __func__, ret);
	}
	uart_dir_work();

	return 0;
}

static int uart_direction(char *str)
{
	uart_dir = strstr(str, "CP") ? CP : AP;
	usel_err("%s: uart direction: %s (%d)\n", __func__, str, uart_dir);

	return 0;
}
__setup("uart_sel=", uart_direction);

static void uart_dir_work(void)
{
#if defined(CONFIG_SEC_SIPC_MODEM_IF)  || defined(CONFIG_SEC_MODEM_IF)
	u32 info_value = 0;

	if (switch_data == NULL)
		return;

	info_value = (switch_data->uart_connect && switch_data->uart_switch_sel);

	if (info_value != mbox_extract_value(MCU_CP, switch_data->mbx_ap_united_status,
				switch_data->sbi_uart_noti_mask, switch_data->sbi_uart_noti_pos)) {
		usel_err("%s: change uart state to %s\n", __func__,
			info_value ? "CP" : "AP");

		mbox_update_value(MCU_CP, switch_data->mbx_ap_united_status, info_value,
			switch_data->sbi_uart_noti_mask, switch_data->sbi_uart_noti_pos);
		if (mbox_extract_value(MCU_CP, switch_data->mbx_ap_united_status,
			switch_data->sbi_uart_noti_mask, switch_data->sbi_uart_noti_pos))
			mbox_set_interrupt(MCU_CP, switch_data->int_uart_noti);
	}
#endif
}

#if defined(CONFIG_SEC_SIPC_MODEM_IF)  || defined(CONFIG_SEC_MODEM_IF)
void cp_recheck_uart_dir(void)
{
	u32 mbx_uart_noti;

	mbx_uart_noti = mbox_extract_value(MCU_CP, switch_data->mbx_ap_united_status,
			switch_data->sbi_uart_noti_mask, switch_data->sbi_uart_noti_pos);
	if (switch_data->uart_switch_sel != mbx_uart_noti)
		usel_err("Uart notifier data is not matched with mbox!\n");

	if (uart_dir == CP) {
		switch_data->uart_connect = true;
		switch_data->uart_switch_sel = CP;
		usel_err("Forcely changed to CP uart!!\n");
		set_uart_sel();
	}
	uart_dir_work();
}
EXPORT_SYMBOL_GPL(cp_recheck_uart_dir);
#endif

#ifdef CONFIG_MUIC_NOTIFIER
static int switch_handle_notification(struct notifier_block *nb,
				unsigned long action, void *data)
{
	muic_attached_dev_t attached_dev = *(muic_attached_dev_t *)data;

	usel_err("%s: action=%lu attached_dev=%d\n", __func__, action, (int)attached_dev);

	if ((attached_dev == ATTACHED_DEV_JIG_UART_OFF_MUIC) ||
		(attached_dev == ATTACHED_DEV_JIG_UART_ON_MUIC) ||
		(attached_dev == ATTACHED_DEV_JIG_UART_OFF_VB_MUIC)) {
		switch (action) {
		case MUIC_NOTIFY_CMD_DETACH:
		case MUIC_NOTIFY_CMD_LOGICALLY_DETACH:
			switch_data->uart_connect = false;
			uart_dir_work();
			break;
		case MUIC_NOTIFY_CMD_ATTACH:
		case MUIC_NOTIFY_CMD_LOGICALLY_ATTACH:
			switch_data->uart_connect = true;
			set_uart_sel();
			break;
			}
	}

	return 0;
}
#endif

static ssize_t usb_sel_show(struct device *dev,
					   struct device_attribute *attr,
					   char *buf)
{
	return sprintf(buf, "PDA\n");
}

static ssize_t usb_sel_store(struct device *dev,
					  struct device_attribute *attr,
					  const char *buf, size_t count)
{
	return count;
}

static ssize_t
uart_sel_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	ssize_t ret = 0;

	ret += snprintf(buf + ret, PAGE_SIZE - ret, "Uart direction: ");

	if (ret < PAGE_SIZE - 1) {
#ifdef CONFIG_SOC_EXYNOS7570
		u32 uart_ctrl;
		exynos_pmu_read(EXYNOS_PMU_UART_IO_SHARE_CTRL, &uart_ctrl);
		ret += snprintf(buf + ret, PAGE_SIZE - ret, (uart_ctrl & SEL_CP_UART_DBG) ?
			"AP\n":"CP\n");
#elif defined CONFIG_SOC_EXYNOS9110
		ret += snprintf(buf + ret, PAGE_SIZE - ret, (switch_data->uart_switch_sel == AP) ?
			"AP\n":"CP\n");
#endif
	} else {
		buf[PAGE_SIZE-2] = '\n';
		buf[PAGE_SIZE-1] = '\0';
		ret = PAGE_SIZE-1;
	}

	return ret;
}

static ssize_t
uart_sel_store(struct device *dev, struct device_attribute *attr,
				const char *buf, size_t count)
{
	usel_err("%s Change UART port path\n", __func__);

	if (!strncasecmp(buf, "AP", 2)) {
		switch_data->uart_switch_sel = AP;
		set_uart_sel();
	} else if (!strncasecmp(buf, "CP", 2)) {
		switch_data->uart_switch_sel = CP;
		set_uart_sel();
	} else {
		usel_err("%s invalid value\n", __func__);
	}

	return count;
}

static DEVICE_ATTR(usb_sel, 0664, usb_sel_show, usb_sel_store);
static DEVICE_ATTR(uart_sel, 0664, uart_sel_show, uart_sel_store);

static struct attribute *modemif_sysfs_attributes[] = {
	&dev_attr_uart_sel.attr,
	&dev_attr_usb_sel.attr,
	NULL
};

static const struct attribute_group uart_sel_sysfs_group = {
	.attrs = modemif_sysfs_attributes,
};

static int uart_sel_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	int err = 0;

	usel_err("%s: uart_sel probe start.\n", __func__);

	switch_data = devm_kzalloc(dev, sizeof(struct uart_sel_data), GFP_KERNEL);
	switch_data->dev = dev;

	err = of_property_read_u32(dev->of_node, "int_ap2cp_uart_noti",
			&switch_data->int_uart_noti);
	if (err) {
		usel_err("SWITCH_SEL parse error! [id]\n");
		return err;
	}
	err = of_property_read_u32(dev->of_node, "mbx_ap2cp_united_status",
			&switch_data->mbx_ap_united_status);
	err = of_property_read_u32(dev->of_node, "sbi_uart_noti_mask",
			&switch_data->sbi_uart_noti_mask);
	err = of_property_read_u32(dev->of_node, "sbi_uart_noti_pos",
			&switch_data->sbi_uart_noti_pos);
	err = of_property_read_u32(dev->of_node, "use_usb_phy",
			&switch_data->use_usb_phy);

	usel_err("SWITCH_SEL use_usb_phy [%d]\n", switch_data->use_usb_phy);

	if (err) {
		usel_err("SWITCH_SEL parse error! [id]\n");
		return err;
	}

	err = device_create_file(dev, &dev_attr_uart_sel);
	if (err) {
		usel_err("can't create uart_sel device file!!!\n");
	}

	switch_data->uart_switch_sel = (uart_dir == AP) ? AP : CP;
	switch_data->uart_connect = false;

#if defined(CONFIG_MUIC_NOTIFIER)
	switch_data->uart_notifier.notifier_call = switch_handle_notification;
	muic_notifier_register(&switch_data->uart_notifier, switch_handle_notification,
					MUIC_NOTIFY_DEV_USB);
#endif

#ifdef CONFIG_SEC_SYSFS
	if (IS_ERR(switch_device)) {
		usel_err("%s Failed to create device(switch)!\n", __func__);
		return -ENODEV;
	}

	/* create sysfs group */
	err = sysfs_create_group(&switch_device->kobj, &uart_sel_sysfs_group);
	if (err) {
		usel_err("%s: failed to create modemif sysfs attribute group\n",
			__func__);
		return -ENODEV;
	}
#endif

	return 0;
}

static int __exit uart_sel_remove(struct platform_device *pdev)
{
	/* TODO */
	return 0;
}

#ifdef CONFIG_PM
static int uart_sel_suspend(struct device *dev)
{
	/* TODO */
	return 0;
}

static int uart_sel_resume(struct device *dev)
{
	/* TODO */
	return 0;
}
#else
#define uart_sel_suspend NULL
#define uart_sel_resume NULL
#endif

static const struct dev_pm_ops uart_sel_pm_ops = {
	.suspend = uart_sel_suspend,
	.resume = uart_sel_resume,
};

static const struct of_device_id exynos_uart_sel_dt_match[] = {
		{ .compatible = "samsung,exynos-uart-sel", },
		{},
};
MODULE_DEVICE_TABLE(of, exynos_uart_sel_dt_match);

static struct platform_driver uart_sel_driver = {
	.probe		= uart_sel_probe,
	.remove		= uart_sel_remove,
	.driver		= {
		.name = "uart_sel",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(exynos_uart_sel_dt_match),
		.pm = &uart_sel_pm_ops,
	},
};
module_platform_driver(uart_sel_driver);

MODULE_DESCRIPTION("UART SEL driver");
MODULE_AUTHOR("<hy50.seo@samsung.com>");
MODULE_LICENSE("GPL");
