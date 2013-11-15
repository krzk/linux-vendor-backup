/* exynos5250 - ASV(Adaptive Supply Voltage)
 *
 * Copyright (c) 2013 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/err.h>
#include <linux/io.h>
#include <linux/slab.h>
#include <linux/power/asv-driver.h>
#include "exynos-asv.h"

#define FUSED_SG_OFFSET		3
#define ORIG_SG_OFFSET		17
#define ORIG_SG_MASK		0xF
#define MOD_SG_OFFSET		21
#define MOD_SG_MASK		0x7

#define ARM_LEVEL_NR		16
#define ARM_GRP_NR		12

#define CHIP_ID_OFFSET		0x4

struct exynos5250_asv_info {
	unsigned int package_id;
	/* we may need more info as global data */
};

static struct exynos5250_asv_info asv_group __initdata;

static unsigned int asv_voltage[ARM_LEVEL_NR][ARM_GRP_NR + 1] __initdata = {
	{ 1700000, 1300000, 1275000, 1275000, 1262500, 1250000, 1225000,
	1212500, 1200000, 1187500, 1175000, 1150000, 1125000 },	/* L0 */
	{ 1600000, 1250000, 1225000, 1225000, 1212500, 1200000, 1187500,
	1175000, 1162500, 1150000, 1137500, 1112500, 1100000 },	/* L1 */
	{ 1500000, 1225000, 1187500, 1175000, 1162500, 1150000, 1137500,
	1125000, 1112500, 1100000, 1087500, 1075000, 1062500 },	/* L2 */
	{ 1400000, 1200000, 1125000, 1125000, 1125000, 1112500, 1100000,
	1087500, 1075000, 1062500, 1050000, 1037500, 1025000 },	/* L3 */
	{ 1300000, 1150000, 1100000, 1100000, 1100000, 1087500, 1075000,
	1062500, 1050000, 1037500, 1025000, 1012500, 1000000 },	/* L4 */
	{ 1200000, 1125000, 1075000, 1075000, 1062500, 1050000, 1037500,
	1025000, 1012500, 1000000,  987500,  975000,  975000 },	/* L5 */
	{ 1100000, 1100000, 1050000, 1050000, 1037500, 1025000, 1012500,
	1000000,  987500,  975000,  962500,  950000,  925000 },	/* L6 */
	{ 1000000, 1075000, 1037500, 1037500, 1012500, 1000000,  987500,
	975000,  962500,  950000,  937500,  925000,  912500 },	/* L7 */
	{ 900000, 1050000, 1025000, 1012500,  987500,  975000,  962500,
	950000,  937500,  925000,  912500,  912500,  900000 },	/* L8 */
	{ 800000, 1025000, 1000000,  987500,  975000,  962500,  950000,
	937500,  925000,  912500,  900000,  900000,  900000 },	/* L9 */
	{ 700000, 1012500,  975000,  962500,  950000,  937500,  925000,
	912500,  900000,  900000,  900000,  900000,  900000 },	/* L10 */
	{ 600000, 1000000,  962500,  950000,  937500,  925000,  912500,
	900000,  900000,  900000,  900000,  900000,  900000 },	/* L11 */
	{ 500000, 975000,  950000,  937500,  925000,  912500,  900000,
	900000,  900000,  900000,  900000,  900000,  887500 },	/* L12 */
	{ 400000, 950000,  937500,  925000,  912500,  900000,  900000,
	900000,  900000,  900000,  900000,  887500,  887500 },	/* L13 */
	{ 300000, 937500,  925000,  912500,  900000,  900000,  900000,
	900000,  900000,  900000,  887500,  887500,  875000 },	/* L14 */
	{ 200000, 925000,  912500,  900000,  900000,  900000,  900000,
	900000,  900000,  887500,  887500,  875000,  875000 },	/* L15 */
};

static int __init exynos5250_get_asv_group(struct asv_info *asv_info)
{
	int exynos_asv_grp;
	u32 exynos_orig_sp;
	u32 exynos_mod_sp;
	u32 package_id = asv_group.package_id;

	/* If ASV group is fused then retrieve it */
	if ((package_id >> FUSED_SG_OFFSET) & 0x1) {
		exynos_orig_sp = (package_id >> ORIG_SG_OFFSET) & ORIG_SG_MASK;
		exynos_mod_sp = (package_id >> MOD_SG_OFFSET) & MOD_SG_MASK;

		exynos_asv_grp = exynos_orig_sp - exynos_mod_sp;
		if (exynos_asv_grp < 0) {
			pr_warn("%s: Invalid ASV group: %d\n", __func__,
				exynos_asv_grp);
			exynos_asv_grp = 0;	/* go for default */
		}
	} else {
		pr_warn("%s: ASV group not fused for : %s\n", __func__,
			asv_info->name);
		exynos_asv_grp = 0;		/* go for default */
	}

	asv_info->asv_grp = exynos_asv_grp;
	return 0;
}

static int __init exynos5250_init_arm_asv_table(struct asv_info *asv_info)
{
	struct asv_freq_table *dvfs_table;
	int i, asv_grp = asv_info->asv_grp;

	dvfs_table = kzalloc(sizeof(*dvfs_table) * ARM_LEVEL_NR, GFP_KERNEL);
	if (!dvfs_table)
		return -ENOMEM;

	for (i = 0; i < ARM_LEVEL_NR; i++) {
		dvfs_table[i].freq = asv_voltage[i][0];
		dvfs_table[i].volt = asv_voltage[i][asv_grp + 1];
	}

	asv_info->dvfs_table = dvfs_table;
	return 0;
}

/* TODO: Implement .init_asv callback to set ABB value */

static struct asv_ops exynos5250_arm_asv_ops __initdata = {
	.get_asv_group = exynos5250_get_asv_group,
	.init_asv_table = exynos5250_init_arm_asv_table,
};

static struct asv_info exynos5250_asv_member[] __initdata = {
	{
		.type		= ASV_ARM,
		.name		= "VDD_ARM",
		.ops		= &exynos5250_arm_asv_ops,
		.nr_dvfs_level	= ARM_LEVEL_NR,
	},
};

int __init exynos5250_asv_init(struct exynos_asv_common *exynos_info)
{
	asv_group.package_id = readl(exynos_info->base + CHIP_ID_OFFSET);
	exynos_info->asv_list = exynos5250_asv_member;
	exynos_info->nr_mem = ARRAY_SIZE(exynos5250_asv_member);

	return 0;
}
