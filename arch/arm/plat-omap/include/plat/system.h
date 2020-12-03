/*
 * Copied from arch/arm/mach-sa1100/include/mach/system.h
 * Copyright (c) 1999 Nicolas Pitre <nico@fluxnic.net>
 */
#ifndef __ASM_ARCH_SYSTEM_H
#define __ASM_ARCH_SYSTEM_H
#include <linux/clk.h>

#include <asm/mach-types.h>
#include <mach/hardware.h>

#include <plat/prcm.h>

#ifndef CONFIG_MACH_VOICEBLUE
#define voiceblue_reset()		do {} while (0)
#else
extern void voiceblue_reset(void);
#endif
#define REBOOTMODE_NORMAL 		(1 << 0)
#define REBOOTMODE_RECOVERY 		(1 << 1)
#define REBOOTMODE_FOTA 		(1 << 2)
#ifdef CONFIG_SAMSUNG_KERNEL_DEBUG
#define REBOOTMODE_LOCKUP 		(1 << 3)
#endif /* CONFIG_SAMSUNG_KERNEL_DEBUG */
#define REBOOTMODE_SHUTDOWN		(1 << 4)
#define REBOOTMODE_DOWNLOAD             (1 << 5)
#ifdef CONFIG_SAMSUNG_KERNEL_DEBUG
#define REBOOTMODE_LOCKUP_USER 		(1 << 6)
#define REBOOTMODE_LOCKUP_CP		(1 << 9)
#endif /* CONFIG_SAMSUNG_KERNEL_DEBUG */

static inline void arch_idle(void)
{
	cpu_do_idle();
}

static inline void omap1_arch_reset(char mode)
{
	/*
	 * Workaround for 5912/1611b bug mentioned in sprz209d.pdf p. 28
	 * "Global Software Reset Affects Traffic Controller Frequency".
	 */
	if (cpu_is_omap5912()) {
		omap_writew(omap_readw(DPLL_CTL) & ~(1 << 4),
				 DPLL_CTL);
		omap_writew(0x8, ARM_RSTCT1);
	}

	if (machine_is_voiceblue())
		voiceblue_reset();
	else
		omap_writew(1, ARM_RSTCT1);
}

struct sec_reboot_mode {
	char *cmd;
	char mode;
};

static inline char omap_get_lsi_compatible_reboot_mode(char mode, const char *cmd)
{
	char new_mode = mode;
	struct sec_reboot_mode mode_tbl[] = {
		{"arm11_fota", 'f'},
		{"arm9_fota", 'f'},
		{"recovery", 'r'},
		{"download", 'd'},
		{"cp_crash", 'C'}
	};
	size_t i, n;
#ifdef CONFIG_SAMSUNG_KERNEL_DEBUG
	if (mode == 'L' || mode == 'U') {
		new_mode = mode;
		goto __return;
	}
#endif /* CONFIG_SAMSUNG_KERNEL_DEBUG */
	if (cmd == NULL)
		goto __return;
	n = sizeof(mode_tbl) / sizeof(struct sec_reboot_mode);
	for (i = 0; i < n; i++) {
		if (!strcmp(cmd, mode_tbl[i].cmd)) {
			new_mode = mode_tbl[i].mode;
			goto __return;
		}
	}

__return:
	return new_mode;
}

static inline void arch_reset(char mode, const char *cmd)
{
	u32 scpad = 0;
	scpad = omap_readl(OMAP343X_CTRL_BASE + 0x918);

#ifdef CONFIG_SAMSUNG_KERNEL_DEBUG
	omap_writel(0x54455352, OMAP343X_CTRL_BASE + 0x9C4); // set to normal reset
#endif /* CONFIG_SAMSUNG_KERNEL_DEBUG */

	/* for the compatibility with LSI chip-set based products */
	mode = omap_get_lsi_compatible_reboot_mode(mode, cmd);

	switch(mode){
		case 'r': /* reboot mode = recovery */ 
			omap_writel(scpad | REBOOTMODE_RECOVERY, OMAP343X_CTRL_BASE + 0x918);
			break;
		case 'f': /* reboot mode = fota */
			omap_writel(scpad | REBOOTMODE_FOTA, OMAP343X_CTRL_BASE + 0x918);
			break;
#ifdef CONFIG_SAMSUNG_KERNEL_DEBUG
		case 'L': /* reboot mode = Lockup */
			omap_writel(scpad | REBOOTMODE_LOCKUP, OMAP343X_CTRL_BASE + 0x918);
			break;
		case 'U': /* reboot mode = Lockup */
			omap_writel(scpad | REBOOTMODE_LOCKUP_USER, OMAP343X_CTRL_BASE + 0x918);
			break;
		case 'C': /* reboot mode = Lockup */
			omap_writel(scpad | REBOOTMODE_LOCKUP_CP, OMAP343X_CTRL_BASE + 0x918);
			break;
#endif /* CONFIG_SAMSUNG_KERNEL_DEBUG */
		case 't': /* reboot mode = shutdown with TA */
		case 'u': /* reboot mode = shutdown with USB */
		case 'j': /* reboot mode = shutdown with JIG */    
			omap_writel(scpad | REBOOTMODE_SHUTDOWN, OMAP343X_CTRL_BASE + 0x918);
			break;
		case 'd': /* reboot mode = download */
			omap_writel(scpad | REBOOTMODE_DOWNLOAD, OMAP343X_CTRL_BASE + 0x918);
			break;

		default: /* reboot mode = normal */ 
			omap_writel(scpad | REBOOTMODE_NORMAL, OMAP343X_CTRL_BASE + 0x918);
			break;
	}
	if (!cpu_class_is_omap2())
		omap1_arch_reset(mode);
	else
		omap_prcm_arch_reset(mode);
}

#endif
