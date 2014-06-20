/*
 * SAMSUNG S5P USB HOST EHCI Controller
 *
 * Copyright (C) 2011 Samsung Electronics Co.Ltd
 * Author: Jingoo Han <jg1.han@samsung.com>
 * Author: Joonyoung Shim <jy0922.shim@samsung.com>
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 *
 */

#include <linux/clk.h>
#include <linux/dma-mapping.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/phy/phy.h>
#include <linux/platform_device.h>
#include <linux/platform_data/usb-ehci-s5p.h>
#include <linux/regulator/consumer.h>
#include <linux/usb/phy.h>
#include <linux/usb/samsung_usb_phy.h>
#include <linux/usb.h>
#include <linux/usb/hcd.h>
#include <linux/usb/otg.h>

#include "ehci.h"

#define DRIVER_DESC "EHCI s5p driver"

#define EHCI_INSNREG00(base)			(base + 0x90)
#define EHCI_INSNREG00_ENA_INCR16		(0x1 << 25)
#define EHCI_INSNREG00_ENA_INCR8		(0x1 << 24)
#define EHCI_INSNREG00_ENA_INCR4		(0x1 << 23)
#define EHCI_INSNREG00_ENA_INCRX_ALIGN		(0x1 << 22)
#define EHCI_INSNREG00_ENABLE_DMA_BURST	\
	(EHCI_INSNREG00_ENA_INCR16 | EHCI_INSNREG00_ENA_INCR8 |	\
	 EHCI_INSNREG00_ENA_INCR4 | EHCI_INSNREG00_ENA_INCRX_ALIGN)
#define INSNREG00(base)                         (base + 0x90)
#define ENA_DMA_INCR                            (0xF << 22)
#define OHCI_SUSP_LGCY                          (1 << 20)

static const char hcd_name[] = "ehci-s5p";
static struct hc_driver __read_mostly s5p_ehci_hc_driver;

static const char * const s5p_ehci_supply_names[] = {
	"vusb_d",		/* digital USB supply */
	"vusb_a",		/* analog USB supply */
};

#define PHY_NUMBER 3
struct s5p_ehci_hcd {
	struct clk *clk1;
	struct clk *clk2;
	int power_on;
	struct phy *phy[PHY_NUMBER];
	struct regulator_bulk_data supplies[ARRAY_SIZE(s5p_ehci_supply_names)];
};

#define to_s5p_ehci(hcd)      (struct s5p_ehci_hcd *)(hcd_to_ehci(hcd)->priv)

static int s5p_ehci_phy_enable(struct s5p_ehci_hcd *s5p_ehci)
{
	struct phy **p = s5p_ehci->phy;
	int i;
	int ret = 0;

	for (i = 0; ret == 0 && i < PHY_NUMBER; i++)
		if (p[i])
			ret = phy_power_on(p[i]);
	if (ret)
		for (i--; i > 0; i--)
			if (p[i])
				phy_power_off(p[i]);

	return ret;
}

static int s5p_ehci_phy_disable(struct s5p_ehci_hcd *s5p_ehci)
{
	struct phy **p = s5p_ehci->phy;
	int i;
	int ret = 0;

	for (i = 0; ret == 0 && i < PHY_NUMBER; i++)
		if (p[i])
			ret = phy_power_off(p[i]);

	return ret;
}

static ssize_t show_ehci_power(struct device *dev,
			       struct device_attribute *attr,
			       char *buf)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct usb_hcd *hcd = platform_get_drvdata(pdev);
	struct s5p_ehci_hcd *s5p_ehci = to_s5p_ehci(hcd);

	return sprintf(buf, "EHCI Power %s\n", (s5p_ehci->power_on) ? "on" : "off");
}

static int s5p_ehci_configurate(struct usb_hcd *hcd)
{
	int delay_count = 0;

	/* This is for waiting phy before ehci configuration */
	do {
		if (readl(hcd->regs))
			break;
		udelay(1);
		++delay_count;
	} while (delay_count < 200);
	if (delay_count)
		dev_info(hcd->self.controller, "phy delay count = %d\n",
			delay_count);

	/* DMA burst Enable, set utmi suspend_on_n */
	writel(readl(INSNREG00(hcd->regs)) | ENA_DMA_INCR | OHCI_SUSP_LGCY,
		INSNREG00(hcd->regs));
	return 0;
}


