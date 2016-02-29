#include <linux/platform_device.h>
#include <linux/gpio.h>
#include <plat/gpio-cfg.h>
#include <linux/ese_p3.h>


#define GPIO_P3_SPI_CS		EXYNOS3_GPC0(2)
#define GPIO_P3_SPI_CLK		EXYNOS3_GPC0(0)
#define GPIO_P3_SPI_MOSI	EXYNOS3_GPC0(4)
#define GPIO_P3_SPI_MISO	EXYNOS3_GPC0(3)





static struct p3_platform_data p3_padat = {
	.irq_gpio = 0,
	.rst_gpio = 0,
	.cs_gpio = GPIO_P3_SPI_CS,
	.clk_gpio = GPIO_P3_SPI_CLK,
	.mosi_gpio = GPIO_P3_SPI_MOSI,
	.miso_gpio = GPIO_P3_SPI_MISO,
};

static struct platform_device ese_p3 = {
	.name = "p3",
	.dev = {
		.platform_data = &p3_padat,
	},
};

void __init exynos3_orbis_ese_p3_init(void)
{
	int ret = 0;

	pr_info("%s\n", __func__);

	ret = platform_device_register(&ese_p3);
	if (ret < 0)
		pr_err("Fail to register platform device(err=%d)\n", ret);
}

