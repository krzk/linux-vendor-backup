/*
 * Copyright (C) 2009 Texas Instruments Inc.
 * Mikkel Christensen <mlc@ti.com>
 *
 * Modified from mach-omap2/board-archer.c
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/input.h>
#include <linux/gpio.h>
#include <linux/input/matrix_keypad.h>
#include <asm/mach-types.h>
#include <linux/err.h>
#include <linux/clk.h>
#include <asm/mach/arch.h>
#include <linux/bootmem.h>
#include <linux/spi/spi.h>

#include <linux/i2c/twl.h>
#include <linux/interrupt.h>
#include <linux/regulator/machine.h>
#include <linux/switch.h>
#include <linux/io.h>
#include <linux/gpio.h>
#include <plat/common.h>
#include <mach/hardware.h>
#include <asm/mach-types.h>
#include <asm/mach/map.h>
#include <plat/board.h>
#include <linux/spi/spi.h>
#include <mach/board-zoom.h>

#include <plat/mcspi.h>
#include <plat/gpio.h>
#include <plat/board.h>
#include <plat/usb.h>
#include <plat/common.h>
#include <plat/dma.h>
#include <plat/gpmc.h>
#include <plat/display.h>
#include <plat/usb.h>
#include <plat/clock.h>
#include <plat/display.h>

#include <asm/io.h>
#include <asm/delay.h>
#include <plat/control.h>
#include <plat/gpmc-smc91x.h>
#include <mach/board-latona.h>
#include <plat/omap-pm.h>
#include <plat/mux.h>
#include <linux/interrupt.h>
#include <plat/control.h>
#include <plat/clock.h>
#include <asm/setup.h>
#include <linux/leds.h>
#include <plat/prcm.h>
#include "cm.h"

#include "mux.h"
#include "sdram-micron-mt46h32m32lf-6.h"
#include "sdram-qimonda-hyb18m512160af-6.h"
#include <linux/ctype.h>
#include "omap3-opp.h"
#include "pm.h"
#include "mmc-twl4030.h"
extern int set_wakeup_gpio(void);
extern int omap_gpio_out_init(void);

#ifdef CONFIG_PM
#include <plat/vrfb.h>
#include <media/videobuf-dma-sg.h>
#include <media/v4l2-device.h>
#include <../../../drivers/media/video/omap/omap_voutdef.h>
#endif

#define ZEUS_CAM
#ifdef ZEUS_CAM
/* include files for cam pmic (power) and cam sensor */
#include "../../../drivers/media/video/cam_pmic.h"
#include "../../../drivers/media/video/ce147.h"
#include "../../../drivers/media/video/s5ka3dfx.h"
struct ce147_platform_data zeus_ce147_platform_data;
struct s5ka3dfx_platform_data zeus_s5ka3dfx_platform_data;
#endif

#if defined( CONFIG_SAMSUNG_PHONE_SVNET )
#include <linux/phone_svn/modemctl.h>
#if defined(CONFIG_PHONE_ONEDRAM_MODULE)
#include <linux/phone_svn/onedram.h>
#elif defined (CONFIG_PHONE_IPC_SPI_MODULE)
#include <linux/phone_svn/ipc_spi.h>
#endif
#include <linux/irq.h>
#endif // CONFIG_SAMSUNG_PHONE_SVNET

struct class *sec_class;
EXPORT_SYMBOL(sec_class);

struct timezone sec_sys_tz;
EXPORT_SYMBOL(sec_sys_tz);

void (*sec_set_param_value) (int idx, void *value);
EXPORT_SYMBOL(sec_set_param_value);

void (*sec_get_param_value) (int idx, void *value);
EXPORT_SYMBOL(sec_get_param_value);

u32 hw_revision;
EXPORT_SYMBOL(hw_revision);

struct device *sio_switch_dev;
EXPORT_SYMBOL(sio_switch_dev);

#ifdef CONFIG_VIDEO_LV8093
#include <media/lv8093.h>
//extern struct imx046_platform_data zoom2_lv8093_platform_data;
#define LV8093_PS_GPIO			7
/* GPIO7 is connected to lens PS pin through inverter */
#define LV8093_PWR_OFF			1
#define LV8093_PWR_ON			(!LV8093_PWR_OFF)
#endif

#ifndef CONFIG_TWL4030_CORE
#error "no power companion board defined!"
#else
#define TWL4030_USING_BROADCAST_MSG
#endif

#ifdef CONFIG_WL127X_RFKILL
#include <linux/wl127x-rfkill.h>
#endif

extern int always_opp5;
int usbsel = 1;
EXPORT_SYMBOL(usbsel);
void (*usbsel_notify) (int) = NULL;
EXPORT_SYMBOL(usbsel_notify);

extern unsigned get_last_off_on_transaction_id(struct device *dev);

//extern unsigned int omap34xx_pins_size;
//extern struct pin_config *omap34xx_pins_ptr;
extern struct omap_board_mux *board_mux_ptr;

#ifdef CONFIG_WL127X_RFKILL
static struct wl127x_rfkill_platform_data wl127x_plat_data = {
	.bt_nshutdown_gpio = OMAP_GPIO_BT_EN,	/* Bluetooth Enable GPIO */
	.fm_enable_gpio = -1,	/* FM Enable GPIO */
};

static struct platform_device zoom2_wl127x_device = {
	.name = "wl127x-rfkill",
	.id = -1,
	.dev.platform_data = &wl127x_plat_data,
};
#endif

#ifdef CONFIG_SAMSUNG_HW_EMU_BOARD
static int zeus_twl4030_keymap[] = {
	KEY(0, 1, KEY_MENU),
	KEY(0, 2, KEY_BACK),
	KEY(1, 1, KEY_CAMERA_FOCUS),
	KEY(1, 2, KEY_VOLUMEUP),
	KEY(2, 1, KEY_CAMERA),
	KEY(2, 2, KEY_VOLUMEDOWN),
	0
};
#else
static int zeus_twl4030_keymap[] = {
	KEY(2, 1, KEY_VOLUMEUP),
	KEY(1, 1, KEY_VOLUMEDOWN),
	0
};
#endif
static struct matrix_keymap_data board_map_data = {
	.keymap = zeus_twl4030_keymap,
	.keymap_size = ARRAY_SIZE(zeus_twl4030_keymap),
};

static struct twl4030_keypad_data board_kp_data = {
	.keymap_data = &board_map_data,
	.rows = 5,
	.cols = 6,
	.rep = 1,
};

static struct resource board_power_key_resources[] = {
	[0] = {
	       // PWRON KEY
	       .start = 0,
	       .end = 0,
	       .flags = IORESOURCE_IRQ | IORESOURCE_IRQ_HIGHLEVEL,
	       },
	[1] = {
	       // HOME KEY
	       .start = 0,
	       .end = 0,
	       .flags = IORESOURCE_IRQ | IORESOURCE_IRQ_HIGHLEVEL,
	       },

};

static struct platform_device board_power_key_device = {
	.name = "power_key_device",
	.id = -1,
	.num_resources = ARRAY_SIZE(board_power_key_resources),
	.resource = board_power_key_resources,
};

#ifdef CONFIG_INPUT_ZEUS_EAR_KEY
static struct resource board_ear_key_resource = {
	.start = 0,
	.end = 0,
	.flags = IORESOURCE_IRQ | IORESOURCE_IRQ_HIGHLEVEL,
};

static struct platform_device board_ear_key_device = {
	.name = "ear_key_device",
	.id = -1,
	.num_resources = 1,
	.resource = &board_ear_key_resource,
};
#endif
static struct resource samsung_charger_resources[] = {
	[0] = {
	       // USB IRQ
	       .start = 0,
	       .end = 0,
	       .flags = IORESOURCE_IRQ | IORESOURCE_IRQ_SHAREABLE,
	       },
	[1] = {
	       // TA IRQ
	       .start = 0,
	       .end = 0,
	       .flags = IORESOURCE_IRQ | IORESOURCE_IRQ_HIGHEDGE | IORESOURCE_IRQ_LOWEDGE,
	       },
	[2] = {
	       // CHG_ING_N
	       .start = 0,
	       .end = 0,
	       .flags = IORESOURCE_IRQ | IORESOURCE_IRQ_HIGHEDGE | IORESOURCE_IRQ_LOWEDGE,
	       },
	[3] = {
	       // CHG_EN
	       .start = 0,
	       .end = 0,
	       .flags = IORESOURCE_IRQ,
	       },
};

static int samsung_charger_config_data[] = {
	// [ CHECK VF USING ADC ]
	/*   1. ENABLE  (true, flase) */
	false,

	/*   2. ADCPORT (ADCPORT NUM) */
	1,

	// [ SUPPORT CHG_ING IRQ FOR CHECKING FULL ]
	/*   1. ENABLE  (true, flase) */
	true,
};

