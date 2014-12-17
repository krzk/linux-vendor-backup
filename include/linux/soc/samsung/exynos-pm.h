/*
 * Copyright (c) 2014 Samsung Electronics Co., Ltd.
 *
 * EXYNOS PM driver header
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __EXYNOS_PM_H
#define __EXYNOS_PM_H

#include <linux/kernel.h>
#include <linux/suspend.h>

#define EXYNOS_CHECK_SLEEP	0x00000BAD

struct exynos_pm_ops {
	int (*prepare)(void);
	/* Called at syscore_ops suspend level */
	int (*prepare_late)(void);
	/* Final machine suspend call */
	int (*suspend)(void);
	/* Called at syscore_ops resume level */
	void (*finish)(void);
};
extern void __init exynos_pm_init(struct exynos_pm_ops *ops);
#endif /* __EXYNOS_PM_H */
