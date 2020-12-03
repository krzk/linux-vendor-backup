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
#include <linux/reboot.h>

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
#include <mach/board-heron.h>

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
#include <mach/board-heron.h>
#include <plat/omap-pm.h>
#include <plat/mux.h>
#include <linux/interrupt.h>
#include <plat/control.h>
#include <plat/clock.h>
#include <asm/setup.h>
#include <linux/leds.h>
#include <plat/prcm.h>
#include <mach/sec_param.h>
#include <mach/sec_log_buf.h>

#include "cm.h"
#include "mux.h"
#include "sdram-micron-mt46h32m32lf-6.h"
#include "sdram-qimonda-hyb18m512160af-6.h"
#include <linux/ctype.h>
#include "omap3-opp.h"
#include "pm.h"
#include "mmc-twl4030.h"

#define POWERUP_REASON "androidboot.mode"

extern int set_wakeup_gpio(void);
extern int omap_gpio_out_init(void);
u32 hw_revision;
extern int get_hw_revision(void);

extern unsigned get_last_off_on_transaction_id(struct device *dev);
extern struct omap_board_mux *board_mux_ptr;

struct class *sec_class;
EXPORT_SYMBOL(sec_class);

struct device *sio_switch_dev;
EXPORT_SYMBOL(sio_switch_dev);

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

int get_hw_revision(void)
{
	return hw_revision;
}
EXPORT_SYMBOL(get_hw_revision);
	
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
//      omap_ctrl_writew(OMAP34XX_MUX_MODE7, 0x018C);
//      omap_ctrl_writew(OMAP34XX_MUX_MODE7, 0x0190);

	printk("BOARD HW REV : %d\n", hw_revision);
}

static void __init omap_board_init_irq(void)
{
	omap_init_irq();
	omap2_init_common_hw(hyb18m512160af6_sdrc_params,
			     NULL,
			     omap3630_mpu_rate_table,
			     omap3630_dsp_rate_table, omap3630_l3_rate_table);

}

struct sec_reboot_code {
	char *cmd;
	int mode;
};

static int omap_board_reboot_call(struct notifier_block *this,
				  unsigned long code, void *cmd)
{
	int mode = REBOOT_MODE_NONE;
	int temp_mode;
	int default_switchsel = 5; 
	
	struct sec_reboot_code reboot_tbl[] = {
		{"arm11_fota", REBOOT_MODE_ARM11_FOTA},
		{"arm9_fota", REBOOT_MODE_ARM9_FOTA},
		{"recovery", REBOOT_MODE_RECOVERY},
		{"download", REBOOT_MODE_DOWNLOAD}
	};
	size_t i, n;

	if ((code == SYS_RESTART) && cmd) {
		n = sizeof(reboot_tbl) / sizeof(struct sec_reboot_code);
		for (i = 0; i < n; i++) {
			if (!strcmp((char *)cmd, reboot_tbl[i].cmd)) {
				mode = reboot_tbl[i].mode;
				break;
			}
		}
	}

	if (code != SYS_POWER_OFF)
		{
		if (sec_get_param_value && sec_set_param_value)
			{
			/*in case of RECOVERY mode we set switch_sel with default value*/
			sec_get_param_value(__REBOOT_MODE, &temp_mode);
			if(temp_mode == REBOOT_MODE_RECOVERY)
				sec_set_param_value(__SWITCH_SEL, &default_switchsel);
			}
		
		/*set normal reboot_mode when reset*/	
		if (sec_set_param_value)
			sec_set_param_value(__REBOOT_MODE, &mode);
		}

	return NOTIFY_DONE;
}

static struct notifier_block omap_board_reboot_notifier = {
	.notifier_call = omap_board_reboot_call,
};

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

	/*RTC support */
	msecure_init();

#ifdef CONFIG_SAMSUNG_KERNEL_DEBUG	// klaatu
	debug_info_init();
#endif

	sec_class = class_create(THIS_MODULE, "sec");
	if (IS_ERR(sec_class)) {
		pr_err("Failed to create class(sec)!\n");
	}

	sio_switch_dev =
	    device_create(sec_class, NULL, MKDEV(0, 0), NULL, "switch");

	if (IS_ERR(sio_switch_dev)) {
		pr_err("Failed to create device(sio_switch)!\n");
	}

	get_board_hw_rev();
	board_peripherals_init();

	register_reboot_notifier(&omap_board_reboot_notifier);

#ifdef CONFIG_SAMSUNG_USE_SEC_LOG_BUF
	sec_log_buf_init();
#endif
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

#ifdef CONFIG_SAMSUNG_USE_SEC_LOG_BUF
	sec_log_buf_reserve_mem();
#endif
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

MACHINE_START(HERON, "Heron Samsung Board")
    .phys_io = 0x48000000,
    .io_pg_offst = ((0xfa000000) >> 18) & 0xfffc,
    .boot_params = 0x80000100,
    .fixup = omap_board_fixup,
    .map_io = omap_board_map_io,
    .init_irq = omap_board_init_irq,
    .init_machine = omap_board_init,
    .timer = &omap_timer,
MACHINE_END
