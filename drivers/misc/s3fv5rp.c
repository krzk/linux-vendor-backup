/*
 *  S3FV5RP - Samsung Secure Element driver
 *
 * Copyright 2016, Homin Lee <suapapa@insignal.co.kr>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 */

#include <linux/init.h>
#include <linux/mutex.h>
#include <linux/spi/spi.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/jiffies.h>
#include <linux/delay.h>
#include <linux/regulator/consumer.h>

struct s3fv5rp_chip {
	struct spi_device *spi;
	struct regulator *reg;
	struct class *se_class;
	struct device *se_dev;

	int gpio_wakeup;
	int gpio_status;
	int gpio_reset;
};

static int s3fv5rp_wating_status_gpio(struct s3fv5rp_chip *chip, int to)
{
	unsigned long timeout = jiffies + msecs_to_jiffies(3000); /* 3S */

	do {
		if (gpio_get_value(chip->gpio_status) == to)
			return 0;

		msleep(10);
	} while (time_before(jiffies, timeout));

	dev_err(&chip->spi->dev, "timeout to wating gpio_status to %d\n", to);

	return -ETIMEDOUT;
}

static int s3fv5rp_write(struct s3fv5rp_chip *chip, const u8 *buf, size_t len)
{
	int ret;

	ret = s3fv5rp_wating_status_gpio(chip, 0);
	if (ret < 0) {
		dev_err(&chip->spi->dev, "%s: timeout\n", __func__);
		return ret;
	}

	ret = spi_write(chip->spi, buf, len);
	if (ret < 0) {
		dev_err(&chip->spi->dev, "%s: error\n", __func__);
		return ret;
	}

	return 0;
}

static int s3fv5rp_read(struct s3fv5rp_chip *chip, u8 *buf, size_t buf_size)
{
	int ret;
	u8 len;

	ret = s3fv5rp_wating_status_gpio(chip, 0);
	if (ret < 0) {
		dev_err(&chip->spi->dev, "%s: timeout\n", __func__);
		return ret;
	}

	ret = spi_read(chip->spi, &len, 1);
	if (ret < 0) {
		dev_err(&chip->spi->dev, "%s: reading len error\n", __func__);
		return ret;
	}

	if (len > buf_size) {
		dev_err(&chip->spi->dev, "%s: buf_size too small\n", __func__);
		return -1;
	}

	ret = spi_read(chip->spi, buf, len);
	if (ret < 0) {
		dev_err(&chip->spi->dev, "%s: reading error\n", __func__);
		return ret;
	}

	return len;
}

static void s3fv5rp_set_fwmode(struct s3fv5rp_chip *chip, bool en)
{
	dev_dbg(&chip->spi->dev, "fwmode %s\n", en ? "enable" : "disable");
	gpio_direction_output(chip->gpio_status, en ? 1 : 0);
	msleep(100);

	dev_dbg(&chip->spi->dev, "GPIO Status = %d\n",
			gpio_get_value(chip->gpio_status));

	if (en == true) {
		gpio_direction_output(chip->gpio_reset, 1);
		msleep(100);
		gpio_direction_output(chip->gpio_reset, 0);
		msleep(100);
		gpio_direction_output(chip->gpio_status, en ? 1 : 0);
		msleep(100);
		gpio_direction_output(chip->gpio_reset, 1);
		msleep(100);
	} else {
		/* Reset device*/
		gpio_direction_output(chip->gpio_reset, 1);
		msleep(100);
		gpio_direction_output(chip->gpio_reset, 0);
		msleep(100);
		gpio_direction_output(chip->gpio_reset, 1);
		msleep(100);
	}

	/* Device wakeup */
	gpio_direction_output(chip->gpio_wakeup, 0);
	msleep(100);
	gpio_direction_output(chip->gpio_wakeup, 1);
	msleep(100);
	gpio_direction_output(chip->gpio_wakeup, 0);
	msleep(100);

	gpio_direction_input(chip->gpio_status);
}

