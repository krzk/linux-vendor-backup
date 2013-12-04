/* linux/drivers/mmc/host/sdhci-s3c.c
 *
 * Copyright 2008 Openmoko Inc.
 * Copyright 2008 Simtec Electronics
 *      Ben Dooks <ben@simtec.co.uk>
 *      http://armlinux.simtec.co.uk/
 *
 * SDHCI (HSMMC) support for Samsung SoC
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/delay.h>
#include <linux/dma-mapping.h>
#include <linux/platform_device.h>
#include <linux/platform_data/mmc-sdhci-s3c.h>
#include <linux/slab.h>
#include <linux/clk.h>
#include <linux/io.h>
#include <linux/gpio.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/pm.h>
#include <linux/pm_runtime.h>

#include <linux/mmc/host.h>
#include <linux/mmc/slot-gpio.h>

#include "sdhci-s3c-regs.h"
#include "sdhci.h"
#include "sdhci-pltfm.h"

#define MAX_BUS_CLK	(4)

/* Number of gpio's used is max data bus width + command and clock lines */
#define NUM_GPIOS(x)	(x + 2)

/**
 * struct sdhci_s3c - S3C SDHCI instance
 * @pdev: The platform device we where created from.
 * @pdata: The platform data for this controller.
 * @clk_bus: The clocks that are available for the SD/MMC bus clock.
 */
struct sdhci_s3c {
	struct platform_device	*pdev;
	struct s3c_sdhci_platdata *pdata;
	struct clk		*clk_bus[MAX_BUS_CLK];
};

/**
 * struct sdhci_s3c_driver_data - S3C SDHCI platform specific driver data
 * @sdhci_quirks: sdhci host specific quirks.
 *
 * Specifies platform specific configuration of sdhci controller.
 * Note: A structure for driver specific platform data is used for future
 * expansion of its usage.
 */
struct sdhci_s3c_drv_data {
	unsigned int	sdhci_quirks;
};

/**
 * get_curclk - convert ctrl2 register to clock source number
 * @ctrl2: Control2 register value.
 */
static u32 get_curclk(u32 ctrl2)
{
	ctrl2 &= S3C_SDHCI_CTRL2_SELBASECLK_MASK;
	ctrl2 >>= S3C_SDHCI_CTRL2_SELBASECLK_SHIFT;

	return ctrl2;
}

static void sdhci_s3c_check_sclk(struct sdhci_host *host)
{
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct sdhci_s3c *ourhost = pltfm_host->priv;
	u32 tmp = readl(host->ioaddr + S3C_SDHCI_CONTROL2);

	if (get_curclk(tmp) != pltfm_host->clock) {
		dev_dbg(&ourhost->pdev->dev, "restored ctrl2 clock setting\n");

		tmp &= ~S3C_SDHCI_CTRL2_SELBASECLK_MASK;
		tmp |= pltfm_host->clock << S3C_SDHCI_CTRL2_SELBASECLK_SHIFT;
		writel(tmp, host->ioaddr + S3C_SDHCI_CONTROL2);
	}
}

/**
 * sdhci_s3c_get_max_clk - callback to get maximum clock frequency.
 * @host: The SDHCI host instance.
 *
 * Callback to return the maximum clock rate acheivable by the controller.
*/
static unsigned int sdhci_s3c_get_max_clk(struct sdhci_host *host)
{
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct sdhci_s3c *ourhost = pltfm_host->priv;
	struct clk *busclk;
	unsigned int rate, max;
	int clk;

	/* note, a reset will reset the clock source */

	sdhci_s3c_check_sclk(host);

	for (max = 0, clk = 0; clk < MAX_BUS_CLK; clk++) {
		busclk = ourhost->clk_bus[clk];
		if (!busclk)
			continue;

		rate = clk_get_rate(busclk);
		if (rate > max)
			max = rate;
	}

	return max;
}

/**
 * sdhci_s3c_consider_clock - consider one the bus clocks for current setting
 * @ourhost: Our SDHCI instance.
 * @src: The source clock index.
 * @wanted: The clock frequency wanted.
 */
