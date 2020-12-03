/*
 * OMAP2/3 clockdomains
 *
 * Copyright (C) 2008 Texas Instruments, Inc.
 * Copyright (C) 2008 Nokia Corporation
 *
 * Written by Paul Walmsley
 */

/*
 * To-Do List
 * -> Port the Sleep/Wakeup dependencies for the domains
 *    from the Power domain framework
 */

#ifndef __ARCH_ARM_MACH_OMAP2_CLOCKDOMAINS_H
#define __ARCH_ARM_MACH_OMAP2_CLOCKDOMAINS_H

#include <plat/clockdomain.h>
#include "cm.h"
#include "prm.h"

/*
 * OMAP2/3-common clockdomains
 *
 * Even though the 2420 has a single PRCM module from the
 * interconnect's perspective, internally it does appear to have
 * separate PRM and CM clockdomains.  The usual test case is
 * sys_clkout/sys_clkout2.
 */

#if defined(CONFIG_ARCH_OMAP24XX) | defined(CONFIG_ARCH_OMAP34XX)

/* This is an implicit clockdomain - it is never defined as such in TRM */
static struct clockdomain wkup_clkdm = {
	.name		= "wkup_clkdm",
	.pwrdm		= { .name = "wkup_pwrdm" },
	.omap_chip	= OMAP_CHIP_INIT(CHIP_IS_OMAP24XX | CHIP_IS_OMAP3430),
};

static struct clockdomain prm_clkdm = {
	.name		= "prm_clkdm",
	.pwrdm		= { .name = "wkup_pwrdm" },
	.omap_chip	= OMAP_CHIP_INIT(CHIP_IS_OMAP24XX | CHIP_IS_OMAP3430),
};

static struct clockdomain cm_clkdm = {
	.name		= "cm_clkdm",
	.pwrdm		= { .name = "core_pwrdm" },
	.omap_chip	= OMAP_CHIP_INIT(CHIP_IS_OMAP24XX | CHIP_IS_OMAP3430),
};

#endif

/*
 * 2420-only clockdomains
 */

#if defined(CONFIG_ARCH_OMAP2420)

static struct clockdomain mpu_2420_clkdm = {
	.name		= "mpu_clkdm",
	.pwrdm		= { .name = "mpu_pwrdm" },
	.flags		= CLKDM_CAN_HWSUP,
	.clkstctrl_reg  = OMAP2420_CM_REGADDR(MPU_MOD, OMAP2_CM_CLKSTCTRL),
	.clktrctrl_mask = OMAP24XX_AUTOSTATE_MPU_MASK,
	.omap_chip	= OMAP_CHIP_INIT(CHIP_IS_OMAP2420),
};

static struct clockdomain iva1_2420_clkdm = {
	.name		= "iva1_clkdm",
	.pwrdm		= { .name = "dsp_pwrdm" },
	.flags		= CLKDM_CAN_HWSUP_SWSUP,
	.clkstctrl_reg  = OMAP2420_CM_REGADDR(OMAP24XX_DSP_MOD,
						 OMAP2_CM_CLKSTCTRL),
	.clktrctrl_mask = OMAP2420_AUTOSTATE_IVA_MASK,
	.omap_chip	= OMAP_CHIP_INIT(CHIP_IS_OMAP2420),
};

static struct clockdomain dsp_2420_clkdm = {
	.name		= "dsp_clkdm",
	.pwrdm		= { .name = "dsp_pwrdm" },
	.flags		= CLKDM_CAN_HWSUP_SWSUP,
	.clkstctrl_reg  = OMAP2420_CM_REGADDR(OMAP24XX_DSP_MOD,
						 OMAP2_CM_CLKSTCTRL),
	.clktrctrl_mask = OMAP24XX_AUTOSTATE_DSP_MASK,
	.omap_chip	= OMAP_CHIP_INIT(CHIP_IS_OMAP2420),
};

