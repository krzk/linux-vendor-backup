/*
 * OMAP Power Management Common Routines
 *
 * Copyright (C) 2005 Texas Instruments, Inc.
 * Copyright (C) 2006-2008 Nokia Corporation
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/platform_device.h>

#include <plat/resource.h>
#include <plat/omap34xx.h>
#include <plat/powerdomain.h>
#include <plat/omap_device.h>

#include <plat/omap-pm.h>
#include "pm.h"
#include "omap3-opp.h"

#ifdef CONFIG_OMAP_PM_SRF
static ssize_t vdd_opp_show(struct kobject *, struct kobj_attribute *, char *);
static ssize_t vdd_opp_store(struct kobject *k, struct kobj_attribute *,
			  const char *buf, size_t n);
static struct kobj_attribute vdd1_opp_attr =
	__ATTR(vdd1_opp, 0644, vdd_opp_show, vdd_opp_store);

static struct kobj_attribute vdd2_opp_attr =
	__ATTR(vdd2_opp, 0644, vdd_opp_show, vdd_opp_store);
static struct kobj_attribute vdd1_lock_attr =
	__ATTR(vdd1_lock, 0644, vdd_opp_show, vdd_opp_store);
static struct kobj_attribute vdd2_lock_attr =
	__ATTR(vdd2_lock, 0644, vdd_opp_show, vdd_opp_store);
static struct kobj_attribute dsp_freq_attr =
	__ATTR(dsp_freq, 0644, vdd_opp_show, vdd_opp_store);
static struct kobj_attribute tick_control_attr =
	__ATTR(tick, 0644, vdd_opp_show, vdd_opp_store);

#endif

#ifdef CONFIG_OMAP_PM_SRF
#include <plat/omap34xx.h>
static int vdd1_locked;
static int vdd2_locked;
static struct device sysfs_cpufreq_dev;
static struct device sysfs_dsp_dev;
extern void tick_nohz_disable(int nohz);

static ssize_t vdd_opp_show(struct kobject *kobj, struct kobj_attribute *attr,
			 char *buf)
{
	int opp_no;
	struct omap_opp *opp_table;

	if (attr == &vdd1_opp_attr)
		return sprintf(buf, "%hu\n", resource_get_level("vdd1_opp"));
	else if (attr == &vdd2_opp_attr)
		return sprintf(buf, "%hu\n", resource_get_level("vdd2_opp"));
	else if (attr == &vdd1_lock_attr)
		return sprintf(buf, "%hu\n", resource_get_opp_lock(VDD1_OPP));
	else if (attr == &vdd2_lock_attr)
		return sprintf(buf, "%hu\n", resource_get_opp_lock(VDD2_OPP));
	else if (attr == &dsp_freq_attr) {
		opp_no = resource_get_level("vdd1_opp");
		opp_table = omap_get_dsp_rate_table();
		return sprintf(buf, "%lu\n", opp_table[opp_no].rate);
	} else
		return -EINVAL;
}

static ssize_t vdd_opp_store(struct kobject *kobj, struct kobj_attribute *attr,
			  const char *buf, size_t n)
{
	unsigned long value;
	int flags = 0;
	struct omap_opp *opp_table;

	if (sscanf(buf, "%lu", &value) != 1)
		return -EINVAL;

	if (attr == &tick_control_attr) {
		if (value == 1)
			tick_nohz_disable(1);
		else if (value == 0)
			tick_nohz_disable(0);
	}
	/* Check locks */
	if (attr == &vdd1_lock_attr) {
		flags = OPP_IGNORE_LOCK;
		attr = &vdd1_opp_attr;
		if (vdd1_locked && value == 0) {
			resource_unlock_opp(VDD1_OPP);
			resource_refresh();
			vdd1_locked = 0;
			return n;
		}
		if (vdd1_locked == 0 && value != 0) {
			resource_lock_opp(VDD1_OPP);
			vdd1_locked = 1;
			if (value < MIN_VDD1_OPP || value > MAX_VDD1_OPP) {
				printk(KERN_ERR "vdd_opp_store: Invalid value\n");
				return -EINVAL;
				}
		resource_set_opp_level(VDD1_OPP, value, flags);
		}
	} else if (attr == &vdd2_lock_attr) {
		flags = OPP_IGNORE_LOCK;
		attr = &vdd2_opp_attr;
		if (vdd2_locked && value == 0) {
			resource_unlock_opp(VDD2_OPP);
			resource_refresh();
			vdd2_locked = 0;
			return n;
		}
		if (vdd2_locked == 0 && value != 0) {
			resource_lock_opp(VDD2_OPP);
			vdd2_locked = 1;
			if (value < MIN_VDD2_OPP || value > MAX_VDD2_OPP) {
				printk(KERN_ERR "vdd_opp_store: Invalid value\n");
				return -EINVAL;
			}
			resource_set_opp_level(VDD2_OPP, value, flags);
		}
	}

	if (attr == &vdd1_opp_attr) {
		if (value < MIN_VDD1_OPP || value > MAX_VDD1_OPP) {
			printk(KERN_ERR "vdd_opp_store: Invalid value\n");
			return -EINVAL;
		}
		opp_table = omap_get_mpu_rate_table();
		omap_pm_set_min_mpu_freq(&sysfs_cpufreq_dev,
					opp_table[value].rate);
	} else if (attr == &dsp_freq_attr) {
		if (cpu_is_omap3630()) {
			if (value < S65M || value > S800M) {
				printk(KERN_ERR "dsp_freq: Invalid value\n");
				return -EINVAL;
			}
		} else {
			opp_table = omap_get_dsp_rate_table();
			if (value < opp_table[MIN_VDD1_OPP].rate
				|| value > opp_table[MAX_VDD1_OPP].rate) {
				printk(KERN_ERR "dsp_freq: Invalid value\n");
				return -EINVAL;
			}
		}
		omap_pm_dsp_set_min_opp(&sysfs_dsp_dev, value);
	} else if (attr == &vdd2_opp_attr) {
		if (value < MIN_VDD2_OPP || value > MAX_VDD2_OPP) {
			printk(KERN_ERR "vdd_opp_store: Invalid value\n");
			return -EINVAL;
		}
		if (cpu_is_omap3430()) {
			if (value == VDD2_OPP2)
				omap_pm_set_min_bus_tput(&sysfs_cpufreq_dev,
						OCP_INITIATOR_AGENT, 83*1000*4);
			else if (value == VDD2_OPP3)
				omap_pm_set_min_bus_tput(&sysfs_cpufreq_dev,
						OCP_INITIATOR_AGENT, 166*1000*4);
		} else if (cpu_is_omap3630()) {
			if (value == VDD2_OPP1)
				omap_pm_set_min_bus_tput(&sysfs_cpufreq_dev,
						OCP_INITIATOR_AGENT, 100*1000*4);
			else if (value == VDD2_OPP2)
				omap_pm_set_min_bus_tput(&sysfs_cpufreq_dev,
						OCP_INITIATOR_AGENT, 200*1000*4);
		}
	} else {
		return -EINVAL;
	}
	return n;
}
#endif

