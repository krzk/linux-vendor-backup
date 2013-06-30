/*
 * Copyright (c) 2011 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * CEC Support for Samsung S5P TVOUT
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/poll.h>
#include <linux/miscdevice.h>

// Modified includes:
#include <linux/sched.h> 
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/clk.h>


struct s5p_platform_cec {
	void	(*cfg_gpio)(struct platform_device *pdev);
};

#include "cec_hw.h"


MODULE_AUTHOR("KyungHwan Kim <kh.k.kim@samsung.com>");
MODULE_DESCRIPTION("Samsung S5P CEC driver");
MODULE_LICENSE("GPL");

#define CEC_IOC_MAGIC			'c'
#define CEC_IOC_SETLADDR		_IOW(CEC_IOC_MAGIC, 0, unsigned int)

/* /dev/cec (Major 10, Minor 242) */
#define CEC_MINOR			242

#define CEC_STATUS_TX_DONE		(1 << 2)
#define CEC_STATUS_TX_ERROR		(1 << 3)
#define CEC_STATUS_RX_DONE		(1 << 18)
#define CEC_STATUS_RX_ERROR		(1 << 19)

#define CEC_TX_BUFF_SIZE		16

static atomic_t hdmi_on = ATOMIC_INIT(0);
static DEFINE_MUTEX(cec_lock);
//struct clk *hdmi_cec_clk;

static int s5p_cec_open(struct inode *inode, struct file *file)
{
	int ret = 0;

	mutex_lock(&cec_lock);
	//clk_enable(hdmi_cec_clk);

	if (atomic_read(&hdmi_on)) {
		tvout_dbg("do not allow multiple open for tvout cec\n");
		ret = -EBUSY;
		goto err_multi_open;
	} else
		atomic_inc(&hdmi_on);

	s5p_cec_reset();

	s5p_cec_set_divider();

	s5p_cec_threshold();

	s5p_cec_unmask_tx_interrupts();

	s5p_cec_set_rx_state(STATE_RX);
	s5p_cec_unmask_rx_interrupts();
	s5p_cec_enable_rx();

err_multi_open:
	mutex_unlock(&cec_lock);

	return ret;
}

static int s5p_cec_release(struct inode *inode, struct file *file)
{
	atomic_dec(&hdmi_on);

	s5p_cec_mask_tx_interrupts();
	s5p_cec_mask_rx_interrupts();

	//clk_disable(hdmi_cec_clk);
	//clk_put(hdmi_cec_clk);

	return 0;
}

static ssize_t s5p_cec_read(struct file *file, char __user *buffer,
			size_t count, loff_t *ppos)
{
	ssize_t retval;
	unsigned long spin_flags;

	if (wait_event_interruptible(cec_rx_struct.waitq,
			atomic_read(&cec_rx_struct.state) == STATE_DONE)) {
		return -ERESTARTSYS;
	}
	spin_lock_irqsave(&cec_rx_struct.lock, spin_flags);

	if (cec_rx_struct.size > count) {
		spin_unlock_irqrestore(&cec_rx_struct.lock, spin_flags);

		return -1;
	}

	if (copy_to_user(buffer, cec_rx_struct.buffer, cec_rx_struct.size)) {
		spin_unlock_irqrestore(&cec_rx_struct.lock, spin_flags);
		printk(KERN_ERR " copy_to_user() failed!\n");

		return -EFAULT;
	}

	retval = cec_rx_struct.size;

	s5p_cec_set_rx_state(STATE_RX);
	spin_unlock_irqrestore(&cec_rx_struct.lock, spin_flags);

	return retval;
}


static ssize_t s5p_cec_write(struct file *file, const char __user *buffer,
			size_t count, loff_t *ppos)
{
	char *data;

	/* check data size */

	if (count > CEC_TX_BUFF_SIZE || count == 0)
		return -1;

	data = kmalloc(count, GFP_KERNEL);

	if (!data) {
		printk(KERN_ERR " kmalloc() failed!\n");

		return -1;
	}

	if (copy_from_user(data, buffer, count)) {
		printk(KERN_ERR " copy_from_user() failed!\n");
		kfree(data);

		return -EFAULT;
	}

	s5p_cec_copy_packet(data, count);

	kfree(data);

	/* wait for interrupt */
	if (wait_event_interruptible(cec_tx_struct.waitq,
		atomic_read(&cec_tx_struct.state)
		!= STATE_TX)) {

		return -ERESTARTSYS;
	}

	if (atomic_read(&cec_tx_struct.state) == STATE_ERROR)
		return -1;

	return count;
}

