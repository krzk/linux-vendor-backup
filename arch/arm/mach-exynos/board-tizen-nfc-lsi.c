#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/gpio_keys.h>
#include <linux/input.h>
#include <linux/i2c.h>
#include <linux/export.h>
#include <linux/interrupt.h>
#include <linux/regulator/consumer.h>

#include <plat/gpio-cfg.h>
#include <plat/devs.h>
#include <plat/iic.h>

#include <mach/irqs.h>
#include <mach/hs-iic.h>
#include <mach/regs-gpio.h>

#include <linux/nfc/sec_nfc.h>

#include "board-universal3250.h"

extern unsigned int system_rev;

#define GPIO_NFC_EN	                EXYNOS3_GPE0(7)
#define GPIO_NFC_FIRMWARE	EXYNOS3_GPE1(0)
#define GPIO_NFC_IRQ          	EXYNOS3_GPX1(6)

#define GPIO_NFC_SDA		EXYNOS3_GPC1(3)
#define GPIO_NFC_SCL		EXYNOS3_GPC1(4)

#define NFC_I2C_ADR                   0x27

/* GPIO_LEVEL_NONE = 2, GPIO_LEVEL_LOW = 0 */
#ifdef CONFIG_NFC
static unsigned int nfc_gpio_table[][4] = {
	{GPIO_NFC_IRQ, 0x0f, 2, S3C_GPIO_PULL_DOWN},
	{GPIO_NFC_EN, S3C_GPIO_OUTPUT, 1, S3C_GPIO_PULL_NONE},
	{GPIO_NFC_FIRMWARE, S3C_GPIO_OUTPUT, 0, S3C_GPIO_PULL_NONE},
};

static struct sec_nfc_platform_data i2c_devs_nfc_pdata = {
	.irq = GPIO_NFC_IRQ,
	.ven = GPIO_NFC_EN,
	.firm = GPIO_NFC_FIRMWARE,
};
#if 0
static struct i2c_board_info i2c_devs6[] = {
	{
		I2C_BOARD_INFO(SEC_NFC_DRIVER_NAME, 0x27),
		.platform_data = &i2c_devs_nfc_pdata,
		.irq =  IRQ_EINT(14),//GPIO_NFC_IRQ,
	},
};
#else
static struct i2c_board_info i2c_devs_nfc[] = {
	{
		I2C_BOARD_INFO(SEC_NFC_DRIVER_NAME, 0x27),
		.platform_data = &i2c_devs_nfc_pdata,
		.irq =  IRQ_EINT(14),/*GPIO_NFC_IRQ,*/
	},
};
#endif

static inline void nfc_setup_gpio(void)
{
	int array_size = ARRAY_SIZE(nfc_gpio_table);
	u32 i, gpio;

	pr_info( "[NFC]sec nfc nfc_setup_gpio \n");

	for (i = 0; i < array_size; i++) {
		gpio = nfc_gpio_table[i][0];
		s3c_gpio_cfgpin(gpio, S3C_GPIO_SFN(nfc_gpio_table[i][1]));
		s3c_gpio_setpull(gpio, nfc_gpio_table[i][3]);
		if (nfc_gpio_table[i][2] != 2)
			gpio_set_value(gpio, nfc_gpio_table[i][2]);
	}

	if (system_rev == 0x00) {

		pr_info("[NFC] Orbis nfc Rev 0.0\n");

		s3c_gpio_setpull(GPIO_NFC_SDA, S3C_GPIO_PULL_NONE);
		s3c_gpio_setpull(GPIO_NFC_SCL, S3C_GPIO_PULL_NONE);
	} else {

		pr_info("[NFC] Orbis nfc Rev 0.1 etc\n");

		s3c_gpio_setpull(EXYNOS3_GPD1(2), S3C_GPIO_PULL_NONE);
		s3c_gpio_setpull(EXYNOS3_GPD1(3), S3C_GPIO_PULL_NONE);
	}

	/*
	s5p_gpio_set_pd_cfg(GPIO_NFC_EN, S5P_GPIO_PD_PREV_STATE);
	s5p_gpio_set_pd_cfg(GPIO_NFC_FIRMWARE, S5P_GPIO_PD_PREV_STATE);
	*/
}



/* In SLP Kernel, i2c_bus number is decided at board file. */
void __init tizen_nfc_init()
{
	pr_info("[NFC]sec nfc tizen_nfc_init\n");
	nfc_setup_gpio();

	 if (system_rev == 0x00) {
		pr_info("[NFC] Orbis nfc Rev 0.0\n");

		s3c_i2c6_set_platdata(NULL);
		i2c_register_board_info(6, i2c_devs_nfc,
			ARRAY_SIZE(i2c_devs_nfc));

		platform_device_register(&s3c_device_i2c6);
	 } else {
		pr_info("[NFC] Orbis nfc Rev 0.1 etc\n");

		s3c_i2c1_set_platdata(NULL);
		i2c_register_board_info(1, i2c_devs_nfc,
			ARRAY_SIZE(i2c_devs_nfc));

		platform_device_register(&s3c_device_i2c1);
	 }
}
#endif
