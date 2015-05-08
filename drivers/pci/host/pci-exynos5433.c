/*
 * PCIe host controller driver for Samsung EXYNOS5433 SoCs
 *
 * Copyright (C) 2015 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/clk.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of_gpio.h>
#include <linux/pci.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include <linux/resource.h>
#include <linux/signal.h>
#include <linux/types.h>
#include <linux/mfd/syscon.h>

#include "pcie-designware.h"

#define to_exynos_pcie(x)	container_of(x, struct exynos_pcie, pp)

/* Pcie structure for Exynos specific data */
struct exynos_pcie {
	void __iomem		*elbi_base;
	void __iomem		*phy_base;
	void __iomem		*block_base;
	int			reset_gpio;
	struct clk		*clk;
	struct clk		*bus_clk;
	struct pcie_port	pp;
	struct regmap		*pmureg;
	/* workaround */
	int			wlanen_gpio;
};

/* PCIe ELBI registers */
#define PCIE_IRQ_PULSE			0x000
#define IRQ_INTA_ASSERT			BIT(0)
#define IRQ_INTB_ASSERT			BIT(2)
#define IRQ_INTC_ASSERT			BIT(4)
#define IRQ_INTD_ASSERT			BIT(6)
#define IRQ_RADM_PM_TO_ACK		BIT(18)
#define PCIE_IRQ_LEVEL			0x004
#define IRQ_RDLH_LINK_UP_INIT		BIT(4)
#define PCIE_IRQ_SPECIAL		0x008
#define PCIE_IRQ_EN_PULSE		0x00c
#define PCIE_IRQ_EN_LEVEL		0x010
#define IRQ_MSI_ENABLE			BIT(2)
#define PCIE_IRQ_EN_SPECIAL		0x014
#define PCIE_SW_WAKE			0x018
#define PCIE_APP_INIT_RESET		0x028
#define PCIE_APP_LTSSM_ENABLE		0x02c
#define PCIE_ELBI_LTSSM_DISABLE		0x0
#define PCIE_ELBI_LTSSM_ENABLE		0x1
#define PCIE_L1_BUG_FIX_ENABLE		0x038
#define PCIE_APP_REQ_EXIT_L1		0x040
#define PCIE_APPS_PM_XMT_TURNOFF	0x04c
#define PCIE_ELBI_RDLH_LINKUP		0x074
#define PCIE_AUX_PM_EN			0x0A4
#define AUX_PM_DISABLE			0x0
#define AUX_PM_ENABLE			0x1
#define PCIE_VEN_MSG_REQ		0x0A8
#define VEN_MSG_REQ_DISABLE		0x0
#define VEN_MSG_REQ_ENABLE		0x1
#define PCIE_VEN_MSG_FMT		0x0AC
#define PCIE_VEN_MSG_TYPE		0x0B0
#define PCIE_VEN_MSG_CODE		0x0D0
#define PCIE_ELBI_SLV_AWMISC		0x11c
#define PCIE_ELBI_SLV_ARMISC		0x120
#define PCIE_ELBI_SLV_DBI_ENABLE	BIT(21)

/* Workaround : PMU phy register offset */
#define PCIE_PMU_PHY_OFFSET		0x730

/* Workaround : Sysreg Fsys register offset and bit */
#define PCIE_PHY_MAC_RESET		0x208
#define PCIE_MAC_RESET			BIT(4)
#define PCIE_L1SUB_CM_CON		0x1010
#define PCIE_REFCLK_GATING_EN		BIT(0)
#define PCIE_PHY_COMMON_RESET		0x1020
#define PCIE_PHY_RESET			BIT(0)
#define PCIE_PHY_GLOBAL_RESET		0x1040
#define PCIE_GLOBAL_RESET		BIT(0)
#define PCIE_REFCLK			BIT(1)
#define PCIE_REFCLK_MASK		0x16
#define PCIE_APP_REQ_EXIT_L1_MODE	BIT(5)

/*
 * Workaround : PHY register offset
 * This register naming based on Exynos5433 TRM(Rev0.1)
 * Meaning of offset naming and functionality can be difference.
 * (In future, needs to fix as exactly naming.)
 * So it makes the offset naming with offset's number.
 */
#define PCIE_PHY_OFFSET(x)		((x) * 0x4)
#define PCIE_PHY_ALL_CMN_PWR_DOWN		BIT(4)
#define PCIE_PHY_ALL_TRSV_PWR_DOWN		BIT(3)