static ssize_t s3fv5rp_sysfs_store_fwmode(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct s3fv5rp_chip *chip = dev_get_drvdata(dev);
	unsigned int value;
	int ret;

	ret = kstrtouint(buf, 10, &value);
	if (ret)
		return -EINVAL;

	s3fv5rp_set_fwmode(chip, value ? true : false);

	return count;
}

static void s3fv5rp_set_blmode(struct s3fv5rp_chip *chip, bool en)
{
	dev_dbg(&chip->spi->dev, "blmode %s\n", en ? "enable" : "disable");

	gpio_direction_output(chip->gpio_reset, 0);
	msleep(100);
	gpio_direction_output(chip->gpio_status, 1);
	msleep(100);
	gpio_direction_output(chip->gpio_reset, 1);
}


static ssize_t s3fv5rp_sysfs_store_blmode(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct s3fv5rp_chip *chip = dev_get_drvdata(dev);
	unsigned int value = 1;
	int ret;

	ret = kstrtouint(buf, 10, &value);
	if (ret)
		return -EINVAL;

	s3fv5rp_set_blmode(chip, value ? true : false);

	return count;
}

static ssize_t s3fv5rp_sysfs_store_vdd(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct s3fv5rp_chip *chip = dev_get_drvdata(dev);
	unsigned int value;
	int ret;

	ret = kstrtouint(buf, 10, &value);
	if (ret)
		return -EINVAL;

	if (!IS_ERR(chip->reg)) {
		if (value)
			ret = regulator_enable(chip->reg);
		else
			ret = regulator_disable(chip->reg);

		if (ret)
			return ret;
	}

	return count;
}

static ssize_t s3fv5rp_sysfs_show_data(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct s3fv5rp_chip *chip = dev_get_drvdata(dev);
	int ret;

	ret = s3fv5rp_read(chip, buf, 256);
	if (ret < 0) {
		dev_err(&chip->spi->dev, "failed to read\n");
		return ret;
	}

	return ret;
}

static ssize_t s3fv5rp_sysfs_store_data(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count)
{
	struct s3fv5rp_chip *chip = dev_get_drvdata(dev);
	int ret;

	if (count > 256)
		return -EINVAL;

	ret = s3fv5rp_write(chip, buf, count);
	if (ret < 0) {
		dev_err(&chip->spi->dev, "failed to write tx_data\n");
		return ret;
	}

	return count;
}

static ssize_t s3fv5rp_sysfs_show_ack(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct s3fv5rp_chip *chip = dev_get_drvdata(dev);
	u8 ack;
	int ret;

	ret = s3fv5rp_wating_status_gpio(chip, 1);
	if (ret < 0) {
		dev_err(&chip->spi->dev, "%s: timeout\n", __func__);
		return ret;
	}

	ret = spi_read(chip->spi, &ack, 1);
	if (ret < 0) {
		dev_err(&chip->spi->dev, "%s: read ack error\n", __func__);
		return ret;
	}

	return sprintf(buf, "0x%02x\n", ack);
}

static ssize_t s3fv5rp_sysfs_store_ack(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct s3fv5rp_chip *chip = dev_get_drvdata(dev);
	unsigned int value;
	int ret;

	if (count > 256)
		return -EINVAL;

	ret = s3fv5rp_wating_status_gpio(chip, 1);
	if (ret < 0) {
		dev_err(&chip->spi->dev, "%s: timeout\n", __func__);
		return ret;
	}

	ret = kstrtouint(buf, 16, &value);
	if (ret)
		return -EINVAL;

	ret = spi_write(chip->spi, &value, 1);
	if (ret < 0) {
		dev_err(&chip->spi->dev, "%s: error\n", __func__);
		return ret;
	}

	return count;
}

static ssize_t s3fv5rp_sysfs_show_gpio(struct device *dev,
		struct device_attribute *attr, char *buf);
static ssize_t s3fv5rp_sysfs_store_gpio(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count);