static struct clockdomain gfx_2420_clkdm = {
	.name		= "gfx_clkdm",
	.pwrdm		= { .name = "gfx_pwrdm" },
	.flags		= CLKDM_CAN_HWSUP_SWSUP,
	.clkstctrl_reg  = OMAP2420_CM_REGADDR(GFX_MOD, OMAP2_CM_CLKSTCTRL),
	.clktrctrl_mask = OMAP24XX_AUTOSTATE_GFX_MASK,
	.omap_chip	= OMAP_CHIP_INIT(CHIP_IS_OMAP2420),
};

static struct clockdomain core_l3_2420_clkdm = {
	.name		= "core_l3_clkdm",
	.pwrdm		= { .name = "core_pwrdm" },
	.flags		= CLKDM_CAN_HWSUP,
	.clkstctrl_reg  = OMAP2420_CM_REGADDR(CORE_MOD, OMAP2_CM_CLKSTCTRL),
	.clktrctrl_mask = OMAP24XX_AUTOSTATE_L3_MASK,
	.omap_chip	= OMAP_CHIP_INIT(CHIP_IS_OMAP2420),
};

static struct clockdomain core_l4_2420_clkdm = {
	.name		= "core_l4_clkdm",
	.pwrdm		= { .name = "core_pwrdm" },
	.flags		= CLKDM_CAN_HWSUP,
	.clkstctrl_reg  = OMAP2420_CM_REGADDR(CORE_MOD, OMAP2_CM_CLKSTCTRL),
	.clktrctrl_mask = OMAP24XX_AUTOSTATE_L4_MASK,
	.omap_chip	= OMAP_CHIP_INIT(CHIP_IS_OMAP2420),
};

static struct clockdomain dss_2420_clkdm = {
	.name		= "dss_clkdm",
	.pwrdm		= { .name = "core_pwrdm" },
	.flags		= CLKDM_CAN_HWSUP,
	.clkstctrl_reg  = OMAP2420_CM_REGADDR(CORE_MOD, OMAP2_CM_CLKSTCTRL),
	.clktrctrl_mask = OMAP24XX_AUTOSTATE_DSS_MASK,
	.omap_chip	= OMAP_CHIP_INIT(CHIP_IS_OMAP2420),
};

#endif   /* CONFIG_ARCH_OMAP2420 */


/*
 * 2430-only clockdomains
 */

#if defined(CONFIG_ARCH_OMAP2430)

static struct clockdomain mpu_2430_clkdm = {
	.name		= "mpu_clkdm",
	.pwrdm		= { .name = "mpu_pwrdm" },
	.flags		= CLKDM_CAN_HWSUP_SWSUP,
	.clkstctrl_reg  = OMAP2430_CM_REGADDR(MPU_MOD,
						 OMAP2_CM_CLKSTCTRL),
	.clktrctrl_mask = OMAP24XX_AUTOSTATE_MPU_MASK,
	.omap_chip	= OMAP_CHIP_INIT(CHIP_IS_OMAP2430),
};

static struct clockdomain mdm_clkdm = {
	.name		= "mdm_clkdm",
	.pwrdm		= { .name = "mdm_pwrdm" },
	.flags		= CLKDM_CAN_HWSUP_SWSUP,
	.clkstctrl_reg  = OMAP2430_CM_REGADDR(OMAP2430_MDM_MOD,
						 OMAP2_CM_CLKSTCTRL),
	.clktrctrl_mask = OMAP2430_AUTOSTATE_MDM_MASK,
	.omap_chip	= OMAP_CHIP_INIT(CHIP_IS_OMAP2430),
};

static struct clockdomain dsp_2430_clkdm = {
	.name		= "dsp_clkdm",
	.pwrdm		= { .name = "dsp_pwrdm" },
	.flags		= CLKDM_CAN_HWSUP_SWSUP,
	.clkstctrl_reg  = OMAP2430_CM_REGADDR(OMAP24XX_DSP_MOD,
						 OMAP2_CM_CLKSTCTRL),
	.clktrctrl_mask = OMAP24XX_AUTOSTATE_DSP_MASK,
	.omap_chip	= OMAP_CHIP_INIT(CHIP_IS_OMAP2430),
};