static unsigned int sdhci_s3c_consider_clock(struct sdhci_host *host,
					     unsigned int src,
					     unsigned int wanted)
{
	unsigned long rate;
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct sdhci_s3c *ourhost = pltfm_host->priv;
	struct clk *clksrc = ourhost->clk_bus[src];
	int div;

	if (!clksrc)
		return UINT_MAX;

	/*
	 * If controller uses a non-standard clock division, find the best clock
	 * speed possible with selected clock source and skip the division.
	 */
	if (host->quirks & SDHCI_QUIRK_NONSTANDARD_CLOCK) {
		rate = clk_round_rate(clksrc, wanted);
		return wanted - rate;
	}

	rate = clk_get_rate(clksrc);

	for (div = 1; div < 256; div *= 2) {
		if ((rate / div) <= wanted)
			break;
	}

	dev_dbg(&ourhost->pdev->dev, "clk %d: rate %ld, want %d, got %ld\n",
		src, rate, wanted, rate / div);

	return wanted - (rate / div);
}

/**
 * sdhci_s3c_set_clock - callback on clock change
 * @host: The SDHCI host being changed
 * @clock: The clock rate being requested.
 *
 * When the card's clock is going to be changed, look at the new frequency
 * and find the best clock source to go with it.
*/
static void sdhci_s3c_set_clock(struct sdhci_host *host, unsigned int clock)
{
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct sdhci_s3c *ourhost = pltfm_host->priv;
	unsigned int best = UINT_MAX;
	unsigned int delta;
	int best_src = 0;
	int src;
	u32 ctrl;

	/* don't bother if the clock is going off. */
	if (clock == 0)
		return;

	for (src = 0; src < MAX_BUS_CLK; src++) {
		delta = sdhci_s3c_consider_clock(host, src, clock);
		if (delta < best) {
			best = delta;
			best_src = src;
		}
	}

	dev_dbg(&ourhost->pdev->dev,
		"selected source %d, clock %d, delta %d\n",
		 best_src, clock, best);

	/* select the new clock source */
	if (pltfm_host->clock != best_src) {
		struct clk *clk = ourhost->clk_bus[best_src];

		clk_prepare_enable(clk);
		clk_disable_unprepare(ourhost->clk_bus[pltfm_host->clock]);

		/* turn clock off to card before changing clock source */
		writew(0, host->ioaddr + SDHCI_CLOCK_CONTROL);

		pltfm_host->clock = best_src;
		host->max_clk = clk_get_rate(clk);

		ctrl = readl(host->ioaddr + S3C_SDHCI_CONTROL2);
		ctrl &= ~S3C_SDHCI_CTRL2_SELBASECLK_MASK;
		ctrl |= best_src << S3C_SDHCI_CTRL2_SELBASECLK_SHIFT;
		writel(ctrl, host->ioaddr + S3C_SDHCI_CONTROL2);
	}

	/* reprogram default hardware configuration */
	writel(S3C64XX_SDHCI_CONTROL4_DRIVE_9mA,
		host->ioaddr + S3C64XX_SDHCI_CONTROL4);

	ctrl = readl(host->ioaddr + S3C_SDHCI_CONTROL2);
	ctrl |= (S3C64XX_SDHCI_CTRL2_ENSTAASYNCCLR |
		  S3C64XX_SDHCI_CTRL2_ENCMDCNFMSK |
		  S3C_SDHCI_CTRL2_ENFBCLKRX |
		  S3C_SDHCI_CTRL2_DFCNT_NONE |
		  S3C_SDHCI_CTRL2_ENCLKOUTHOLD);
	writel(ctrl, host->ioaddr + S3C_SDHCI_CONTROL2);

	/* reconfigure the controller for new clock rate */
	ctrl = (S3C_SDHCI_CTRL3_FCSEL1 | S3C_SDHCI_CTRL3_FCSEL0);
	if (clock < 25 * 1000000)
		ctrl |= (S3C_SDHCI_CTRL3_FCSEL3 | S3C_SDHCI_CTRL3_FCSEL2);
	writel(ctrl, host->ioaddr + S3C_SDHCI_CONTROL3);
}