/* Workaround code to use broadcom device driver */
static struct exynos_pcie *g_pcie;

void exynos_pcie_register_dump(struct pcie_port *pp)
{
	struct exynos_pcie *g_pcie = to_exynos_pcie(pp);
	u32 i;

	for (i = 0; i < 0x134; i = i + 4)
		printk("link reg %x : %x\n", i, readl(g_pcie->elbi_base + i));

	for (i = 0; i < 0x60; i++)
		printk("phy reg %x * 4: %x\n", i, readl(g_pcie->phy_base + (i * 4)));
}

static inline void exynos_pcie_writel(void __iomem *base, u32 val, u32 offset)
{
	writel(val, base + offset);
}

static inline u32 exynos_pcie_readl(void __iomem *base, u32 offset)
{
	return readl(base + offset);
}

static void exynos_pcie_clear_irq_pulse(struct pcie_port *pp)
{
	u32 val;
	struct exynos_pcie *ep = to_exynos_pcie(pp);

	val = exynos_pcie_readl(ep->elbi_base, PCIE_IRQ_PULSE);
	exynos_pcie_writel(ep->elbi_base, val, PCIE_IRQ_PULSE);
}

static void exynos_pcie_enable_irq_pulse(struct pcie_port *pp)
{
	u32 val;
	struct exynos_pcie *ep = to_exynos_pcie(pp);

	/* enable INTX interrupt */
	val = IRQ_INTA_ASSERT | IRQ_INTB_ASSERT |
		IRQ_INTC_ASSERT | IRQ_INTD_ASSERT;
	exynos_pcie_writel(ep->elbi_base, val, PCIE_IRQ_EN_PULSE);

	exynos_pcie_writel(ep->elbi_base, 0, PCIE_IRQ_EN_LEVEL);

	exynos_pcie_writel(ep->elbi_base, 0, PCIE_IRQ_EN_SPECIAL);
}

static irqreturn_t exynos_pcie_irq_handler(int irq, void *arg)
{
	struct pcie_port *pp = arg;

	exynos_pcie_clear_irq_pulse(pp);

	return IRQ_HANDLED;
}

static void exynos_pcie_sideband_dbi_w_mode(struct pcie_port *pp, bool on)
{
	u32 val;
	struct exynos_pcie *ep = to_exynos_pcie(pp);

	val = exynos_pcie_readl(ep->elbi_base, PCIE_ELBI_SLV_AWMISC);
	if (on)
		val |= PCIE_ELBI_SLV_DBI_ENABLE;
	else
		val &= ~PCIE_ELBI_SLV_DBI_ENABLE;
	exynos_pcie_writel(ep->elbi_base, val, PCIE_ELBI_SLV_AWMISC);
}

static void exynos_pcie_sideband_dbi_r_mode(struct pcie_port *pp, bool on)
{
	u32 val;
	struct exynos_pcie *ep = to_exynos_pcie(pp);

	val = exynos_pcie_readl(ep->elbi_base, PCIE_ELBI_SLV_ARMISC);
	if (on)
		val |= PCIE_ELBI_SLV_DBI_ENABLE;
	else
		val &= ~PCIE_ELBI_SLV_DBI_ENABLE;
	exynos_pcie_writel(ep->elbi_base, val, PCIE_ELBI_SLV_ARMISC);
}

