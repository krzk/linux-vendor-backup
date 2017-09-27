/*
 * SAMSUNG UNIVERSAL3250 machine file
 *
 * Copyright (c) 2012 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/platform_device.h>
#include <linux/serial_core.h>
#include <linux/io.h>
#include <linux/memblock.h>
#include <linux/notifier.h>
#include <linux/reboot.h>
#include <linux/i2c.h>
#include <linux/cma.h>
#include <linux/ion.h>

#include <asm/mach/arch.h>
#include <asm/hardware/gic.h>
#include <asm/mach-types.h>
#include <asm/setup.h>
#include <mach/map.h>
#include <mach/memory.h>
#include <mach/sec_debug.h>

#include <plat/cpu.h>
#include <plat/devs.h>
#include <plat/regs-serial.h>
#include <plat/clock.h>
#include <plat/iic.h>
#include <plat/adc.h>
#include <plat/watchdog.h>
#include <mach/regs-pmu.h>
#include <mach/gpio.h>
#include <mach/pmu.h>
#include <plat/gpio-cfg.h>

#include "common.h"
#include "board-universal3250.h"

#ifdef CONFIG_MACH_WC1
#include <linux/isa1000a_vibrator.h>
#endif

#if defined(CONFIG_LEDS_AN30259A) && defined(CONFIG_MACH_BLINK)
#include <linux/i2c-gpio.h>
#define BLINK_HW_REV_EMUL 0x0
extern unsigned int system_rev;
#define I2C_IF_LED 15
#endif



#ifdef CONFIG_BT_BCM4334W
#include "include/board-bluetooth-bcm.h"
#endif

/*rfkill device registeration*/
#ifdef CONFIG_BT_BCM4334W
static struct platform_device bcm4334w_bluetooth_device = {
	.name = "bcm4334w_bluetooth",
	.id = -1,
};
#endif

#define UNIVERSAL3250_UCON_DEFAULT	(S3C2410_UCON_TXILEVEL |	\
				 S3C2410_UCON_RXILEVEL |	\
				 S3C2410_UCON_TXIRQMODE |	\
				 S3C2410_UCON_RXIRQMODE |	\
				 S3C2410_UCON_RXFIFO_TOI |	\
				 S3C2443_UCON_RXERR_IRQEN)

#define UNIVERSAL3250_ULCON_DEFAULT	S3C2410_LCON_CS8

#define UNIVERSAL3250_UFCON_DEFAULT	(S3C2410_UFCON_FIFOMODE |	\
				 S5PV210_UFCON_TXTRIG4 |	\
				 S5PV210_UFCON_RXTRIG4)

#ifdef CONFIG_INPUT_REGULATOR_HAPTIC
static struct platform_device regulator_haptic_device = {
	.name = "regulator-haptic",
	.id = 30,
};
#endif

#ifdef CONFIG_MACH_WC1
struct isa1000a_vibrator_platform_data isa1000a_vibrator_data = {
#if 1
		.duty = 39105, /* 39500*99/100,*/
#if 0
		.period = 33244,
#else
		.period = 39500,	/* 99% */
#endif
		.max_timeout = 10000,
		.regulator_name = "vdd_ldo1",
		.pwm_id = 0,
#else
		haptic, duty = < 32580 > ;
		haptic, period = < 33244 > ;
		haptic, max_timeout = < 10000 > ;
		haptic, regulator_name = "VDD_MOTOR_3V0";
		haptic, pwm_id = < 0 > ;
#endif
};


static struct platform_device isa1000a_haptic_device = {
	.name = "isa1000a-vibrator",
	.dev.platform_data = &isa1000a_vibrator_data,
};
#endif