static DEVICE_ATTR(fwmode, 0220, NULL, s3fv5rp_sysfs_store_fwmode);
static DEVICE_ATTR(blmode, 0220, NULL, s3fv5rp_sysfs_store_blmode);
static DEVICE_ATTR(vdd, 0220, NULL, s3fv5rp_sysfs_store_vdd);
static DEVICE_ATTR(gpio_wakeup, 0660,
		s3fv5rp_sysfs_show_gpio, s3fv5rp_sysfs_store_gpio);
static DEVICE_ATTR(gpio_status, 0660,
		s3fv5rp_sysfs_show_gpio, s3fv5rp_sysfs_store_gpio);
static DEVICE_ATTR(gpio_reset, 0660,
		s3fv5rp_sysfs_show_gpio, s3fv5rp_sysfs_store_gpio);
static DEVICE_ATTR(data, 0660,
		s3fv5rp_sysfs_show_data, s3fv5rp_sysfs_store_data);
static DEVICE_ATTR(ack, 0660,
		s3fv5rp_sysfs_show_ack, s3fv5rp_sysfs_store_ack);

static const struct attribute *s3fv5rp_attrs[] = {
	&dev_attr_fwmode.attr,
	&dev_attr_blmode.attr,
	&dev_attr_vdd.attr,
	&dev_attr_gpio_wakeup.attr,
	&dev_attr_gpio_status.attr,
	&dev_attr_gpio_reset.attr,
	&dev_attr_data.attr,
	&dev_attr_ack.attr,
	NULL,
};

static ssize_t s3fv5rp_sysfs_show_gpio(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct s3fv5rp_chip *chip = dev_get_drvdata(dev);
	int gpio;

	if (attr == &dev_attr_gpio_wakeup)
		gpio = chip->gpio_wakeup;
	else if (attr == &dev_attr_gpio_status)
		gpio = chip->gpio_status;
	else if (attr == &dev_attr_gpio_reset)
		gpio = chip->gpio_reset;
	else
		gpio = -EINVAL;

	if (gpio < 0) {
		dev_err(&chip->spi->dev, "invalid gpio %d\n", gpio);
		return gpio;
	}

	gpio_direction_input(gpio);

	return sprintf(buf, "%d\n", gpio_get_value(gpio));
}

static ssize_t s3fv5rp_sysfs_store_gpio(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct s3fv5rp_chip *chip = dev_get_drvdata(dev);
	unsigned int value;
	int ret = 0, gpio;

	if (attr == &dev_attr_gpio_wakeup)
		gpio = chip->gpio_wakeup;
	else if (attr == &dev_attr_gpio_status)
		gpio = chip->gpio_status;
	else if (attr == &dev_attr_gpio_reset)
		gpio = chip->gpio_reset;
	else
		gpio = -EINVAL;

	if (gpio < 0) {
		dev_err(&chip->spi->dev, "invalid gpio %d\n", gpio);
		return gpio;
	}

	ret = kstrtouint(buf, 10, &value);
	if (ret)
		return -EINVAL;

	gpio_direction_output(gpio, value ? 1 : 0);

	return count;
}

static int s3fv5rp_of_get_gpio(struct s3fv5rp_chip *chip, const char *lable,
		unsigned long flag)
{
	struct spi_device *spi = chip->spi;
	struct device_node *np = spi->dev.of_node;
	int pin, ret;

	pin = of_get_named_gpio(np, lable, 0);
	if (!gpio_is_valid(pin)) {
		dev_err(&spi->dev, "%s property not found\n", lable);
		return -EINVAL;
	}

	ret = devm_gpio_request_one(&spi->dev, pin, flag, lable);
	if (ret) {
		dev_err(&spi->dev, "failed to set direction gpio %s\n", lable);
		return ret;
	}

	return pin;
}

