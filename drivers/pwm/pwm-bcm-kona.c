/*
 * Copyright (C) 2013 Broadcom Corporation
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation version 2.
 *
 * This program is distributed "as is" WITHOUT ANY WARRANTY of any
 * kind, whether express or implied; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/clk.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/ioport.h>
#include <linux/math64.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/pwm.h>
#include <linux/slab.h>
#include <linux/types.h>

#define KONA_PWM_CHANNEL_CNT		6

#define PWM_CONTROL_OFFSET		(0x00000000)
#define PWM_CONTROL_INITIAL		(0x3f3f3f00)
#define PWMOUT_POLARITY(chan)		(0x1 << (8 + chan))
#define PWMOUT_ENABLE(chan)		(0x1 << chan)

#define PRESCALE_OFFSET			(0x00000004)
#define PRESCALE_SHIFT(chan)		(chan << 2)
#define PRESCALE_MASK(chan)		(~(0x7 << (chan << 2)))
#define PRESCALE_MIN			(0x00000000)
#define PRESCALE_MAX			(0x00000007)

#define PERIOD_COUNT_OFFSET(chan)	(0x00000008 + (chan << 3))
#define PERIOD_COUNT_MIN		(0x00000002)
#define PERIOD_COUNT_MAX		(0x00ffffff)

#define DUTY_CYCLE_HIGH_OFFSET(chan)	(0x0000000c + (chan << 3))
#define DUTY_CYCLE_HIGH_MIN		(0x00000000)
#define DUTY_CYCLE_HIGH_MAX		(0x00ffffff)

struct kona_pwmc {
	struct pwm_chip chip;
	void __iomem *base;
	struct clk *clk;
};

static void kona_pwmc_apply_settings(struct kona_pwmc *kp, int chan)
{
	/* New settings take effect on rising edge of enable  bit */
	writel(readl(kp->base + PWM_CONTROL_OFFSET) & ~PWMOUT_ENABLE(chan),
	       kp->base + PWM_CONTROL_OFFSET);
	writel(readl(kp->base + PWM_CONTROL_OFFSET) | PWMOUT_ENABLE(chan),
	       kp->base + PWM_CONTROL_OFFSET);
}

static int kona_pwmc_config(struct pwm_chip *chip, struct pwm_device *pwm,
				int duty_ns, int period_ns)
{
	struct kona_pwmc *kp = dev_get_drvdata(chip->dev);
	u64 val, div, clk_rate;
	unsigned long prescale = PRESCALE_MIN, pc, dc;
	int chan = pwm->hwpwm;

	/*
	 * Find period count, duty count and prescale to suit duty_ns and
	 * period_ns. This is done according to formulas described below:
	 *
	 * period_ns = 10^9 * (PRESCALE + 1) * PC / PWM_CLK_RATE
	 * duty_ns = 10^9 * (PRESCALE + 1) * DC / PWM_CLK_RATE
	 *
	 * PC = (PWM_CLK_RATE * period_ns) / (10^9 * (PRESCALE + 1))
	 * DC = (PWM_CLK_RATE * duty_ns) / (10^9 * (PRESCALE + 1))
	 */

	clk_rate = clk_get_rate(kp->clk);
	while (1) {
		div = 1000000000;
		div *= 1 + prescale;
		val = clk_rate * period_ns;
		pc = div64_u64(val, div);
		val = clk_rate * duty_ns;
		dc = div64_u64(val, div);

		/* If duty_ns or period_ns are not achievable then return */
		if (pc < PERIOD_COUNT_MIN || dc < DUTY_CYCLE_HIGH_MIN)
			return -EINVAL;

		/*
		 * If pc or dc have crossed their upper limit, then increase
		 * prescale and recalculate pc and dc.
		 */
		if (pc > PERIOD_COUNT_MAX || dc > DUTY_CYCLE_HIGH_MAX) {
			if (++prescale > PRESCALE_MAX)
				return -EINVAL;
			continue;
		}
		break;
	}

	/* Program prescale */
	writel((readl(kp->base + PRESCALE_OFFSET) & PRESCALE_MASK(chan)) |
	       prescale << PRESCALE_SHIFT(chan),
	       kp->base + PRESCALE_OFFSET);

	/* Program period count */
	writel(pc, kp->base + PERIOD_COUNT_OFFSET(chan));

	/* Program duty cycle high count */
	writel(dc, kp->base + DUTY_CYCLE_HIGH_OFFSET(chan));

	if (test_bit(PWMF_ENABLED, &pwm->flags))
		kona_pwmc_apply_settings(kp, chan);

	return 0;
}