static struct s3c2410_uartcfg universal3250_uartcfgs[] __initdata = {
	[0] = {
		.hwport		= 0,
		.flags		= 0,
		.ucon		= UNIVERSAL3250_UCON_DEFAULT,
		.ulcon		= UNIVERSAL3250_ULCON_DEFAULT,
		.ufcon		= UNIVERSAL3250_UFCON_DEFAULT,
#ifdef CONFIG_BT_BCM4334W
		.wake_peer	= bcm_bt_lpm_exit_lpm_locked,
#endif
	},
	[1] = {
		.hwport		= 1,
		.flags		= 0,
		.ucon		= UNIVERSAL3250_UCON_DEFAULT,
		.ulcon		= UNIVERSAL3250_ULCON_DEFAULT,
		.ufcon		= UNIVERSAL3250_UFCON_DEFAULT,
	},
	[2] = {
		.hwport		= 2,
		.flags		= 0,
		.ucon		= UNIVERSAL3250_UCON_DEFAULT,
		.ulcon		= UNIVERSAL3250_ULCON_DEFAULT,
		.ufcon		= UNIVERSAL3250_UFCON_DEFAULT,
	},
	[3] = {
		.hwport		= 3,
		.flags		= 0,
		.ucon		= UNIVERSAL3250_UCON_DEFAULT,
		.ulcon		= UNIVERSAL3250_ULCON_DEFAULT,
		.ufcon		= UNIVERSAL3250_UFCON_DEFAULT,
	},
};


#if defined(CONFIG_LEDS_AN30259A) && defined(CONFIG_MACH_BLINK)
static struct i2c_gpio_platform_data gpio_i2c_if_led;

struct platform_device s3c_device_i2c_if_led = {
	.name = "i2c-gpio",
	.id = I2C_IF_LED,
	.dev.platform_data = &gpio_i2c_if_led,
};

static struct i2c_board_info i2c_devs_led[] __initdata = {
	{
		I2C_BOARD_INFO("an30259a", 0x30),
	},
};

static bool is_blink_hw_emul(void){
	if(system_rev == BLINK_HW_REV_EMUL)
		return true;
	else
		return false;
}

static void set_led_i2c_pin(bool is_emul){

	if(is_emul){
		gpio_i2c_if_led.scl_pin = EXYNOS4_GPA1(3);
		gpio_i2c_if_led.sda_pin = EXYNOS4_GPA1(2);
	}else{
		gpio_i2c_if_led.scl_pin = EXYNOS4_GPA0(7);
		gpio_i2c_if_led.sda_pin = EXYNOS4_GPA0(6);
	}
}
#endif

static void __init universal3250_map_io(void)
{
	exynos_init_io(NULL, 0);
}

#include <linux/dma-contiguous.h>

extern struct platform_device exynos_drm_device;
extern phys_addr_t arm_lowmem_limit;
/*
 * tizen logo buffer information structure
 *
 * @size: logo buffer size
 * @addr: logo buffer start address
 */
struct tizen_mem_logo_info {
	unsigned long	size;
	unsigned long	addr;
};

static struct tizen_mem_logo_info __initdata logo_info;

static int __init tizen_mem_parse_cmdline(char *str)
{
	char *ret_str;

	if (!str)
		return -EINVAL;

	logo_info.size = memparse(str, &ret_str);
	if (*ret_str == '@')
		logo_info.addr = memparse(ret_str + 1, &ret_str);

	return ret_str > str ? 0 : -EINVAL;
}
early_param("fbmem", tizen_mem_parse_cmdline);

static int __initdata total_memsize;

static int __init tizen_totalmem_parse_cmdline(char *str)
{
	char *ret_str;

	if (!str)
		return -EINVAL;

	total_memsize = memparse(str, &ret_str);

	return ret_str > str ? 0 : -EINVAL;
}
early_param("mem", tizen_totalmem_parse_cmdline);

extern int dma_declare_contiguous(struct device *dev, unsigned long size,
			   phys_addr_t base, phys_addr_t limit);

extern void __init exynos3_universal3250_gpio_init(void);