static long s5p_cec_ioctl(struct file *file, unsigned int cmd,
			  unsigned long arg)
{
	u32 laddr;
	
	printk("s5p_cec_read, cmd = %u, arg = %lu", cmd, arg);

	switch (cmd) {
	case CEC_IOC_SETLADDR:
		if (get_user(laddr, (u32 __user *) arg))
			return -EFAULT;

		tvout_dbg("logical address = 0x%02x\n", laddr);

		s5p_cec_set_addr(laddr);
		break;

	default:
		return -EINVAL;
	}

	return 0;
}

static u32 s5p_cec_poll(struct file *file, poll_table *wait)
{
	poll_wait(file, &cec_rx_struct.waitq, wait);

	if (atomic_read(&cec_rx_struct.state) == STATE_DONE)
		return POLLIN | POLLRDNORM;

	return 0;
}

static const struct file_operations cec_fops = {
	.owner			= THIS_MODULE,
	.open			= s5p_cec_open,
	.release		= s5p_cec_release,
	.read			= s5p_cec_read,
	.write			= s5p_cec_write,
	.unlocked_ioctl		= s5p_cec_ioctl,
	.poll			= s5p_cec_poll,
};

static struct miscdevice cec_misc_device = {
	.minor			= CEC_MINOR,
	.name			= "CEC",
	.fops			= &cec_fops,
};

static irqreturn_t s5p_cec_irq_handler(int irq, void *dev_id)
{
	u32 status = s5p_cec_get_status();

	if (status & CEC_STATUS_TX_DONE) {
		if (status & CEC_STATUS_TX_ERROR) {
			tvout_dbg(" CEC_STATUS_TX_ERROR!\n");
			s5p_cec_set_tx_state(STATE_ERROR);
		} else {
			tvout_dbg(" CEC_STATUS_TX_DONE!\n");
			s5p_cec_set_tx_state(STATE_DONE);
		}

		s5p_clr_pending_tx();

		wake_up_interruptible(&cec_tx_struct.waitq);
	}

	if (status & CEC_STATUS_RX_DONE) {
		if (status & CEC_STATUS_RX_ERROR) {
			tvout_dbg(" CEC_STATUS_RX_ERROR!\n");
			s5p_cec_rx_reset();

		} else {
			u32 size;

			tvout_dbg(" CEC_STATUS_RX_DONE!\n");

			/* copy data from internal buffer */
			size = status >> 24;

			spin_lock(&cec_rx_struct.lock);

			s5p_cec_get_rx_buf(size, cec_rx_struct.buffer);

			cec_rx_struct.size = size;

			s5p_cec_set_rx_state(STATE_DONE);

			spin_unlock(&cec_rx_struct.lock);

			s5p_cec_enable_rx();
		}

		/* clear interrupt pending bit */
		s5p_clr_pending_rx();

		wake_up_interruptible(&cec_rx_struct.waitq);
	}
	return IRQ_HANDLED;
}

