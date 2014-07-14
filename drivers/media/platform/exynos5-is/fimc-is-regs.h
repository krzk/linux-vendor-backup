/*
 * Samsung Exynos5 SoC series FIMC-IS driver
 *
 * Copyright (c) 2013 Samsung Electronics Co., Ltd
 * Arun Kumar K <arun.kk@samsung.com>
 * Kil-yeon Lim <kilyeon.im@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef FIMC_IS_REGS_H
#define FIMC_IS_REGS_H

/* MCUCTL register */
#define MCUCTL				0x00
/* MCU Controller Register */
#define MCUCTLR				(MCUCTL+0x00)
#define MCUCTLR_AXI_ISPX_AWCACHE(x)	((x) << 16)
#define MCUCTLR_AXI_ISPX_ARCACHE(x)	((x) << 12)
#define MCUCTLR_MSWRST			(1 << 0)
/* Boot Base OFfset Address Register */
#define BBOAR				(MCUCTL+0x04)
#define BBOAR_BBOA(x)			((x) << 0)

/* Interrupt Generation Register 0 from Host CPU to VIC */
#define INTGR0				(MCUCTL+0x08)
#define INTGR0_INTGC(n)			(1 << ((n) + 16))
#define INTGR0_INTGD(n)			(1 << (n))

/* Interrupt Clear Register 0 from Host CPU to VIC */
#define INTCR0				(MCUCTL+0x0c)
#define INTCR0_INTCC(n)			(1 << ((n) + 16))
#define INTCR0_INTCD(n)			(1 << (n))

/* Interrupt Mask Register 0 from Host CPU to VIC */
#define INTMR0				(MCUCTL+0x10)
#define INTMR0_INTMC(n)			(1 << ((n) + 16))
#define INTMR0_INTMD(n)			(1 << (n))

/* Interrupt Status Register 0 from Host CPU to VIC */
#define INTSR0				(MCUCTL+0x14)
#define INTSR0_GET_INTSD(n, x)		(((x) >> (n)) & 0x1)
#define INTSR0_GET_INTSC(n, x)		(((x) >> ((n) + 16)) & 0x1)

/* Interrupt Mask Status Register 0 from Host CPU to VIC */
#define INTMSR0				(MCUCTL+0x18)
#define INTMSR0_GET_INTMSD(n, x)	(((x) >> (n)) & 0x1)
#define INTMSR0_GET_INTMSC(n, x)	(((x) >> ((n) + 16)) & 0x1)

/* Interrupt Generation Register 1 from ISP CPU to Host IC */
#define INTGR1				(MCUCTL+0x1c)
#define INTGR1_INTGC(n)			(1 << (n))

/* Interrupt Clear Register 1 from ISP CPU to Host IC */
#define INTCR1				(MCUCTL+0x20)
#define INTCR1_INTCC(n)			(1 << (n))

/* Interrupt Mask Register 1 from ISP CPU to Host IC */
#define INTMR1				(MCUCTL+0x24)
#define INTMR1_INTMC(n)			(1 << (n))

/* Interrupt Status Register 1 from ISP CPU to Host IC */
#define INTSR1				(MCUCTL+0x28)
/* Interrupt Mask Status Register 1 from ISP CPU to Host IC */
#define INTMSR1				(MCUCTL+0x2c)
/* Interrupt Clear Register 2 from ISP BLK's interrupts to Host IC */
#define INTCR2				(MCUCTL+0x30)
#define INTCR2_INTCC(n)			(1 << (n))

/* Interrupt Mask Register 2 from ISP BLK's interrupts to Host IC */
#define INTMR2				(MCUCTL+0x34)
#define INTMR2_INTMCIS(n)		(1 << (n))

/* Interrupt Status Register 2 from ISP BLK's interrupts to Host IC */
#define INTSR2				(MCUCTL+0x38)
/* Interrupt Mask Status Register 2 from ISP BLK's interrupts to Host IC */
#define INTMSR2				(MCUCTL+0x3c)
/* General Purpose Output Control Register (0~17) */
#define GPOCTLR				(MCUCTL+0x40)
#define GPOCTLR_GPOG(n, x)		((x) << (n))

/* General Purpose Pad Output Enable Register (0~17) */
#define GPOENCTLR			(MCUCTL+0x44)
#define GPOENCTLR_GPOEN0(n, x)		((x) << (n))

/* General Purpose Input Control Register (0~17) */
#define GPICTLR				(MCUCTL+0x48)

/* IS Shared Registers between ISP CPU and HOST CPU */
#define ISSR(n)			(MCUCTL + 0x80 + 4*(n))

/* PMU for FIMC-IS*/
#define EXYNOS5_PMUREG_CMU_RESET_ISP		0x1584
#define PMUREG_ISP_ARM_CONFIGURATION		0x2280
#define PMUREG_ISP_ARM_STATUS			0x2284
#define PMUREG_ISP_ARM_OPTION			0x2288
/* Low Power Interface settings */
#define PMUREG_ISP_LOW_POWER_OFF		0x0004

/* EXYNOS3 (EXYNOS3250)*/
#define EXYNOS3250_PMUREG_CMU_RESET_ISP		0x1174
#define PMUREG_ISP_CONFIGURATION		0x3ca0
#define PMUREG_ISP_STATUS			0x3ca4
#define PMUREG_ISP_ARM_SYS_PWR_REG		0x1050
#define PMUREG_CMU_SYSCLK_ISP_SYS_PWR_REG	0x13b8

#define PA_FIMC_IS_GIC_C			0x121E0000
#define PA_FIMC_IS_GIC_D			0x121F0000
#endif
