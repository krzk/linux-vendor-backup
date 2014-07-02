/* exynos5410 - ASV(Adaptive Supply Voltage)
 *
 * Copyright (c) 2014 Hardkernel Co., Ltd.
 *		http://www.hardkernel.com/
 *
 * Author: Hakjoo Kim<ruppi.kim@hardkernel.com>
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/err.h>
#include <linux/io.h>
#include <linux/slab.h>
#include <linux/clk.h>
#include <linux/bitrev.h>
#include <linux/power/asv-driver.h>
#include "exynos-asv.h"
#include "exynos5410-asv.h"
#include "exynos5410-bin2-asv.h"
#include <mach/regs-pmu.h>

static unsigned special_lot_group;
static bool is_special_lot;
static bool is_speedgroup;
static struct exynos5410_asv_info asv_group __initdata;
enum table_version asv_table_version;
enum volt_offset asv_volt_offset[5][2];
static const char * const special_lot_list[] = {
	"NZXK8",
	"NZXKR",
	"NZXT6",
};

bool get_asv_is_bin2(void)
{
	return asv_table_version == ASV_TABLE_BIN2;
}
EXPORT_SYMBOL_GPL(get_asv_is_bin2);

static unsigned int exynos5410_add_volt_offset(unsigned int voltage, enum volt_offset offset)
{
	switch (offset) {
	case VOLT_OFFSET_0MV:
		break;
	case VOLT_OFFSET_25MV:
		voltage += 25000;
		break;
	case VOLT_OFFSET_50MV:
		voltage += 50000;
		break;
	case VOLT_OFFSET_75MV:
		voltage += 75000;
		break;
	};

	return voltage;
}

static unsigned int exynos5410_apply_volt_offset(unsigned int voltage, enum asv_type_id target_type)
{
	if (!is_speedgroup)
		return voltage;

	if (voltage > BASE_VOLTAGE_OFFSET)
		voltage = exynos5410_add_volt_offset(voltage, asv_volt_offset[target_type][0]);
	else
		voltage = exynos5410_add_volt_offset(voltage, asv_volt_offset[target_type][1]);

	return voltage;
}

void exynos5410_set_abb(struct asv_info *asv_info)
{
	void __iomem *target_reg;
	unsigned int target_value;

	switch (asv_info->type) {
	case ASV_ARM:
	case ASV_KFC:
		target_reg = EXYNOS5410_BB_CON0;
		target_value = arm_asv_abb_info[asv_info->asv_grp];
		break;
	case ASV_INT_MIF_L0:
	case ASV_INT_MIF_L1:
	case ASV_INT_MIF_L2:
	case ASV_INT_MIF_L3:
	case ASV_MIF:
		target_reg = EXYNOS5410_BB_CON1;
		target_value = int_asv_abb_info[asv_info->asv_grp];
		break;
	default:
		return;
	}

	set_abb(target_reg, target_value);
}

static int __init exynos5410_get_asv_group(struct asv_info *asv_info)
{
	unsigned int i;
	int exynos_asv_grp = 0;
	const unsigned int (*refer_use_table_get_asv)
				[ARM_LEVEL_NR];
	const unsigned int (*refer_table_get_asv)
				[ARM_LEVEL_NR];

	if (is_special_lot) {
		exynos_asv_grp = special_lot_group;	
		goto special_lot;
	}
	refer_use_table_get_asv = exynos5410_refer_use_table_get_asv;
	refer_table_get_asv = exynos5410_refer_table_get_asv;

	for (i = 0; i < asv_info->nr_dvfs_level; i++) {
		if (refer_use_table_get_asv[0][i] &&
			asv_group.ids <= refer_table_get_asv[0][i]) {
			exynos_asv_grp = i;
			break;
		}
				
		if (refer_use_table_get_asv[1][i] &&
			asv_group.hpm <= refer_table_get_asv[1][i]) {
			exynos_asv_grp = i;
			break;
		}
		if(i >= (asv_info->nr_dvfs_level-1))
			pr_info("No matching ASV group for IDS:%d  HPM:%d\n",
				asv_group.ids, asv_group.hpm);
	}
special_lot:
	asv_info->asv_grp = exynos_asv_grp;
	return 0;
}

static int __init exynos5410_init_asv_table(struct asv_info *asv_info)
{
	int i;
	struct asv_freq_table *dvfs_table;
	int asv_grp = asv_info->asv_grp;
	unsigned int level = asv_info->nr_dvfs_level;
	const unsigned int (*asv_voltage)[EXYNOS5410_MAX_GRP_NR + 1];

	exynos5410_set_abb(asv_info);

	dvfs_table = kzalloc(sizeof(*dvfs_table) * level, GFP_KERNEL);
	if (!dvfs_table)
		return -ENOMEM;

	switch (asv_info->type) {
	case ASV_ARM:
		asv_voltage = get_asv_is_bin2() ? arm_asv_voltage_bin2 : arm_asv_voltage;
		break;
	case ASV_KFC:
		asv_voltage = get_asv_is_bin2() ? kfc_asv_voltage_bin2 : kfc_asv_voltage;
		break;
	default:
		return -EINVAL;
	}
	for (i = 0; i < level; i++) {
		dvfs_table[i].freq = asv_voltage[i][0];

		dvfs_table[i].volt = 
			exynos5410_apply_volt_offset(asv_voltage[i][asv_grp + 1], asv_info->type);
	}

	asv_info->dvfs_table = dvfs_table;
	return 0;
}

static bool exynos5410_check_lot_id(struct exynos_asv_common *asv_info)
{
	u32 lot_id = asv_group.lot_id;
	unsigned int rev_lot;
	unsigned int i;
	unsigned int tmp;

	is_special_lot = false;
	rev_lot = bitrev32(lot_id);
	asv_info->lot_name[0] = 'N';
	lot_id = (rev_lot >> 11) & 0x1FFFFF;

	for (i = 4; i >= 1; i --) {
		tmp = lot_id % 36;
		lot_id /= 36;
		asv_info->lot_name[i] =
				(tmp < 10) ? (tmp + '0') : ((tmp - 10) + 'A');
	}

	for (i = 0; i < ARRAY_SIZE(special_lot_list); i++) {
		if (!strncmp(asv_info->lot_name, special_lot_list[i],
								LOT_ID_LEN)) {
			is_special_lot = true;
			goto out;
		}
	}
 out:
	pr_info("Exynos5410 ASV : Lot ID is %s[%s]\n", asv_info->lot_name,
				is_special_lot ? "Special" : "Non Special");
	return is_special_lot;
}

/* TODO: Implement .init_asv callback to set ABB value */