static int samsung_battery_config_data[] = {
	// [ SUPPORT MONITORING CHARGE CURRENT FOR CHECKING FULL ]
	/*   1. ENABLE  (true, flase) */
	false,
	/*   2. ADCPORT (ADCPORT NUM) */
	4,

	// [ SUPPORT MONITORING TEMPERATURE OF THE SYSTEM FOR BLOCKING CHARGE ]
	/*   1. ENABLE  (true, flase) */
	true,

	/*   2. ADCPORT (ADCPORT NUM) */
	0,
};

static struct platform_device samsung_charger_device = {
	.name = "secChargerDev",
	.id = -1,
	.num_resources = ARRAY_SIZE(samsung_charger_resources),
	.resource = samsung_charger_resources,

	.dev = {
		.platform_data = &samsung_charger_config_data,
		}
};

static struct platform_device samsung_battery_device = {
	.name = "secBattMonitor",
	.id = -1,
	.num_resources = 0,
	.dev = {
		.platform_data = &samsung_battery_config_data,
		}
};

static struct platform_device samsung_virtual_rtc_device = {
	.name = "virtual_rtc",
	.id = -1,
};

// cdy_100111 add vibrator device 
static struct platform_device samsung_vibrator_device = {
	.name = "secVibrator",
	.id = -1,
	.num_resources = 0,
};

static struct platform_device samsung_pl_sensor_power_device = {
	.name = "secPLSensorPower",
	.id = -1,
	.num_resources = 0,
};

// ryun 20100216 for KEY LED driver
static struct led_info sec_keyled_list[] = {
	{
	 .name = "button-backlight",
	 },
};

static struct led_platform_data sec_keyled_data = {
	.num_leds = ARRAY_SIZE(sec_keyled_list),
	.leds = sec_keyled_list,
};

static struct platform_device samsung_led_device = {
	.name = "secLedDriver",
	.id = -1,
	.num_resources = 0,
	.dev = {
		.platform_data = &sec_keyled_data,
		},
};
//
static struct platform_device samsung_aat1402_led_device = {
	.name           = "aat1402-leds",
	.id             = -1,
	.num_resources  = 0,
};
	
//ZEUS_LCD
static struct omap2_mcspi_device_config board_lcd_mcspi_config = {
	.turbo_mode = 0,
	.single_channel = 1,	/* 0: slave, 1: master */
};

#if defined( CONFIG_PHONE_IPC_SPI_MODULE )
//IPC_SPI
static struct omap2_mcspi_device_config board_ipc_spi_mcspi_config = {
	.turbo_mode     =   0,
	.single_channel =   1,
};
#elif defined( CONFIG_PHONE_LOOPBACK_TEST )
//SPI_LoopBack_Test
static struct omap2_mcspi_device_config board_spi_loopback_test_mcspi_config = {
	.turbo_mode     =   0,
	.single_channel =   1,
};
#endif // CONFIG_PHONE_IPC_SPI, CONFIG_PHONE_LOOPBACK_TEST

static struct spi_board_info board_spi_board_info[] __initdata = {
#ifdef CONFIG_SAMSUNG_HW_REL_BOARD	
	[0] = {
		   .modalias = "nt35510_disp_spi",
		   .bus_num = 1,
		   .chip_select = 0,
		   .max_speed_hz = 375000,
		   .controller_data = &board_lcd_mcspi_config,
		   },		// Hydis WVGA LCD	
#else
	[0] = {
		   .modalias = "s6d16a0x21_disp_spi",
		   .bus_num = 1,
		   .chip_select = 0,
		   .max_speed_hz = 375000,
		   .controller_data = &board_lcd_mcspi_config,
		   },		//SHARP HVGA LCD
#endif

#if defined( CONFIG_PHONE_IPC_SPI_MODULE )
	[ 1 ] = {
		.modalias = "ipc_spi",
		.bus_num = 2,
		.chip_select = 0,
		.max_speed_hz = 24000000,
		.controller_data = &board_ipc_spi_mcspi_config,
	}, //IPC_SPI
#elif defined( CONFIG_PHONE_LOOPBACK_TEST )
	[ 1 ] = {
		.modalias = "spi_loopback_test",
		.bus_num = 2,
		.chip_select = 0,
		.max_speed_hz = 24000000,
		.controller_data = &board_spi_loopback_test_mcspi_config,
	}, //SPI_LoopBack_Test
#endif // CONFIG_PHONE_IPC_SPI, CONFIG_PHONE_LOOPBACK_TEST

};

#ifdef CONFIG_SAMSUNG_HW_REL_BOARD	
static struct omap_dss_device board_lcd_device = {
	.name = "lcd",
	.driver_name = "nt35510_panel",
	.type = OMAP_DISPLAY_TYPE_DPI,
	.phy.dpi.data_lines = 24,
	.platform_enable = NULL,
	.platform_disable = NULL,
};
#else
static struct omap_dss_device board_lcd_device = {
	.name = "lcd",
	.driver_name = "s6d16a0x21_panel",
	.type = OMAP_DISPLAY_TYPE_DPI,
	.phy.dpi.data_lines = 24,
	.platform_enable = NULL,
	.platform_disable = NULL,
};
#endif


static struct omap_dss_device *board_dss_devices[] = {
	&board_lcd_device,
};

static struct omap_dss_board_info board_dss_data = {
	.get_last_off_on_transaction_id = get_last_off_on_transaction_id,
	.num_devices = ARRAY_SIZE(board_dss_devices),
	.devices = board_dss_devices,
	.default_device = &board_lcd_device,
};

static struct platform_device board_dss_device = {
	.name = "omapdss",
	.id = -1,
	.dev = {
		.platform_data = &board_dss_data,
		},
};

#ifdef CONFIG_FB_OMAP2
static struct resource board_vout_resource[3 - CONFIG_FB_OMAP2_NUM_FBS] = {
};
#else
static struct resource board_vout_resource[2] = {
};
#endif

#ifdef CONFIG_PM
struct vout_platform_data zeus_vout_data = {
	.set_min_bus_tput = omap_pm_set_min_bus_tput,
	.set_max_mpu_wakeup_lat = omap_pm_set_max_mpu_wakeup_lat,
	.set_min_mpu_freq = omap_pm_set_min_mpu_freq,
};
#endif

static struct platform_device zeus_vout_device = {
	.name = "omap_vout",
	.num_resources = ARRAY_SIZE(board_vout_resource),
	.resource = &board_vout_resource[0],
	.id = -1,
#ifdef CONFIG_PM
	.dev = {
		.platform_data = &zeus_vout_data,
		}
#else
	.dev = {
		.platform_data = NULL,
		}
#endif
};

static struct gpio_switch_platform_data headset_switch_data = {
	.name = "h2w",
	.gpio = OMAP_GPIO_DET_3_5,	/* Omap3430 GPIO_27 For samsung zeus */
};

static struct platform_device headset_switch_device = {
	.name = "switch-gpio",
	.dev = {
		.platform_data = &headset_switch_data,
		}
};

static struct platform_device sec_device_dpram = {
	.name = "dpram-device",
	.id = -1,
};

/* SIO Switch */
struct platform_device sec_sio_switch = {
	.name = "switch-sio",
	.id = -1,
};

#define OMAP_GPIO_SYS_DRM_MSECURE 22
static int __init msecure_init(void)
{
	int ret = 0;

#ifdef CONFIG_RTC_DRV_TWL4030
	/* 3430ES2.0 doesn't have msecure/gpio-22 line connected to T2 */
	if (omap_type() == OMAP2_DEVICE_TYPE_GP) {
		void __iomem *msecure_pad_config_reg =
		    omap_ctrl_base_get() + 0xA3C;
		int mux_mask = 0x04;
		u16 tmp;
#if 1
		ret = gpio_request(OMAP_GPIO_SYS_DRM_MSECURE, "msecure");
		if (ret < 0) {
			printk(KERN_ERR "msecure_init: can't"
			       "reserve GPIO:%d !\n",
			       OMAP_GPIO_SYS_DRM_MSECURE);
			goto out;
		}
		/*
		 * TWL4030 will be in secure mode if msecure line from OMAP
		 * is low. Make msecure line high in order to change the
		 * TWL4030 RTC time and calender registers.
		 */
		tmp = __raw_readw(msecure_pad_config_reg);
		tmp &= 0xF8;	/* To enable mux mode 03/04 = GPIO_RTC */
		tmp |= mux_mask;	/* To enable mux mode 03/04 = GPIO_RTC */
		__raw_writew(tmp, msecure_pad_config_reg);

		gpio_direction_output(OMAP_GPIO_SYS_DRM_MSECURE, 1);
#endif
	}
out:
#endif
	return ret;
}

#if defined( CONFIG_SAMSUNG_PHONE_SVNET )
#if defined( CONFIG_PHONE_ONEDRAM )
static void onedram_cfg_gpio( void )
{
	// Mux Setting -> mux_xxxx_rxx.c

	// Irq Setting - Onedram Mailbox Int
	set_irq_type( OMAP_GPIO_IRQ( 181 ), IRQ_TYPE_EDGE_RISING );
}