static struct clockdomain gfx_2430_clkdm = {
	.name		= "gfx_clkdm",
	.pwrdm		= { .name = "gfx_pwrdm" },
	.flags		= CLKDM_CAN_HWSUP_SWSUP,
	.clkstctrl_reg  = OMAP2430_CM_REGADDR(GFX_MOD, OMAP2_CM_CLKSTCTRL),
	.clktrctrl_mask = OMAP24XX_AUTOSTATE_GFX_MASK,
	.omap_chip	= OMAP_CHIP_INIT(CHIP_IS_OMAP2430),
};

static struct clockdomain core_l3_2430_clkdm = {
	.name		= "core_l3_clkdm",
	.pwrdm		= { .name = "core_pwrdm" },
	.flags		= CLKDM_CAN_HWSUP,
	.clkstctrl_reg  = OMAP2430_CM_REGADDR(CORE_MOD, OMAP2_CM_CLKSTCTRL),
	.clktrctrl_mask = OMAP24XX_AUTOSTATE_L3_MASK,
	.omap_chip	= OMAP_CHIP_INIT(CHIP_IS_OMAP2430),
};

static struct clockdomain core_l4_2430_clkdm = {
	.name		= "core_l4_clkdm",
	.pwrdm		= { .name = "core_pwrdm" },
	.flags		= CLKDM_CAN_HWSUP,
	.clkstctrl_reg  = OMAP2430_CM_REGADDR(CORE_MOD, OMAP2_CM_CLKSTCTRL),
	.clktrctrl_mask = OMAP24XX_AUTOSTATE_L4_MASK,
	.omap_chip	= OMAP_CHIP_INIT(CHIP_IS_OMAP2430),
};

static struct clockdomain dss_2430_clkdm = {
	.name		= "dss_clkdm",
	.pwrdm		= { .name = "core_pwrdm" },
	.flags		= CLKDM_CAN_HWSUP,
	.clkstctrl_reg  = OMAP2430_CM_REGADDR(CORE_MOD, OMAP2_CM_CLKSTCTRL),
	.clktrctrl_mask = OMAP24XX_AUTOSTATE_DSS_MASK,
	.omap_chip	= OMAP_CHIP_INIT(CHIP_IS_OMAP2430),
};

#endif    /* CONFIG_ARCH_OMAP2430 */


/*
 * 34xx clockdomains
 */

#if defined(CONFIG_ARCH_OMAP34XX)

static struct clockdomain mpu_34xx_clkdm = {
	.name		= "mpu_clkdm",
	.pwrdm		= { .name = "mpu_pwrdm" },
	.flags		= CLKDM_CAN_HWSUP | CLKDM_CAN_FORCE_WAKEUP,
	.clkstctrl_reg	= OMAP34XX_CM_REGADDR(MPU_MOD, OMAP2_CM_CLKSTCTRL),
	.clktrctrl_mask = OMAP3430_CLKTRCTRL_MPU_MASK,
	.omap_chip	= OMAP_CHIP_INIT(CHIP_IS_OMAP3430),
};

static struct clockdomain neon_clkdm = {
	.name		= "neon_clkdm",
	.pwrdm		= { .name = "neon_pwrdm" },
	.flags		= CLKDM_CAN_HWSUP_SWSUP,
	.clkstctrl_reg	= OMAP34XX_CM_REGADDR(OMAP3430_NEON_MOD,
						 OMAP2_CM_CLKSTCTRL),
	.clktrctrl_mask = OMAP3430_CLKTRCTRL_NEON_MASK,
	.omap_chip	= OMAP_CHIP_INIT(CHIP_IS_OMAP3430),
};