static int s5p_cec_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct s5p_platform_cec *pdata;
	u8 *buffer = NULL;
	int irq_num;
	int ret = 0;
	
	dev_info(dev, "probe start\n");

	pdata = to_tvout_plat(&pdev->dev);
	if (pdata && pdata->cfg_gpio)
	{
		printk(KERN_INFO "s5p_cec_probe: pdata=%p\n", pdata);
		pdata->cfg_gpio(pdev);
	}

	/* get ioremap addr */
	ret = s5p_cec_mem_probe(pdev);
	if (ret != 0) {
		printk(KERN_ERR  "failed to s5p_cec_mem_probe ret = %d\n", ret);
		goto err_mem_probe;
	}

	if (misc_register(&cec_misc_device)) {
		printk(KERN_WARNING " Couldn't register device 10, %d.\n", CEC_MINOR);
		ret = -EBUSY;
		goto err_misc_register;
	}

	irq_num = platform_get_irq(pdev, 0);
	if (irq_num < 0) {
		printk(KERN_ERR  "failed to get %s irq resource\n", "cec");
		ret = -EBUSY;
		goto err_get_irq;
	}

	if (request_irq(irq_num, s5p_cec_irq_handler, IRQF_DISABLED, pdev->name, &pdev->id)) {
		printk(KERN_ERR  "failed to install %s irq (%d)\n", "cec", ret);
		ret = -EBUSY;
		goto err_request_irq;
	}

	init_waitqueue_head(&cec_rx_struct.waitq);
	spin_lock_init(&cec_rx_struct.lock);
	init_waitqueue_head(&cec_tx_struct.waitq);

	buffer = kmalloc(CEC_TX_BUFF_SIZE, GFP_KERNEL);
	if (!buffer) {
		printk(KERN_ERR " kmalloc() failed!\n");
		misc_deregister(&cec_misc_device);
		ret = -EIO;
		goto err_kmalloc;
	}

	cec_rx_struct.buffer = buffer;
	cec_rx_struct.size   = 0;
	
#if 0	// Someone on the internet does this, most people don't:
	clk = clk_get(&pdev->dev, clk_name);
	if (IS_ERR(clk)) {
		printk(KERN_ERR "failed to find clock %s\n", clk_name);
		return -ENOENT;
	}
#endif

err_kmalloc:
	free_irq(irq_num, NULL);
err_request_irq:
err_get_irq:
	misc_deregister(&cec_misc_device);
err_misc_register:
err_mem_probe:
	return ret;
}


static int s5p_cec_remove(struct platform_device *pdev)
{
	int irq_num = platform_get_irq(pdev, 0);
	printk(KERN_INFO "s5p_cec_remove, irq=%i\n", irq_num); 

	free_irq(irq_num, NULL);
	misc_deregister(&cec_misc_device);

	return 0;
}

#ifdef CONFIG_PM
static int s5p_cec_suspend(struct platform_device *dev, pm_message_t state)
{
	printk(KERN_INFO "s5p_cec_suspend is a NOP\n");
	return 0;
}

static int s5p_cec_resume(struct platform_device *dev)
{
	printk(KERN_INFO "s5p_cec_resume is a NOP\n");
	return 0;
}

#else
#define s5p_cec_suspend NULL
#define s5p_cec_resume NULL
#endif

#if 0
static struct platform_device_id hdmi_driver_types[] = {
	{
		.name		= "s5pv210-hdmi",
	}, {
		.name		= "exynos4-hdmi",
	}, {
		/* end node */
	}
};
#endif

static struct platform_driver s5p_cec_driver = {
	.probe		= s5p_cec_probe,
	.remove		= s5p_cec_remove,
	//.id_table   = hdmi_driver_types,	// Shouldn't be necessary
	.suspend	= s5p_cec_suspend,
	.resume		= s5p_cec_resume,
	.driver		= {
		.name		= "s5p-cec",
		.owner		= THIS_MODULE,
	},
};

#if 0
	// The rest of the modules in s5p-tv do this:
	// It makes debugging the probe a bit trickier, so don't do this now
	module_platform_driver(s5p_cec_driver);
#else
	static int __init s5p_cec_init(void)
	{
		printk(KERN_INFO "S5P CEC for TVOUT Driver, Copyright (c) 2011 Samsung Electronics Co., LTD.\n");
		request_module("s5p-hdmi");
		return platform_driver_register(&s5p_cec_driver);
	}

	static void __exit s5p_cec_exit(void)
	{
		printk(KERN_INFO "S5P CEC for TVOUT Driver, exiting\n");
		kfree(cec_rx_struct.buffer);
		platform_driver_unregister(&s5p_cec_driver);
	}

	module_init(s5p_cec_init);
	module_exit(s5p_cec_exit);
#endif