int s3fv5rp_parse_dt(struct s3fv5rp_chip *chip)
{
	struct spi_device *spi = chip->spi;

	if (!spi->dev.of_node) {
		dev_err(&spi->dev, "failed to read device tree\n");
		return -EINVAL;
	}

	chip->gpio_wakeup = s3fv5rp_of_get_gpio(chip,
			"gpio-wakeup", GPIOF_OUT_INIT_LOW);
	if (chip->gpio_wakeup < 0) {
		dev_err(&spi->dev, "failed to get gpio-wakeup\n");
		return chip->gpio_wakeup;
	}

	chip->gpio_status = s3fv5rp_of_get_gpio(chip,
			"gpio-status", GPIOF_INIT_LOW);
	if (chip->gpio_status < 0) {
		dev_err(&spi->dev, "failed to get gpio-status\n");
		return chip->gpio_status;
	}

	chip->gpio_reset = s3fv5rp_of_get_gpio(chip,
			"gpio-reset", GPIOF_OUT_INIT_HIGH);
	if (chip->gpio_reset < 0) {
		dev_err(&spi->dev, "failed to get gpio-reset\n");
		return chip->gpio_reset;
	}

	return 0;
}

static int s3fv5rp_probe(struct spi_device *spi)
{
	struct s3fv5rp_chip *chip;
	int ret;

	chip = devm_kzalloc(&spi->dev, sizeof(*chip), GFP_KERNEL);
	if (!chip)
		return -ENOMEM;

	chip->spi = spi;
	spi_set_drvdata(spi, chip);

	ret = s3fv5rp_parse_dt(chip);
	if (ret < 0)
		return ret;

	spi->bits_per_word = 8;
	ret = spi_setup(spi);
	if (ret < 0) {
		dev_err(&spi->dev, "failed to setup spi device\n");
		return ret;
	}

	chip->reg = devm_regulator_get(&spi->dev, "vdd_se");
	if (IS_ERR(chip->reg))
		dev_err(&spi->dev, "failed to get regulator\n");
	else {
		ret = regulator_enable(chip->reg);
		if (ret) {
			dev_err(&spi->dev, "failed to enable regulator\n");
		}
	}

	chip->se_class = class_create(THIS_MODULE, "se");
	if (IS_ERR(chip->se_class)) {
		dev_err(&spi->dev, "failed to create se_class\n");
		return PTR_ERR(chip->se_class);
	}

	chip->se_dev = device_create(chip->se_class, NULL, 0, "%s", "se0");
	if (IS_ERR(chip->se_dev)) {
		dev_err(&spi->dev, "failed to create se_dev\n");
		ret = PTR_ERR(chip->se_dev);
		goto err_device;
	}

	ret = sysfs_create_files(&chip->se_dev->kobj, s3fv5rp_attrs);
	if (ret) {
		dev_err(&spi->dev, "failed to create sysfs files\n");
		goto err_sysfs;
	}

	dev_set_drvdata(chip->se_dev, chip);

	/* set to general mode */
	s3fv5rp_set_fwmode(chip, false);

	return 0;

err_sysfs:
	device_destroy(chip->se_class, 0);
err_device:
	class_destroy(chip->se_class);

	return ret;
}

static int s3fv5rp_remove(struct spi_device *spi)
{
	struct s3fv5rp_chip *chip = spi_get_drvdata(spi);

	sysfs_remove_files(&spi->dev.kobj, s3fv5rp_attrs);
	device_destroy(chip->se_class, 0);
	class_destroy(chip->se_class);

	if (!IS_ERR(chip->reg))
		regulator_disable(chip->reg);

	return 0;
}

static const struct of_device_id s3fv5rp_dt_ids[] = {
	{ .compatible = "samsung,s3fv5rp" },
	{},
};
MODULE_DEVICE_TABLE(of, s3fv5rp_dt_ids);

static struct spi_driver s3fv5rp_driver = {
	.driver = {
		.name		= "samsung,s3fv5rp",
		.owner		= THIS_MODULE,
		.of_match_table	= of_match_ptr(s3fv5rp_dt_ids),
	},
	.probe		= s3fv5rp_probe,
	.remove		= s3fv5rp_remove,
};
module_spi_driver(s3fv5rp_driver);

MODULE_AUTHOR("Homin Lee <suapapa@insignal.co.kr>");
MODULE_DESCRIPTION("s3fv5rp driver");
MODULE_LICENSE("GPL v2");
