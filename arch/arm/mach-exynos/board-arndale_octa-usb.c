/*
 * Copyright (c) 2013 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/interrupt.h>
#include <linux/gpio.h>
#include <linux/platform_device.h>
#include <linux/clk.h>
#include <linux/platform_data/dwc3-exynos.h>

#include <plat/ehci.h>
#include <plat/devs.h>
#include <plat/usb-phy.h>
#include <plat/gpio-cfg.h>

#include <mach/ohci.h>
#include <mach/usb3-drd.h>
#include <mach/usb-switch.h>

static int arndale_octa_vbus_ctrl(struct platform_device *pdev, int on)
{
	int phy_num = pdev->id;
	unsigned gpio;
	int ret = -EINVAL;

	if (phy_num == 0)
		gpio = EXYNOS5420_GPG0(5);
	else if (phy_num == 1)
		gpio = EXYNOS5420_GPG1(4);
	else
		return ret;

	ret = gpio_request(gpio, "UDRD3_VBUSCTRL");
	if (ret < 0) {
		pr_err("failed to request UDRD3_%d_VBUSCTRL\n",
				phy_num);
		return ret;
	}

	gpio_set_value(gpio, !!on);
	gpio_free(gpio);

	return ret;
}

#define ARNDALE_OCTA_ID0_GPIO	EXYNOS5420_GPX1(7)
#define ARNDALE_OCTA_ID1_GPIO	EXYNOS5420_GPX2(4)
#define ARNDALE_OCTA_VBUS0_GPIO	EXYNOS5420_GPX0(6)
#define ARNDALE_OCTA_VBUS1_GPIO	EXYNOS5420_GPX2(3)

static int arndale_octa_get_id_state(struct platform_device *pdev)
{
	int phy_num = pdev->id;
	unsigned gpio;

	if (phy_num == 0)
		gpio = ARNDALE_OCTA_ID0_GPIO;
	else if (phy_num == 1)
		gpio = ARNDALE_OCTA_ID1_GPIO;
	else
		return -EINVAL;

	return gpio_get_value(gpio);
}

static bool arndale_octa_get_bsession_valid(struct platform_device *pdev)
{
	int phy_num = pdev->id;
	unsigned gpio;

	if (phy_num == 0)
		gpio = ARNDALE_OCTA_VBUS0_GPIO;
	else if (phy_num == 1)
		gpio = ARNDALE_OCTA_VBUS1_GPIO;
	else
		/*
		 * If something goes wrong, we return true,
		 * because we don't want switch stop working.
		 */
		return true;

	return !!gpio_get_value(gpio);
}

static struct exynos4_ohci_platdata arndale_octa_ohci_pdata __initdata;
static struct s5p_ehci_platdata arndale_octa_ehci_pdata __initdata;
static struct dwc3_exynos_data arndale_octa_drd_pdata __initdata = {
	.udc_name		= "exynos-ss-udc",
	.xhci_name		= "exynos-xhci",
	.phy_type		= S5P_USB_PHY_DRD,
	.phy_init		= s5p_usb_phy_init,
	.phy_exit		= s5p_usb_phy_exit,
	.phy_crport_ctrl	= exynos5_usb_phy_crport_ctrl,
	.vbus_ctrl		= arndale_octa_vbus_ctrl,
	.get_id_state		= arndale_octa_get_id_state,
	.get_bses_vld		= arndale_octa_get_bsession_valid,
	.irq_flags		= IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING,
};

static void __init arndale_octa_ohci_init(void)
{
	exynos4_ohci_set_platdata(&arndale_octa_ohci_pdata);
}

static void __init arndale_octa_ehci_init(void)
{
	s5p_ehci_set_platdata(&arndale_octa_ehci_pdata);
}

static void __init arndale_octa_drd_phy_shutdown(struct platform_device *pdev)
{
	int phy_num = pdev->id;
	struct clk *clk;

	switch (phy_num) {
	case 0:
		clk = clk_get_sys("exynos-dwc3.0", "usbdrd30");
		break;
	case 1:
		clk = clk_get_sys("exynos-dwc3.1", "usbdrd30");
		break;
	default:
		clk = NULL;
		break;
	}

	if (IS_ERR_OR_NULL(clk)) {
		pr_err("failed to get DRD%d phy clock\n", phy_num);
		return;
	}

	if (clk_enable(clk)) {
		pr_err("failed to enable DRD%d clock\n", phy_num);
		return;
	}

	s5p_usb_phy_exit(pdev, S5P_USB_PHY_DRD);

	clk_disable(clk);
}

