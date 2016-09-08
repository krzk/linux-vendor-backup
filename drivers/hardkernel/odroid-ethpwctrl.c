/*
 * odroid-ethpwctrl.c: Ethernet Power Control driver for ODROID-XU3/4
 *
 * Copyright (c) 2016 Hardkernel Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <linux/module.h>
#include <linux/device.h>
#include <linux/kobject.h>
#include <linux/sysfs.h>
#include <linux/init.h>
#include <linux/string.h>

extern int s2mps11_pmic_ethonoff(unsigned char status);
unsigned long eth_power = 1;

static ssize_t eth_power_show(struct class *class,
			      struct class_attribute *attr,
			      char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%s\n", eth_power == 1 ? "on" : "off");
}

static ssize_t eth_power_store(struct class *class,
			       struct class_attribute *attr,
			       const char *buf, size_t count)
{
	unsigned int val;

	if (strncmp(buf, "on", 2) == 0)
		val = 1;
	else if (strncmp(buf, "off", 3) == 0)
		val = 0;
	else
		return -EINVAL;

	s2mps11_pmic_ethonoff(val);
	eth_power = val;

	return count;
}

static struct class_attribute eth_pwctrl_class_attrs[] = {
	__ATTR(eth_power, S_IWUSR | S_IRUGO, eth_power_show, eth_power_store),
	__ATTR_NULL,
};

static struct class eth_pwctrl_class = {
	.name		= "eth_power_ctrl",
	.class_attrs	= eth_pwctrl_class_attrs,
};

static int __init ethpwctrl_sysfs_init(void)
{
	return class_register(&eth_pwctrl_class);
}
module_init(ethpwctrl_sysfs_init);

static void __exit ethpwctrl_sysfs_exit(void)
{
	class_unregister(&eth_pwctrl_class);
}
module_exit(ethpwctrl_sysfs_exit);

MODULE_DESCRIPTION("Ethernet power control driver for ODROID-XU3/4 boards");
MODULE_AUTHOR("Brian Kim <brian.kim@hardkernel.com>");
MODULE_LICENSE("GPL v2");
