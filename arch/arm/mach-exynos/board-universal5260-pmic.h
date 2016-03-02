/*
 * Copyright (c) 2013 Samsung Electronics Co., Ltd.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef __MACH_EXYNOS_BOARD_UNIVERSAL5260_PMIC_H
#define __MACH_EXYNOS_BOARD_UNIVERSAL5260_PMIC_H

#if defined(CONFIG_MACH_HLLTE)
extern void board_hl_pmic_init(void);
#elif defined(CONFIG_MACH_HL3G)
extern void board_hl_pmic_init(void);
#endif

void __init exynos5_universal5260_pmic_init(void)
{
#if defined(CONFIG_MACH_HLLTE)
	board_hl_pmic_init();
#elif defined(CONFIG_MACH_HL3G)
	board_hl_pmic_init();
#endif
}

#endif /* __MACH_EXYNOS_BOARD_UNIVERSAL5260_PMIC_H */