static struct onedram_platform_data onedram_data = {
	.cfg_gpio = onedram_cfg_gpio,
};

static struct resource onedram_res[] = {
	[ 0 ] = {
		.start = ( 0x80000000 + 0x05000000 ), // Physical memory address
		.end = ( 0x80000000 + 0x05000000 + 0x1000000 - 1 ),
		.flags = IORESOURCE_MEM,
	},
	[ 1 ] = {
		.start = OMAP_GPIO_IRQ( 181 ), // Irq - Onedram Mailbox Int
		.end = OMAP_GPIO_IRQ( 181 ),
		.flags = IORESOURCE_IRQ,
	},
};

static struct platform_device onedram = {
	.name = "onedram",
	.id = -1,
	.num_resources = ARRAY_SIZE( onedram_res ),
	.resource = onedram_res,
	.dev = {
		.platform_data = &onedram_data,
	},
};
#elif defined( CONFIG_PHONE_IPC_SPI_MODULE )
static void ipc_spi_cfg_gpio( void );

static struct ipc_spi_platform_data ipc_spi_data = {
	.gpio_ipc_mrdy = OMAP_GPIO_IPC_MRDY,
	.gpio_ipc_srdy = OMAP_GPIO_IPC_SRDY,	

	.cfg_gpio = ipc_spi_cfg_gpio,
};

static struct resource ipc_spi_res[] = {
	[ 0 ] = {
		.start = OMAP_GPIO_IRQ( OMAP_GPIO_IPC_SRDY ),
		.end = OMAP_GPIO_IRQ( OMAP_GPIO_IPC_SRDY ),
		.flags = IORESOURCE_IRQ,
	},
};

static struct platform_device ipc_spi = {
	.name = "onedram",
	.id = -1,
	.num_resources = ARRAY_SIZE( ipc_spi_res ),
	.resource = ipc_spi_res,
	.dev = {
		.platform_data = &ipc_spi_data,
	},
};

static void ipc_spi_cfg_gpio( void )
{
	int err = 0;
	
	unsigned gpio_ipc_mrdy = ipc_spi_data.gpio_ipc_mrdy;
	unsigned gpio_ipc_srdy = ipc_spi_data.gpio_ipc_srdy;

	// Mux Setting -> mux_xxxx_rxx.c

	err = gpio_request( gpio_ipc_mrdy, "IPC_MRDY" );
	if( err ) {
		printk( "ipc_spi_cfg_gpio - fail to request gpio %s : %d\n", "IPC_MRDY", err );
	}
	else {
		gpio_direction_output( gpio_ipc_mrdy, 0 );
	}

	err = gpio_request( gpio_ipc_srdy, "IPC_SRDY" );
	if( err ) {
		printk( "ipc_spi_cfg_gpio - fail to request gpio %s : %d\n", "IPC_SRDY", err );
	}
	else {
		gpio_direction_input( gpio_ipc_srdy );
	}
	
	// Irq Setting -
	set_irq_type( OMAP_GPIO_IRQ( OMAP_GPIO_IPC_SRDY ), IRQ_TYPE_EDGE_RISING );
}
#endif // CONFIG_PHONE_ONEDRAM, CONFIG_PHONE_IPC_SPI

static void modemctl_cfg_gpio( void );

static struct modemctl_platform_data mdmctl_data = {
	.name = "xmm",
	
	.gpio_phone_active = OMAP_GPIO_PHONE_ACTIVE,
	.gpio_pda_active = OMAP_GPIO_PDA_ACTIVE,
	.gpio_cp_reset = OMAP_GPIO_CP_RST, // cp_rst gpio - 43
	//.gpio_phone_on = GPIO_PHONE_ON,
	//.gpio_usim_boot = GPIO_USIM_BOOT,
	//.gpio_flm_sel = GPIO_FLM_SEL,
	//.gpio_sim_ndetect = GPIO_SIM_nDETECT,
	
	.cfg_gpio = modemctl_cfg_gpio,
};

static struct resource mdmctl_res[] = {
	[ 0 ] = {
		.start = OMAP_GPIO_IRQ( OMAP_GPIO_PHONE_ACTIVE ), // phone active irq
		.end = OMAP_GPIO_IRQ( OMAP_GPIO_PHONE_ACTIVE ),
		.flags = IORESOURCE_IRQ,
	},
};

static struct platform_device modemctl = {
	.name = "modemctl",
	.id = -1,
	.num_resources = ARRAY_SIZE( mdmctl_res ),
	.resource = mdmctl_res,
	.dev = {
		.platform_data = &mdmctl_data,
	},
};

static void modemctl_cfg_gpio( void )
{
	int err = 0;
	
	unsigned gpio_cp_rst = mdmctl_data.gpio_cp_reset;
	unsigned gpio_pda_active = mdmctl_data.gpio_pda_active;
	unsigned gpio_phone_active = mdmctl_data.gpio_phone_active;

	// Mux Setting -> mux_xxxx_rxx.c

	err = gpio_request( gpio_cp_rst, "CP_RST" );
	if( err ) {
		printk( "modemctl_cfg_gpio - fail to request gpio %s : %d\n", "CP_RST", err );
	}
	else {
		gpio_direction_output( gpio_cp_rst, 0 );
	}

	err = gpio_request( gpio_pda_active, "PDA_ACTIVE" );
	if( err ) {
		printk( "modemctl_cfg_gpio - fail to request gpio %s : %d\n", "PDA_ACTIVE", err );
	}
	else {
		gpio_direction_output( gpio_pda_active, 0 );
	}

	err = gpio_request( gpio_phone_active, "PHONE_ACTIVE" );
	if( err ) {
		printk( "modemctl_cfg_gpio - fail to request gpio %s : %d\n", "PHONE_ACTIVE", err );
	}
	else {
		gpio_direction_input( gpio_phone_active );
	}
	
	set_irq_type( OMAP_GPIO_IRQ( OMAP_GPIO_PHONE_ACTIVE ), IRQ_TYPE_EDGE_BOTH );

	//set_irq_type( gpio_sim_ndetect, IRQ_TYPE_EDGE_BOTH );
}
#endif // CONFIG_SAMSUNG_PHONE_SVNET

static struct platform_device *board_devices[] __initdata = {
	&board_dss_device,

//      &zeus_vout_device,   //commented to remove double registration of omap_vout
	&headset_switch_device,
#ifdef CONFIG_WL127X_RFKILL
	&zoom2_wl127x_device,
#endif
	&board_power_key_device,
#ifdef CONFIG_INPUT_ZEUS_EAR_KEY
	&board_ear_key_device,
#endif
	&samsung_battery_device,
	&samsung_charger_device,
	&samsung_vibrator_device,	// cdy_100111 add vibrator device
	&sec_device_dpram,
	&samsung_pl_sensor_power_device,
	&samsung_led_device,
	&samsung_aat1402_led_device,
	&sec_sio_switch,
#ifdef CONFIG_RTC_DRV_VIRTUAL
	&samsung_virtual_rtc_device,
#endif

#if defined( CONFIG_SAMSUNG_PHONE_SVNET )

#if defined( CONFIG_PHONE_ONEDRAM_MODULE )
	&onedram,
#elif defined( CONFIG_PHONE_IPC_SPI_MODULE )
	&ipc_spi,
#endif	
//	&modemctl,
#endif // CONFIG_SAMSUNG_PHONE_SVNET
};

static inline void __init board_init_power_key(void)
{
	board_power_key_resources[0].start = gpio_to_irq(OMAP_GPIO_KEY_PWRON);
	if (gpio_request(OMAP_GPIO_KEY_PWRON, "power_key_irq") < 0) {
		printk(KERN_ERR
		       "\n FAILED TO REQUEST GPIO %d for POWER KEY IRQ \n",
		       OMAP_GPIO_KEY_PWRON);
		return;
	}
	board_power_key_resources[1].start = gpio_to_irq(OMAP_GPIO_KEY_HOME);
	if (gpio_request(OMAP_GPIO_KEY_HOME, "home_key_irq") < 0) {
		printk(KERN_ERR
		       "\n FAILED TO REQUEST GPIO %d for VOLDN KEY IRQ \n",
		       OMAP_GPIO_KEY_HOME);
		return;
	}
	gpio_direction_input(OMAP_GPIO_KEY_PWRON);
	gpio_direction_input(OMAP_GPIO_KEY_HOME);
}

#ifdef CONFIG_INPUT_ZEUS_EAR_KEY
static inline void __init board_init_ear_key(void)
{
	board_ear_key_resource.start = gpio_to_irq(OMAP_GPIO_EAR_SEND_END);
	if (gpio_request(OMAP_GPIO_EAR_SEND_END, "ear_key_irq") < 0) {
		printk(KERN_ERR
		       "\n FAILED TO REQUEST GPIO %d for POWER KEY IRQ \n",
		       OMAP_GPIO_EAR_SEND_END);
		return;
	}
	gpio_direction_input(OMAP_GPIO_EAR_SEND_END);

}
#endif

