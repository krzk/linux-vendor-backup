/*
 * ASV(Adaptive Supply Voltage) common core
 *
 * Copyright (c) 2013 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/io.h>
#include <linux/pm_opp.h>
#include <linux/slab.h>
#include <linux/device.h>
#include <linux/power/asv-driver.h>

static LIST_HEAD(asv_list);
static DEFINE_MUTEX(asv_mutex);

struct asv_member {
	struct list_head		node;
	struct asv_info *asv_info;
};

static void add_asv_member(struct asv_member *asv_mem)
{
	mutex_lock(&asv_mutex);
	list_add_tail(&asv_mem->node, &asv_list);
	mutex_unlock(&asv_mutex);
}

static struct asv_member *asv_get_mem(enum asv_type_id asv_type)
{
	struct asv_member *asv_mem;
	struct asv_info *asv_info;

	list_for_each_entry(asv_mem, &asv_list, node) {
		asv_info = asv_mem->asv_info;
		if (asv_type == asv_info->type)
			return asv_mem;
	}

	return NULL;
}

unsigned int asv_get_volt(enum asv_type_id target_type,
						unsigned int target_freq)
{
	struct asv_member *asv_mem = asv_get_mem(target_type);
	struct asv_freq_table *dvfs_table;
	struct asv_info *asv_info;
	unsigned int i;

	if (!asv_mem)
		return 0;

	asv_info = asv_mem->asv_info;
	dvfs_table = asv_info->dvfs_table;

	for (i = 0; i < asv_info->nr_dvfs_level; i++) {
		if (dvfs_table[i].freq == target_freq)
			return dvfs_table[i].volt;
	}

	return 0;
}

int asv_init_opp_table(struct device *dev, enum asv_type_id target_type)
{
	struct asv_member *asv_mem = asv_get_mem(target_type);
	struct asv_info *asv_info;
	struct asv_freq_table *dvfs_table;
	unsigned int i;

	if (!asv_mem)
		return -EINVAL;

	asv_info = asv_mem->asv_info;
	dvfs_table = asv_info->dvfs_table;

	for (i = 0; i < asv_info->nr_dvfs_level; i++) {
		if (dev_pm_opp_add(dev, dvfs_table[i].freq * 1000,
			dvfs_table[i].volt)) {
			dev_warn(dev, "Failed to add OPP %d\n",
				 dvfs_table[i].freq);
			continue;
		}
	}

	return 0;
}

static struct asv_member *asv_init_member(struct asv_info *asv_info)
{
	struct asv_member *asv_mem;
	int ret = 0;

	if (!asv_info) {
		pr_err("No ASV info provided\n");
		return NULL;
	}

	asv_mem = kzalloc(sizeof(*asv_mem), GFP_KERNEL);
	if (!asv_mem) {
		pr_err("Allocation failed for member: %s\n", asv_info->name);
		return NULL;
	}

	asv_mem->asv_info = kmemdup(asv_info, sizeof(*asv_info), GFP_KERNEL);
	if (!asv_mem->asv_info) {
		pr_err("Copying asv_info failed for member: %s\n",
			asv_info->name);
		kfree(asv_mem);
		return NULL;
	}
	asv_info = asv_mem->asv_info;

	if (asv_info->ops->get_asv_group) {
		ret = asv_info->ops->get_asv_group(asv_info);
		if (ret) {
			pr_err("get_asv_group failed for %s : %d\n",
				asv_info->name, ret);
			goto err;
		}
	}

	if (asv_info->ops->init_asv)
		ret = asv_info->ops->init_asv(asv_info);
		if (ret) {
			pr_err("asv_init failed for %s : %d\n",
				asv_info->name, ret);
			goto err;
		}

	/* In case of parsing table from DT, we may need to add flag to identify
	DT supporting members and call init_asv_table from asv_init_opp_table(
	after getting dev_node from dev,if required), instead of calling here.
	*/

	if (asv_info->ops->init_asv_table) {
		ret = asv_info->ops->init_asv_table(asv_info);
		if (ret) {
			pr_err("init_asv_table failed for %s : %d\n",
				asv_info->name, ret);
			goto err;
		}
	}

	if (!asv_info->nr_dvfs_level || !asv_info->dvfs_table) {
		pr_err("No dvfs_table for %s\n", asv_info->name);
		goto err;
	}

	pr_info("Registered asv member: %s with group: %d",
		asv_info->name, asv_info->asv_grp);

	return asv_mem;
err:
	kfree(asv_mem->asv_info);
	kfree(asv_mem);
	return NULL;
}

void register_asv_member(struct asv_info *list, unsigned int nr_member)
{
	struct asv_member *asv_mem;
	int cnt;

	for (cnt = 0; cnt < nr_member; cnt++) {
		asv_mem = asv_init_member(&list[cnt]);

		if (asv_mem)
			add_asv_member(asv_mem);
	}
}
