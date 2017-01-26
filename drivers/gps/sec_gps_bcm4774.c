#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/gpio.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <asm/uaccess.h>
#include <linux/errno.h>
#include <linux/err.h>
#include <linux/types.h>
#include <linux/proc_fs.h>
#include <linux/sec_sysfs.h>

static struct device *gps_dev;
struct class *gps_class;
EXPORT_SYMBOL_GPL(gps_class);

static unsigned int gps_pwr_on = 0;
static struct platform_device bcm4774 = {
	.name = "bcm4774",
	.id = -1,
};

static ssize_t show_gps_pwr_en(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int ret;

	ret = gpio_get_value(gps_pwr_on);

	pr_info("%s:%d GPIO_GPS_PWR_EN is %d\n", __func__, __LINE__, ret);
	return sprintf(buf, "%d\n", ret);
}

static ssize_t set_gps_pwr_en(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	int enable;

	sscanf(buf, "%u", &enable);
	//if (kstrtoll(buf, 10, &enable) < 0)
	//	return -EINVAL;

	pr_info("%s:%d endable:%d \n", __func__, __LINE__, enable);
	if (enable)
		gpio_direction_output(gps_pwr_on, 1);
	else
		gpio_direction_output(gps_pwr_on, 0);

	return size;
}

static DEVICE_ATTR(gps_pwr_en, 0640, show_gps_pwr_en, set_gps_pwr_en);

static int __init gps_bcm4774_init(void)
{
	const char *gps_node = "broadcom,bcm4774";
	const char *gps_pwr_en = "gps-pwr-en";
	struct device_node *root_node = NULL;
	int ret = 0;

	pr_info("%s\n", __func__);

	platform_device_register(&bcm4774);

	if (!gps_class) {
		gps_class = class_create(THIS_MODULE, "gps");
		if (IS_ERR(gps_class))
			return PTR_ERR(gps_class);
	}

	gps_dev = device_create(gps_class, NULL, 0, NULL, "gps");
	if (IS_ERR(gps_dev)) {
		pr_err("%s Failed to create device(gps)!\n", __func__);
		ret = -ENODEV;
		goto err_sec_device_create;
	}

	root_node = of_find_compatible_node(NULL, NULL, gps_node);
	if(!root_node) {
		pr_err("failed to get device node of %s\n", gps_node);
		ret = -ENODEV;
		goto err_sec_device_create;
	}

	gps_pwr_on = of_get_named_gpio(root_node, gps_pwr_en, 0);
	if(!gpio_is_valid(gps_pwr_on)) {
		pr_err("%s: Invalid gpio pin : %d\n", __func__, gps_pwr_on);
		ret = -ENODEV;
		goto err_find_node;
	}

	ret = gpio_request(gps_pwr_on, "GPS_PWR_EN");
	if (ret) {
		pr_err("%s, fail to request gpio(GPS_PWR_EN)\n", __func__);
		goto err_find_node;
	}

	gpio_direction_output(gps_pwr_on, 0);

	ret = device_create_file(gps_dev, &dev_attr_gps_pwr_en);
	if (ret) {
		pr_err("%s, fail to create file gps_pwr_en\n", __func__);
		goto err_find_node;
	}

	return 0;

err_find_node:
	of_node_put(root_node);
err_sec_device_create:
	return ret;
}

device_initcall(gps_bcm4774_init);
