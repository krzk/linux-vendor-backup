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
#include "smc.h"

/*
 * Function signature: void exynos_smc(u32 cmd, u32 arg1, u32 arg2, u32 arg3)
 */
ENTRY(exynos_smc)
        dsb     sy
        smc     #0
        ret
ENDPROC(exynos_smc)

ENTRY(exynos_smc_readsfr)
        stp     x1, x3, [sp, #-16]!

        /* Currently, it supports only 32bit addresses */
        lsr     w1, w0, #2
        orr     w1, w1, #SMC_REG_CLASS_SFR_R
        mov     w0, #SMC_CMD_REG
        dsb     sy
        smc     #0
        ldp     x1, x3, [sp], #16
        cmp     x0, #0
        csel    x2, x2, x0, eq
        str     x2, [x1]

        ret
ENDPROC(exynos_smc_readsfr)