static void exynos_pcie_init_phy(struct pcie_port *pp)
{
	struct exynos_pcie *ep = to_exynos_pcie(pp);

	exynos_pcie_writel(ep->phy_base, 0x11, PCIE_PHY_OFFSET(0x3));

	/* band gap reference on */
	exynos_pcie_writel(ep->phy_base, 0, PCIE_PHY_OFFSET(0x20));
	exynos_pcie_writel(ep->phy_base, 0, PCIE_PHY_OFFSET(0x4b));

	/* jitter tunning */
	exynos_pcie_writel(ep->phy_base, 0x34, PCIE_PHY_OFFSET(0x4));
	exynos_pcie_writel(ep->phy_base, 0x02, PCIE_PHY_OFFSET(0x7));
	exynos_pcie_writel(ep->phy_base, 0x41, PCIE_PHY_OFFSET(0x21));
	exynos_pcie_writel(ep->phy_base, 0x7F, PCIE_PHY_OFFSET(0x14));
	exynos_pcie_writel(ep->phy_base, 0xC0, PCIE_PHY_OFFSET(0x15));
	exynos_pcie_writel(ep->phy_base, 0x61, PCIE_PHY_OFFSET(0x36));

	/* D0 uninit.. */
	exynos_pcie_writel(ep->phy_base, 0x44, PCIE_PHY_OFFSET(0x3D));

	/* 24MHz */
	exynos_pcie_writel(ep->phy_base, 0x94, PCIE_PHY_OFFSET(0x8));
	exynos_pcie_writel(ep->phy_base, 0xA7, PCIE_PHY_OFFSET(0x9));
	exynos_pcie_writel(ep->phy_base, 0x93, PCIE_PHY_OFFSET(0xA));
	exynos_pcie_writel(ep->phy_base, 0x6B, PCIE_PHY_OFFSET(0xC));
	exynos_pcie_writel(ep->phy_base, 0xA5, PCIE_PHY_OFFSET(0xF));
	exynos_pcie_writel(ep->phy_base, 0x34, PCIE_PHY_OFFSET(0x16));
	exynos_pcie_writel(ep->phy_base, 0xA3, PCIE_PHY_OFFSET(0x17));
	exynos_pcie_writel(ep->phy_base, 0xA7, PCIE_PHY_OFFSET(0x1A));
	exynos_pcie_writel(ep->phy_base, 0x71, PCIE_PHY_OFFSET(0x23));
	exynos_pcie_writel(ep->phy_base, 0x4C, PCIE_PHY_OFFSET(0x24));

	exynos_pcie_writel(ep->phy_base, 0x0E, PCIE_PHY_OFFSET(0x26));
	exynos_pcie_writel(ep->phy_base, 0x14, PCIE_PHY_OFFSET(0x7));
	exynos_pcie_writel(ep->phy_base, 0x48, PCIE_PHY_OFFSET(0x43));
	exynos_pcie_writel(ep->phy_base, 0x44, PCIE_PHY_OFFSET(0x44));
	exynos_pcie_writel(ep->phy_base, 0x03, PCIE_PHY_OFFSET(0x45));
	exynos_pcie_writel(ep->phy_base, 0xA7, PCIE_PHY_OFFSET(0x48));
	exynos_pcie_writel(ep->phy_base, 0x13, PCIE_PHY_OFFSET(0x54));
	exynos_pcie_writel(ep->phy_base, 0x04, PCIE_PHY_OFFSET(0x31));
	exynos_pcie_writel(ep->phy_base, 0, PCIE_PHY_OFFSET(0x32));
}

static void exynos_pcie_assert_phy_reset(struct pcie_port *pp)
{
	struct exynos_pcie *ep = to_exynos_pcie(pp);
	u32 val;

	val = exynos_pcie_readl(ep->block_base, PCIE_PHY_COMMON_RESET);
	val |= PCIE_PHY_RESET;
	exynos_pcie_writel(ep->block_base, val, PCIE_PHY_COMMON_RESET);

	/* PHY Mac Reset */
	val = exynos_pcie_readl(ep->block_base, PCIE_PHY_MAC_RESET);
	val &= ~PCIE_MAC_RESET;
	exynos_pcie_writel(ep->block_base, val, PCIE_PHY_MAC_RESET);


	/* PHY refclk 24MHz */
	val = exynos_pcie_readl(ep->block_base, PCIE_PHY_GLOBAL_RESET);
	val &= ~PCIE_REFCLK_MASK;
	val |= PCIE_REFCLK;
	exynos_pcie_writel(ep->block_base, val, PCIE_PHY_GLOBAL_RESET);

	/* PHY Global Reset */
	val = exynos_pcie_readl(ep->block_base, PCIE_PHY_GLOBAL_RESET);
	val &= ~PCIE_GLOBAL_RESET;
	exynos_pcie_writel(ep->block_base, val, PCIE_PHY_GLOBAL_RESET);

	/* initialize phy */
	exynos_pcie_init_phy(pp);

	/* PHY Common Reset */
	val = exynos_pcie_readl(ep->block_base, PCIE_PHY_COMMON_RESET);
	val &= ~PCIE_PHY_RESET;
	exynos_pcie_writel(ep->block_base, val, PCIE_PHY_COMMON_RESET);

	/* PHY Mac Reset */
	val = exynos_pcie_readl(ep->block_base, PCIE_PHY_MAC_RESET);
	val |= PCIE_MAC_RESET;
	exynos_pcie_writel(ep->block_base, val, PCIE_PHY_MAC_RESET);


	val = exynos_pcie_readl(ep->elbi_base, PCIE_SW_WAKE);
	val &= ~(0x1 << 1);
	exynos_pcie_writel(ep->elbi_base, val, PCIE_SW_WAKE);
}

