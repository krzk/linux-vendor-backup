/* linux/arch/arm/mach-exynos/dev-runtime_pm.c
 *
 * Copyright (c) 2014 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com
 *
 * EXYNOS - Runtime PM Test Driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/kobject.h>

#include <plat/cpu.h>
#include <mach/pm_domains.h>

#ifdef CONFIG_SLEEP_MONITOR
#include <linux/power/sleep_monitor.h>
#endif

struct platform_device exynos_device_runtime_pm = {
	.name	= "runtime_pm_test",
	.id	= -1,
};

static ssize_t show_power_domain(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct exynos_pm_domain **pd_index = NULL;
	struct pm_domain_data *pdd;
	unsigned int not_suspended;
	int ret = 0;

	pd_index = exynos_pm_get_power_domain();
	if (!pd_index) {
		pr_err(PM_DOMAIN_PREFIX "domain data is not found\n");
		return -EINVAL;
	}

	/* lookup master pd */
	for_each_power_domain(pd_index) {
		if ((*pd_index)->base) {
			ret += snprintf(buf+ret, PAGE_SIZE-ret, "%-13s - %-3s, ",
					(*pd_index)->pd.name,
					(*pd_index)->check_status((*pd_index)) ? "on" : "off");

			ret += snprintf(buf+ret, PAGE_SIZE-ret, "%08x %08x %08x\n",
						__raw_readl((*pd_index)->base+0x0),
						__raw_readl((*pd_index)->base+0x4),
						__raw_readl((*pd_index)->base+0x8));
		} else {
			ret += snprintf(buf+ret, PAGE_SIZE-ret, "%-13s - \n",
					(*pd_index)->pd.name);
		}
		ret += snprintf(buf+ret, PAGE_SIZE-ret, "  gpd_status=%d sd_count=%d ",
				(*pd_index)->pd.status, atomic_read(&((*pd_index)->pd.sd_count)));

		list_for_each_entry(pdd, &((*pd_index)->pd.dev_list), list_node) {
			not_suspended = 0;
			if (pdd->dev->driver && (!pm_runtime_suspended(pdd->dev)
			    || pdd->dev->power.irq_safe || to_gpd_data(pdd)->always_on))
				not_suspended++;

			ret += snprintf(buf+ret, PAGE_SIZE-ret, "%-13s - %-3s, ",
					kobject_name(&pdd->dev->kobj),
					not_suspended ? "on" : "off");
		}
		ret += snprintf(buf+ret, PAGE_SIZE-ret, "\n");
	}

	return ret;
}

static int exynos_pd_power_on(struct device *dev, const char * device_name)
{
	struct exynos_pm_domain **pd_index = NULL;
	int ret = 0;
	struct gpd_timing_data gpd_td = {
		.stop_latency_ns = 50000,
		.start_latency_ns = 50000,
		.save_state_latency_ns = 500000,
		.restore_state_latency_ns = 500000,
	};

	pd_index = exynos_pm_get_power_domain();

	if (!pd_index) {
		pr_err(PM_DOMAIN_PREFIX "domain data is not found\n");
		return -EINVAL;
	}

	/* lookup master pd */
	for_each_power_domain(pd_index) {
		if (strcmp((*pd_index)->pd.name, device_name)) {
			continue;
		}

		if ((*pd_index)->check_status((*pd_index))) {
			pr_err(PM_DOMAIN_PREFIX"%s is already on.\n", (*pd_index)->pd.name);
			break;
		}

		while (1) {
			ret = __pm_genpd_add_device(&((*pd_index)->pd), dev, &gpd_td);
			if (ret != -EAGAIN)
				break;
			cond_resched();
		}
		if (!ret) {
			pm_genpd_dev_need_restore(dev, true);
			pr_info(PM_DOMAIN_PREFIX"%s, Device : %s Registered\n", (*pd_index)->pd.name, dev_name(dev));
		} else
			pr_err(PM_DOMAIN_PREFIX"%s cannot add device %s\n", (*pd_index)->pd.name, dev_name(dev));

		pm_runtime_enable(dev);
		pm_runtime_get_sync(dev);
		pr_info(PM_DOMAIN_PREFIX"%s: power on.\n", (*pd_index)->pd.name);
	}

	return ret;
}

