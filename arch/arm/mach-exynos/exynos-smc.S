/*
 * Copyright (C) 2012 Samsung Electronics.
 *
 * Copied from omap-smc.S Copyright (C) 2010 Texas Instruments, Inc.
 *
 * This program is free software,you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/linkage.h>

/*
 * Function signature: void exynos_smc(u32 cmd, u32 arg1, u32 arg2, u32 arg3)
 */

ENTRY(exynos_smc)
	stmfd	sp!, {r4-r11, lr}
	dsb
	smc	#0
	ldmfd	sp!, {r4-r11, pc}
ENDPROC(exynos_smc)

/*
 * Function signature: u32 exynos_smc2(u32 cmd, u32 arg1, u32 arg2, u32 arg3)
 * return r2
 */

ENTRY(exynos_smc2)
	stmfd	sp!, {r4-r11, lr}
	dsb
	smc	#0
	mov	r0, r2
	ldmfd	sp!, {r4-r11, pc}
ENDPROC(exynos_smc2)