static ssize_t store_ehci_power(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct usb_hcd *hcd = platform_get_drvdata(pdev);
	struct s5p_ehci_hcd *s5p_ehci = to_s5p_ehci(hcd);
	int power_on;
	int irq;
	int retval;

	if (sscanf(buf, "%d", &power_on) != 1)
		return -EINVAL;

	device_lock(dev);
	if (!power_on && s5p_ehci->power_on) {
		printk(KERN_DEBUG "%s: EHCI turns off\n", __func__);
		s5p_ehci->power_on = 0;
		usb_remove_hcd(hcd);

		s5p_ehci_phy_disable(s5p_ehci);
	} else if (power_on) {
		printk(KERN_DEBUG "%s: EHCI turns on\n", __func__);
		if (s5p_ehci->power_on) {
			usb_remove_hcd(hcd);
		}

		s5p_ehci_phy_enable(s5p_ehci);

		s5p_ehci_configurate(hcd);

		irq = platform_get_irq(pdev, 0);
		retval = usb_add_hcd(hcd, irq,
				IRQF_DISABLED | IRQF_SHARED);
		if (retval < 0) {
			dev_err(dev, "Power On Fail\n");
			goto exit;
		}

		s5p_ehci->power_on = 1;
	}
exit:
	device_unlock(dev);
	return count;
}
static DEVICE_ATTR(ehci_power, 0664, show_ehci_power, store_ehci_power);

static inline int create_ehci_sys_file(struct ehci_hcd *ehci)
{
	return device_create_file(ehci_to_hcd(ehci)->self.controller,
			&dev_attr_ehci_power);
}

static inline void remove_ehci_sys_file(struct ehci_hcd *ehci)
{
	device_remove_file(ehci_to_hcd(ehci)->self.controller,
			&dev_attr_ehci_power);
}

static void s5p_setup_vbus_gpio(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	int err;
	int gpio;

	if (!dev->of_node)
		return;

	gpio = of_get_named_gpio(dev->of_node, "samsung,vbus-gpio", 0);
	if (!gpio_is_valid(gpio))
		return;

	err = devm_gpio_request_one(dev, gpio, GPIOF_OUT_INIT_HIGH,
				    "ehci_vbus_gpio");
	if (err)
		dev_err(dev, "can't request ehci vbus gpio %d", gpio);
}