static inline void __init board_init_battery(void)
{
	samsung_charger_resources[0].start = 0;	//gpio_to_irq(OMAP_GPIO_USBSW_NINT);;    
	if (gpio_request(OMAP_GPIO_TA_NCONNECTED, "ta_nconnected irq") < 0) {
		printk(KERN_ERR
		       "Failed to request GPIO%d for ta_nconnected IRQ\n",
		       OMAP_GPIO_TA_NCONNECTED);
		samsung_charger_resources[1].start = -1;
	} else {
		samsung_charger_resources[1].start =
		    gpio_to_irq(OMAP_GPIO_TA_NCONNECTED);
		omap_set_gpio_debounce_time(OMAP_GPIO_TA_NCONNECTED, 3);
		omap_set_gpio_debounce(OMAP_GPIO_TA_NCONNECTED, true);
	}

	if (gpio_request(OMAP_GPIO_CHG_ING_N, "charge full irq") < 0) {
		printk(KERN_ERR
		       "Failed to request GPIO%d for charge full IRQ\n",
		       OMAP_GPIO_CHG_ING_N);
		samsung_charger_resources[2].start = -1;
	} else {
		samsung_charger_resources[2].start =
		    gpio_to_irq(OMAP_GPIO_CHG_ING_N);
		omap_set_gpio_debounce_time(OMAP_GPIO_CHG_ING_N, 3);
		omap_set_gpio_debounce(OMAP_GPIO_CHG_ING_N, true);
	}

	if (gpio_request(OMAP_GPIO_CHG_EN, "Charge enable gpio") < 0) {
		printk(KERN_ERR
		       "Failed to request GPIO%d for charge enable gpio\n",
		       OMAP_GPIO_CHG_EN);
		samsung_charger_resources[3].start = -1;
	} else {
		samsung_charger_resources[3].start =
		    gpio_to_irq(OMAP_GPIO_CHG_EN);
	}

}

static void __init omap_board_init_irq(void)
{
	omap_init_irq();
	omap2_init_common_hw(hyb18m512160af6_sdrc_params,
			     NULL,
			     omap3630_mpu_rate_table,
			     omap3630_dsp_rate_table, omap3630_l3_rate_table);

	//omap_gpio_init();  //Crashing in kernel-2.6.32

	//zeus_init_smc91x();

	board_init_power_key();
#ifdef CONFIG_INPUT_ZEUS_EAR_KEY
	board_init_ear_key();
#endif
	board_init_battery();

}

static struct regulator_consumer_supply board_vdda_dac_supply = {
	.supply = "vdda_dac",
	.dev = &board_dss_device.dev,
};

/* REVISIT: These audio entries can be removed once MFD code is merged */

static struct regulator_consumer_supply board_vsim_supply = {
	.supply = "vmmc_aux",
};

static struct regulator_consumer_supply board_vmmc1_supply = {
	.supply = "vmmc",
};

static struct regulator_consumer_supply board_vmmc2_supply = {
	.supply = "vmmc",
};

static struct regulator_consumer_supply board_vaux1_supply = {
	.supply = "vaux1",
};

static struct regulator_consumer_supply board_vaux2_supply = {
	.supply = "vaux2",
};

static struct regulator_consumer_supply board_vaux3_supply = {
	.supply = "vaux3",
};

static struct regulator_consumer_supply board_vaux4_supply = {
	.supply = "vaux4",
};

static struct regulator_consumer_supply board_vpll2_supply = {
	.supply = "vpll2",
};

struct regulator_init_data board_vdac = {
	.constraints = {
			.min_uV = 1800000,
			.max_uV = 1800000,
			.valid_modes_mask = REGULATOR_MODE_NORMAL | REGULATOR_MODE_STANDBY,
			.valid_ops_mask = REGULATOR_CHANGE_MODE | REGULATOR_CHANGE_STATUS,
			},
	.num_consumer_supplies = 1,
	.consumer_supplies = &board_vdda_dac_supply,
};

/* VMMC1 for OMAP VDD_MMC1 (i/o) and MMC1 card */
static struct regulator_init_data board_vmmc1 = {
	.constraints = {
			.min_uV = 1850000,
			.max_uV = 3150000,
			.valid_modes_mask = REGULATOR_MODE_NORMAL | REGULATOR_MODE_STANDBY,
			.valid_ops_mask = REGULATOR_CHANGE_VOLTAGE | REGULATOR_CHANGE_MODE | REGULATOR_CHANGE_STATUS,
			},
	.num_consumer_supplies = 1,
	.consumer_supplies = &board_vmmc1_supply,
};

/* VMMC2 for MMC2 card */
static struct regulator_init_data board_vmmc2 = {
	.constraints = {
			.min_uV = 1850000,
			.max_uV = 1850000,
			.apply_uV = true,
			.valid_modes_mask = REGULATOR_MODE_NORMAL | REGULATOR_MODE_STANDBY,
			.valid_ops_mask = REGULATOR_CHANGE_MODE | REGULATOR_CHANGE_STATUS,
			},
	.num_consumer_supplies = 1,
	.consumer_supplies = &board_vmmc2_supply,
};

/* VAUX1 for PL_SENSOR */
static struct regulator_init_data board_aux1 = {
	.constraints = {
			.min_uV = 3000000,
			.max_uV = 3000000,
			.valid_modes_mask = REGULATOR_MODE_NORMAL | REGULATOR_MODE_STANDBY,
			.valid_ops_mask = REGULATOR_CHANGE_MODE | REGULATOR_CHANGE_STATUS,
			},
	.num_consumer_supplies = 1,
	.consumer_supplies = &board_vaux1_supply,
};

/* VAUX2 for PL_SENSOR */
static struct regulator_init_data board_aux2 = {
	.constraints = {
			.min_uV = 2800000,
			.max_uV = 2800000,
			.apply_uV = true,
			.valid_modes_mask = REGULATOR_MODE_NORMAL | REGULATOR_MODE_STANDBY,
			.valid_ops_mask = REGULATOR_CHANGE_MODE | REGULATOR_CHANGE_STATUS,
			},
	.num_consumer_supplies = 1,
	.consumer_supplies = &board_vaux2_supply,
};

/* VSIM for OMAP VDD_MMC1A (i/o for DAT4..DAT7) */
static struct regulator_init_data board_vsim = {
	.constraints = {
			.min_uV = 1800000,
			.max_uV = 3000000,
			.always_on = true,
			.boot_on = true,
			.valid_modes_mask = REGULATOR_MODE_NORMAL | REGULATOR_MODE_STANDBY,
			.valid_ops_mask = REGULATOR_CHANGE_VOLTAGE | REGULATOR_CHANGE_MODE | REGULATOR_CHANGE_STATUS,
			},
	.num_consumer_supplies = 1,
	.consumer_supplies = &board_vsim_supply,
};

/* VAUX3 for LCD */
static struct regulator_init_data board_aux3 = {
	.constraints = {
			.min_uV = 1800000,
			.max_uV = 1800000,
			.boot_on = true,
			.valid_modes_mask = REGULATOR_MODE_NORMAL | REGULATOR_MODE_STANDBY,
			.valid_ops_mask = REGULATOR_CHANGE_MODE | REGULATOR_CHANGE_STATUS,
			},
	.num_consumer_supplies = 1,
	.consumer_supplies = &board_vaux3_supply,
};

/* VAUX4 for LCD */
static struct regulator_init_data board_aux4 = {
	.constraints = {
			.min_uV = 2800000,
			.max_uV = 2800000,
			.boot_on = true,
			.valid_modes_mask = REGULATOR_MODE_NORMAL | REGULATOR_MODE_STANDBY,
			.valid_ops_mask = REGULATOR_CHANGE_MODE | REGULATOR_CHANGE_STATUS,
			},
	.num_consumer_supplies = 1,
	.consumer_supplies = &board_vaux4_supply,
};

/* VPLL2 for LCD */
static struct regulator_init_data board_vpll2 = {
	.constraints = {
			.min_uV = 1800000,
			.max_uV = 1800000,
			.boot_on = true,
			.valid_modes_mask = REGULATOR_MODE_NORMAL | REGULATOR_MODE_STANDBY,
			.valid_ops_mask = REGULATOR_CHANGE_MODE | REGULATOR_CHANGE_STATUS,
			},
	.num_consumer_supplies = 1,
	.consumer_supplies = &board_vpll2_supply,
};

//ZEUS_LCD
static struct omap_lcd_config board_lcd_config __initdata = {
	.ctrl_name = "internal",
};

//ZEUS_LCD
static struct omap_uart_config board_uart_config __initdata = {
#ifdef CONFIG_SERIAL_OMAP_CONSOLE
	.enabled_uarts = ((1 << 0) | (1 << 1) | (1 << 2)),
#else
	.enabled_uarts = ((1 << 0) | (1 << 1)),
#endif
};

