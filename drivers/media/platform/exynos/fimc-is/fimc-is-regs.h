/*
 * Samsung Exynos5 SoC series FIMC-IS driver
 *
 * Copyright (c) 2011 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef FIMC_IS_REGS_H
#define FIMC_IS_REGS_H

/* MCUCTL register offsets */

/* MCU Controller Register */
#define MCUCTLR				(0x00)
#define MCUCTLR_AXI_ISPX_AWCACHE(x)	((x) << 16)
#define MCUCTLR_AXI_ISPX_ARCACHE(x)	((x) << 12)
#define MCUCTLR_MSWRST			(1 << 0)
/* Boot Base OFfset Address Register */
#define BBOAR				(0x04)
#define BBOAR_BBOA(x)			((x) << 0)
/* Interrupt Generation Register 0 from Host CPU to VIC */
#define INTGR0				(0x08)
#define INTGR0_INTGC(n)			(1 << ((n) + 16))
#define INTGR0_INTGD(n)			(1 << (n))

/* Interrupt Clear Register 0 from Host CPU to VIC */
#define INTCR0				(0x0c)
#define INTCR0_INTCC(n)			(1 << ((n) + 16))
#define INTCR0_INTCD(n)			(1 << (n))

/* Interrupt Mask Register 0 from Host CPU to VIC */
#define INTMR0				(0x10)
#define INTMR0_INTMC(n)			(1 << ((n) + 16))
#define INTMR0_INTMD(n)			(1 << (n))

/* Interrupt Status Register 0 from Host CPU to VIC */
#define INTSR0				(0x14)
#define INTSR0_GET_INTSD(n, x)		(((x) >> (n)) & 0x1)
#define INTSR0_GET_INTSC(n, x)		(((x) >> ((n) + 16)) & 0x1)

/* Interrupt Mask Status Register 0 from Host CPU to VIC */
#define INTMSR0				(0x18)
#define INTMSR0_GET_INTMSD(n, x)	(((x) >> (n)) & 0x1)
#define INTMSR0_GET_INTMSC(n, x)	(((x) >> ((n) + 16)) & 0x1)

/* Interrupt Generation Register 1 from ISP CPU to Host IC */
#define INTGR1				(0x1c)
#define INTGR1_INTGC(n)			(1 << (n))

/* Interrupt Clear Register 1 from ISP CPU to Host IC */
#define INTCR1				(0x20)
#define INTCR1_INTCC(n)			(1 << (n))

/* Interrupt Mask Register 1 from ISP CPU to Host IC */
#define INTMR1				(0x24)
#define INTMR1_INTMC(n)			(1 << (n))

/* Interrupt Status Register 1 from ISP CPU to Host IC */
#define INTSR1				(0x28)
/* Interrupt Mask Status Register 1 from ISP CPU to Host IC */
#define INTMSR1				(0x2c)
/* Interrupt Clear Register 2 from ISP BLK's interrupts to Host IC */
#define INTCR2				(0x30)
#define INTCR2_INTCC(n)			(1 << (n))

/* Interrupt Mask Register 2 from ISP BLK's interrupts to Host IC */
#define INTMR2				(0x34)
#define INTMR2_INTMCIS(n)		(1 << (n))

/* Interrupt Status Register 2 from ISP BLK's interrupts to Host IC */
#define INTSR2				(0x38)
/* Interrupt Mask Status Register 2 from ISP BLK's interrupts to Host IC */
#define INTMSR2				(0x3c)
/* General Purpose Output Control Register (0~17) */
#define GPOCTLR				(0x40)
#define GPOCTLR_GPOG(n, x)		((x) << (n))

/* General Purpose Pad Output Enable Register (0~17) */
#define GPOENCTLR			(0x44)
#define GPOENCTLR_GPOEN17(n ,x)		((x) << (n))

/* General Purpose Input Control Register (0~17) */
#define GPICTLR				(0x48)

/* IS register bank shared  between ISP CPU and HOST CPU */
/* n = 0...63 */
/* ISSR0 - Command Host -> IS */
/* ISSR1 - Sensor ID for Command */
/* ISSR2...5 - Parameter 1...4 */
/* ISSR10 - Command IS -> Host */
/* ISSR11 - Sensor ID for Command */
/* ISSR12...15 - Parameter 1...4 */
/* ISSR20 - ISP_FRAME_DONE : Sensor ID */
/* ISSR21 - ISP_FRAME_DONE : Parameter 1 */
/* ISSR24 - SCALERC_FRAME_DONE : Sensor ID */
/* ISSR25 - SCALERC_FRAME_DONE : Parameter 1 */
/* ISSR28 - 3DNR_FRAME_DONE : Sensor ID */
/* ISSR29 - 3DNR_FRAME_DONE : Parameter 1 */
/* ISSR32 - SCALERP_FRAME_DONE : Sensor ID */
/* ISSR33 - SCALERP_FRAME_DONE : Parameter 1 */

#define ISSR(n)				(0x80 + ((n) * 4))

#define PMUREG_ISP_ARM_CONFIGURATION	(0x2580)
#define PMUREG_ISP_ARM_STATUS		(0x2584)
#define PMUREG_ISP_ARM_OPTION		(0x2588)
#define PMUREG_ISP_LOW_POWER_OFF	(0x0004)
#define PMUREG_ISP_CONFIGURATION	(0x4140)
#define PMUREG_ISP_STATUS		(0x4144)

#define SYSREG_GSCBLK_CFG1		(S3C_VA_SYS + 0x0224)
#define SYSREG_ISPBLK_CFG		(S3C_VA_SYS + 0x022C)
/* FIMC-IS GIC register region base addresses */
#define PA_FIMC_IS_GIC_C		(0x141E0000)
#define PA_FIMC_IS_GIC_D		(0x141F0000)

#endif
