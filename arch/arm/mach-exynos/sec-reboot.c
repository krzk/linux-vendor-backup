#include <linux/delay.h>
#include <linux/err.h>
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

struct sec_reboot_platform_data {
	struct device *dev;
	struct extcon_dev *edev;
	int power_button;
};

static struct sec_reboot_platform_data *g_pdata;

enum cable_type {
	NONE = 0,
	CHARGER,
	UART,
};

static int is_cable_attached(void)
{
#ifdef CONFIG_EXTCON
	/* Use extcon subsystem to check state of jig cable */
	static char *cables[] = {
		/*
		 * FIXME: This function has strong dependency on extcon provider
		 * (muic device) because of unfixed cable name among all extcon
		 * provider. It has potential issue ,if board use different muic
		 * device for controlling external cable. So, the dependency
		 * between sec-reboot.c and extcon device driver will be removed
		 * by using attribute feature of each cable.
		 */
		"USB",
		"TA",
		/*
		 * JIG UART cable makes JIGONB pin low and then PMIC will ignore
		 * PS_HOLD pin state. As a result, power-off try will be failed.
		 */
		"JIG-UART-OFF",
	};
	int i;

	/* Use extcon subsystem to check the state of cable */
	for (i = 0 ; i < ARRAY_SIZE(cables) ; i++) {
		if (g_pdata &&
			extcon_get_cable_state(g_pdata->edev, cables[i])) {
			if (!strncmp(cables[i], "JIG-UART", 8))
				return UART;
			else
				return CHARGER;
		}
	}
#endif
	/* None of cables are attached */
	return NONE;
}

static void sec_power_off(void)
{
	int cable, poweroff_try = 0;

	if (g_pdata && !g_pdata->edev) {
		g_pdata->edev = extcon_get_edev_by_phandle(g_pdata->dev, 0);
		if (IS_ERR(g_pdata->edev)) {
			dev_err(g_pdata->dev, "couldn't get extcon device\n");
			return;
		}
	}

	local_irq_disable();

	while (1) {
		/* Check reboot charging */
		if ((cable = is_cable_attached()) || (poweroff_try >= 5)) {
			pr_emerg("%s : Can't power off, reboot!"
				 " (cable=%d, poweroff_try=%d)\n",
				 __func__, cable, poweroff_try);

			/* To enter LP charging */
			if (cable == CHARGER)
				writel(0x0, S5P_INFORM2);

			flush_cache_all();
			outer_flush_all();

			exynos4_restart(0, 0);

			pr_emerg("%s: waiting for reboot\n", __func__);
			while (1)
				;
		}

		/* Check if power button is released. */
		if (!g_pdata || gpio_get_value(g_pdata->power_button)) {
			/*
			 * Power-off code for EXYNOS
			 * EXYNOS series can be power off by setting PS_HOLD pin
			 * from High to Low. The pin state can be set via PMU's
			 * register, PS_HOLD_CONTROL(R/W, 0x1002_330C).
			 */
			pr_emerg("%s: set PS_HOLD low\n", __func__);
			writel(readl(S5P_PS_HOLD_CONTROL) & 0xFFFFFEFF,
			       S5P_PS_HOLD_CONTROL);

			pr_emerg
			    ("%s: Should not reach here! (poweroff_try:%d)\n",
			     __func__, ++poweroff_try);
		} else {
			/* if power button is not released, wait and retry */
			pr_info("%s: PowerButton is not released.\n", __func__);
		}
		mdelay(1000);
	}
}

#define REBOOT_MODE_PREFIX	0x12345670
#define REBOOT_MODE_NONE	0
#define REBOOT_MODE_DOWNLOAD	1
#define REBOOT_MODE_UPLOAD	2
#define REBOOT_MODE_CHARGING	3
#define REBOOT_MODE_RECOVERY	4
#define REBOOT_MODE_FOTA	5
#define REBOOT_MODE_FOTA_BL	6		/* update bootloader */
#define REBOOT_MODE_SECURE	7		/* image secure check fail */

#define REBOOT_SET_PREFIX	0xabc00000
#define REBOOT_SET_DEBUG	0x000d0000
#define REBOOT_SET_SWSEL	0x000e0000
#define REBOOT_SET_SUD		0x000f0000