static struct omap_board_config_kernel board_config[] __initdata = {
	{OMAP_TAG_UART, &board_uart_config},
	{OMAP_TAG_LCD, &board_lcd_config},	//ZEUS_LCD
};

#if 1				//def _USE_TWL_BCI_MODULE_
static int board_batt_table[] = {
/* 0 C*/
	30800, 29500, 28300, 27100,
	26000, 24900, 23900, 22900, 22000, 21100, 20300, 19400, 18700, 17900,
	17200, 16500, 15900, 15300, 14700, 14100, 13600, 13100, 12600, 12100,
	11600, 11200, 10800, 10400, 10000, 9630, 9280, 8950, 8620, 8310,
	8020, 7730, 7460, 7200, 6950, 6710, 6470, 6250, 6040, 5830,
	5640, 5450, 5260, 5090, 4920, 4760, 4600, 4450, 4310, 4170,
	4040, 3910, 3790, 3670, 3550
};

static struct twl4030_bci_platform_data board_bci_data = {
	.battery_tmp_tbl = board_batt_table,
	.tblsize = ARRAY_SIZE(board_batt_table),
};
#endif

static struct twl4030_hsmmc_info mmc[] __initdata = {
	{
	 .name = "external",
	 .mmc = 1,
	 .wires = 4,
	 .gpio_wp = -EINVAL,
	 .power_saving = true,
	 },
	{
	 .name = "internal",
	 .mmc = 2,
	 .wires = 8,
	 .gpio_cd = -EINVAL,
	 .gpio_wp = -EINVAL,
	 .nonremovable = true,
	 .power_saving = true,
	 },
	{
	 .mmc = 3,
	 .wires = 4,
	 .gpio_cd = -EINVAL,
	 .gpio_wp = -EINVAL,
	 },
	{}			/* Terminator */
};

static int board_twl_gpio_setup(struct device *dev, unsigned gpio,
				 unsigned ngpio)
{
	/* gpio + 0 is "mmc0_cd" (input/IRQ) */
	//mmc[0].gpio_cd = gpio + 0;
	mmc[0].gpio_cd = 23;
	twl4030_mmc_init(mmc);

	/* link regulators to MMC adapters ... we "know" the
	 * regulators will be set up only *after* we return.
	 */
	board_vmmc1_supply.dev = mmc[0].dev;
	board_vsim_supply.dev = mmc[0].dev;
	board_vmmc2_supply.dev = mmc[1].dev;

	return 0;
}

static struct twl4030_gpio_platform_data board_gpio_data = {
	.gpio_base = OMAP_MAX_GPIO_LINES,
	.irq_base = TWL4030_GPIO_IRQ_BASE,
	.irq_end = TWL4030_GPIO_IRQ_END,
	.setup = board_twl_gpio_setup,
};

static struct twl4030_usb_data board_usb_data = {
	.usb_mode = T2_USB_MODE_ULPI,
};

static struct twl4030_madc_platform_data board_madc_data = {
	.irq_line = 1,
};

static struct twl4030_ins __initdata sleep_on_seq[] = {
#ifdef TWL4030_USING_BROADCAST_MSG
	{MSG_BROADCAST
	 (DEV_GRP_NULL, RES_GRP_RC, RES_TYPE_ALL, 0x0, RES_STATE_OFF), 2},
	{MSG_BROADCAST
	 (DEV_GRP_NULL, RES_GRP_ALL, RES_TYPE_ALL, 0x0, RES_STATE_SLEEP), 2},

#else
	/* Turn off HFCLKOUT */
	{MSG_SINGULAR(DEV_GRP_P1, RES_HFCLKOUT, RES_STATE_OFF), 2},
	/* Turn OFF VDD1 */
	{MSG_SINGULAR(DEV_GRP_P1, RES_VDD1, RES_STATE_OFF), 2},
	/* Turn OFF VDD2 */
	{MSG_SINGULAR(DEV_GRP_P1, RES_VDD2, RES_STATE_OFF), 2},
	/* Turn OFF VPLL1 */
	{MSG_SINGULAR(DEV_GRP_P1, RES_VPLL1, RES_STATE_OFF), 2},
#endif
};

static struct twl4030_script sleep_on_script __initdata = {
	.script = sleep_on_seq,
	.size = ARRAY_SIZE(sleep_on_seq),
	.flags = TRITON_SLEEP_SCRIPT,
};

static struct twl4030_ins wakeup_p12_seq[] __initdata = {
#ifdef TWL4030_USING_BROADCAST_MSG
	{MSG_BROADCAST(DEV_GRP_NULL, RES_GRP_RES, RES_TYPE_ALL, 0x2, RES_STATE_ACTIVE), 2},
	{MSG_BROADCAST(DEV_GRP_NULL, RES_GRP_ALL, RES_TYPE_ALL, 0x0, RES_STATE_ACTIVE), 2},
#else
	/* Turn on HFCLKOUT */
	{MSG_SINGULAR(DEV_GRP_P1, RES_HFCLKOUT, RES_STATE_ACTIVE), 2},
	/* Turn ON VDD1 */
	{MSG_SINGULAR(DEV_GRP_P1, RES_VDD1, RES_STATE_ACTIVE), 2},
	/* Turn ON VDD2 */
	{MSG_SINGULAR(DEV_GRP_P1, RES_VDD2, RES_STATE_ACTIVE), 2},
	/* Turn ON VPLL1 */
	{MSG_SINGULAR(DEV_GRP_P1, RES_VPLL1, RES_STATE_ACTIVE), 2},
#endif
};

static struct twl4030_script wakeup_p12_script __initdata = {
	.script = wakeup_p12_seq,
	.size = ARRAY_SIZE(wakeup_p12_seq),
	.flags = TWL4030_WAKEUP12_SCRIPT,
};

static struct twl4030_ins wakeup_p3_seq[] __initdata = {
#ifdef TWL4030_USING_BROADCAST_MSG
	{MSG_BROADCAST(DEV_GRP_NULL, RES_GRP_RC, RES_TYPE_ALL, 0x0, RES_STATE_ACTIVE), 2},
#else
	{MSG_SINGULAR(DEV_GRP_P1, RES_HFCLKOUT, RES_STATE_ACTIVE), 2},
#endif
};

static struct twl4030_script wakeup_p3_script __initdata = {
	.script = wakeup_p3_seq,
	.size = ARRAY_SIZE(wakeup_p3_seq),
	.flags = TWL4030_WAKEUP3_SCRIPT,
};

static struct twl4030_ins wrst_seq[] __initdata = {
/*
 * Reset twl4030.
 * Reset VDD1 regulator.
 * Reset VDD2 regulator.
 * Reset VPLL1 regulator.
 * Enable sysclk output.
 * Reenable twl4030.
 */
#ifdef NOT_SUPPORT_HW_RESET_DURING_SLEEP
	{MSG_SINGULAR(DEV_GRP_NULL, RES_RESET, RES_STATE_OFF), 2},
	{MSG_SINGULAR(DEV_GRP_P1, RES_VDD1, RES_STATE_WRST), 15},
	{MSG_SINGULAR(DEV_GRP_P1, RES_VDD2, RES_STATE_WRST), 15},
	{MSG_SINGULAR(DEV_GRP_P1, RES_VPLL1, RES_STATE_WRST), 0x60},
	{MSG_SINGULAR(DEV_GRP_P1, RES_HFCLKOUT, RES_STATE_ACTIVE), 2},
	{MSG_SINGULAR(DEV_GRP_NULL, RES_RESET, RES_STATE_ACTIVE), 2},
#else
	{MSG_SINGULAR(DEV_GRP_NULL, RES_RESET, RES_STATE_OFF), 2},
	{MSG_SINGULAR(DEV_GRP_P1, RES_Main_Ref, RES_STATE_WRST), 2},
	{MSG_SINGULAR(DEV_GRP_P1, RES_VIO, RES_STATE_ACTIVE), 15},	// VIO active
	{MSG_SINGULAR(DEV_GRP_P3, RES_CLKEN, RES_STATE_ACTIVE), 2},	// CLKEN active
	{MSG_SINGULAR(DEV_GRP_P1, RES_VPLL1, RES_STATE_ACTIVE), 2},	// VPLL1 active
	{MSG_SINGULAR(DEV_GRP_P1, RES_VDD2, RES_STATE_ACTIVE), 15},	// VDD2 active
	{MSG_SINGULAR(DEV_GRP_P1, RES_VDD1, RES_STATE_ACTIVE), 25},	// VDD1 active
// {MSG_BROADCAST(DEV_GRP_NULL, RES_GRP_ALL, RES_TYPE_ALL, RES_TYPE_ALL, RES_STATE_WRST), 75},            // All resources reset
	{MSG_BROADCAST(DEV_GRP_P1, RES_GRP_RC, RES_TYPE_ALL, RES_TYPE_ALL, RES_STATE_ACTIVE), 2},	// All RC resources active
	{MSG_SINGULAR(DEV_GRP_NULL, RES_RESET, RES_STATE_ACTIVE), 2},
#endif
};