static int exynos_pcie_reset(struct pcie_port *pp)
{
	struct exynos_pcie *ep = to_exynos_pcie(pp);
	u32 val;

	exynos_pcie_assert_phy_reset(pp);

	/* Setup root complext */
	dw_pcie_setup_rc(pp);

	if (ep->reset_gpio) {
		gpio_set_value(ep->reset_gpio, 1);
		msleep(80);
	}

	/* assert LTSSM enable */
	exynos_pcie_writel(ep->elbi_base, PCIE_ELBI_LTSSM_ENABLE,
			PCIE_APP_LTSSM_ENABLE);

	val = exynos_pcie_readl(ep->elbi_base, PCIE_IRQ_LEVEL);
	if (val & IRQ_RDLH_LINK_UP_INIT)
		return 0;

	return -EPIPE;
}

static int exynos_pcie_establish_link(struct pcie_port *pp)
{
	struct exynos_pcie *ep = to_exynos_pcie(pp);
	u32 val;
	int count = 10, ret;

	if (dw_pcie_link_up(pp)) {
		dev_info(pp->dev, "Link already up\n");
		return 0;
	}

	/* PMU control at here */
	if (ep->pmureg) {
		if (regmap_update_bits(ep->pmureg, PCIE_PMU_PHY_OFFSET, BIT(0), 1))
			dev_warn(pp->dev, "Failed to update regmap bit.\n");
	}

	val = exynos_pcie_readl(ep->block_base, PCIE_PHY_GLOBAL_RESET);
	val &= ~PCIE_APP_REQ_EXIT_L1_MODE;
	exynos_pcie_writel(ep->block_base, val, PCIE_PHY_GLOBAL_RESET);

	val = exynos_pcie_readl(ep->block_base, PCIE_L1SUB_CM_CON);
	val &= ~PCIE_REFCLK_GATING_EN;
	exynos_pcie_writel(ep->block_base, val, PCIE_L1SUB_CM_CON);

	exynos_pcie_assert_phy_reset(pp);

	/* Setup root complex */
	dw_pcie_setup_rc(pp);

	/* assert LTSSM enable */
	exynos_pcie_writel(ep->elbi_base, PCIE_ELBI_LTSSM_ENABLE,
			PCIE_APP_LTSSM_ENABLE);

	while (!dw_pcie_link_up(pp)) {
		usleep_range(100, 1000);
		if (--count) {
			ret = exynos_pcie_reset(pp);
			if (!ret)
				break;
			continue;
		}

		/* Workaround */
		exynos_pcie_register_dump(pp);
		dev_err(pp->dev, "Failed PCIe Link up!\n");
		return -EPIPE;
	}

	dev_info(pp->dev, "Link up!\n");
	return 0;
}

static int exynos_pcie_link_up(struct pcie_port *pp)
{
	struct exynos_pcie *ep = to_exynos_pcie(pp);
	u32 val;

	val = exynos_pcie_readl(ep->elbi_base, PCIE_ELBI_RDLH_LINKUP);
	val &= 0x1f;

	if (val >= 0x0d && val <= 0x15)
		return 1;

	return 0;
}

static int exynos_pcie_host_init(struct pcie_port *pp)
{
	exynos_pcie_enable_irq_pulse(pp);

	return exynos_pcie_establish_link(pp);
}

static inline void exynos_pcie_readl_rc(struct pcie_port *pp,
					void __iomem *dbi_base, u32 *val)
{
	exynos_pcie_sideband_dbi_r_mode(pp, true);
	*val = readl(dbi_base);
	exynos_pcie_sideband_dbi_r_mode(pp, false);
}

static inline void exynos_pcie_writel_rc(struct pcie_port *pp,
					u32 val, void __iomem *dbi_base)
{
	exynos_pcie_sideband_dbi_w_mode(pp, true);
	writel(val, dbi_base);
	exynos_pcie_sideband_dbi_w_mode(pp, false);
}

static int exynos_pcie_rd_own_conf(struct pcie_port *pp, int where, int size,
				u32 *val)
{
	int ret;

	exynos_pcie_sideband_dbi_r_mode(pp, true);
	ret = dw_pcie_cfg_read(pp->dbi_base + (where & ~0x3), where, size, val);
	exynos_pcie_sideband_dbi_r_mode(pp, false);
	return ret;
}

