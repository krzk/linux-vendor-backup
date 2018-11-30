/*
 *sec_hw_param.c
 *
 * Copyright (c) 2016 Samsung Electronics Co., Ltd
 *              http://www.samsung.com
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 *
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/sec_sysfs.h>
#include <linux/uaccess.h>
#include <linux/soc/samsung/exynos-soc.h>
#include <linux/io.h>

#include <soc/samsung/cal-if.h>
#if defined(CONFIG_SOC_EXYNOS9110)
#include <dt-bindings/clock/exynos9110.h>
#endif

#define DATA_SIZE 1024
#define LOT_STRING_LEN 5

extern struct exynos_chipid_info exynos_soc_info;
extern const char *soc_ap_id;
extern unsigned int system_rev;
static char warranty = 'D';

static int __init sec_hw_param_bin(char *arg)
{
	warranty = (char)*arg;
	return 0;
}

early_param("sec_debug.bin", sec_hw_param_bin);

static u32 chipid_reverse_value(u32 value, u32 bitcnt)
{
	int tmp, ret = 0;
	int i;

	for (i = 0; i < bitcnt; i++) {
		tmp = (value >> i) & 0x1;
		ret += tmp << ((bitcnt - 1) - i);
	}

	return ret;
}

static void chipid_dec_to_36(u32 in, char *p)
{
	int mod;
	int i;

	for (i = LOT_STRING_LEN - 1; i >= 1; i--) {
		mod = in % 36;
		in /= 36;
		p[i] = (mod < 10) ? (mod + '0') : (mod - 10 + 'A');
	}

	p[0] = 'N';
	p[LOT_STRING_LEN] = '\0';
}


static ssize_t sec_hw_param_ap_info_show(struct kobject *kobj,
					 struct kobj_attribute *attr, char *buf)
{
	ssize_t info_size = 0;
	int reverse_id_0 = 0;
	u32 tmp = 0;
	char lot_id[LOT_STRING_LEN + 1];

	reverse_id_0 = chipid_reverse_value(exynos_soc_info.lot_id, 32);
	tmp = (reverse_id_0 >> 11) & 0x1FFFFF;
	chipid_dec_to_36(tmp, lot_id);

	info_size +=
		snprintf(buf, DATA_SIZE, "\"AP\":\"%s EVT%d.%d\",",
			soc_ap_id, exynos_soc_info.revision>>4, exynos_soc_info.revision%16);
	info_size +=
		snprintf((char *)(buf + info_size), DATA_SIZE - info_size,
			"\"HW_REV\":\"%d\",", system_rev);
	info_size +=
	    snprintf((char *)(buf + info_size), DATA_SIZE - info_size,
		     "\"BIN\":\"%c\",", warranty);
	info_size +=
	    snprintf((char *)(buf + info_size), DATA_SIZE - info_size,
		     "\"LOT_ID\":\"%s\",", lot_id);
	info_size +=
	    snprintf((char *)(buf + info_size), DATA_SIZE - info_size,
		     "\"ASV_LIT\":\"%d\",", cal_asv_get_grp(ACPM_DVFS_CPUCL0));
	info_size +=
	    snprintf((char *)(buf + info_size), DATA_SIZE - info_size,
		     "\"ASV_MIF\":\"%d\",", cal_asv_get_grp(ACPM_DVFS_MIF));
	info_size +=
	    snprintf((char *)(buf + info_size), DATA_SIZE - info_size,
		     "\"IDS_BIG\":\"%d\"\n", cal_asv_get_ids_info(ACPM_DVFS_CPUCL0));

	return info_size;
}

static struct kobj_attribute sec_hw_param_ap_info_attr =
        __ATTR(ap_info, 0440, sec_hw_param_ap_info_show, NULL);

static struct attribute *sec_hw_param_attributes[] = {
	&sec_hw_param_ap_info_attr.attr,
	NULL,
};

static struct attribute_group sec_hw_param_attr_group = {
	.attrs = sec_hw_param_attributes,
};

static int __init sec_hw_param_init(void)
{
	int ret = 0;
	struct device *dev;

	dev = sec_device_create(NULL, "sec_hw_param");

	ret = sysfs_create_group(&dev->kobj, &sec_hw_param_attr_group);
	if (ret)
		pr_err("%s : could not create sysfs noden", __func__);

	return 0;
}

device_initcall(sec_hw_param_init);
