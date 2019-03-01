// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2014 - 2019 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 *
 * Exynos5433 OTP device support
 */

#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/errno.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/mtd/mtd.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/types.h>

#define	REG_OTP_LOCK0				0x00
#define	REG_OTP_LOCK1				0x04
#define	REG_OTP_SECURE_READ_DATA		0x08
#define	REG_OTP_NONSECURE_READ_DATA		0x0c
#define	REG_OTP_CON_CONTROL			0x10
#define	REG_OTP_CON_CONFIG			0x14
#define	REG_OTP_IF				0x18
#define	REG_OTP_INT_STATUS			0x1c
#define	REG_OTP_INT_EN				0x20
#define	REG_OTP_CON_TIME_PARA_0			0x24
#define	REG_OTP_CON_TIME_PARA_1			0x28
#define	REG_OTP_CON_TIME_PARA_2			0x2c
#define	REG_OTP_CON_TIME_PARA_3			0x30
#define	REG_OTP_CON_TIME_PARA_4			0x34
#define	REG_OTP_CON_TIME_PARA_5			0x38
#define	REG_OTP_CON_TIME_PARA_6			0x3c
#define	REG_OTP_CON_TIME_PARA_7			0x40
#define	REG_OTP_ADD_LOCK			0x44
#define	REG_OTP_CUSTOM_LOCK0			0x48
#define	REG_OTP_CUSTOM_LOCK01			0x4c

struct exynos_otp_priv {
	struct device *dev;
	struct clk_bulk_data clks[2];
	void __iomem *regs;

	struct mtd_info mtd;
};

static unsigned int otp_reg_read(struct exynos_otp_priv *priv,
				 unsigned int offset)
{
	return readl_relaxed(priv->regs + offset);
}

static void otp_reg_write(struct exynos_otp_priv *priv, unsigned int offset,
			  unsigned int data)
{
	writel_relaxed(data, priv->regs + offset);
}

static int exynos_otp_cmd_init(struct exynos_otp_priv *priv)
{
	unsigned int error_count = 0;
	int result = 0;
	u32 reg;

	otp_reg_write(priv, REG_OTP_CON_CONTROL, 0x01);

	while (true) {
		if (otp_reg_read(priv, REG_OTP_INT_STATUS) & 0x01)
			break;

		error_count++;
		if (error_count > 0xffffff) {
			result = -ETIMEDOUT;
			break;
		}
	}

	reg = otp_reg_read(priv, REG_OTP_INT_STATUS);
	otp_reg_write(priv, REG_OTP_INT_STATUS, (reg | 0x01));

	return result;
}

static int exynos_otp_cmd_standby(struct exynos_otp_priv *priv)
{
	unsigned int error_count = 0;
	int result = 0;
	u32 reg;

	/* Set standby command */
	reg = otp_reg_read(priv, REG_OTP_CON_CONTROL);
	otp_reg_write(priv, REG_OTP_CON_CONTROL, reg | 0x08);

	while (true) {
		if (otp_reg_read(priv, REG_OTP_INT_STATUS) & 0x08)
			break;

		error_count++;
		if (error_count > 0xffffff) {
			result = -ETIMEDOUT;
			break;
		}
	}

	reg = otp_reg_read(priv, REG_OTP_INT_STATUS);
	otp_reg_write(priv, REG_OTP_INT_STATUS, reg | 0x08);

	return result;
}

