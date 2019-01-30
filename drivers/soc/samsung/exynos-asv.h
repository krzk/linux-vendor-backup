// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2018 Samsung Electronics Co., Ltd.
 *	      http://www.samsung.com/
 *
 * Samsung Exynos SoC Adaptive Supply Voltage support
 */

#define EXYNOS_ASV_MAX_NUM	16

 /* HPM, IDS values to select target group */
struct asv_limit_entry {
	unsigned int hpm;
	unsigned int ids;
};

struct exynos_asv_subsys {
	int id;
	char *cpu_dt_compat;

	unsigned int base_volt;
	unsigned int dvfs_nr;
	unsigned int offset_volt_h;
	unsigned int offset_volt_l;
	const u32 (*asv_table)[EXYNOS_ASV_MAX_NUM + 1];
};

struct exynos_asv {
	struct exynos_asv_subsys arm;
	struct exynos_asv_subsys kfc;

	int (*opp_get_voltage)(struct exynos_asv_subsys *subs, int idx,
			       unsigned int frequency, unsigned int voltage);
	unsigned int group;
	unsigned int table;

	unsigned int bin2;
	unsigned int special_lot;
};