static int exynos_pcie_wr_own_conf(struct pcie_port *pp, int where, int size,
				u32 val)
{
	int ret;

	exynos_pcie_sideband_dbi_w_mode(pp, true);
	ret = dw_pcie_cfg_write(pp->dbi_base + (where & ~0x3),
			where, size, val);
	exynos_pcie_sideband_dbi_w_mode(pp, false);
	return ret;
}

static int exynos_pcie_power_enabled(struct pcie_port *pp)
{
	struct exynos_pcie *ep = to_exynos_pcie(pp);

	return gpio_get_value(ep->wlanen_gpio);
}

static struct pcie_host_ops exynos_pcie_host_ops = {
	.readl_rc = exynos_pcie_readl_rc,
	.writel_rc = exynos_pcie_writel_rc,
	.rd_own_conf = exynos_pcie_rd_own_conf,
	.wr_own_conf = exynos_pcie_wr_own_conf,
	.host_init = exynos_pcie_host_init,
	.link_up = exynos_pcie_link_up,
	.power_enabled = exynos_pcie_power_enabled,
};

static int __init exynos_pcie_probe(struct platform_device *pdev)
{
	struct exynos_pcie *exynos_pcie;
	struct device_node *np = pdev->dev.of_node;
	struct pcie_port *pp;
	struct resource *res;
	int ret;

	exynos_pcie = devm_kzalloc(&pdev->dev, sizeof(*exynos_pcie),
			GFP_KERNEL);
	if (!exynos_pcie)
		return -ENOMEM;

	pp = &exynos_pcie->pp;
	pp->dev = &pdev->dev;

	exynos_pcie->reset_gpio = of_get_named_gpio(np, "reset-gpio", 0);
	if (exynos_pcie->reset_gpio < 0)
		return exynos_pcie->reset_gpio;

	exynos_pcie->wlanen_gpio = of_get_named_gpio(np, "wlanen-gpio", 0);
	if (exynos_pcie->wlanen_gpio < 0)
		return exynos_pcie->wlanen_gpio;

	exynos_pcie->clk = devm_clk_get(&pdev->dev, "pcie");
	if (IS_ERR(exynos_pcie->clk)) {
		dev_err(&pdev->dev, "Failed to get pcie rc clock\n");
		return PTR_ERR(exynos_pcie->clk);
	}
	ret = clk_prepare_enable(exynos_pcie->clk);
	if (ret)
		return ret;

	exynos_pcie->bus_clk = devm_clk_get(&pdev->dev, "pcie_bus");
	if (IS_ERR(exynos_pcie->bus_clk)) {
		dev_err(&pdev->dev, "Failed to get pcie bus clock\n");
		ret = PTR_ERR(exynos_pcie->bus_clk);
		goto fail_clk;
	}
	ret = clk_prepare_enable(exynos_pcie->bus_clk);
	if (ret)
		goto fail_clk;

	/* Application Register */
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	exynos_pcie->elbi_base = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(exynos_pcie->elbi_base)) {
		ret = PTR_ERR(exynos_pcie->elbi_base);
		goto fail_bus_clk;
	}

	/* Physical Layer Register */
	res = platform_get_resource(pdev, IORESOURCE_MEM, 1);
	exynos_pcie->phy_base = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(exynos_pcie->phy_base)) {
		ret = PTR_ERR(exynos_pcie->phy_base);
		goto fail_bus_clk;
	}

	/* Workaround Block register(System register) */
	res = platform_get_resource(pdev, IORESOURCE_MEM, 2);
	exynos_pcie->block_base = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(exynos_pcie->block_base))
		goto fail_bus_clk;

	exynos_pcie->pmureg = syscon_regmap_lookup_by_phandle(pdev->dev.of_node,
			"samsung,pmureg-phandle");
	if (IS_ERR(exynos_pcie->pmureg)) {
		dev_warn(&pdev->dev, "Pmureg syscon regmap lookup failed.\n");
		exynos_pcie->pmureg = NULL;
	}

	/* Workaround code to use broadcom device driver */
	g_pcie = exynos_pcie;

	pp->irq = platform_get_irq(pdev, 0);
	if (!pp->irq) {
		dev_err(&pdev->dev, "failed to get irq\n");
		ret = -ENODEV;
		goto fail_bus_clk;
	}
	ret = devm_request_irq(&pdev->dev, pp->irq, exynos_pcie_irq_handler,
				IRQF_SHARED, "exynos-pcie", pp);
	if (ret) {
		dev_err(&pdev->dev, "failed to request irq\n");
		goto fail_bus_clk;
	}

	pp->root_bus_nr = -1;
	pp->ops = &exynos_pcie_host_ops;

	ret = dw_pcie_host_init(pp);
	if (ret) {
		dev_err(&pdev->dev, "failed to initialize host\n");
		goto fail_bus_clk;
	}

	platform_set_drvdata(pdev, exynos_pcie);

	return 0;