static struct twl4030_script wrst_script __initdata = {
	.script = wrst_seq,
	.size = ARRAY_SIZE(wrst_seq),
	.flags = TWL4030_WRST_SCRIPT,
};

static struct twl4030_script *twl4030_scripts[] __initdata = {
	&sleep_on_script,
	&wakeup_p12_script,
	&wakeup_p3_script,
	&wrst_script,
};

static struct twl4030_resconfig twl4030_rconfig[] = {
	{.resource = RES_HFCLKOUT,.devgroup = DEV_GRP_P3,.type = -1,.type2 = -1},
#ifdef TWL4030_USING_BROADCAST_MSG
	{.resource = RES_CLKEN,.devgroup = DEV_GRP_P3,.type = -1,.type2 = -1},
	{.resource = RES_VAUX1,.devgroup = DEV_GRP_NULL,.type = -1,.type2 = -1},
	{.resource = RES_VAUX2,.devgroup = DEV_GRP_NULL,.type = -1,.type2 = -1},
#ifndef CONFIG_FB_OMAP_BOOTLOADER_INIT
	{.resource = RES_VAUX3,.devgroup = DEV_GRP_NULL,.type = -1,.type2 = -1},
	{.resource = RES_VAUX4,.devgroup = DEV_GRP_NULL,.type = -1,.type2 = -1},
#endif
	{.resource = RES_VMMC1,.devgroup = DEV_GRP_NULL,.type = -1,.type2 = -1},
	{.resource = RES_VMMC2,.devgroup = DEV_GRP_NULL,.type = -1,.type2 = -1},
	{.resource = RES_VPLL2,.devgroup = DEV_GRP_P1,.type = -1,.type2 = -1},
	{.resource = RES_VSIM,.devgroup = DEV_GRP_P1,.type = -1,.type2 = -1},
	{.resource = RES_VDAC,.devgroup = DEV_GRP_NULL,.type = -1,.type2 = -1},
	{.resource = RES_VUSB_1V5,.devgroup = DEV_GRP_NULL,.type = -1,.type2 = -1},
	{.resource = RES_VUSB_1V8,.devgroup = DEV_GRP_NULL,.type = -1,.type2 = -1},
	{.resource = RES_VUSB_3V1,.devgroup = DEV_GRP_NULL,.type = -1,.type2 = -1},
	{.resource = RES_VINTANA1,.devgroup = DEV_GRP_P1,.type = -1,.type2 = -1},
	{.resource = RES_VINTANA2,.devgroup = DEV_GRP_NULL,.type = -1,.type2 = -1},
	{.resource = RES_VINTDIG,.devgroup = DEV_GRP_P1,.type = -1,.type2 = -1},
#endif
	{.resource = RES_VDD1,.devgroup = DEV_GRP_P1,.type = -1,.type2 = -1},
	{.resource = RES_VDD2,.devgroup = DEV_GRP_P1,.type = -1,.type2 = -1},
	{0, 0},
};

static struct twl4030_power_data board_t2scripts_data __initdata = {
	.scripts = twl4030_scripts,
	.num = ARRAY_SIZE(twl4030_scripts),
	.resource_config = twl4030_rconfig,
};

static struct twl4030_codec_audio_data board_audio_data = {
	.audio_mclk = 26000000,
};

static struct twl4030_codec_data board_codec_data = {
	.audio_mclk = 26000000,
	.audio = &board_audio_data,
};

static struct twl4030_platform_data board_twldata = {
	.irq_base = TWL4030_IRQ_BASE,
	.irq_end = TWL4030_IRQ_END,

	/* platform_data for children goes here */
	.bci = &board_bci_data,
	.madc = &board_madc_data,
	.usb = &board_usb_data,
	.gpio = &board_gpio_data,
	.keypad = &board_kp_data,
	.codec = &board_codec_data,
	//.power = &board_t2scripts_data,
	.vmmc1 = &board_vmmc1,
	.vmmc2 = &board_vmmc2,
	.vsim = &board_vsim,
	.vdac = &board_vdac,
	.vaux1 = &board_aux1,
	.vaux2 = &board_aux2,
	.vaux3 = &board_aux3,
	.vaux4 = &board_aux4,
	.vpll2 = &board_vpll2,
};

#if  0				//(CONFIG_SAMSUNG_HW_REV < ARCHER_REV10)
static void synaptics_dev_init(void)
{
	if (gpio_request(OMAP_GPIO_TOUCH_IRQ, "touch") < 0) {
		printk(KERN_ERR "can't get synaptics pen down GPIO\n");
		return;
	}
	gpio_direction_input(OMAP_GPIO_TOUCH_IRQ);
	omap_set_gpio_debounce(OMAP_GPIO_TOUCH_IRQ, 0);	//  added this to remove the debouncing feature
}

static int synaptics_power(int power_state)
{
	int ret = 0;
	if (power_state) {
		if (ret != twl4030_i2c_write_u8(TWL4030_MODULE_PM_RECEIVER,
						0x09, TWL4030_VAUX2_DEDICATED))
			return -EIO;
		if (ret != twl4030_i2c_write_u8(TWL4030_MODULE_PM_RECEIVER,
						0x20, TWL4030_VAUX2_DEV_GRP))
			return -EIO;
	} else {
		if (ret != twl4030_i2c_write_u8(TWL4030_MODULE_PM_RECEIVER,
		0x00,
					 TWL4030_VAUX2_DEV_GRP))
			return -EIO;
	}

// WL127X Power on by eugene : It affect lower throughput
	if (ret != twl4030_i2c_write_u8(TWL4030_MODULE_PM_RECEIVER, 0xE0,
					TWL4030_VSIM_DEV_GRP)) {
		return -EIO;
	}
	printk(KERN_ERR "twl4030_i2c_write_u8 called\n");

	return 0;
}

static struct synaptics_i2c_rmi_platform_data synaptics_platform_data[] = {
	{
	 .version = 0x0,
	 .power = &synaptics_power,
	 .flags = SYNAPTICS_SWAP_XY,
	 }
};
#endif
#ifdef CONFIG_MICROUSBIC_INTR
static void microusbic_dev_init(void)
{
	if (gpio_request(OMAP_GPIO_JACK_NINT,"micro USB IC irq") < 0) {
		printk(KERN_ERR " GFree can't get microusb pen down GPIO\n");
		return;
	}
	omap_set_gpio_direction(OMAP_GPIO_JACK_NINT, 1);
}
#endif
struct i2c_board_info __initdata board_i2c_boardinfo[] = {
	{
		I2C_BOARD_INFO("twl5030", 0x48),
		.flags = I2C_CLIENT_WAKE,
		.irq = INT_34XX_SYS_NIRQ,
		.platform_data = &board_twldata,
	},
};

static struct i2c_board_info __initdata board_i2c_boardinfo1[] = {
#if defined(CONFIG_FSA9480_MICROUSB)
	{
		I2C_BOARD_INFO("fsa9480", 0x25),
		.flags = I2C_CLIENT_WAKE,
		.irq = OMAP_GPIO_IRQ(OMAP_GPIO_JACK_NINT),
	},
#elif defined(CONFIG_MICROUSBIC_INTR)
	{
		I2C_BOARD_INFO("microusbic", 0x25),
	},
#endif

#if defined(CONFIG_SND_SOC_MAX9877)
	{
		I2C_BOARD_INFO("max9877", 0x4d),
	},
#elif defined(CONFIG_SND_SOC_MAX97000)
	{
		I2C_BOARD_INFO("max97000", 0x4d),
	},
#endif
	{
		I2C_BOARD_INFO(CE147_DRIVER_NAME, CE147_I2C_ADDR),
		.platform_data = &zeus_ce147_platform_data,
	},
	{
		I2C_BOARD_INFO(S5KA3DFX_DRIVER_NAME, S5KA3DFX_I2C_ADDR),
		.platform_data = &zeus_s5ka3dfx_platform_data,
	},
	{
		I2C_BOARD_INFO("cam_pmic", CAM_PMIC_I2C_ADDR),
	},
	{
		I2C_BOARD_INFO("secFuelgaugeDev", 0x36),
		.flags = I2C_CLIENT_WAKE,
		.irq = OMAP_GPIO_IRQ(OMAP_GPIO_FUEL_INT_N),
	},
#if !defined(CONFIG_INPUT_GP2A_USE_GPIO_I2C)
	{
		I2C_BOARD_INFO("gp2a", 0x44),
	},
#endif
#if !defined(CONFIG_INPUT_YAS529_USE_GPIO_I2C)
	{
		I2C_BOARD_INFO("yas529", 0x2E),
	},
#endif
	{
		I2C_BOARD_INFO("Si4709_driver", 0x10),			
	},
};