static int s5p_ehci_probe(struct platform_device *pdev)
{
	struct s5p_ehci_hcd *s5p_ehci;
	struct phy *phy;
	struct device_node *child;
	struct usb_hcd *hcd;
	struct ehci_hcd *ehci;
	struct resource *res;
	int phy_number;
	int irq;
	int err;
	int i;

	/*
	 * Right now device-tree probed devices don't get dma_mask set.
	 * Since shared usb code relies on it, set it here for now.
	 * Once we move to full device tree support this will vanish off.
	 */
	if (!pdev->dev.dma_mask)
		pdev->dev.dma_mask = &pdev->dev.coherent_dma_mask;
	if (!pdev->dev.coherent_dma_mask)
		pdev->dev.coherent_dma_mask = DMA_BIT_MASK(32);

	s5p_setup_vbus_gpio(pdev);

	hcd = usb_create_hcd(&s5p_ehci_hc_driver,
			     &pdev->dev, dev_name(&pdev->dev));
	if (!hcd) {
		dev_err(&pdev->dev, "Unable to create HCD\n");
		return -ENOMEM;
	}
	s5p_ehci = to_s5p_ehci(hcd);

	for_each_available_child_of_node(pdev->dev.of_node, child) {
		err = of_property_read_u32(child, "reg", &phy_number);
		if (err) {
			dev_err(&pdev->dev, "Failed to parse device tree\n");
			of_node_put(child);
			return err;
		}
		if (phy_number >= PHY_NUMBER) {
			dev_err(&pdev->dev, "Failed to parse device tree - number out of range\n");
			of_node_put(child);
			return -EINVAL;
		}
		phy = devm_of_phy_get(&pdev->dev, child, 0);
		of_node_put(child);
		if (IS_ERR(phy)) {
			dev_err(&pdev->dev, "Failed to get phy number %d",
								phy_number);
			return PTR_ERR(phy);
		}
		s5p_ehci->phy[phy_number] = phy;
	}

	s5p_ehci->clk1 = devm_clk_get(&pdev->dev, "usbhost");

	if (IS_ERR(s5p_ehci->clk1)) {
		dev_err(&pdev->dev, "Failed to get usbhost clock\n");
		err = PTR_ERR(s5p_ehci->clk1);
		goto fail_clk1;
	}

	err = clk_prepare_enable(s5p_ehci->clk1);
	if (err)
		goto fail_clk1;

	s5p_ehci->clk2 = devm_clk_get(&pdev->dev, "otg");

	if (IS_ERR(s5p_ehci->clk2)) {
		dev_err(&pdev->dev, "Failed to get otg clock\n");
		err = PTR_ERR(s5p_ehci->clk2);
		goto fail_clk2;
	}

	err = clk_prepare_enable(s5p_ehci->clk2);
	if (err)
		goto fail_clk2;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		dev_err(&pdev->dev, "Failed to get I/O memory\n");
		err = -ENXIO;
		goto fail_io;
	}

	hcd->rsrc_start = res->start;
	hcd->rsrc_len = resource_size(res);
	hcd->regs = devm_ioremap(&pdev->dev, res->start, hcd->rsrc_len);
	if (!hcd->regs) {
		dev_err(&pdev->dev, "Failed to remap I/O memory\n");
		err = -ENOMEM;
		goto fail_io;
	}

	irq = platform_get_irq(pdev, 0);
	if (!irq) {
		dev_err(&pdev->dev, "Failed to get IRQ\n");
		err = -ENODEV;
		goto fail_io;
	}

	/* regulators */
	for (i = 0; i < ARRAY_SIZE(s5p_ehci->supplies); i++)
		s5p_ehci->supplies[i].supply = s5p_ehci_supply_names[i];

	err = devm_regulator_bulk_get(&pdev->dev, ARRAY_SIZE(s5p_ehci->supplies),
				 s5p_ehci->supplies);
	if (err) {
		dev_err(&pdev->dev, "Failed to request regulators\n");
		goto fail_get_reg;
	}

	err = regulator_bulk_enable(ARRAY_SIZE(s5p_ehci->supplies),
				    s5p_ehci->supplies);

	if (err) {
		dev_err(&pdev->dev, "Failed to enable regulators\n");
		goto fail_enable_reg;
	}


	s5p_ehci_phy_enable(s5p_ehci);

	ehci = hcd_to_ehci(hcd);
	ehci->caps = hcd->regs;

	/* DMA burst Enable */
	writel(EHCI_INSNREG00_ENABLE_DMA_BURST, EHCI_INSNREG00(hcd->regs));

	err = usb_add_hcd(hcd, irq, IRQF_SHARED);
	if (err) {
		dev_err(&pdev->dev, "Failed to add USB HCD\n");
		goto fail_add_hcd;
	}

	platform_set_drvdata(pdev, hcd);

	create_ehci_sys_file(ehci);
	s5p_ehci->power_on = 1;

	return 0;

fail_add_hcd:
	s5p_ehci_phy_disable(s5p_ehci);
	regulator_bulk_disable(ARRAY_SIZE(s5p_ehci->supplies),
				    s5p_ehci->supplies);
fail_enable_reg:
fail_get_reg:
fail_io:
	clk_disable_unprepare(s5p_ehci->clk2);
fail_clk2:
	clk_disable_unprepare(s5p_ehci->clk1);
fail_clk1:
	usb_put_hcd(hcd);
	return err;
}

