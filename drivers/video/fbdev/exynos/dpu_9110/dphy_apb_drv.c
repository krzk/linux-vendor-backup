#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/clk.h>
#include <linux/clk-provider.h>
#include <linux/mutex.h>
#include <linux/wait.h>
#include <linux/platform_device.h>
#include <linux/phy/phy.h>
#include <linux/io.h>
#include <linux/irq.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/kthread.h>
#include <linux/pm_runtime.h>
#include <linux/of_gpio.h>
#include <linux/device.h>
#include <linux/module.h>
#include <linux/exynos-ss.h>
#include <video/mipi_display.h>

static int dphy_apb_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;

	pm_runtime_enable(dev);

	pr_info("dphy_apb driver has been probed.\n");

	return 0;
}

static int dphy_apb_remove(struct platform_device *pdev)
{
	pm_runtime_disable(&pdev->dev);

	pr_info("dphy_apb driver removed.\n");

	return 0;
}

static void dphy_apb_shutdown(struct platform_device *pdev)
{
	pr_info("%s -\n", __func__);
}

static const struct of_device_id dphy_apb_of_match[] = {
	{ .compatible = "samsung,dphy-apb" },
	{},
};
MODULE_DEVICE_TABLE(of, dphy_apb_of_match);

static struct platform_driver dphy_apb_driver __refdata = {
	.probe          = dphy_apb_probe,
	.remove         = dphy_apb_remove,
	.shutdown       = dphy_apb_shutdown,
	.driver = {
		.name       = "dphy_apb",
		.owner      = THIS_MODULE,
		.of_match_table = of_match_ptr(dphy_apb_of_match),
		.suppress_bind_attrs = true,
	}
};

static int __init dphy_apb_init(void)
{
	int ret = platform_driver_register(&dphy_apb_driver);

	if (ret)
		pr_err("dphy_apb driver register failed\n");

	return ret;
}
device_initcall(dphy_apb_init);

static void __exit dphy_apb_exit(void)
{
	platform_driver_unregister(&dphy_apb_driver);
}

module_exit(dphy_apb_exit);
MODULE_DESCRIPTION("Samusung EXYNOS DPHY APB driver");