static struct clockdomain iva2_clkdm = {
	.name		= "iva2_clkdm",
	.pwrdm		= { .name = "iva2_pwrdm" },
	.flags		= CLKDM_CAN_HWSUP_SWSUP,
	.clkstctrl_reg	= OMAP34XX_CM_REGADDR(OMAP3430_IVA2_MOD,
						 OMAP2_CM_CLKSTCTRL),
	.clktrctrl_mask = OMAP3430_CLKTRCTRL_IVA2_MASK,
	.omap_chip	= OMAP_CHIP_INIT(CHIP_IS_OMAP3430),
};

static struct clockdomain gfx_3430es1_clkdm = {
	.name		= "gfx_clkdm",
	.pwrdm		= { .name = "gfx_pwrdm" },
	.flags		= CLKDM_CAN_HWSUP_SWSUP,
	.clkstctrl_reg	= OMAP34XX_CM_REGADDR(GFX_MOD, OMAP2_CM_CLKSTCTRL),
	.clktrctrl_mask = OMAP3430ES1_CLKTRCTRL_GFX_MASK,
	.omap_chip	= OMAP_CHIP_INIT(CHIP_IS_OMAP3430ES1),
};

static struct clockdomain sgx_clkdm = {
	.name		= "sgx_clkdm",
	.pwrdm		= { .name = "sgx_pwrdm" },
	.flags		= CLKDM_CAN_HWSUP_SWSUP,
	.clkstctrl_reg	= OMAP34XX_CM_REGADDR(OMAP3430ES2_SGX_MOD,
						 OMAP2_CM_CLKSTCTRL),
	.clktrctrl_mask = OMAP3430ES2_CLKTRCTRL_SGX_MASK,
	.omap_chip	= OMAP_CHIP_INIT(CHIP_GE_OMAP3430ES2),
};

/*
 * The die-to-die clockdomain was documented in the 34xx ES1 TRM, but
 * then that information was removed from the 34xx ES2+ TRM.  It is
 * unclear whether the core is still there, but the clockdomain logic
 * is there, and must be programmed to an appropriate state if the
 * CORE clockdomain is to become inactive.
 */
static struct clockdomain d2d_clkdm = {
	.name		= "d2d_clkdm",
	.pwrdm		= { .name = "core_pwrdm" },
	.flags		= CLKDM_CAN_HWSUP_SWSUP,
	.clkstctrl_reg	= OMAP34XX_CM_REGADDR(CORE_MOD, OMAP2_CM_CLKSTCTRL),
	.clktrctrl_mask = OMAP3430ES1_CLKTRCTRL_D2D_MASK,
	.omap_chip	= OMAP_CHIP_INIT(CHIP_IS_OMAP3430),
};

static struct clockdomain core_l3_34xx_clkdm = {
	.name		= "core_l3_clkdm",
	.pwrdm		= { .name = "core_pwrdm" },
	.flags		= CLKDM_CAN_HWSUP,
	.clkstctrl_reg	= OMAP34XX_CM_REGADDR(CORE_MOD, OMAP2_CM_CLKSTCTRL),
	.clktrctrl_mask = OMAP3430_CLKTRCTRL_L3_MASK,
	.omap_chip	= OMAP_CHIP_INIT(CHIP_IS_OMAP3430),
};

static struct clockdomain core_l4_34xx_clkdm = {
	.name		= "core_l4_clkdm",
	.pwrdm		= { .name = "core_pwrdm" },
	.flags		= CLKDM_CAN_HWSUP,
	.clkstctrl_reg	= OMAP34XX_CM_REGADDR(CORE_MOD, OMAP2_CM_CLKSTCTRL),
	.clktrctrl_mask = OMAP3430_CLKTRCTRL_L4_MASK,
	.omap_chip	= OMAP_CHIP_INIT(CHIP_IS_OMAP3430),
};