static inline void __init exynos_reserve_mem(void)
{
#ifndef CONFIG_ARM_DMA_USE_IOMMU
	if (dma_declare_contiguous(&exynos_drm_device.dev,
					32 << 20, 0x48000000, 0))
		printk(KERN_ERR "failed to assign memory for drm driver.\n");
#else
	/*
	 * Check arm_lowmem_limit to know the size of memory.
	 * CMA should be declared within the available memory.
	 */
	if (arm_lowmem_limit > 0x60000000) {
		if (dma_declare_contiguous(&exynos_drm_device.dev,
					16 << 20, logo_info.addr, 0))
			printk(KERN_ERR "failed to assign memory for drm driver.\n");
	} else {
		if (dma_declare_contiguous(&exynos_drm_device.dev,
					16 << 20, 0x48000000, 0))
			printk(KERN_ERR "failed to assign memory for drm driver.\n");
	}
#endif
	exynos3_universal3250_gpio_init();

}

#ifdef CONFIG_SAMSUNG_DEV_ADC
static struct s3c_adc_platdata universal3250_adc_data __initdata = {
	.phy_init	= s3c_adc_phy_init,
	.phy_exit	= s3c_adc_phy_exit,
};
#endif

#ifdef CONFIG_S3C_DEV_WDT
/* WDT */
static struct s3c_watchdog_platdata smdk5410_watchdog_platform_data = {
	exynos_pmu_wdt_control,
	PMU_WDT_RESET_TYPE0,
};
#endif

static struct platform_device *universal3250_devices[] __initdata = {
#ifdef CONFIG_SAMSUNG_DEV_ADC
	&s3c_device_adc,
#endif
#ifdef CONFIG_S3C_DEV_WDT
	&s3c_device_wdt,
#endif
#ifdef CONFIG_MALI400
	&exynos4_device_g3d,
#endif

#if defined(CONFIG_LEDS_AN30259A) && defined(CONFIG_MACH_BLINK)
	&s3c_device_i2c_if_led,
#endif

#ifdef CONFIG_BT_BCM4334W
	&bcm4334w_bluetooth_device,
#endif
#ifdef CONFIG_INPUT_REGULATOR_HAPTIC
	&regulator_haptic_device,
#endif
#ifdef CONFIG_MACH_WC1
	&isa1000a_haptic_device,
#endif

};

static void __init universal3250_machine_init(void)
{
#ifdef CONFIG_SAMSUNG_DEV_ADC
	s3c_adc_set_platdata(&universal3250_adc_data);
#endif
#ifdef CONFIG_S3C_DEV_WDT
	s3c_watchdog_set_platdata(&smdk5410_watchdog_platform_data);
#endif

	exynos3_universal3250_clock_init();
	exynos3_universal3250_mmc_init();
	exynos3_universal3250_power_init();
	exynos3_universal3250_battery_init();
	exynos3_universal3250_input_init();
	exynos3_b2_mfd_init();
	exynos3_universal3250_usb_init();
	tizen_display_init();
#ifdef CONFIG_SND_SOC_SAMSUNG_B2_YMU831
	exynos3_b2_audio_init();
#endif
#ifdef CONFIG_SND_SOC_SAMSUNG_EXYNOS3_I2S_DUMMY
	exynos3_i2s_dummy_audio_init();
#endif
#ifdef CONFIG_SND_SOC_SAMSUNG_WC1_WM1831
	exynos3_orbis_audio_init();
#endif
	exynos3_sensor_init();
#ifdef CONFIG_ICE4_FPGA
	exynos3_b2_fpga_init();
#endif

#if defined(CONFIG_MACH_B2) || defined(CONFIG_MACH_WC1)
	exynos3_universal3250_vibrator_init();
#endif

	exynos3_universal3250_media_init();
#if defined(CONFIG_VIDEO_EXYNOS_FIMC_IS)
	exynos3_universal3250_camera_init();
#endif
	exynos3_thermistor_init();

#if defined(CONFIG_LEDS_AN30259A) && defined(CONFIG_MACH_BLINK)
	set_led_i2c_pin(is_blink_hw_emul());
	i2c_register_board_info(I2C_IF_LED, i2c_devs_led,
				ARRAY_SIZE(i2c_devs_led));
#endif

#ifdef CONFIG_WLAN
	brcm_wlan_init();
#endif
#ifdef CONFIG_NFC
	tizen_nfc_init();
#endif
#ifdef CONFIG_ESE_P3_LSI
	exynos3_orbis_ese_p3_init();
#endif
	platform_add_devices(universal3250_devices, ARRAY_SIZE(universal3250_devices));
}