/**
 * sdhci_s3c_get_min_clock - callback to get minimal supported clock value
 * @host: The SDHCI host being queried
 *
 * To init mmc host properly a minimal clock value is needed. For high system
 * bus clock's values the standard formula gives values out of allowed range.
 * The clock still can be set to lower values, if clock source other then
 * system bus is selected.
*/
static unsigned int sdhci_s3c_get_min_clock(struct sdhci_host *host)
{
	unsigned int delta, min = UINT_MAX;
	int src;

	for (src = 0; src < MAX_BUS_CLK; src++) {
		delta = sdhci_s3c_consider_clock(host, src, 0);
		if (delta == UINT_MAX)
			continue;
		/* delta is a negative value in this case */
		if (-delta < min)
			min = -delta;
	}
	return min;
}

/* sdhci_cmu_get_max_clk - callback to get maximum clock frequency.*/
static unsigned int sdhci_cmu_get_max_clock(struct sdhci_host *host)
{
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct sdhci_s3c *ourhost = pltfm_host->priv;

	return clk_round_rate(ourhost->clk_bus[pltfm_host->clock], UINT_MAX);
}

/* sdhci_cmu_get_min_clock - callback to get minimal supported clock value. */
static unsigned int sdhci_cmu_get_min_clock(struct sdhci_host *host)
{
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct sdhci_s3c *ourhost = pltfm_host->priv;

	/*
	 * initial clock can be in the frequency range of
	 * 100KHz-400KHz, so we set it as max value.
	 */
	return clk_round_rate(ourhost->clk_bus[pltfm_host->clock], 400000);
}

/* sdhci_cmu_set_clock - callback on clock change.*/
static void sdhci_cmu_set_clock(struct sdhci_host *host, unsigned int clock)
{
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct sdhci_s3c *ourhost = pltfm_host->priv;
	struct device *dev = &ourhost->pdev->dev;
	unsigned long timeout;
	u16 clk = 0;

	/* don't bother if the clock is going off */
	if (clock == 0)
		return;

	sdhci_s3c_set_clock(host, clock);

	clk_set_rate(ourhost->clk_bus[pltfm_host->clock], clock);

	host->clock = clock;

	clk = SDHCI_CLOCK_INT_EN;
	sdhci_writew(host, clk, SDHCI_CLOCK_CONTROL);

	/* Wait max 20 ms */
	timeout = 20;
	while (!((clk = sdhci_readw(host, SDHCI_CLOCK_CONTROL))
		& SDHCI_CLOCK_INT_STABLE)) {
		if (timeout == 0) {
			dev_err(dev, "%s: Internal clock never stabilised.\n",
				mmc_hostname(host->mmc));
			return;
		}
		timeout--;
		mdelay(1);
	}

	clk |= SDHCI_CLOCK_CARD_EN;
	sdhci_writew(host, clk, SDHCI_CLOCK_CONTROL);
}

/**
 * sdhci_s3c_platform_bus_width - support 8bit buswidth
 * @host: The SDHCI host being queried
 * @width: MMC_BUS_WIDTH_ macro for the bus width being requested
 *
 * We have 8-bit width support but is not a v3 controller.
 * So we add platform_bus_width() and support 8bit width.
 */
static int sdhci_s3c_platform_bus_width(struct sdhci_host *host, int width)
{
	u8 ctrl;

	ctrl = sdhci_readb(host, SDHCI_HOST_CONTROL);

	switch (width) {
	case MMC_BUS_WIDTH_8:
		ctrl |= SDHCI_CTRL_8BITBUS;
		ctrl &= ~SDHCI_CTRL_4BITBUS;
		break;
	case MMC_BUS_WIDTH_4:
		ctrl |= SDHCI_CTRL_4BITBUS;
		ctrl &= ~SDHCI_CTRL_8BITBUS;
		break;
	default:
		ctrl &= ~SDHCI_CTRL_4BITBUS;
		ctrl &= ~SDHCI_CTRL_8BITBUS;
		break;
	}

	sdhci_writeb(host, ctrl, SDHCI_HOST_CONTROL);

	return 0;
}

