#include <linux/delay.h>
#include <linux/pm.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <asm/cacheflush.h>
#include <asm/system.h>
#include <mach/regs-pmu.h>
#include <linux/gpio.h>
#include "common.h"
#include <linux/extcon.h>
#include <linux/of.h>
#include <linux/of_gpio.h>

struct odroid_reboot_platform_data {
	struct device *dev;
	int power_gpio;
};

static struct odroid_reboot_platform_data *g_pdata;

static void odroid_reboot(char str, const char *cmd)
{
	local_irq_disable();

	writel(0x12345678, S5P_INFORM2);	/* Don't enter lpm mode */
	writel(0x0, S5P_INFORM4);		/* Reset reboot count */

        gpio_set_value(g_pdata->power_gpio, 0);
        mdelay(150);
        gpio_set_value(g_pdata->power_gpio, 1);

	flush_cache_all();
	outer_flush_all();

	exynos4_restart(0, 0);

	pr_emerg("%s: waiting for reboot\n", __func__);
	while (1)
		;
}

static struct odroid_reboot_platform_data *odroid_reboot_parse_dt(struct device *dev)
{
	struct device_node *np = dev->of_node;
	struct odroid_reboot_platform_data *pdata;

	pdata = devm_kzalloc(dev, sizeof(struct odroid_reboot_platform_data),
			     GFP_KERNEL);
	if (!pdata)
		return NULL;

	pdata->power_gpio = of_get_named_gpio(np, "reset-gpio", 0);
	if (!gpio_is_valid(pdata->power_gpio)) {
		dev_err(dev, "invalied power-gpio\n");
		return NULL;
	}
	devm_gpio_request(dev, pdata->power_gpio, "reset-gpio");

	return pdata;
}

static int odroid_reboot_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;

	if (pdev->dev.of_node)
		g_pdata = odroid_reboot_parse_dt(dev);

	if (!g_pdata) {
		dev_err(&pdev->dev, "failed to get platform data\n");
		return -EINVAL;
	}

	g_pdata->dev = &pdev->dev;

	/* Set machine specific functions */
	arm_pm_restart = odroid_reboot;

	return 0;
}

static struct of_device_id odroid_reboot_of_match[] = {
	{ .compatible = "hardkernel,odroid-reboot", },
	{ },
};

static struct platform_driver odroid_reboot_driver = {
	.probe		= odroid_reboot_probe,
	.driver		= {
		.name	= "odroid-reboot",
		.owner	= THIS_MODULE,
		.of_match_table = of_match_ptr(odroid_reboot_of_match),
	}
};

module_platform_driver(odroid_reboot_driver);