fail_bus_clk:
	clk_disable_unprepare(exynos_pcie->bus_clk);
fail_clk:
	clk_disable_unprepare(exynos_pcie->clk);
	return ret;
}

/*
 * Workaround : To enable the broadcom device.
 */
void exynos_pcie_poweron(void)
{
	u32 val;

	if (g_pcie) {
		clk_prepare_enable(g_pcie->clk);
		clk_prepare_enable(g_pcie->bus_clk);

		val = exynos_pcie_readl(g_pcie->phy_base, PCIE_PHY_OFFSET(0x55));
		val &= ~PCIE_PHY_ALL_TRSV_PWR_DOWN;
		exynos_pcie_writel(g_pcie->phy_base, val, PCIE_PHY_OFFSET(0x55));

		udelay(100);

		val = exynos_pcie_readl(g_pcie->phy_base, PCIE_PHY_OFFSET(0x21));
		val &= ~PCIE_PHY_ALL_CMN_PWR_DOWN;
		exynos_pcie_writel(g_pcie->phy_base, val, PCIE_PHY_OFFSET(0x21));

		exynos_pcie_establish_link(&g_pcie->pp);

		val = exynos_pcie_readl(g_pcie->elbi_base, PCIE_IRQ_SPECIAL);
		exynos_pcie_writel(g_pcie->elbi_base, val, PCIE_IRQ_SPECIAL);
	}
}

/*
 * Workaround : To enable the broadcom device.
 */
void exynos_pcie_poweroff(void)
{
	u32 val;

	if (g_pcie) {
		exynos_pcie_writel(g_pcie->elbi_base, PCIE_ELBI_LTSSM_DISABLE,
				PCIE_APP_LTSSM_ENABLE);

		val = exynos_pcie_readl(g_pcie->phy_base, PCIE_PHY_OFFSET(0x55));
		val |= PCIE_PHY_ALL_TRSV_PWR_DOWN;
		exynos_pcie_writel(g_pcie->phy_base, val, PCIE_PHY_OFFSET(0x55));

		val = exynos_pcie_readl(g_pcie->phy_base, PCIE_PHY_OFFSET(0x21));
		val |= PCIE_PHY_ALL_CMN_PWR_DOWN;
		exynos_pcie_writel(g_pcie->phy_base, val, PCIE_PHY_OFFSET(0x21));

		clk_disable_unprepare(g_pcie->bus_clk);
		clk_disable_unprepare(g_pcie->clk);
	}
}

static int exynos_pcie_remove(struct platform_device *pdev)
{
	struct exynos_pcie *ep = platform_get_drvdata(pdev);

	clk_disable_unprepare(ep->bus_clk);
	clk_disable_unprepare(ep->clk);

	return 0;
}