static struct sdhci_ops sdhci_s3c_ops = {
	.get_max_clock		= sdhci_s3c_get_max_clk,
	.set_clock		= sdhci_s3c_set_clock,
	.get_min_clock		= sdhci_s3c_get_min_clock,
	.platform_bus_width	= sdhci_s3c_platform_bus_width,
};

static void sdhci_s3c_notify_change(struct platform_device *dev, int state)
{
	struct sdhci_host *host = platform_get_drvdata(dev);
	unsigned long flags;

	if (host) {
		spin_lock_irqsave(&host->lock, flags);
		if (state) {
			dev_dbg(&dev->dev, "card inserted.\n");
			host->flags &= ~SDHCI_DEVICE_DEAD;
			host->quirks |= SDHCI_QUIRK_BROKEN_CARD_DETECTION;
		} else {
			dev_dbg(&dev->dev, "card removed.\n");
			host->flags |= SDHCI_DEVICE_DEAD;
			host->quirks &= ~SDHCI_QUIRK_BROKEN_CARD_DETECTION;
		}
		tasklet_schedule(&host->card_tasklet);
		spin_unlock_irqrestore(&host->lock, flags);
	}
}

#ifdef CONFIG_OF
static struct s3c_sdhci_platdata *sdhci_s3c_parse_dt(struct device *dev)
{
	struct s3c_sdhci_platdata *pdata = NULL;
	struct device_node *node = dev->of_node;
	u32 max_width;
	int gpio;

	pdata = devm_kzalloc(dev, sizeof(*pdata), GFP_KERNEL);
	if (!pdata) {
		dev_err(dev, "No memory for pdata\n");
		return ERR_PTR(-EINVAL);
	}

	/* if the bus-width property is not specified, assume width as 1 */
	if (of_property_read_u32(node, "bus-width", &max_width))
		max_width = 1;
	pdata->max_width = max_width;

	/* get the card detection method */
	if (of_get_property(node, "broken-cd", NULL)) {
		pdata->cd_type = S3C_SDHCI_CD_NONE;
		return pdata;
	}

	if (of_get_property(node, "non-removable", NULL)) {
		pdata->cd_type = S3C_SDHCI_CD_PERMANENT;
		return pdata;
	}

	gpio = of_get_named_gpio(node, "cd-gpios", 0);
	if (gpio_is_valid(gpio)) {
		pdata->cd_type = S3C_SDHCI_CD_GPIO;
		pdata->ext_cd_gpio = gpio;
		if (of_get_property(node, "cd-inverted", NULL))
			pdata->ext_cd_gpio_invert = 1;
		return pdata;
	} else if (gpio != -ENOENT) {
		dev_err(dev, "invalid card detect gpio specified\n");
		return ERR_PTR(-EINVAL);
	}

	/* assuming internal card detect that will be configured by pinctrl */
	pdata->cd_type = S3C_SDHCI_CD_INTERNAL;
	return pdata;
}
#else
static struct s3c_sdhci_platdata *sdhci_s3c_parse_dt(struct device *dev)
{
	return ERR_PTR(-EINVAL);
}
#endif

static const struct of_device_id sdhci_s3c_dt_match[];

static inline struct sdhci_s3c_drv_data *sdhci_s3c_get_driver_data(
			struct platform_device *pdev)
{
#ifdef CONFIG_OF
	if (pdev->dev.of_node) {
		const struct of_device_id *match;
		match = of_match_node(sdhci_s3c_dt_match, pdev->dev.of_node);
		return (struct sdhci_s3c_drv_data *)match->data;
	}
#endif
	return (struct sdhci_s3c_drv_data *)
			platform_get_device_id(pdev)->driver_data;
}

static struct sdhci_pltfm_data sdhci_s3c_pdata = {
	.quirks = SDHCI_QUIRK_NO_ENDATTR_IN_NOPDESC |
		SDHCI_QUIRK_NO_HISPD_BIT | SDHCI_QUIRK_NO_BUSY_IRQ |
		SDHCI_QUIRK_MULTIBLOCK_READ_ACMD12 |
		SDHCI_QUIRK_BROKEN_ADMA_ZEROLEN_DESC |
		SDHCI_QUIRK_32BIT_DMA_ADDR | SDHCI_QUIRK_32BIT_DMA_SIZE |
		SDHCI_QUIRK_DATA_TIMEOUT_USES_SDCLK,
	.ops = &sdhci_s3c_ops,
};