#if 0
//#if defined(CONFIG_VIDEO_LV8093) && defined(CONFIG_VIDEO_OMAP3)
	{
	 I2C_BOARD_INFO(LV8093_NAME, LV8093_AF_I2C_ADDR),
	 .platform_data = &zoom2_lv8093_platform_data,
	 },
#endif

static struct i2c_board_info __initdata board_i2c_boardinfo_4[] = {	//Added for i2c3 register-CY8

	{
	 I2C_BOARD_INFO("melfas_ts", 0x40),	// 10010(A1)(A0)  A1=PD0, A0=M(0=12bit, 1=8bit)
	 .type = "melfas_ts",
	 //       .platform_data = &tsc2007_info,
	 },

};

static int __init omap_i2c_init(void)
{
/* Disable OMAP 3630 internal pull-ups for I2Ci */
	if (cpu_is_omap3630()) {

		u32 prog_io;

		prog_io = omap_ctrl_readl(OMAP343X_CONTROL_PROG_IO1);
		/* Program (bit 19)=1 to disable internal pull-up on I2C1 */
		prog_io |= OMAP3630_PRG_I2C1_PULLUPRESX;
		/* Program (bit 0)=1 to disable internal pull-up on I2C2 */
		prog_io |= OMAP3630_PRG_I2C2_PULLUPRESX;
		omap_ctrl_writel(prog_io, OMAP343X_CONTROL_PROG_IO1);

		prog_io = omap_ctrl_readl(OMAP36XX_CONTROL_PROG_IO2);
		/* Program (bit 7)=1 to disable internal pull-up on I2C3 */
		prog_io |= OMAP3630_PRG_I2C3_PULLUPRESX;
		omap_ctrl_writel(prog_io, OMAP36XX_CONTROL_PROG_IO2);

		prog_io = omap_ctrl_readl(OMAP36XX_CONTROL_PROG_IO_WKUP1);
		/* Program (bit 5)=1 to disable internal pull-up on I2C4(SR) */
		prog_io |= OMAP3630_PRG_SR_PULLUPRESX;
		omap_ctrl_writel(prog_io, OMAP36XX_CONTROL_PROG_IO_WKUP1);
	}
/*
	omap_register_i2c_bus(1, 2400, zoom_i2c_boardinfo,
			ARRAY_SIZE(zoom_i2c_boardinfo));
	omap_register_i2c_bus(2, 400, NULL, 0);
	omap_register_i2c_bus(3, 400, NULL, 0);
*/
#if 0
	omap_register_i2c_bus(1, 100, board_i2c_boardinfo,
			      ARRAY_SIZE(board_i2c_boardinfo));
	omap_register_i2c_bus(2, 100, board_i2c_boardinfo1,
			      ARRAY_SIZE(board_i2c_boardinfo1));
	omap_register_i2c_bus(3, 400, board_i2c_boardinfo_4,
			      ARRAY_SIZE(board_i2c_boardinfo_4));
#else
	/* CSR ID:- OMAPS00222372 Changed the order of I2C Bus Registration 
	 *  Previously I2C1 channel 1 was being registered followed by I2C2 but since
	 *  TWL4030-USB module had a dependency on FSA9480 USB Switch device which is
	 *  connected to I2C2 channel, changed the order such that I2C channel 2 will get
	 *  registered first and then followed by I2C1 channel. */
	omap_register_i2c_bus(2, 200, NULL, board_i2c_boardinfo1,
			      ARRAY_SIZE(board_i2c_boardinfo1));
	omap_register_i2c_bus(1, 400, NULL, board_i2c_boardinfo,
			      ARRAY_SIZE(board_i2c_boardinfo));
	omap_register_i2c_bus(3, 100, NULL, board_i2c_boardinfo_4,
			      ARRAY_SIZE(board_i2c_boardinfo_4));
#endif
	return 0;
}

static void config_wlan_gpio(void)
{
	int ret = 0;

#if 1				//TI HS.Yoon 20100827 for enabling WLAN_IRQ wakeup
	omap_writel(omap_readl(0x480025E8) | 0x410C0000, 0x480025E8);
	omap_writew(0x10C, 0x48002194);
#endif
        ret = gpio_request(OMAP_GPIO_WLAN_HOST_WAKE, "wifi_irq");
	if (ret < 0) {
		printk(KERN_ERR "%s: can't reserve GPIO: %d\n", __func__,
                        OMAP_GPIO_WLAN_HOST_WAKE);
		return;
	}
	ret = gpio_request(OMAP_GPIO_WLAN_EN, "wifi_pmena");
	if (ret < 0) {
		printk(KERN_ERR "%s: can't reserve GPIO: %d\n", __func__,
		       OMAP_GPIO_WLAN_EN);
		gpio_free(OMAP_GPIO_WLAN_EN);
		return;
	}
        gpio_direction_input(OMAP_GPIO_WLAN_HOST_WAKE);
	gpio_direction_output(OMAP_GPIO_WLAN_EN, 0);

#ifdef __TRISCOPE__
	gpio_set_value(OMAP_GPIO_WLAN_EN, 1);
#endif
}

static void config_camera_gpio(void)
{
}
//#endif
#if 0
static int __init wl127x_vio_leakage_fix(void)
{
	int ret = 0;

	ret = gpio_request(OMAP_GPIO_BT_NSHUTDOWN, "wl127x_bten");
	if (ret < 0) {
		printk(KERN_ERR "wl127x_bten gpio_%d request fail",
		       OMAP_GPIO_BT_NSHUTDOWN);
		goto fail;
	}

	gpio_direction_output(OMAP_GPIO_BT_NSHUTDOWN, 1);
	mdelay(10);
	gpio_direction_output(OMAP_GPIO_BT_NSHUTDOWN, 0);
	udelay(64);

	gpio_free(OMAP_GPIO_BT_NSHUTDOWN);

fail:
	return ret;
}
#endif
static void mod_clock_correction(void)
{
	cm_write_mod_reg(0x00, OMAP3430ES2_SGX_MOD, CM_CLKSEL);
	cm_write_mod_reg(0x04, OMAP3430_CAM_MOD, CM_CLKSEL);
}

int config_twl4030_resource_remap(void)
{
	int ret = 0;

	printk("Start config_twl4030_resource_remap\n");

	ret = twl_i2c_write_u8(TWL4030_MODULE_PM_RECEIVER, REMAP_OFF,
			       TWL4030_VDD1_REMAP);
	if (ret)
		printk(" board-file: fail to set reousrce remap "
		       "TWL4030_VDD1_REMAP\n");

	ret = twl_i2c_write_u8(TWL4030_MODULE_PM_RECEIVER, REMAP_OFF,
			       TWL4030_VDD2_REMAP);
	if (ret)
		printk(" board-file: fail to set reousrce remap "
		       "TWL4030_VDD2_REMAP\n");

	ret = twl_i2c_write_u8(TWL4030_MODULE_PM_RECEIVER, REMAP_OFF,
			       TWL4030_VPLL1_REMAP);
	if (ret)
		printk(" board-file: fail to set reousrce remap "
		       "TWL4030_VPLL1_REMAP\n");

	ret = twl_i2c_write_u8(TWL4030_MODULE_PM_RECEIVER, REMAP_OFF,
			       TWL4030_VPLL2_REMAP);
	if (ret)
		printk(" board-file: fail to set reousrce remap "
		       "TWL4030_VPLL2_REMAP\n");

	ret = twl_i2c_write_u8(TWL4030_MODULE_PM_RECEIVER, REMAP_OFF,
			       TWL4030_HFCLKOUT_REMAP);
	if (ret)
		printk(" board-file: fail to set reousrce remap "
		       "TWL4030_HFCLKOUT_REMAP\n");

	ret = twl_i2c_write_u8(TWL4030_MODULE_PM_RECEIVER, REMAP_OFF,
			       TWL4030_VINTANA2_REMAP);
	if (ret)
		printk(" board-file: fail to set reousrce remap "
		       "TWL4030_VINTANA2_REMAP\n");

	ret = twl_i2c_write_u8(TWL4030_MODULE_PM_RECEIVER, REMAP_SLEEP,
			       TWL4030_VINTDIG_REMAP);
	if (ret)
		printk(" board-file: fail to set reousrce remap "
		       "TWL4030_VINTDIG_REMAP\n");

	ret = twl_i2c_write_u8(TWL4030_MODULE_PM_RECEIVER, REMAP_ACTIVE,
			       TWL4030_VSIM_REMAP);
	if (ret)
		printk(" board-file: fail to set reousrce remap "
		       "TWL4030_VSIM_REMAP\n");

	// [ - In order to prevent damage to the PMIC(TWL4030 or 5030), 
	//     VINTANA1 should maintain active state even though the system is in offmode.
	ret = twl_i2c_write_u8(TWL4030_MODULE_PM_RECEIVER, REMAP_ACTIVE,
			       TWL4030_VINTANA1_REMAP);
	if (ret)
		printk(" board-file: fail to set reousrce remap "
		       "TWL4030_VINTANA1_REMAP\n");
	// ]

	return ret;
}