unsigned get_last_off_on_transaction_id(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct omap_device *odev = to_omap_device(pdev);
	struct powerdomain *pwrdm;

	/*
	 * REVISIT complete hwmod implementation is not yet done for OMAP3
	 * So we are using basic clockframework for OMAP3
	 * for OMAP4 we continue using hwmod framework
	 */
	if (odev) {
		if (cpu_is_omap44xx())
			pwrdm = omap_device_get_pwrdm(odev);
		else {
			if (!strcmp(pdev->name, "omapdss"))
				pwrdm = pwrdm_lookup("dss_pwrdm");
			else if (!strcmp(pdev->name, "musb_hdrc"))
				pwrdm = pwrdm_lookup("core_pwrdm");
			else if (!strcmp(pdev->name, "mmci-omap-hs"))
				pwrdm = pwrdm_lookup("core_pwrdm");
			else
				return 0;
		}

		if (pwrdm)
			return pwrdm->state_counter[0];
	}

	return 0;
}

static int __init omap_pm_init(void)
{
	int error = -EINVAL;

#ifdef CONFIG_OMAP_PM_SRF
	error = sysfs_create_file(power_kobj,
				  &tick_control_attr.attr);
	if (error) {
		printk(KERN_ERR "sysfs_create_file failed: %d\n", error);
		return error;
	}
	error = sysfs_create_file(power_kobj,
				  &dsp_freq_attr.attr);
	if (error) {
		printk(KERN_ERR "sysfs_create_file failed: %d\n", error);
		return error;
	}

	error = sysfs_create_file(power_kobj,
				  &vdd1_opp_attr.attr);
	if (error) {
		printk(KERN_ERR "sysfs_create_file failed: %d\n", error);
		return error;
	}
	error = sysfs_create_file(power_kobj,
				  &vdd2_opp_attr.attr);
	if (error) {
		printk(KERN_ERR "sysfs_create_file failed: %d\n", error);
		return error;
	}

	error = sysfs_create_file(power_kobj, &vdd1_lock_attr.attr);
	if (error) {
		printk(KERN_ERR "sysfs_create_file failed: %d\n", error);
		return error;
	}

	error = sysfs_create_file(power_kobj, &vdd2_lock_attr.attr);
	if (error) {
		printk(KERN_ERR "sysfs_create_file failed: %d\n", error);
		return error;
	}
#endif

	return error;
}
late_initcall(omap_pm_init);
