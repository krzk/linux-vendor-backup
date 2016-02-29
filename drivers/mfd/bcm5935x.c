/*
 *  bcm5935x.c
 *  mfd core driver for BCM 5935X
 *
 *  Copyright (C) 2012 Samsung Electronics
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#undef DEBUG

#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/mutex.h>
#include <linux/module.h>
#include <linux/mfd/core.h>
#include <linux/uaccess.h>
#include <linux/input.h>

#include <linux/battery/bcm5935x.h>

int bcm5935x_read_reg(struct i2c_client *i2c, int reg)
{
	struct bcm5935x_dev *bcm5935x = i2c_get_clientdata(i2c);
	int ret;

	mutex_lock(&bcm5935x->i2c_lock);
	ret = i2c_smbus_read_byte_data(i2c, reg);
	mutex_unlock(&bcm5935x->i2c_lock);

	dev_dbg(&i2c->dev, "%s:reg[0x%x],val[0x%x]\n", __func__, reg, ret);
	if (ret < 0) {
		/* retry once */
		mutex_lock(&bcm5935x->i2c_lock);
		ret = i2c_smbus_read_byte_data(i2c, reg);
		mutex_unlock(&bcm5935x->i2c_lock);

		if (ret < 0)
			dev_err(&i2c->dev, "%s: err %d\n", __func__, ret);
	}

	return ret;
}

int bcm5935x_write_reg(struct i2c_client *i2c, int reg, int value)
{
	struct bcm5935x_dev *bcm5935x = i2c_get_clientdata(i2c);
	int ret;

	mutex_lock(&bcm5935x->i2c_lock);
	ret = i2c_smbus_write_byte_data(i2c, reg, value);
	mutex_unlock(&bcm5935x->i2c_lock);

	dev_dbg(&i2c->dev, "%s:reg[0x%x],val[0x%x]", __func__, reg, value);
	if (ret < 0) {
		/* retry once */
		mutex_lock(&bcm5935x->i2c_lock);
		ret = i2c_smbus_write_byte_data(i2c, reg, value);
		mutex_unlock(&bcm5935x->i2c_lock);
		if (ret < 0)
			dev_err(&i2c->dev, "%s: err %d\n", __func__, ret);
	}
	return ret;
}

static struct mfd_cell bcm5935x_devs[] = {
	{ .name = "bcm5935x_wpc_low", },
	{ .name = "bcm5935x_wpc_high", },

};

static int __devinit bcm5935x_i2c_probe(struct i2c_client *i2c,
						const struct i2c_device_id *id)
{
	struct bcm5935x_dev *bcm5935x;
	struct bcm5935x_platform_data *pdata = i2c->dev.platform_data;
	
	int ret = 0;
	
	pr_info("%s: BCM5935X i2c Driver Loading\n", __func__);

	bcm5935x = kzalloc(sizeof(struct bcm5935x_dev), GFP_KERNEL);
	if (bcm5935x == NULL)
		return -ENOMEM;

	i2c->adapter->retries = 0;
	
	i2c_set_clientdata(i2c, bcm5935x);
	bcm5935x->dev = &i2c->dev;
	bcm5935x->i2c_low = i2c;

	bcm5935x->is_wpc_ready = false;

	mutex_init(&bcm5935x->i2c_lock);
	
	bcm5935x->i2c_high = i2c_new_dummy(i2c->adapter, SEC_WPC_I2C_SLAVEADDR2);
	i2c_set_clientdata(bcm5935x->i2c_high, bcm5935x);

	if (pdata) {
		bcm5935x->pdata = pdata;
	} else {
		ret = -EIO;
		goto err1;
	}

	ret = mfd_add_devices(bcm5935x->dev, -1, bcm5935x_devs,
				ARRAY_SIZE(bcm5935x_devs), NULL, 0);
	if (ret < 0)
		goto err1;
	
	return ret;
	
	err1:
		if (bcm5935x->i2c_high)
			i2c_unregister_device(bcm5935x->i2c_high);

		kfree(bcm5935x);
		return ret;
}

static int __devexit bcm5935x_i2c_remove(
						struct i2c_client *client)
{
	return 0;
}

static int bcm5935x_i2c_suspend(struct device *dev)
{
	return 0;
}

static int bcm5935x_i2c_resume(struct device *dev)
{
	return 0;
}

static void bcm5935x_i2c_shutdown(struct i2c_client *client)
{
	dev_info(&client->dev, "%s\n", __func__);

}

static const struct i2c_device_id bcm5935x_i2c_id[] = {
	{"bcm5935x", 0},
	{}
};

static const struct dev_pm_ops bcm5935x_i2c_pm_ops = {
	.suspend = bcm5935x_i2c_suspend,
	.resume  = bcm5935x_i2c_resume,
};

MODULE_DEVICE_TABLE(i2c, bcm5935x_i2c_id);

static struct i2c_driver bcm5935x_i2c_driver = {
	.driver = {
		   .name = "bcm5935x",
			.owner = THIS_MODULE,
#ifdef CONFIG_PM
			.pm = &bcm5935x_i2c_pm_ops,
#endif
		   },
	.probe	= bcm5935x_i2c_probe,
	.remove	= __devexit_p(bcm5935x_i2c_remove),
	.shutdown   = bcm5935x_i2c_shutdown,
	.id_table   = bcm5935x_i2c_id,
};

static int __init bcm5935x_i2c_init(void)
{
	pr_info("%s\n", __func__);
	return i2c_add_driver(&bcm5935x_i2c_driver);
}
/* init early so consumer devices can complete system boot */
subsys_initcall(bcm5935x_i2c_init);

static void __exit bcm5935x_i2c_exit(void)
{
	i2c_del_driver(&bcm5935x_i2c_driver);
}
module_exit(bcm5935x_i2c_exit);

MODULE_DESCRIPTION("BCM 5935X multi-function core driver");
MODULE_AUTHOR("Samsung Electronics");
MODULE_LICENSE("GPL");