static int exynos_pd_power_off(struct device *dev, const char * device_name)
{
	struct exynos_pm_domain **pd_index = NULL;
	int ret = 0;

	pd_index = exynos_pm_get_power_domain();

	if (!pd_index) {
		pr_err(PM_DOMAIN_PREFIX "domain data is not found\n");
		return -EINVAL;
	}

	/* lookup master pd */
	for_each_power_domain(pd_index) {
		if (strcmp((*pd_index)->pd.name, device_name)) {
			continue;
		}

		if (!(*pd_index)->check_status((*pd_index))) {
			pr_err(PM_DOMAIN_PREFIX"%s is already off.\n", (*pd_index)->pd.name);
			break;
		}

		pm_runtime_put_sync(dev);
		pm_runtime_disable(dev);

		while (1) {
			ret = pm_genpd_remove_device(&((*pd_index)->pd), dev);
			if (ret != -EAGAIN)
				break;
			cond_resched();
		}
		if (ret)
			pr_err(PM_DOMAIN_PREFIX"%s cannot remove device %s\n", (*pd_index)->pd.name, dev_name(dev));
		pr_info(PM_DOMAIN_PREFIX"%s: power off.\n", (*pd_index)->pd.name);
	}

	return ret;

}

static int exynos_pd_longrun_test(struct device *dev, const char * device_name)
{
	struct exynos_pm_domain **pd_index = NULL;
	int i, ret = 0;
	struct gpd_timing_data gpd_td = {
		.stop_latency_ns = 50000,
		.start_latency_ns = 50000,
		.save_state_latency_ns = 500000,
		.restore_state_latency_ns = 500000,
	};

	pd_index = exynos_pm_get_power_domain();

	if (!pd_index) {
		pr_err(PM_DOMAIN_PREFIX "domain data is not found\n");
		return -EINVAL;
	}

	/* lookup master pd */
	for_each_power_domain(pd_index) {
		if (strcmp((*pd_index)->pd.name, device_name)) {
			continue;
		}

		if ((*pd_index)->check_status((*pd_index))) {
			pr_err(PM_DOMAIN_PREFIX"%s is working. Stop testing\n", (*pd_index)->pd.name);
			break;
		}

		while (1) {
			ret = __pm_genpd_add_device(&(*pd_index)->pd, dev, &gpd_td);
			if (ret != -EAGAIN)
				break;
			cond_resched();
		}
		if (!ret) {
			pm_genpd_dev_need_restore(dev, true);
			pr_info(PM_DOMAIN_PREFIX"%s, Device : %s Registered\n", (*pd_index)->pd.name, dev_name(dev));
		} else
			pr_err(PM_DOMAIN_PREFIX"%s cannot add device %s\n", (*pd_index)->pd.name, dev_name(dev));

		pr_info("%s: test start.\n", (*pd_index)->pd.name);
		pm_runtime_enable(dev);
		for (i=0; i<100; i++) {
			pm_runtime_get_sync(dev);
			mdelay(50);
			pm_runtime_put_sync(dev);
			mdelay(50);
		}
		pr_info("%s: test done.\n", (*pd_index)->pd.name);
		pm_runtime_disable(dev);

		while (1) {
			ret = pm_genpd_remove_device(&(*pd_index)->pd, dev);
			if (ret != -EAGAIN)
				break;
			cond_resched();
		}
		if (ret)
			pr_err(PM_DOMAIN_PREFIX"%s cannot remove device %s\n", (*pd_index)->pd.name, dev_name(dev));
	}

	return ret;
}

static ssize_t store_power_domain_test(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	char device_name[32], test_name[32];

	sscanf(buf, "%s %s", device_name, test_name);

	switch (test_name[0]) {
	case '1':
		exynos_pd_power_on(dev, device_name);
		break;
	case '0':
		exynos_pd_power_off(dev, device_name);
		break;
	case 't':
		exynos_pd_longrun_test(dev, device_name);
		break;
	default:
		pr_info(PM_DOMAIN_PREFIX"echo \"device\" \"test\" > control\n");
	}

	return count;
}

static DEVICE_ATTR(control, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH, show_power_domain, store_power_domain_test);