static int sdhci_s3c_probe(struct platform_device *pdev)
{
	struct sdhci_s3c_drv_data *drv_data;
	struct device *dev = &pdev->dev;
	struct sdhci_host *host;
	struct sdhci_s3c *sc;
	int ret = 0, ptr, clks;
	struct sdhci_pltfm_host *pltfm_host;

	if (!pdev->dev.platform_data && !pdev->dev.of_node) {
		dev_err(dev, "no device data specified\n");
		return -ENOENT;
	}

	host = sdhci_pltfm_init(pdev, &sdhci_s3c_pdata);
	if (IS_ERR(host)) {
		dev_err(dev, "sdhci_alloc_host() failed\n");
		return PTR_ERR(host);
	}

	sc = devm_kzalloc(dev, sizeof(struct sdhci_s3c), GFP_KERNEL);
	if (!sc) {
		ret = -ENOMEM;
		goto err_pdata_io_clk;
	}

	pltfm_host = sdhci_priv(host);
	pltfm_host->priv = sc;

	if (pdev->dev.of_node) {
		sc->pdata = sdhci_s3c_parse_dt(&pdev->dev);
		if (!sc->pdata)
			goto err_pdata_io_clk;
	} else
		memcpy(&sc->pdata, &pdev->dev.platform_data, sizeof(sc->pdata));

	drv_data = sdhci_s3c_get_driver_data(pdev);
	if (drv_data)
		host->quirks |= drv_data->sdhci_quirks;

	sc->pdev = pdev;

	pltfm_host->clk = devm_clk_get(dev, "hsmmc");
	if (IS_ERR(pltfm_host->clk)) {
		dev_err(dev, "failed to get io clock\n");
		ret = PTR_ERR(pltfm_host->clk);
		goto err_pdata_io_clk;
	}

	/* enable the local io clock and keep it running for the moment. */
	clk_prepare_enable(pltfm_host->clk);

	for (clks = 0, ptr = 0; ptr < MAX_BUS_CLK; ptr++) {
		struct clk *clk;
		char name[14];

		snprintf(name, 14, "mmc_busclk.%d", ptr);
		clk = devm_clk_get(dev, name);
		if (IS_ERR(clk))
			continue;

		clks++;
		sc->clk_bus[ptr] = clk;

		/*
		 * save current clock index to know which clock bus
		 * is used later in overriding functions.
		 */
		pltfm_host->clock = ptr;

		dev_info(dev, "clock source %d: %s (%ld Hz)\n",
			 ptr, name, clk_get_rate(clk));
	}

	if (clks == 0) {
		dev_err(dev, "failed to find any bus clocks\n");
		ret = -ENOENT;
		goto err_no_busclks;
	}

#ifndef CONFIG_PM_RUNTIME
	clk_prepare_enable(sc->clk_bus[pltfm_host->clock]);
#endif

	/* Ensure we have minimal gpio selected CMD/CLK/Detect */
	if (sc->pdata->cfg_gpio)
		sc->pdata->cfg_gpio(pdev, sc->pdata->max_width);

#ifndef CONFIG_MMC_SDHCI_S3C_DMA

	/* we currently see overruns on errors, so disable the SDMA
	 * support as well. */
	host->quirks |= SDHCI_QUIRK_BROKEN_DMA;

#endif /* CONFIG_MMC_SDHCI_S3C_DMA */

	if (sc->pdata->cd_type == S3C_SDHCI_CD_NONE ||
	    sc->pdata->cd_type == S3C_SDHCI_CD_PERMANENT)
		host->quirks |= SDHCI_QUIRK_BROKEN_CARD_DETECTION;

	if (sc->pdata->cd_type == S3C_SDHCI_CD_PERMANENT)
		host->mmc->caps = MMC_CAP_NONREMOVABLE;

	switch (sc->pdata->max_width) {
	case 8:
		host->mmc->caps |= MMC_CAP_8_BIT_DATA;
	case 4:
		host->mmc->caps |= MMC_CAP_4_BIT_DATA;
		break;
	}

	if (sc->pdata->pm_caps)
		host->mmc->pm_caps |= sc->pdata->pm_caps;

	/*
	 * If controller does not have internal clock divider,
	 * we can use overriding functions instead of default.
	 */
	if (host->quirks & SDHCI_QUIRK_NONSTANDARD_CLOCK) {
		sdhci_s3c_ops.set_clock = sdhci_cmu_set_clock;
		sdhci_s3c_ops.get_min_clock = sdhci_cmu_get_min_clock;
		sdhci_s3c_ops.get_max_clock = sdhci_cmu_get_max_clock;
	}

	/* It supports additional host capabilities if needed */
	if (sc->pdata->host_caps)
		host->mmc->caps |= sc->pdata->host_caps;

	if (sc->pdata->host_caps2)
		host->mmc->caps2 |= sc->pdata->host_caps2;

	pm_runtime_enable(&pdev->dev);
	pm_runtime_set_autosuspend_delay(&pdev->dev, 50);
	pm_runtime_use_autosuspend(&pdev->dev);
	pm_suspend_ignore_children(&pdev->dev, 1);

	ret = sdhci_add_host(host);
	if (ret) {
		dev_err(dev, "sdhci_add_host() failed\n");
		pm_runtime_forbid(&pdev->dev);
		pm_runtime_get_noresume(&pdev->dev);
		goto err_req_regs;
	}

	/* The following two methods of card detection might call
	   sdhci_s3c_notify_change() immediately, so they can be called
	   only after sdhci_add_host(). Setup errors are ignored. */
	if (sc->pdata->cd_type == S3C_SDHCI_CD_EXTERNAL &&
	    sc->pdata->ext_cd_init)
		sc->pdata->ext_cd_init(&sdhci_s3c_notify_change);
	if (sc->pdata->cd_type == S3C_SDHCI_CD_GPIO &&
	    gpio_is_valid(sc->pdata->ext_cd_gpio)) {
		ret = mmc_gpio_request_cd(host->mmc, sc->pdata->ext_cd_gpio);
		if (ret) {
			dev_err(dev,
				"failed to request card detect gpio\n");
			goto err_req_cd;
		}
		irq_set_irq_wake(gpio_to_irq(sc->pdata->ext_cd_gpio), 1);
	}

#ifdef CONFIG_PM_RUNTIME
	if (sc->pdata->cd_type != S3C_SDHCI_CD_INTERNAL)
		clk_disable_unprepare(pltfm_host->clk);
#endif
	return 0;

 err_req_cd:
	mmc_gpio_free_cd(host->mmc);

 err_req_regs:
#ifndef CONFIG_PM_RUNTIME
	clk_disable_unprepare(sc->clk_bus[pltfm_host->clock]);
#endif

 err_no_busclks:
	clk_disable_unprepare(pltfm_host->clk);

 err_pdata_io_clk:
	sdhci_free_host(host);

	return ret;
}