static struct omap_musb_board_data musb_board_data = {
	.interface_type = MUSB_INTERFACE_ULPI,
	.mode = MUSB_OTG,
	.power = 100,
};

#define POWERUP_REASON "androidboot.mode"
int get_powerup_reason(char *str)
{
	extern char *saved_command_line;
	char *next, *start = NULL;
	int i;

	i = strlen(POWERUP_REASON);
	next = saved_command_line;

	while ((next = strchr(next, 'a')) != NULL) {
		if (!strncmp(next, POWERUP_REASON, i)) {
			start = next;
			break;
		} else {
			next++;
		}
	}
	if (!start)
		return -EPERM;
	i = 0;
	start = strchr(start, '=') + 1;
	while (*start != ' ') {
		str[i++] = *start++;
		if (i > 14) {
			printk(KERN_INFO "Invalid Powerup reason\n");
			return -EPERM;
		}
	}
	str[i] = '\0';
	return 0;
}

#ifdef CONFIG_SAMSUNG_KERNEL_DEBUG	// klaatu

typedef struct {
	char Magic[4];
	char BuildRev[12];
	char BuildDate[12];
	char BuildTime[9];
	void *Excp_reserve1;
	void *Excp_reserve2;
	void *Excp_reserve3;
	void *Excp_reserve4;
} gExcpDebugInfo_t;

void debug_info_init(void)
{
	gExcpDebugInfo_t *debug_info;

	debug_info = phys_to_virt(0x95000000) - sizeof(gExcpDebugInfo_t);

	memcpy(debug_info->Magic, "DBG", 4);
	memcpy(debug_info->BuildRev, CONFIG_REV_STR, 12);
	memcpy(debug_info->BuildDate, __DATE__, 12);
	memcpy(debug_info->BuildTime, __TIME__, 9);
}
#endif

static void __init get_board_hw_rev(void)
{
	int ret;

	ret = gpio_request(OMAP_GPIO_HW_REV0, "HW_REV0");
	if (ret < 0) {
		printk("fail to get gpio : %d, res : %d\n", OMAP_GPIO_HW_REV0,
		       ret);
		return;
	}

	ret = gpio_request(OMAP_GPIO_HW_REV1, "HW_REV1");
	if (ret < 0) {
		printk("fail to get gpio : %d, res : %d\n", OMAP_GPIO_HW_REV1,
		       ret);
		return;
	}

	gpio_direction_input(OMAP_GPIO_HW_REV0);
	gpio_direction_input(OMAP_GPIO_HW_REV1);

	hw_revision = gpio_get_value(OMAP_GPIO_HW_REV0);
	hw_revision |= (gpio_get_value(OMAP_GPIO_HW_REV1) << 1);

	gpio_free(OMAP_GPIO_HW_REV0);
	gpio_free(OMAP_GPIO_HW_REV1);

	// safe mode reconfiguration
//	omap_ctrl_writew(OMAP34XX_MUX_MODE7, 0x018C);
//	omap_ctrl_writew(OMAP34XX_MUX_MODE7, 0x0190);

	printk("BOARD HW REV : %d\n", hw_revision);
}

#define OMAP_GPIO_TSP_INT 142

static void Atmel_dev_init(void)
{
	/* Set the ts_gpio pin mux */
	if (gpio_request(OMAP_GPIO_TSP_INT, "touch_atmel") < 0) {
		printk(KERN_ERR "can't get synaptics pen down GPIO\n");
		return;
	}
	gpio_direction_input(OMAP_GPIO_TSP_INT);
}

void __init board_peripherals_init(void)
{

	/*omap_i2c_init(); *//*Moving omap_i2c_init to be called directly from archer init */
	spi_register_board_info(board_spi_board_info,
				ARRAY_SIZE(board_spi_board_info));
//      platform_device_register(&board_dss_device);
	Atmel_dev_init();
	omap_serial_init();
	config_wlan_gpio();

	config_camera_gpio();
	mod_clock_correction();
	usb_musb_init(&musb_board_data);
	//usb_ehci_init();  //Proper arguments should be passed

#if 0
	sec_class = class_create(THIS_MODULE, "sec");
	if (IS_ERR(sec_class)) {
		pr_err("Failed to create class(sec)!\n");
	}
//      board_param_sysfs_init();
#endif
}

static void __init omap_board_init(void)
{
	char str_powerup_reason[15];
	u32 regval;

	printk("\n.....[Archer] Initializing...\n");

	// change reset duration (PRM_RSTTIME register)
	regval = omap_readl(0x48307254);
	regval |= 0xFF;
	omap_writew(regval, 0x48307254);

	omap3_mux_init(board_mux_ptr, OMAP_PACKAGE_CBP);
	printk("Mux Setting Done\n");

	if (omap_gpio_out_init()) {
		printk("zeus gpio ouput set fail!!!!\n");
	}
	printk("Gpio Output Setting Done");

	set_wakeup_gpio();
	get_powerup_reason(str_powerup_reason);
	printk("\n\n <Powerup Reason : %s>\n\n", str_powerup_reason);
	omap_i2c_init();	/*Moved here from peripheral init */
	platform_add_devices(board_devices, ARRAY_SIZE(board_devices));
// For Regulator Framework [
	board_vaux1_supply.dev = &samsung_pl_sensor_power_device.dev;
	board_vaux2_supply.dev = &samsung_pl_sensor_power_device.dev;
	board_vaux3_supply.dev = &board_lcd_device.dev;
	board_vaux4_supply.dev = &board_lcd_device.dev;
	board_vpll2_supply.dev = &board_lcd_device.dev;
//      zeus_vmmc2_supply.dev = &zeus_lcd_device.dev; 
// ]
	board_usb_data.batt_dev = &samsung_battery_device.dev;
	board_usb_data.charger_dev = &samsung_charger_device.dev;
	board_usb_data.switch_dev = &headset_switch_device.dev;	//Added for Regulator

	omap_board_config = board_config;
	omap_board_config_size = ARRAY_SIZE(board_config);
	/*RTC support */
	msecure_init();

#if  0				//(CONFIG_SAMSUNG_HW_REV < ARCHER_REV10)
	synaptics_dev_init();
#endif
#ifdef CONFIG_MICROUSBIC_INTR
	microusbic_dev_init();
#endif
#ifdef CONFIG_SAMSUNG_KERNEL_DEBUG	// klaatu
	debug_info_init();
#endif

    sec_class = class_create(THIS_MODULE, "sec");
    if (IS_ERR(sec_class))
    {
        pr_err("Failed to create class(sec)!\n");
    }

	sio_switch_dev = device_create(sec_class, NULL, MKDEV(0, 0), NULL, "switch");


	if (IS_ERR(sio_switch_dev))
	{
		pr_err("Failed to create device(sio_switch)!\n");
	}

	get_board_hw_rev();

	board_peripherals_init();
}

static void __init bootloader_reserve_sdram(void)
{
	u32 paddr;
	u32 size = 0x80000;

	paddr = 0x95000000;

	paddr -= size;

	if (reserve_bootmem(paddr, size, BOOTMEM_EXCLUSIVE) < 0) {
		pr_err("FB: failed to reserve VRAM\n");
	}
}

static void __init omap_board_map_io(void)
{
	omap2_set_globals_343x();
	omap2_map_common_io();
	bootloader_reserve_sdram();
}

static void __init omap_board_fixup(struct machine_desc *desc,
				     struct tag *tags, char **cmdline,
				     struct meminfo *mi)
{
#if CONFIG_SAMSUNG_REL_HW_REV >= 1
	mi->bank[0].start = 0x80000000;
	mi->bank[0].size = 256 * SZ_1M;	/* DDR_CS0 256MB */
	mi->bank[0].node = 0;

	mi->bank[1].start = 0x90000000;
	mi->bank[1].size = 256 * SZ_1M;	/* DDR_CS1 256MB */
	mi->bank[1].node = 0;
#else
	mi->bank[0].start = 0x80000000;
	mi->bank[0].size = 512 * SZ_1M;	// DDR 512MB
	mi->bank[0].node = 0;

	mi->bank[1].start = 0xA0000000;
	mi->bank[1].size = 80 * SZ_1M;	// OneDRAM 80MB
	mi->bank[1].node = 0;
#endif /* CONFIG_SAMSUNG_REL_HW_REV */

	mi->nr_banks = 2;
}

MACHINE_START(LATONA, "Latona Samsung Board")
    .phys_io = 0x48000000,
    .io_pg_offst = ((0xfa000000) >> 18) & 0xfffc,
    .boot_params = 0x80000100,
    .fixup = omap_board_fixup,
    .map_io = omap_board_map_io,
    .init_irq = omap_board_init_irq,
    .init_machine = omap_board_init,
    .timer = &omap_timer,
MACHINE_END