static int kona_pwmc_enable(struct pwm_chip *chip, struct pwm_device *pwm)
{
	return kona_pwmc_config(chip, pwm, pwm->duty_cycle, pwm->period);
}

static void kona_pwmc_disable(struct pwm_chip *chip, struct pwm_device *pwm)
{
	struct kona_pwmc *kp = dev_get_drvdata(chip->dev);
	int chan = pwm->hwpwm;

	/*
	 * The PWM hardware lacks a proper way to be disabled so
	 * we just program zero duty cycle high count instead
	 */

	writel(0, kp->base + DUTY_CYCLE_HIGH_OFFSET(chan));
	kona_pwmc_apply_settings(kp, chan);
}

static const struct pwm_ops kona_pwm_ops = {
	.config = kona_pwmc_config,
	.owner = THIS_MODULE,
	.enable = kona_pwmc_enable,
	.disable = kona_pwmc_disable,
};

static int kona_pwmc_probe(struct platform_device *pdev)
{
	struct kona_pwmc *kp;
	struct resource *res;
	int ret = 0;

	dev_dbg(&pdev->dev, "bcm_kona_pwmc probe\n");

	kp = devm_kzalloc(&pdev->dev, sizeof(*kp), GFP_KERNEL);
	if (kp == NULL)
		return -ENOMEM;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	kp->base = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(kp->base))
		return PTR_ERR(kp->base);

	kp->clk = devm_clk_get(&pdev->dev, NULL);
	if (IS_ERR(kp->clk)) {
		dev_err(&pdev->dev, "Clock get failed : Err %d\n", ret);
		return PTR_ERR(kp->clk);
	}

	ret = clk_prepare_enable(kp->clk);
	if (ret < 0)
		return ret;

	/* Set smooth mode, push/pull, and normal polarity for all channels */
	writel(PWM_CONTROL_INITIAL, kp->base + PWM_CONTROL_OFFSET);

	dev_set_drvdata(&pdev->dev, kp);

	kp->chip.dev = &pdev->dev;
	kp->chip.ops = &kona_pwm_ops;
	kp->chip.base = -1;
	kp->chip.npwm = KONA_PWM_CHANNEL_CNT;

	ret = pwmchip_add(&kp->chip);
	if (ret < 0) {
		clk_disable_unprepare(kp->clk);
		dev_err(&pdev->dev, "pwmchip_add() failed: Err %d\n", ret);
	}

	return ret;
}

static int kona_pwmc_remove(struct platform_device *pdev)
{
	struct kona_pwmc *kp = platform_get_drvdata(pdev);

	clk_disable_unprepare(kp->clk);
	return pwmchip_remove(&kp->chip);
}

static const struct of_device_id bcm_kona_pwmc_dt[] = {
	{.compatible = "brcm,kona-pwm"},
	{},
};
MODULE_DEVICE_TABLE(of, bcm_kona_pwmc_dt);

static struct platform_driver kona_pwmc_driver = {

	.driver = {
		   .name = "bcm-kona-pwm",
		   .owner = THIS_MODULE,
		   .of_match_table = bcm_kona_pwmc_dt,
		   },

	.probe = kona_pwmc_probe,
	.remove = kona_pwmc_remove,
};

module_platform_driver(kona_pwmc_driver);

MODULE_AUTHOR("Broadcom");
MODULE_DESCRIPTION("Driver for KONA PWMC");
MODULE_LICENSE("GPL");
MODULE_VERSION("1.0");