static ssize_t show_pd_active_time(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct exynos_pm_domain **pd_index = NULL;
	ktime_t now, active_time, total_active_time;
	int ret = 0;

	pd_index = exynos_pm_get_power_domain();
	if (!pd_index) {
		pr_err(PM_DOMAIN_PREFIX "domain data is not found\n");
		return -EINVAL;
	}

	ret += snprintf(buf+ret, PAGE_SIZE-ret, "pd-name\t\tpd-status\t"
		"active_since\tlast_change\ttotal_active_time\n");

	/* lookup master pd */
	for_each_power_domain(pd_index) {
		if ((*pd_index)->base) {
			total_active_time = (*pd_index)->total_active_time;
			if ((*pd_index)->pd.status == GPD_STATE_ACTIVE) {
				now = ktime_get();
				active_time = ktime_sub(now, (*pd_index)->last_time);
				total_active_time = ktime_add(total_active_time,
					ktime_sub(now, (*pd_index)->start_active_time));
			} else
				active_time = ktime_set(0, 0);

			ret += snprintf(buf+ret, PAGE_SIZE-ret, "%-8s\t%-8s\t"
					"%lld\t\t%lld\t\t%lld\t\t\n",
					(*pd_index)->pd.name,
					(*pd_index)->pd.status  ? "off" : "on",
					ktime_to_ms(active_time),
					ktime_to_ms((*pd_index)->last_time),
					ktime_to_ms(total_active_time));
		} else {
			ret += snprintf(buf+ret, PAGE_SIZE-ret, "%-12s - \n",
					(*pd_index)->pd.name);
		}
	}

	return ret;
}

static DEVICE_ATTR(active_time, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH, show_pd_active_time, NULL);

static struct attribute *control_device_attrs[] = {
	&dev_attr_control.attr,
	&dev_attr_active_time.attr,
	NULL,
};

static const struct attribute_group control_device_attr_group = {
	.attrs = control_device_attrs,
};

#ifdef CONFIG_SLEEP_MONITOR
int pm_domains_check_cb(void *dev, unsigned int *val, int check_level, int caller_type)
{
	struct exynos_pm_domain **pd_index = NULL;
	struct pm_domain_data *pdd;
	int ret = 0;
	int index = 0;

	pd_index = exynos_pm_get_power_domain();
	if (!pd_index) {
		pr_err(PM_DOMAIN_PREFIX "domain data is not found\n");
		return DEVICE_ERR_1;
	}

	/* lookup master pd */
	for_each_power_domain(pd_index) {
		if ((*pd_index)->base) {
				if ((*pd_index)->check_status((*pd_index))) {
					ret++;
					*val = *val | 1 << index;
				} else
					*val = *val | 0 << index;
		} else if ((*pd_index)->pd.status == GPD_STATE_ACTIVE) {
			ret++;
			*val = *val | 1 << index;
		} else {
			*val = *val | 0 << index;
		}

		index++;

		list_for_each_entry(pdd, &((*pd_index)->pd.dev_list), list_node) {
			if (pdd->dev->driver && (!pm_runtime_suspended(pdd->dev)
			    || pdd->dev->power.irq_safe || to_gpd_data(pdd)->always_on)) {
				ret++;
				*val = *val | 1 << index;
			} else {
				*val = *val | 0 << index;

			}
			index++;
		}
	}

	return (ret < DEVICE_UNKNOWN) ? ret : (DEVICE_UNKNOWN - 1);

}

static struct sleep_monitor_ops pm_domains_ops = {
	.read_cb_func = pm_domains_check_cb,
};
#endif

static int runtime_pm_test_probe(struct platform_device *pdev)
{
	struct class *runtime_pm_class;
	struct device *runtime_pm_dev;
	int ret;

	runtime_pm_class = class_create(THIS_MODULE, "runtime_pm");
	runtime_pm_dev = device_create(runtime_pm_class, NULL, 0, NULL, "test");
	ret = sysfs_create_group(&runtime_pm_dev->kobj, &control_device_attr_group);
	if (ret) {
		pr_err("Runtime PM Test : error to create sysfs\n");
		return -EINVAL;
	}

	pm_runtime_enable(&pdev->dev);

#ifdef CONFIG_SLEEP_MONITOR
	sleep_monitor_register_ops((void *)pdev, &pm_domains_ops, SLEEP_MONITOR_PMDOMAINS);
#endif

	return 0;
}

static int runtime_pm_test_runtime_suspend(struct device *dev)
{
	pr_info("Runtime PM Test : Runtime_Suspend\n");
	return 0;
}

static int runtime_pm_test_runtime_resume(struct device *dev)
{
	pr_info("Runtime PM Test : Runtime_Resume\n");
	return 0;
}

static struct dev_pm_ops pm_ops = {
	.runtime_suspend = runtime_pm_test_runtime_suspend,
	.runtime_resume = runtime_pm_test_runtime_resume,
};

static struct platform_driver runtime_pm_test_driver = {
	.probe		= runtime_pm_test_probe,
	.driver		= {
		.name	= "runtime_pm_test",
		.owner	= THIS_MODULE,
		.pm	= &pm_ops,
	},
};

static int __init runtime_pm_test_driver_init(void)
{
	platform_device_register(&exynos_device_runtime_pm);

	return platform_driver_register(&runtime_pm_test_driver);
}
arch_initcall_sync(runtime_pm_test_driver_init);