int exynos_otp_cmd_read(struct exynos_otp_priv *priv, u32 addr, u32 *val)
{
	u32 error_count = 0;
	u32 reg;

	/* 1. Set address */
	/* OTP_IF: program data[31], address [14:0] */
	reg = otp_reg_read(priv, REG_OTP_IF);
	reg = (reg & 0xffff0000) | (addr & 0x7fff);
	otp_reg_write(priv, REG_OTP_IF, reg);

	/* 2. Set read command */
	reg = otp_reg_read(priv, REG_OTP_CON_CONTROL);
	otp_reg_write(priv, REG_OTP_CON_CONTROL, reg | 0x02);

	/* 3. Check read status */
	while (true) {
		reg = otp_reg_read(priv, REG_OTP_INT_STATUS);

		/* Check read done */
		if (reg & 0x02) {
			dev_dbg(priv->dev, "OTP read sucessfull\n");
			break;
		}

		/* Check secure fail */
		if (reg & 0x80) {
			dev_err(priv->dev, "OTP error: SECURE_FAIL\n");
			return -EPERM;
		}

		error_count++;
		if (error_count > 0xffffff) {
			dev_err(priv->dev, "OTP read timeout\n");
			return -ETIMEDOUT;
		}
	}

	/* 4. Checking bit [14:13] */
	reg = (otp_reg_read(priv, REG_OTP_IF) & 0x6000) >> 13;

	if (reg & 0x2) {
		/* Read SECURE DATA [bit [14:13]= 1:0 or 1:1] */
		*val = otp_reg_read(priv, REG_OTP_SECURE_READ_DATA);
		dev_dbg(priv->dev, "read SECURE DATA= 0x%x\n", *val);
	} else if (reg & 0x1) {
		/* Hardware only accessible [bit [14:13]= 0:1] */
		dev_err(priv->dev, "UNACCESSIBLE_REGION\n");
		return -EACCES;
	} else if (!(reg & 0x3)) {
		/* Read NON SECURE DATA [bit [14:13]= 0:0] */
		*val = otp_reg_read(priv, REG_OTP_NONSECURE_READ_DATA);
		dev_dbg(priv->dev, "read NON SECURE DATA= 0x%x\n", *val);
	}

	reg = otp_reg_read(priv, REG_OTP_INT_STATUS);
	otp_reg_write(priv, REG_OTP_INT_STATUS, reg | 0x02);

	return 0;
}

int exynos_mtd_read(struct mtd_info *mtd, loff_t addr, size_t len,
		      size_t *bytes_read, u_char *buf)
{
	struct exynos_otp_priv *priv = mtd->priv;
	u32 *data = (u32 *)buf;
	u32 val = 0;
	unsigned int count;
	int result, ret;

	ret = clk_bulk_prepare_enable(ARRAY_SIZE(priv->clks), priv->clks);
	if (ret < 0)
		return ret;

	result = exynos_otp_cmd_init(priv);
	if (result < 0) {
		dev_err(priv->dev, "otp_cmd_init() failed: %d\n", result);
		clk_bulk_disable_unprepare(ARRAY_SIZE(priv->clks), priv->clks);
		return result;
	}

	len = (len + 3) / 4;

	for (count = 0; count < len; count++) {
		result = exynos_otp_cmd_read(priv, addr + count /** 32*/, &val);
		if (result < 0) {
			dev_err(priv->dev, "OTP read failed: %d\n", result);
			break;
		}
		data[count] = val;
		*bytes_read += 4;
	}

	ret = exynos_otp_cmd_standby(priv);
	if (ret < 0)
		dev_err(priv->dev, "otp_cmd_standby() failed: %d\n", ret);

	clk_bulk_disable_unprepare(ARRAY_SIZE(priv->clks), priv->clks);

	return result;
}

static int exynos_otp_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct exynos_otp_priv *priv;
	struct resource *mem_res;
	int ret;

	priv = devm_kzalloc(dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	mem_res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	priv->regs = devm_ioremap_resource(dev, mem_res);
	if (IS_ERR(priv->regs))
		return PTR_ERR(priv->regs);

	priv->clks[0].id = "pclk";
	priv->clks[1].id = "sclk";

	priv->dev = dev;

	ret = devm_clk_bulk_get(dev, ARRAY_SIZE(priv->clks), priv->clks);
	if (ret < 0)
		return ret;

	platform_set_drvdata(pdev, priv);

	priv->mtd.owner = THIS_MODULE;
	priv->mtd.priv = priv;
	priv->mtd.size = 0x10000; /* FIXME: Replace with real OTP size */
	priv->mtd.writesize = 4;
	priv->mtd.name = "exynos5433-otp";
	priv->mtd._read = exynos_mtd_read;

	return mtd_device_register(&priv->mtd, NULL, 0);
}

static const struct of_device_id exynos_otp_of_match[] = {
	{ .compatible = "samsung,exynos5433-otp" },
	{ },
};
MODULE_DEVICE_TABLE(of, exynos_otp_of_match);

static struct platform_driver exynos_otp_driver = {
	.probe		= exynos_otp_probe,
	.driver		= {
		.of_match_table = exynos_otp_of_match,
		.name		= "exynos-otp",
	},
};
module_platform_driver(exynos_otp_driver);

MODULE_DESCRIPTION("Exynos SoC OTP memory driver");
MODULE_LICENSE("GPL v2");