static struct clockdomain dss_34xx_clkdm = {
	.name		= "dss_clkdm",
	.pwrdm		= { .name = "dss_pwrdm" },
	.flags		= CLKDM_CAN_HWSUP_SWSUP,
	.clkstctrl_reg	= OMAP34XX_CM_REGADDR(OMAP3430_DSS_MOD,
						 OMAP2_CM_CLKSTCTRL),
	.clktrctrl_mask = OMAP3430_CLKTRCTRL_DSS_MASK,
	.omap_chip	= OMAP_CHIP_INIT(CHIP_IS_OMAP3430),
};

static struct clockdomain cam_clkdm = {
	.name		= "cam_clkdm",
	.pwrdm		= { .name = "cam_pwrdm" },
	.flags		= CLKDM_CAN_HWSUP_SWSUP,
	.clkstctrl_reg	= OMAP34XX_CM_REGADDR(OMAP3430_CAM_MOD,
						 OMAP2_CM_CLKSTCTRL),
	.clktrctrl_mask = OMAP3430_CLKTRCTRL_CAM_MASK,
	.omap_chip	= OMAP_CHIP_INIT(CHIP_IS_OMAP3430),
};

static struct clockdomain usbhost_clkdm = {
	.name		= "usbhost_clkdm",
	.pwrdm		= { .name = "usbhost_pwrdm" },
	.flags		= CLKDM_CAN_HWSUP_SWSUP,
	.clkstctrl_reg	= OMAP34XX_CM_REGADDR(OMAP3430ES2_USBHOST_MOD,
						 OMAP2_CM_CLKSTCTRL),
	.clktrctrl_mask = OMAP3430ES2_CLKTRCTRL_USBHOST_MASK,
	.omap_chip	= OMAP_CHIP_INIT(CHIP_GE_OMAP3430ES2),
};

static struct clockdomain per_clkdm = {
	.name		= "per_clkdm",
	.pwrdm		= { .name = "per_pwrdm" },
	.flags		= CLKDM_CAN_HWSUP_SWSUP,
	.clkstctrl_reg	= OMAP34XX_CM_REGADDR(OMAP3430_PER_MOD,
						 OMAP2_CM_CLKSTCTRL),
	.clktrctrl_mask = OMAP3430_CLKTRCTRL_PER_MASK,
	.omap_chip	= OMAP_CHIP_INIT(CHIP_IS_OMAP3430),
};

/*
 * Disable hw supervised mode for emu_clkdm, because emu_pwrdm is
 * switched of even if sdti is in use
 */
static struct clockdomain emu_clkdm = {
	.name		= "emu_clkdm",
	.pwrdm		= { .name = "emu_pwrdm" },
	.flags		= /* CLKDM_CAN_ENABLE_AUTO |  */CLKDM_CAN_SWSUP,
	.clkstctrl_reg	= OMAP34XX_CM_REGADDR(OMAP3430_EMU_MOD,
						 OMAP2_CM_CLKSTCTRL),
	.clktrctrl_mask = OMAP3430_CLKTRCTRL_EMU_MASK,
	.omap_chip	= OMAP_CHIP_INIT(CHIP_IS_OMAP3430),
};

static struct clockdomain dpll1_clkdm = {
	.name		= "dpll1_clkdm",
	.pwrdm		= { .name = "dpll1_pwrdm" },
	.omap_chip	= OMAP_CHIP_INIT(CHIP_IS_OMAP3430),
};

static struct clockdomain dpll2_clkdm = {
	.name		= "dpll2_clkdm",
	.pwrdm		= { .name = "dpll2_pwrdm" },
	.omap_chip	= OMAP_CHIP_INIT(CHIP_IS_OMAP3430),
};

static struct clockdomain dpll3_clkdm = {
	.name		= "dpll3_clkdm",
	.pwrdm		= { .name = "dpll3_pwrdm" },
	.omap_chip	= OMAP_CHIP_INIT(CHIP_IS_OMAP3430),
};

static struct clockdomain dpll4_clkdm = {
	.name		= "dpll4_clkdm",
	.pwrdm		= { .name = "dpll4_pwrdm" },
	.omap_chip	= OMAP_CHIP_INIT(CHIP_IS_OMAP3430),
};