#ifdef CONFIG_PM
static int exynos_pcie_suspend_noirq(struct device *dev)
{
	struct exynos_pcie *ep = dev_get_drvdata(dev);
	u32 val, count = 0;

	if (!gpio_get_value(ep->wlanen_gpio))
		return 0;

	exynos_pcie_writel(ep->elbi_base, VEN_MSG_REQ_DISABLE,
			PCIE_VEN_MSG_REQ);

	val = exynos_pcie_readl(ep->block_base, PCIE_PHY_GLOBAL_RESET);
	val |= PCIE_APP_REQ_EXIT_L1_MODE;
	exynos_pcie_writel(ep->block_base, val, PCIE_PHY_GLOBAL_RESET);

	exynos_pcie_writel(ep->elbi_base, 0x1, PCIE_APP_REQ_EXIT_L1);
	exynos_pcie_writel(ep->elbi_base, 0x1, PCIE_VEN_MSG_FMT);
	exynos_pcie_writel(ep->elbi_base, 0x13, PCIE_VEN_MSG_TYPE);
	exynos_pcie_writel(ep->elbi_base, 0x19, PCIE_VEN_MSG_CODE);
	exynos_pcie_writel(ep->elbi_base, VEN_MSG_REQ_ENABLE,
			PCIE_VEN_MSG_REQ);

	while (count < 1000) {
		if (exynos_pcie_readl(ep->elbi_base, PCIE_IRQ_PULSE) & IRQ_RADM_PM_TO_ACK) {
			printk("ack message is ok\n");
			break;
		}
		udelay(10);
		count++;
	}
	exynos_pcie_writel(ep->elbi_base, 0x0, PCIE_APP_REQ_EXIT_L1);

	count = 0;
	do {
		val = exynos_pcie_readl(ep->elbi_base, PCIE_ELBI_RDLH_LINKUP);
		val = val & 0x1f;
		if (val == 0x15) {
			printk("Recevied enter_l23 LLLP packet\n");
			break;
		}
		udelay(10);
		count++;
	} while (count < 1000);

	return 0;
}

static int exynos_pcie_resume_noirq(struct device *dev)
{
	struct exynos_pcie *ep = dev_get_drvdata(dev);
	struct pcie_port *pp = &ep->pp;
	u32 val;

	if (!gpio_get_value(ep->wlanen_gpio)) {
		clk_prepare_enable(ep->clk);
		clk_prepare_enable(ep->bus_clk);

		exynos_pcie_enable_irq_pulse(&ep->pp);

		/* PMU control at here */
		if (ep->pmureg) {
			if (regmap_update_bits(ep->pmureg, PCIE_PMU_PHY_OFFSET, BIT(0), 1))
				dev_warn(pp->dev, "Failed to update regmap bit.\n");
		}

		val = exynos_pcie_readl(ep->block_base, PCIE_PHY_GLOBAL_RESET);
		val &= ~PCIE_APP_REQ_EXIT_L1_MODE;
		exynos_pcie_writel(ep->block_base, val, PCIE_PHY_GLOBAL_RESET);

		val = exynos_pcie_readl(ep->block_base, PCIE_L1SUB_CM_CON);
		val &= ~PCIE_REFCLK_GATING_EN;
		exynos_pcie_writel(ep->block_base, val, PCIE_L1SUB_CM_CON);

		exynos_pcie_assert_phy_reset(pp);

		val = exynos_pcie_readl(ep->phy_base, PCIE_PHY_OFFSET(0x55));
		val |= PCIE_PHY_ALL_TRSV_PWR_DOWN;
		exynos_pcie_writel(ep->phy_base, val, PCIE_PHY_OFFSET(0x55));

		val = exynos_pcie_readl(ep->phy_base, PCIE_PHY_OFFSET(0x21));
		val |= PCIE_PHY_ALL_CMN_PWR_DOWN;
		exynos_pcie_writel(ep->phy_base, val, PCIE_PHY_OFFSET(0x21));

		clk_disable_unprepare(ep->bus_clk);
		clk_disable_unprepare(ep->clk);

		return 0;
	}

	exynos_pcie_host_init(pp);

	return 0;
}
#else
#define exynos_pcie_suspend_noirq	NULL
#define exynos_pcie_resume_noirq	NULL
#endif /* CONFIG_PM */

static const struct dev_pm_ops exynos_pcie_pm_ops = {
	.suspend_noirq	= exynos_pcie_suspend_noirq,
	.resume_noirq	= exynos_pcie_resume_noirq,
};

static const struct of_device_id exynos_pcie_of_match[] = {
	{ .compatible = "samsung,exynos5433-pcie", },
	{},
};
MODULE_DEVICE_TABLE(of, exynos_pcie_of_match);

static struct platform_driver exynos_pcie_driver = {
	.remove		= exynos_pcie_remove,
	.driver		= {
		.name		= "exynos-pcie",
		.of_match_table = exynos_pcie_of_match,
		.pm		= &exynos_pcie_pm_ops,
	},
};

static int __init exynos_pcie_init(void)
{
	return platform_driver_probe(&exynos_pcie_driver, exynos_pcie_probe);
}
late_initcall(exynos_pcie_init);

MODULE_AUTHOR("Jingoo Han<jg1.han@samsung.com>");
MODULE_AUTHOR("Jaehoon CHung <jh80.chung@samsung.com>");
MODULE_LICENSE("GPL v2");
