/*
 *
 * Copyright (C) 2013 Samsung Electronics Co.Ltd
 * Authors:
 *	Eunchul Kim <chulspro.kim@samsung.com>
 *	Sangmin Lee <lsmin.lee@samsung.com>
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 *
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fb.h>
#include <linux/dispfreq.h>
#include <linux/gcd.h>

#define dispfreq_info(dev, fmt, args...) \
	do { \
		dev_info(dev, "pid: %d, %s:"fmt"\n",\
			task_pid_nr(current), __func__, ##args); \
	} while (0);

#define dispfreq_err(dev, fmt, args...) \
	do { \
		dev_err(dev, "pid: %d, %s:"fmt"\n",\
			task_pid_nr(current), __func__, ##args); \
	} while (0);

/*
 * Display frequency supports various clock frequency level.
 * If user application set the refresh rate, dispfreq driver calculate
 * right clock frequency of display controller.
 */

static struct class *dispfreq_class;

static int dispfreq_calculate_vscr(struct dispfreq_device *dfd)
{
	struct dispfreq_properties *props = &dfd->props;
	struct fb_videomode *timing = props->timing;
	u32 vscr, hsize, vsize;

	if (timing->i80en) {
		vscr = timing->xres * timing->yres *
		    (timing->cs_setup_time +
		    timing->wr_setup_time + timing->wr_act_time +
		    timing->wr_hold_time + 1);
		/*
		 * Basically, the refresh rate of TE and vsync is 60Hz.
		 * In case of using i80 lcd panel, we need to do fimd
		 * trigger so that graphics ram in the lcd panel to be
		 * updated.
		 *
		 * And, we do fimd trigger every time TE interrupt
		 * handler occurs to resolve tearing issue on the lcd
		 * panel. However, this way doesn't avoid the tearing
		 * issue because fimd i80 frame done interrupt doesn't
		 * occur since fimd trigger before next TE interrupt.
		 *
		 * So makes video clock speed up two times so that the
		 * fimd i80 frame done interrupt can occur prior to
		 * next TE interrupt.
		*/

		vscr *= 2;

	} else {
		hsize = timing->left_margin + timing->right_margin +
			timing->hsync_len + timing->xres;
		vsize = timing->upper_margin + timing->lower_margin +
			timing->vsync_len + timing->yres;

		vscr = hsize * vsize;
	}

	return vscr;
}

static void dfd_device_release(struct device *dev)
{
	struct dispfreq_device *dfd = to_dispfreq_device(dev);
	kfree(dfd);
}

struct dispfreq_device *dispfreq_device_register(const char *name,
		struct device *parent_dev, void *devdata,
		const struct dispfreq_ops *ops,
		const struct dispfreq_properties *props)
{
	struct dispfreq_device *dfd;
	int ret;

	dfd = kzalloc(sizeof(struct dispfreq_device), GFP_KERNEL);
	if (!dfd)
		return ERR_PTR(-ENOMEM);

	mutex_init(&dfd->ops_lock);

	dfd->dev.class = dispfreq_class;
	dfd->dev.parent = parent_dev;
	dfd->dev.release = dfd_device_release;

	dev_set_name(&dfd->dev, name);
	dev_set_drvdata(&dfd->dev, devdata);

	if (props)
		memcpy(&dfd->props, props, sizeof(struct dispfreq_properties));

	dfd->vscr = dispfreq_calculate_vscr(dfd);

	ret = device_register(&dfd->dev);
	if (ret) {
		kfree(dfd);
		return ERR_PTR(ret);
	}

	dfd->ops = ops;

	return dfd;
}
EXPORT_SYMBOL(dispfreq_device_register);

void dispfreq_device_unregister(struct dispfreq_device *dfd)
{
	if (!dfd)
		return;

	mutex_lock(&dfd->ops_lock);
	dfd->ops = NULL;
	mutex_unlock(&dfd->ops_lock);

	device_unregister(&dfd->dev);
}
EXPORT_SYMBOL(dispfreq_device_unregister);

static int dispfreq_update_clk(struct dispfreq_device *dfd)
{
	const struct dispfreq_ops *ops = dfd->ops;
	struct dispfreq_properties *props = &dfd->props;
	struct fb_videomode *timing = props->timing;
	struct clksrc_clk *clksrc;
	struct clk *clk, *parent_clk;
	unsigned long clk_rate;
	u32 vclk, cmu_div, fimd_div;
	int ret = 0;

	clk = ops->get_clk(dfd);
	clksrc = container_of(clk, struct clksrc_clk, clk);

	parent_clk = clk_get_parent(&clksrc->clk);
	if (!parent_clk) {
		ret = -EFAULT;
		dispfreq_err(&dfd->dev, "failed to get parent %s.",
			clksrc->clk.name);
		goto out;
	}

	vclk = dfd->vscr * props->refresh;
	clk_rate = clk_get_rate(parent_clk);
	cmu_div = DIV_ROUND_CLOSEST(clk_rate, vclk);
	fimd_div = ops->get_fimd_div(dfd) + 1;

	cmu_div /= fimd_div;
	cmu_div -= 1;
	cmu_div = (cmu_div << clksrc->reg_div.size) | cmu_div;

	ret = ops->set_clk(dfd, clksrc, cmu_div);
	if (ret) {
		dispfreq_err(&dfd->dev, "failed to set clock.");
		ret = -EFAULT;
		goto out;
	}

	timing->pixclock = KHZ2PICOS(vclk / 1000);
	timing->refresh = props->refresh;
out:
	return ret;
}