static struct clockdomain dpll5_clkdm = {
	.name		= "dpll5_clkdm",
	.pwrdm		= { .name = "dpll5_pwrdm" },
	.omap_chip	= OMAP_CHIP_INIT(CHIP_GE_OMAP3430ES2),
};

#endif   /* CONFIG_ARCH_OMAP34XX */

#include "clockdomains44xx.h"

/*
 * Clockdomain-powerdomain hwsup dependencies (34XX only)
 */

static struct clkdm_pwrdm_autodep clkdm_pwrdm_autodeps[] = {

#ifdef CONFIG_ARCH_OMAP34XX
	{
		.pwrdm	   = { .name = "mpu_pwrdm" },
		.omap_chip = OMAP_CHIP_INIT(CHIP_IS_OMAP3430)
	},
	{
		.pwrdm	   = { .name = "iva2_pwrdm" },
		.omap_chip = OMAP_CHIP_INIT(CHIP_IS_OMAP3430)
	},
	{
		.pwrdm	   = { .name = NULL },
	}
#endif

};

/*
 * List of clockdomain pointers per platform
 */

static struct clockdomain *clockdomains_omap[] = {

#if defined(CONFIG_ARCH_OMAP24XX) | defined(CONFIG_ARCH_OMAP34XX)
	&wkup_clkdm,
	&cm_clkdm,
	&prm_clkdm,
#endif

#ifdef CONFIG_ARCH_OMAP2420
	&mpu_2420_clkdm,
	&iva1_2420_clkdm,
	&dsp_2420_clkdm,
	&gfx_2420_clkdm,
	&core_l3_2420_clkdm,
	&core_l4_2420_clkdm,
	&dss_2420_clkdm,
#endif

#ifdef CONFIG_ARCH_OMAP2430
	&mpu_2430_clkdm,
	&mdm_clkdm,
	&dsp_2430_clkdm,
	&gfx_2430_clkdm,
	&core_l3_2430_clkdm,
	&core_l4_2430_clkdm,
	&dss_2430_clkdm,
#endif

#ifdef CONFIG_ARCH_OMAP34XX
	&mpu_34xx_clkdm,
	&neon_clkdm,
	&iva2_clkdm,
	&gfx_3430es1_clkdm,
	&sgx_clkdm,
	&d2d_clkdm,
	&core_l3_34xx_clkdm,
	&core_l4_34xx_clkdm,
	&dss_34xx_clkdm,
	&cam_clkdm,
	&usbhost_clkdm,
	&per_clkdm,
	&emu_clkdm,
	&dpll1_clkdm,
	&dpll2_clkdm,
	&dpll3_clkdm,
	&dpll4_clkdm,
	&dpll5_clkdm,
#endif

#ifdef CONFIG_ARCH_OMAP4
	&l4_cefuse_44xx_clkdm,
	&l4_cfg_44xx_clkdm,
	&tesla_44xx_clkdm,
	&l3_gfx_44xx_clkdm,
	&ivahd_44xx_clkdm,
	&l4_secure_44xx_clkdm,
	&l4_per_44xx_clkdm,
	&abe_44xx_clkdm,
	&l3_instr_44xx_clkdm,
	&l3_init_44xx_clkdm,
	&mpuss_44xx_clkdm,
	&mpu0_44xx_clkdm,
	&mpu1_44xx_clkdm,
	&l3_emif_44xx_clkdm,
	&l4_ao_44xx_clkdm,
	&ducati_44xx_clkdm,
	&l3_2_44xx_clkdm,
	&l3_1_44xx_clkdm,
	&l3_d2d_44xx_clkdm,
	&iss_44xx_clkdm,
	&l3_dss_44xx_clkdm,
	&l4_wkup_44xx_clkdm,
	&emu_sys_44xx_clkdm,
	&l3_dma_44xx_clkdm,
#endif

	NULL,
};

#endif