static struct asv_ops exynos5410_asv_ops __initdata = {
	.get_asv_group = exynos5410_get_asv_group,
	.init_asv_table = exynos5410_init_asv_table,
};

static struct asv_info exynos5410_asv_member[] __initdata = {
	{
		.type		= ASV_ARM,
		.name		= "VDD_ARM",
		.ops		= &exynos5410_asv_ops,
		.nr_dvfs_level	= ARM_LEVEL_NR,
	}, {
		.type		= ASV_KFC,
		.name		= "VDD_KFC",
		.ops		= &exynos5410_asv_ops,
		.nr_dvfs_level	= KFC_LEVEL_NR,
	},
};

int __init exynos5410_asv_init(struct exynos_asv_common *exynos_info)
{
	struct clk *clk_chipid;

	/* lot ID Check */
	clk_chipid = clk_get(NULL, "chipid");
	if (IS_ERR(clk_chipid)) {
		pr_info("EXYNOS5410 ASV : cannot find chipid clock!\n");
		return -EINVAL;
	}
	clk_enable(clk_chipid);

	special_lot_group = 0;
	is_speedgroup = false;

	pr_debug("%s \n", __func__);
	asv_group.package_id = readl(exynos_info->base + CHIP_ID_OFFSET);
	asv_group.aux_info = readl(exynos_info->base + CHIP_AUXINFO_OFFSET);
	asv_group.lot_id = readl(exynos_info->base + CHIP_ID0_OFFSET);
	pr_info("pro_id: 0x%x, lot_id: 0x%x\n", readl(exynos_info->base), asv_group.lot_id);

	is_special_lot = exynos5410_check_lot_id(exynos_info);
	if(is_special_lot)
		goto set_asv_member;

	if ((asv_group.package_id >> EXYNOS5410_USESG_OFFSET) & EXYNOS5410_USESG_MASK) {
		if (!((asv_group.package_id >> EXYNOS5410_SG_BSIGN_OFFSET) &
					EXYNOS5410_SG_BSIGN_MASK))
			special_lot_group = ((asv_group.package_id >> EXYNOS5410_SG_A_OFFSET) & EXYNOS5410_SG_A_MASK)
				-((asv_group.package_id >> EXYNOS5410_SG_B_OFFSET) & EXYNOS5410_SG_B_MASK);
		else
			special_lot_group = ((asv_group.package_id >> EXYNOS5410_SG_A_OFFSET) & EXYNOS5410_SG_A_MASK)
				+((asv_group.package_id >> EXYNOS5410_SG_B_OFFSET) & EXYNOS5410_SG_B_MASK);
		is_speedgroup = true;
		pr_info("EXYNOS5410 ASV : Use Fusing Speed Group %d\n", special_lot_group);
	} else {
		asv_group.hpm = (asv_group.aux_info >> EXYNOS5410_TMCB_OFFSET) & 
								EXYNOS5410_TMCB_MASK;
		asv_group.ids = (asv_group.package_id >> EXYNOS5410_IDS_OFFSET) &
								EXYNOS5410_IDS_MASK;
	}
	if(!asv_group.hpm) {
		is_special_lot = true;
		pr_err("Exynos5410 ASV : invalid IDS value\n");
	}
	pr_info("EXYNOS5410 ASV : %s IDS : %d HPM : %d\n", exynos_info->lot_name,
		asv_group.ids, asv_group.hpm);

	asv_table_version = (asv_group.package_id >> EXYNOS5410_TABLE_OFFSET) & EXYNOS5410_TABLE_MASK;

	if (get_asv_is_bin2()) {
		exynos5410_asv_member[0].nr_dvfs_level = ARM_BIN2_LEVEL_NR;
		exynos5410_asv_member[1].nr_dvfs_level = KFC_BIN2_LEVEL_NR;
	}

	asv_volt_offset[ASV_ARM][0] = (asv_group.aux_info >> EXYNOS5410_EGLLOCK_UP_OFFSET) & EXYNOS5410_EGLLOCK_UP_MASK;
	asv_volt_offset[ASV_ARM][1] = (asv_group.aux_info >> EXYNOS5410_KFCLOCK_DN_OFFSET) & EXYNOS5410_EGLLOCK_DN_MASK;
	asv_volt_offset[ASV_KFC][0] = (asv_group.aux_info >> EXYNOS5410_KFCLOCK_UP_OFFSET) & EXYNOS5410_KFCLOCK_UP_MASK;
	asv_volt_offset[ASV_KFC][1] = (asv_group.aux_info >> EXYNOS5410_KFCLOCK_DN_OFFSET) & EXYNOS5410_KFCLOCK_DN_MASK;
	asv_volt_offset[ASV_INT][0] = (asv_group.aux_info >> EXYNOS5410_INTLOCK_UP_OFFSET) & EXYNOS5410_INTLOCK_UP_MASK;
	asv_volt_offset[ASV_INT][1] = (asv_group.aux_info >> EXYNOS5410_INTLOCK_DN_OFFSET) & EXYNOS5410_INTLOCK_DN_MASK;
	asv_volt_offset[ASV_G3D][0] = (asv_group.aux_info >> EXYNOS5410_G3DLOCK_UP_OFFSET) & EXYNOS5410_G3DLOCK_UP_MASK;
	asv_volt_offset[ASV_G3D][1] = (asv_group.aux_info >> EXYNOS5410_G3DLOCK_DN_OFFSET) & EXYNOS5410_G3DLOCK_DN_MASK;
	asv_volt_offset[ASV_MIF][0] = (asv_group.aux_info >> EXYNOS5410_MIFLOCK_UP_OFFSET) & EXYNOS5410_MIFLOCK_UP_MASK;
	asv_volt_offset[ASV_MIF][1] = (asv_group.aux_info >> EXYNOS5410_MIFLOCK_DN_OFFSET) & EXYNOS5410_MIFLOCK_DN_MASK;

set_asv_member:
	clk_disable(clk_chipid);

	exynos_info->asv_list = exynos5410_asv_member;
	exynos_info->nr_mem = ARRAY_SIZE(exynos5410_asv_member);

	return 0;
}