static ssize_t dispfreq_show_min_refresh(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct dispfreq_device *dfd = to_dispfreq_device(dev);
	struct dispfreq_properties *props = &dfd->props;

	return sprintf(buf, "%d\n", props->min_refresh);
}

static ssize_t dispfreq_show_max_refresh(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct dispfreq_device *dfd = to_dispfreq_device(dev);
	struct dispfreq_properties *props = &dfd->props;

	return sprintf(buf, "%d\n", props->max_refresh);
}

static ssize_t dispfreq_show_refresh(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct dispfreq_device *dfd = to_dispfreq_device(dev);
	const struct dispfreq_ops *ops = dfd->ops;

	if (ops->get_refresh)
		return sprintf(buf, "%d\n", ops->get_refresh(dfd));
	else
		return -EINVAL;
}

static ssize_t dispfreq_store_refresh(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct dispfreq_device *dfd = to_dispfreq_device(dev);
	struct dispfreq_properties *props = &dfd->props;
	const struct dispfreq_ops *ops = dfd->ops;
	unsigned long refresh;
	u32 old_refresh;
	int ret;

	ret = kstrtoul(buf, 0, &refresh);
	if (ret)
		return ret;

	dispfreq_info(dev, "refresh[%ld]", refresh);

	ret = ops->get_pm_state(dfd);
	if (!ret) {
		dispfreq_err(&dfd->dev, "pm state is off.");
		goto input_err;
	}

	if ((refresh > props->max_refresh) || (refresh < props->min_refresh)) {
		dispfreq_err(dev, "refresh should be %d to %d.",
			props->min_refresh, props->max_refresh);
		goto input_err;
	}

	if (props->refresh == refresh)
		goto out;

	mutex_lock(&dfd->ops_lock);
	old_refresh = props->refresh;
	props->refresh = refresh;

	ret = dispfreq_update_clk(dfd);
	if (ret) {
		props->refresh = old_refresh;
		dispfreq_err(dev, "failed to update clock.");
		goto update_err;
	}

	mutex_unlock(&dfd->ops_lock);

out:
	return count;
update_err:
	mutex_unlock(&dfd->ops_lock);
input_err:
	return -EINVAL;
}

static struct device_attribute dfd_device_attributes[] = {
	__ATTR(refresh, S_IRUGO|S_IWUSR,
		dispfreq_show_refresh, dispfreq_store_refresh),
	__ATTR(min_refresh, S_IRUGO,
		dispfreq_show_min_refresh, NULL),
	__ATTR(max_refresh, S_IRUGO,
		dispfreq_show_max_refresh, NULL),
	__ATTR_NULL,
};

static int dispfreq_suspend(struct device *dev, pm_message_t state)
{
	struct dispfreq_device *dfd = to_dispfreq_device(dev);

	mutex_lock(&dfd->ops_lock);

	/* TODO */

	mutex_unlock(&dfd->ops_lock);

	return 0;
}

static int dispfreq_resume(struct device *dev)
{
	struct dispfreq_device *dfd = to_dispfreq_device(dev);

	mutex_lock(&dfd->ops_lock);

	/* TODO */

	mutex_unlock(&dfd->ops_lock);

	return 0;
}

static void __exit dispfreq_class_exit(void)
{
	class_destroy(dispfreq_class);
}

static int __init dispfreq_class_init(void)
{
	dispfreq_class = class_create(THIS_MODULE, "dispfreq");
	if (IS_ERR(dispfreq_class)) {
		printk(KERN_WARNING "Unable to create dispfreq class; errno = %ld\n",
				PTR_ERR(dispfreq_class));
		return PTR_ERR(dispfreq_class);
	}

	dispfreq_class->dev_attrs = dfd_device_attributes;
	dispfreq_class->suspend = dispfreq_suspend;
	dispfreq_class->resume = dispfreq_resume;

	return 0;
}

postcore_initcall(dispfreq_class_init);
module_exit(dispfreq_class_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Eunchul Kim <chulspro.kim@samsung.com>");
MODULE_AUTHOR("Sangmin Lee <lsmin.lee@samsung.com>");
MODULE_DESCRIPTION("Display Frequency Control Abstraction");