static int sdhci_s3c_remove(struct platform_device *pdev)
{
	struct sdhci_host *host =  platform_get_drvdata(pdev);
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct sdhci_s3c *sc = pltfm_host->priv;
	struct s3c_sdhci_platdata *pdata = sc->pdata;

	if (pdata->cd_type == S3C_SDHCI_CD_EXTERNAL && pdata->ext_cd_cleanup)
		pdata->ext_cd_cleanup(&sdhci_s3c_notify_change);

	if (pdata->ext_cd_gpio)
		mmc_gpio_free_cd(host->mmc);

#ifdef CONFIG_PM_RUNTIME
	if (pdata->cd_type != S3C_SDHCI_CD_INTERNAL)
		clk_prepare_enable(pltfm_host->clk);
#endif
	sdhci_remove_host(host, 1);

	pm_runtime_dont_use_autosuspend(&pdev->dev);
	pm_runtime_disable(&pdev->dev);

#ifndef CONFIG_PM_RUNTIME
	clk_disable_unprepare(sc->clk_bus[pltfm_host->clock]);
#endif
	clk_disable_unprepare(pltfm_host->clk);

	sdhci_free_host(host);
	platform_set_drvdata(pdev, NULL);

	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int sdhci_s3c_suspend(struct device *dev)
{
	struct sdhci_host *host = dev_get_drvdata(dev);

	return sdhci_suspend_host(host);
}

static int sdhci_s3c_resume(struct device *dev)
{
	struct sdhci_host *host = dev_get_drvdata(dev);

	return sdhci_resume_host(host);
}
#endif

#ifdef CONFIG_PM_RUNTIME
static int sdhci_s3c_runtime_suspend(struct device *dev)
{
	struct sdhci_host *host = dev_get_drvdata(dev);
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct sdhci_s3c *ourhost = pltfm_host->priv;
	int ret;

	ret = sdhci_runtime_suspend_host(host);
	clk_disable_unprepare(ourhost->clk_bus[pltfm_host->clock]);
	clk_disable_unprepare(pltfm_host->clk);
	return ret;
}

static int sdhci_s3c_runtime_resume(struct device *dev)
{
	struct sdhci_host *host = dev_get_drvdata(dev);
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct sdhci_s3c *ourhost = pltfm_host->priv;
	int ret;

	clk_prepare_enable(pltfm_host->clk);
	clk_prepare_enable(ourhost->clk_bus[pltfm_host->clock]);
	ret = sdhci_runtime_resume_host(host);
	return ret;
}
#endif

#ifdef CONFIG_PM
static const struct dev_pm_ops sdhci_s3c_pmops = {
	SET_SYSTEM_SLEEP_PM_OPS(sdhci_s3c_suspend, sdhci_s3c_resume)
	SET_RUNTIME_PM_OPS(sdhci_s3c_runtime_suspend, sdhci_s3c_runtime_resume,
			   NULL)
};

#define SDHCI_S3C_PMOPS (&sdhci_s3c_pmops)

#else
#define SDHCI_S3C_PMOPS NULL
#endif

#if defined(CONFIG_CPU_EXYNOS4210) || defined(CONFIG_SOC_EXYNOS4212)
static struct sdhci_s3c_drv_data exynos4_sdhci_drv_data = {
	.sdhci_quirks = SDHCI_QUIRK_NONSTANDARD_CLOCK,
};
#define EXYNOS4_SDHCI_DRV_DATA ((kernel_ulong_t)&exynos4_sdhci_drv_data)
#else
#define EXYNOS4_SDHCI_DRV_DATA ((kernel_ulong_t)NULL)
#endif

static struct platform_device_id sdhci_s3c_driver_ids[] = {
	{
		.name		= "s3c-sdhci",
		.driver_data	= (kernel_ulong_t)NULL,
	}, {
		.name		= "exynos4-sdhci",
		.driver_data	= EXYNOS4_SDHCI_DRV_DATA,
	},
	{ }
};
MODULE_DEVICE_TABLE(platform, sdhci_s3c_driver_ids);

#ifdef CONFIG_OF
static const struct of_device_id sdhci_s3c_dt_match[] = {
	{ .compatible = "samsung,s3c6410-sdhci", },
	{ .compatible = "samsung,exynos4210-sdhci",
		.data = (void *)EXYNOS4_SDHCI_DRV_DATA },
	{},
};
MODULE_DEVICE_TABLE(of, sdhci_s3c_dt_match);
#endif

static struct platform_driver sdhci_s3c_driver = {
	.probe		= sdhci_s3c_probe,
	.remove		= sdhci_s3c_remove,
	.id_table	= sdhci_s3c_driver_ids,
	.driver		= {
		.owner	= THIS_MODULE,
		.name	= "s3c-sdhci",
		.of_match_table = of_match_ptr(sdhci_s3c_dt_match),
		.pm	= SDHCI_S3C_PMOPS,
	},
};

module_platform_driver(sdhci_s3c_driver);

MODULE_DESCRIPTION("Samsung SDHCI (HSMMC) glue");
MODULE_AUTHOR("Ben Dooks, <ben@simtec.co.uk>");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:s3c-sdhci");