static void __init __maybe_unused arndale_octa_drd0_init(void)
{
	/* Initialize DRD0 gpio */
	if (gpio_request_one(EXYNOS5420_GPG0(5), GPIOF_OUT_INIT_LOW,
				"UDRD3_0_VBUSCTRL")) {
		pr_err("failed to request UDRD3_0_VBUSCTRL\n");
	} else {
		s3c_gpio_setpull(EXYNOS5420_GPG0(5), S3C_GPIO_PULL_NONE);
		gpio_free(EXYNOS5420_GPG0(5));
	}

	if (gpio_request_one(ARNDALE_OCTA_ID0_GPIO, GPIOF_IN, "UDRD3_0_ID")) {
		pr_err("failed to request UDRD3_0_ID\n");
		arndale_octa_drd_pdata.id_irq = -1;
	} else {
		s3c_gpio_cfgpin(ARNDALE_OCTA_ID0_GPIO, S3C_GPIO_SFN(0xF));
		s3c_gpio_setpull(ARNDALE_OCTA_ID0_GPIO, S3C_GPIO_PULL_NONE);
		gpio_free(ARNDALE_OCTA_ID0_GPIO);

		arndale_octa_drd_pdata.id_irq = gpio_to_irq(ARNDALE_OCTA_ID0_GPIO);
	}

	if (gpio_request_one(ARNDALE_OCTA_VBUS0_GPIO, GPIOF_IN, "UDRD3_0_VBUS")) {
		pr_err("failed to request UDRD3_0_VBUS\n");
		arndale_octa_drd_pdata.vbus_irq = -1;
	} else {
		s3c_gpio_cfgpin(ARNDALE_OCTA_VBUS0_GPIO, S3C_GPIO_SFN(0xF));
		s3c_gpio_setpull(ARNDALE_OCTA_VBUS0_GPIO, S3C_GPIO_PULL_NONE);
		gpio_free(ARNDALE_OCTA_VBUS0_GPIO);

		arndale_octa_drd_pdata.vbus_irq = gpio_to_irq(ARNDALE_OCTA_VBUS0_GPIO);
	}

	arndale_octa_drd_pdata.quirks = 0;
#if !defined(CONFIG_USB_XHCI_EXYNOS)
	arndale_octa_drd_pdata.quirks |= SKIP_XHCI;
#endif
#if !defined(CONFIG_USB_EXYNOS5_USB3_DRD_CH0)
	arndale_octa_drd_pdata.quirks |= SKIP_UDC;
#elif !defined(CONFIG_USB_SUSPEND) || !defined(CONFIG_USB_XHCI_EXYNOS)
	arndale_octa_drd_pdata.quirks |= (FORCE_RUN_PERIPHERAL | SKIP_XHCI);
#endif
	arndale_octa_drd_pdata.quirks |= LOW_VBOOST;

	exynos5_usb3_drd0_set_platdata(&arndale_octa_drd_pdata);
}