static void __init universal3250_init_early(void)
{
	sec_debug_init();
	s3c24xx_init_clocks(24000000);
	s3c24xx_init_uarts(universal3250_uartcfgs, ARRAY_SIZE(universal3250_uartcfgs));
}
#if defined(CONFIG_MACH_WC1)
MACHINE_START(WC1, "WC1-S")
	.init_irq	= exynos3_init_irq,
	.init_early	= universal3250_init_early,
	.map_io		= universal3250_map_io,
	.handle_irq	= gic_handle_irq,
	.init_machine	= universal3250_machine_init,
	.timer		= &exynos4_timer,
	.restart	= exynos3_restart,
	.reserve	= exynos_reserve_mem,
MACHINE_END
#elif defined(CONFIG_MACH_SLIM)
MACHINE_START(SLIM, "PONTE-SLIM")
	.init_irq	= exynos3_init_irq,
	.init_early	= universal3250_init_early,
	.map_io		= universal3250_map_io,
	.handle_irq	= gic_handle_irq,
	.init_machine	= universal3250_machine_init,
	.timer		= &exynos4_timer,
	.restart	= exynos3_restart,
	.reserve	= exynos_reserve_mem,
MACHINE_END
#elif defined(CONFIG_MACH_BLINK)
MACHINE_START(BLINK, "BLINK")
	.init_irq	= exynos3_init_irq,
	.init_early	= universal3250_init_early,
	.map_io		= universal3250_map_io,
	.handle_irq	= gic_handle_irq,
	.init_machine	= universal3250_machine_init,
	.timer		= &exynos4_timer,
	.restart	= exynos3_restart,
	.reserve	= exynos_reserve_mem,
MACHINE_END
#elif defined(CONFIG_MACH_WINGTIP)
MACHINE_START(WINGTIP, "WINGTIP")
	.init_irq	= exynos3_init_irq,
	.init_early	= universal3250_init_early,
	.map_io		= universal3250_map_io,
	.handle_irq	= gic_handle_irq,
	.init_machine	= universal3250_machine_init,
	.timer		= &exynos4_timer,
	.restart	= exynos3_restart,
	.reserve	= exynos_reserve_mem,
MACHINE_END
#elif defined(CONFIG_MACH_B2)
MACHINE_START(B2, "B2")
	.init_irq	= exynos3_init_irq,
	.init_early	= universal3250_init_early,
	.map_io		= universal3250_map_io,
	.handle_irq	= gic_handle_irq,
	.init_machine	= universal3250_machine_init,
	.timer		= &exynos4_timer,
	.restart	= exynos3_restart,
	.reserve	= exynos_reserve_mem,
MACHINE_END
#elif defined(CONFIG_MACH_HWP)
MACHINE_START(HWP, "HWP")
	.init_irq	= exynos3_init_irq,
	.init_early	= universal3250_init_early,
	.map_io		= universal3250_map_io,
	.handle_irq	= gic_handle_irq,
	.init_machine	= universal3250_machine_init,
	.timer		= &exynos4_timer,
	.restart	= exynos3_restart,
	.reserve	= exynos_reserve_mem,
MACHINE_END
#else
MACHINE_START(UNIVERSAL3250, "UNIVERSAL3250")
	.init_irq	= exynos3_init_irq,
	.init_early	= universal3250_init_early,
	.map_io		= universal3250_map_io,
	.handle_irq	= gic_handle_irq,
	.init_machine	= universal3250_machine_init,
	.timer		= &exynos4_timer,
	.restart	= exynos3_restart,
	.reserve 	= exynos_reserve_mem,
MACHINE_END
#endif