static void sec_reboot(char str, const char *cmd)
{
	local_irq_disable();

	pr_emerg("%s (%d, %s)\n", __func__, str, cmd ? cmd : "(null)");

	writel(0x12345678, S5P_INFORM2);	/* Don't enter lpm mode */

	if (!cmd) {
		writel(REBOOT_MODE_PREFIX | REBOOT_MODE_NONE, S5P_INFORM3);
	} else {
		unsigned long value;
		if (!strcmp(cmd, "fota"))
			writel(REBOOT_MODE_PREFIX | REBOOT_MODE_FOTA,
			       S5P_INFORM3);
		else if (!strcmp(cmd, "fota_bl"))
			writel(REBOOT_MODE_PREFIX | REBOOT_MODE_FOTA_BL,
			       S5P_INFORM3);
		else if (!strcmp(cmd, "recovery"))
			writel(REBOOT_MODE_PREFIX | REBOOT_MODE_RECOVERY,
			       S5P_INFORM3);
		else if (!strcmp(cmd, "download"))
			writel(REBOOT_MODE_PREFIX | REBOOT_MODE_DOWNLOAD,
			       S5P_INFORM3);
		else if (!strcmp(cmd, "upload"))
			writel(REBOOT_MODE_PREFIX | REBOOT_MODE_UPLOAD,
			       S5P_INFORM3);
		else if (!strcmp(cmd, "secure"))
			writel(REBOOT_MODE_PREFIX | REBOOT_MODE_SECURE,
			       S5P_INFORM3);
		else if (!strncmp(cmd, "debug", 5)
			 && !kstrtoul(cmd + 5, 0, &value))
			writel(REBOOT_SET_PREFIX | REBOOT_SET_DEBUG | value,
			       S5P_INFORM3);
		else if (!strncmp(cmd, "swsel", 5)
			 && !kstrtoul(cmd + 5, 0, &value))
			writel(REBOOT_SET_PREFIX | REBOOT_SET_SWSEL | value,
			       S5P_INFORM3);
		else if (!strncmp(cmd, "sud", 3)
			 && !kstrtoul(cmd + 3, 0, &value))
			writel(REBOOT_SET_PREFIX | REBOOT_SET_SUD | value,
			       S5P_INFORM3);
		else if (!strncmp(cmd, "emergency", 9))
			writel(0, S5P_INFORM3);
		else
			writel(REBOOT_MODE_PREFIX | REBOOT_MODE_NONE,
			       S5P_INFORM3);
	}

	flush_cache_all();
	outer_flush_all();

	exynos4_restart(0, 0);

	pr_emerg("%s: waiting for reboot\n", __func__);
	while (1)
		;
}

#ifdef CONFIG_OF
static struct sec_reboot_platform_data *sec_reboot_parse_dt(struct device *dev)
{
	struct device_node *np = dev->of_node;
	struct sec_reboot_platform_data *pdata;

	if (!np)
		return NULL;

	pdata = devm_kzalloc(dev, sizeof(struct sec_reboot_platform_data),
			     GFP_KERNEL);
	if (!pdata)
		return ERR_PTR(ENOMEM);

	pdata->power_button = of_get_named_gpio(np, "power-gpio", 0);
	if (!gpio_is_valid(pdata->power_button)) {
		dev_err(dev, "invalied power-gpio\n");
		return NULL;
	}

	return pdata;
}
#else
static struct sec_reboot_platform_data *sec_reboot_parse_dt(struct device *dev)
{
	return ERR_PTR(-ENODEV);
}
#endif

static int sec_reboot_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;

	if (pdev->dev.platform_data)
		g_pdata = dev_get_platdata(dev);
	else if (pdev->dev.of_node)
		g_pdata = sec_reboot_parse_dt(dev);
	else
		g_pdata = NULL;

	if (!g_pdata) {
		dev_err(&pdev->dev, "failed to get platform data\n");
		return -EINVAL;
	}

	g_pdata->dev = &pdev->dev;

	/* Set machine specific functions */
	pm_power_off = sec_power_off;
	arm_pm_restart = sec_reboot;

	return 0;
}

static int sec_reboot_remove(struct platform_device *pdev)
{
	return 0;
}

static struct of_device_id sec_reboot_of_match[] = {
	{ .compatible = "samsung,sec-reboot", },
	{ },
};

static struct platform_driver sec_reboot_driver = {
	.probe		= sec_reboot_probe,
	.remove		= sec_reboot_remove,
	.driver		= {
		.name	= "sec-reboot",
		.owner	= THIS_MODULE,
		.of_match_table = of_match_ptr(sec_reboot_of_match),
	}
};

module_platform_driver(sec_reboot_driver);