static void __init __maybe_unused arndale_octa_drd1_init(void)
{
	/* Initialize DRD1 gpio */
	if (gpio_request_one(EXYNOS5420_GPG1(4), GPIOF_OUT_INIT_LOW,
				"UDRD3_1_VBUSCTRL")) {
		pr_err("failed to request UDRD3_1_VBUSCTRL\n");
	} else {
		s3c_gpio_setpull(EXYNOS5420_GPG1(4), S3C_GPIO_PULL_NONE);
		gpio_free(EXYNOS5420_GPG1(4));
	}

	if (gpio_request_one(ARNDALE_OCTA_ID1_GPIO, GPIOF_IN, "UDRD3_1_ID")) {
		pr_err("failed to request UDRD3_1_ID\n");
		arndale_octa_drd_pdata.id_irq = -1;
	} else {
		s3c_gpio_cfgpin(ARNDALE_OCTA_ID1_GPIO, S3C_GPIO_SFN(0xF));
		s3c_gpio_setpull(ARNDALE_OCTA_ID1_GPIO, S3C_GPIO_PULL_NONE);
		gpio_free(ARNDALE_OCTA_ID1_GPIO);

		arndale_octa_drd_pdata.id_irq = gpio_to_irq(ARNDALE_OCTA_ID1_GPIO);
	}

	if (gpio_request_one(ARNDALE_OCTA_VBUS1_GPIO, GPIOF_IN, "UDRD3_1_VBUS")) {
		pr_err("failed to request UDRD3_1_VBUS\n");
		arndale_octa_drd_pdata.vbus_irq = -1;
	} else {
		s3c_gpio_cfgpin(ARNDALE_OCTA_VBUS1_GPIO, S3C_GPIO_SFN(0xF));
		s3c_gpio_setpull(ARNDALE_OCTA_VBUS1_GPIO, S3C_GPIO_PULL_NONE);
		gpio_free(ARNDALE_OCTA_VBUS1_GPIO);

		arndale_octa_drd_pdata.vbus_irq = gpio_to_irq(ARNDALE_OCTA_VBUS1_GPIO);
	}

	arndale_octa_drd_pdata.quirks = 0;
#if !defined(CONFIG_USB_XHCI_EXYNOS)
	arndale_octa_drd_pdata.quirks |= SKIP_XHCI;
#endif
#if !defined(CONFIG_USB_EXYNOS5_USB3_DRD_CH1)
	arndale_octa_drd_pdata.quirks |= SKIP_UDC;
#elif !defined(CONFIG_USB_SUSPEND) || !defined(CONFIG_USB_XHCI_EXYNOS)
	arndale_octa_drd_pdata.quirks |= (FORCE_RUN_PERIPHERAL | SKIP_XHCI);
#endif
	arndale_octa_drd_pdata.quirks |= LOW_VBOOST;

	exynos5_usb3_drd1_set_platdata(&arndale_octa_drd_pdata);
}

#ifdef CONFIG_USB_EXYNOS_SWITCH
static struct s5p_usbswitch_platdata arndale_octa_usbswitch_pdata __initdata;

static void __init arndale_octa_usbswitch_init(void)
{
	struct s5p_usbswitch_platdata *pdata = &arndale_octa_usbswitch_pdata;
	int err;

#if defined(CONFIG_USB_EHCI_S5P) || defined(CONFIG_USB_OHCI_EXYNOS)
	pdata->gpio_host_detect = EXYNOS5420_GPX1(6);
	err = gpio_request_one(pdata->gpio_host_detect, GPIOF_IN,
			"HOST_DETECT");
	if (err) {
		pr_err("failed to request host gpio\n");
		return;
	}

	s3c_gpio_cfgpin(pdata->gpio_host_detect, S3C_GPIO_SFN(0xF));
	s3c_gpio_setpull(pdata->gpio_host_detect, S3C_GPIO_PULL_NONE);
	gpio_free(pdata->gpio_host_detect);

	pdata->gpio_host_vbus = 0;
#endif

	s5p_usbswitch_set_platdata(pdata);
}
#endif

static struct platform_device *arndale_octa_usb_devices[] __initdata = {
	&exynos4_device_ohci,
	&s5p_device_ehci,
	&exynos5_device_usb3_drd0,
	&exynos5_device_usb3_drd1,
};

void __init exynos5_arndale_octa_usb_init(void)
{
	arndale_octa_ohci_init();
	arndale_octa_ehci_init();

	/*
	 * Shutdown DRD PHYs to reduce power consumption.
	 * Later, DRD driver will turn on only the PHY it needs.
	 */
	arndale_octa_drd_phy_shutdown(&exynos5_device_usb3_drd0);
	arndale_octa_drd_phy_shutdown(&exynos5_device_usb3_drd1);
	arndale_octa_drd0_init();
	arndale_octa_drd1_init();
#ifdef CONFIG_USB_EXYNOS_SWITCH
	arndale_octa_usbswitch_init();
#endif
	platform_add_devices(arndale_octa_usb_devices,
			ARRAY_SIZE(arndale_octa_usb_devices));
}