static int s5p_ehci_remove(struct platform_device *pdev)
{
	struct usb_hcd *hcd = platform_get_drvdata(pdev);
	struct s5p_ehci_hcd *s5p_ehci = to_s5p_ehci(hcd);

	s5p_ehci->power_on = 0;
	remove_ehci_sys_file(hcd_to_ehci(hcd));
	usb_remove_hcd(hcd);

	s5p_ehci_phy_disable(s5p_ehci);

	clk_disable_unprepare(s5p_ehci->clk1);
	clk_disable_unprepare(s5p_ehci->clk2);

	regulator_bulk_disable(ARRAY_SIZE(s5p_ehci->supplies),
				    s5p_ehci->supplies);
	usb_put_hcd(hcd);

	return 0;
}

static void s5p_ehci_shutdown(struct platform_device *pdev)
{
	struct usb_hcd *hcd = platform_get_drvdata(pdev);

	if (hcd->driver->shutdown)
		hcd->driver->shutdown(hcd);
}

#ifdef CONFIG_PM
static int s5p_ehci_suspend(struct device *dev)
{
	struct usb_hcd *hcd = dev_get_drvdata(dev);
	struct s5p_ehci_hcd *s5p_ehci = to_s5p_ehci(hcd);

	bool do_wakeup = device_may_wakeup(dev);
	int rc;

	rc = ehci_suspend(hcd, do_wakeup);

	s5p_ehci_phy_disable(s5p_ehci);

	clk_disable_unprepare(s5p_ehci->clk1);
	clk_disable_unprepare(s5p_ehci->clk2);

	return rc;
}

static int s5p_ehci_resume(struct device *dev)
{
	struct usb_hcd *hcd = dev_get_drvdata(dev);
	struct  s5p_ehci_hcd *s5p_ehci = to_s5p_ehci(hcd);

	clk_prepare_enable(s5p_ehci->clk1);
	clk_prepare_enable(s5p_ehci->clk2);

	s5p_ehci_phy_enable(s5p_ehci);

	/* DMA burst Enable */
	writel(EHCI_INSNREG00_ENABLE_DMA_BURST, EHCI_INSNREG00(hcd->regs));

	ehci_resume(hcd, true);
	return 0;
}
#else
#define s5p_ehci_suspend	NULL
#define s5p_ehci_resume		NULL
#endif

static const struct dev_pm_ops s5p_ehci_pm_ops = {
	.suspend	= s5p_ehci_suspend,
	.resume		= s5p_ehci_resume,
};

#ifdef CONFIG_OF
static const struct of_device_id exynos_ehci_match[] = {
	{ .compatible = "samsung,exynos-ehci" },
	{},
};
MODULE_DEVICE_TABLE(of, exynos_ehci_match);
#endif

static struct platform_driver s5p_ehci_driver = {
	.probe		= s5p_ehci_probe,
	.remove		= s5p_ehci_remove,
	.shutdown	= s5p_ehci_shutdown,
	.driver = {
		.name	= "s5p-ehci",
		.owner	= THIS_MODULE,
		.pm	= &s5p_ehci_pm_ops,
		.of_match_table = of_match_ptr(exynos_ehci_match),
	}
};
static const struct ehci_driver_overrides s5p_overrides __initdata = {
	.extra_priv_size = sizeof(struct s5p_ehci_hcd),
};

static int __init ehci_s5p_init(void)
{
	if (usb_disabled())
		return -ENODEV;

	pr_info("%s: " DRIVER_DESC "\n", hcd_name);
	ehci_init_driver(&s5p_ehci_hc_driver, &s5p_overrides);
	return platform_driver_register(&s5p_ehci_driver);
}
module_init(ehci_s5p_init);

static void __exit ehci_s5p_cleanup(void)
{
	platform_driver_unregister(&s5p_ehci_driver);
}
module_exit(ehci_s5p_cleanup);

MODULE_DESCRIPTION(DRIVER_DESC);
MODULE_ALIAS("platform:s5p-ehci");
MODULE_AUTHOR("Jingoo Han");
MODULE_AUTHOR("Joonyoung Shim");
MODULE_LICENSE("GPL v2");
