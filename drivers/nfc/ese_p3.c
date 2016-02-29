/*
 * Copyright (C) 2015 Samsung Electronics. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

 /**
 * \addtogroup spi_driver for Oberthur ESE P3
 *
 * @{ */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/list.h>
#include <linux/irq.h>
#include <linux/jiffies.h>
#include <linux/uaccess.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/platform_device.h>
#include <linux/gpio.h>
#include <linux/miscdevice.h>
#include <linux/spinlock.h>
#include <linux/sched.h>
#include <linux/poll.h>
#include <linux/regulator/consumer.h>
#include <linux/ioctl.h>
#ifdef CONFIG_OF
#include <linux/of_gpio.h>
#endif
#include <linux/ese_p3.h>

#include <linux/wakelock.h>
#include <plat/gpio-cfg.h>



/* Device driver's configuration macro */
/* Macro to configure poll/interrupt based req*/
#undef P3_IRQ_ENABLE
//#define P3_IRQ_ENABLE

/* Macro to configure reset to p3*/
#undef P3_RST_ENABLE
//#define P3_RST_ENABLE

/* Macro to configure Hard/Soft reset to P3 */
//#define P3_HARD_RESET
#undef P3_HARD_RESET

#ifdef P3_HARD_RESET
static struct regulator *p3_regulator = NULL;
#else
#endif

#undef PANIC_DEBUG

#define P3_IRQ   33 /* this is the same used in omap3beagle.c */
#define P3_RST  138

#define P3_SPI_CLOCK     8000000;

/* size of maximum read/write buffer supported by driver */
#define MAX_BUFFER_SIZE   259U

/* static struct class *p3_device_class; */

/* Different driver debug lever */
enum P3_DEBUG_LEVEL {
    P3_DEBUG_OFF,
    P3_FULL_DEBUG
};

/* Variable to store current debug level request by ioctl */
static unsigned char debug_level = P3_FULL_DEBUG;

#define P3_DBG_MSG(msg...)  \
        switch(debug_level)      \
        {                        \
        case P3_DEBUG_OFF:      \
        break;                 \
        case P3_FULL_DEBUG:     \
        printk(KERN_INFO "[ESE-P3] :  " msg); \
        break; \
        default:                 \
        printk(KERN_ERR "[ESE-P3] :  Wrong debug level %d", debug_level); \
        break; \
        } \

#define P3_ERR_MSG(msg...) printk(KERN_ERR "[ESE-P3] : " msg );

/* Device specific macro and structure */
struct p3_dev {
	wait_queue_head_t read_wq; /* wait queue for read interrupt */
	struct mutex buffer_mutex; /* buffer mutex */
	struct platform_device *dev;  /* platform device structure */
	struct miscdevice p3_device; /* char device as misc driver */
	unsigned int rst_gpio; /* SW Reset gpio */
	unsigned int irq_gpio; /* P3 will interrupt DH for any ntf */
	bool irq_enabled; /* flag to indicate irq is used */
	unsigned char enable_poll_mode; /* enable the poll mode */
	spinlock_t irq_enabled_lock; /*spin lock for read irq */

	unsigned char *null_buffer;
	unsigned char *buffer;

	bool tz_mode;
	spinlock_t ese_spi_lock;

	bool isGpio_cfgDone;
	bool enabled_clk;
#ifdef FEATURE_ESE_WAKELOCK
	struct wake_lock ese_lock;
#endif
	int cs_gpio;
	int clk_gpio;
	int mosi_gpio;
	int miso_gpio;
};

/* T==1 protocol specific global data */
const unsigned char SOF = 0xA5u;

static void p3_spi_read(struct p3_dev *p3dev, char *buf, size_t len) {
	int i, j;

	/* gpio_set_value_cansleep(p3dev->cs_gpio, 0); */
	udelay(200);
	for (i= 0; i < len; i++) {
		for(j = 7; j >= 0; j-- ) {
			gpio_set_value_cansleep(p3dev->clk_gpio, 0);

			buf[i] = buf[i] |gpio_get_value_cansleep(p3dev->miso_gpio) << j;

			gpio_set_value_cansleep(p3dev->clk_gpio, 1);
		}
		udelay(100); /* need to check */
	}

	/* gpio_set_value_cansleep(p3dev->cs_gpio, 1); */
	P3_DBG_MSG("%s: len = %d\n", __func__, len);
}

static void p3_spi_write(struct p3_dev *p3dev, const char *buf, size_t len) {
	int i, j;

	/* gpio_set_value_cansleep(p3dev->cs_gpio, 0); */
	udelay(200);

	for (i= 0; i < len; i++) {
		for(j = 7; j >= 0; j-- ) {
			gpio_set_value_cansleep(p3dev->clk_gpio, 0);

			if (buf[i] & (1 << j))
				gpio_set_value_cansleep(p3dev->mosi_gpio, 1);
			else
				gpio_set_value_cansleep(p3dev->mosi_gpio, 0);

			gpio_set_value_cansleep(p3dev->clk_gpio, 1);
		}
		udelay(100); /* need to check */
	}

	/* gpio_set_value_cansleep(p3dev->cs_gpio, 1); */
	P3_DBG_MSG("%s: len = %d\n", __func__, len);
}

static int p3_spi_xfer(struct p3_dev *p3dev, struct p3_ioctl_transfer *tr) {
	int i, j;
	unsigned int missing = 0;

	if (p3dev == NULL || tr == NULL)
		return -EFAULT;

	if (tr->len > DEFAULT_BUFFER_SIZE || !tr->len)
		return -EMSGSIZE;

	memset(p3dev->null_buffer, 0x00, DEFAULT_BUFFER_SIZE);
	memset(p3dev->buffer, 0x00, DEFAULT_BUFFER_SIZE);

	if (tr->tx_buffer != NULL) {
		if (copy_from_user(p3dev->null_buffer,
				tr->tx_buffer, tr->len) != 0)
			return -EFAULT;
	}

	/* gpio_set_value_cansleep(p3dev->cs_gpio, 0); */
	udelay(200);

	for (i = 0; i < tr->len; i++) {
		for(j = 7; j >= 0; j-- ) {
			gpio_set_value_cansleep(p3dev->clk_gpio, 0);

			if (p3dev->null_buffer[i] & (1 << j))
				gpio_set_value_cansleep(p3dev->mosi_gpio, 1);
			else
				gpio_set_value_cansleep(p3dev->mosi_gpio, 0);

			p3dev->buffer[i] = p3dev->buffer[i] |gpio_get_value_cansleep(p3dev->miso_gpio) << j;

			gpio_set_value_cansleep(p3dev->clk_gpio, 1);
		}
		udelay(100); /* need to check */
	}

	/* gpio_set_value_cansleep(p3dev->cs_gpio, 1); */

	if (tr->rx_buffer != NULL) {
		missing = (unsigned int)copy_to_user(tr->rx_buffer, p3dev->buffer, tr->len);
		if (missing != 0)
			tr->len = tr->len - missing;
	}
#if 0
	P3_DBG_MSG("[ESE] %s : write data (%d) = ", __func__, tr->len);

	for (i = 0; i < tr->len; i++) {
		pr_info("[ESE] 0x%x ", tr->tx_buffer[i]);
	}
	pr_info("\n");

	P3_DBG_MSG("[ESE] %s : read data = ", __func__);
	for (i = 0; i < tr->len; i++) {
		pr_info("[ESE] 0x%x ", p3dev->buffer[i]);
	}
	pr_info("\n");
#endif
	P3_DBG_MSG("%s p3_spi_xfer, length=%d\n", __func__, tr->len);

	return 0;
}


static int p3_set_clk(struct p3_dev *p3_device, unsigned long arg)
{
	int ret_val = 0;

	P3_DBG_MSG("%s\n", __func__);
	return ret_val;
}

static int p3_enable_clk(struct p3_dev *p3_device)
{
	int ret_val = 0;

	P3_DBG_MSG("%s\n", __func__);
	return ret_val;
}

static int p3_disable_clk(struct p3_dev *p3_device)
{
	int ret_val = 0;

	P3_DBG_MSG("%s\n", __func__);
	return ret_val;
}

static int p3_enable_cs(struct p3_dev *p3_device)
{
	int ret_val = 0;

	P3_DBG_MSG("%s\n", __func__);
	gpio_set_value_cansleep(p3_device->cs_gpio, 0);
	usleep_range(50,70);
	return ret_val;
}


static int p3_disable_cs(struct p3_dev *p3_device)
{
	int ret_val = 0;

	P3_DBG_MSG("%s\n", __func__);
	gpio_set_value_cansleep(p3_device->cs_gpio, 1);
	return ret_val;
}

static int p3_enable_clk_cs(struct p3_dev *p3_device)
{
	P3_DBG_MSG("%s\n", __func__);
	return 0;
}

static int p3_disable_clk_cs(struct p3_dev *p3_device)
{
	P3_DBG_MSG("%s\n", __func__);
	return 0;
}

static int p3_regulator_onoff(struct p3_dev *data, int onoff)
{
	int rc = 0;
	struct regulator *regulator_vdd_1p8;

	regulator_vdd_1p8 = regulator_get(NULL, "vdd_ese_1.8");
	if (IS_ERR(regulator_vdd_1p8) || regulator_vdd_1p8 == NULL) {
		P3_ERR_MSG("%s - vdd_1p8 regulator_get fail\n", __func__);
		return -ENODEV;
	}

	P3_DBG_MSG("%s - onoff = %d\n", __func__, onoff);
	if (onoff == 1) {
		rc = regulator_enable(regulator_vdd_1p8);
		if (rc) {
			P3_ERR_MSG("%s - enable vdd_1p8 failed, rc=%d\n",
				__func__, rc);
			goto done;
		}
	} else {
		rc = regulator_disable(regulator_vdd_1p8);
		if (rc) {
			P3_ERR_MSG("%s - disable vdd_1p8 failed, rc=%d\n",
				__func__, rc);
			goto done;
		}
	}

	/*data->regulator_is_enable = (u8)onoff;*/

	done:
	regulator_put(regulator_vdd_1p8);
	P3_DBG_MSG("%s\n", __func__);
	return rc;
}

static int p3_rw_spi_message(struct p3_dev *p3_device,
				 unsigned long arg)
{
	struct p3_ioctl_transfer   *dup = NULL;
	int err = 0;
#ifdef PANIC_DEBUG
	unsigned int addr_rx = 0;
	unsigned int addr_tx = 0;
	unsigned int addr_len = 0;
#endif

	P3_DBG_MSG("%s\n", __func__);
	dup = kmalloc(sizeof(struct p3_ioctl_transfer), GFP_KERNEL);
	if (dup == NULL)
		return -ENOMEM;

#if 0//def PANIC_DEBUG
	addr_rx = (unsigned int)(&dup->rx_buffer);
	addr_tx = (unsigned int)(&dup->tx_buffer);
	addr_len = (unsigned int)(&dup->len);
#endif
	if (copy_from_user(dup, (void *)arg,
			   sizeof(struct p3_ioctl_transfer)) != 0) {
		kfree(dup);
		return -EFAULT;
	} else {
#if 0//def PANIC_DEBUG
		if ((addr_rx != (unsigned int)(&dup->rx_buffer)) ||
			(addr_tx != (unsigned int)(&dup->tx_buffer)) ||
			(addr_len != (unsigned int)(&dup->len)))
		P3_ERR_MSG("%s invalid addr!!! rx=%x, tx=%x, len=%x\n",
			__func__, (unsigned int)(&dup->rx_buffer),
			(unsigned int)(&dup->tx_buffer),
			(unsigned int)(&dup->len));
#endif
		err = p3_spi_xfer(p3_device, dup);
		if (err != 0) {
			kfree(dup);
			P3_ERR_MSG("%s xfer failed!\n", __func__);
			return err;
		}
	}
	if (copy_to_user((void *)arg, dup,
			 sizeof(struct p3_ioctl_transfer)) != 0)
		return -EFAULT;
	kfree(dup);
	return 0;
}

static int p3_swing_release_cs(struct p3_dev *p3_device, int cnt)
{
	int i;

	for(i=0; i< cnt ; i++)
	{
		udelay(1);
		gpio_set_value_cansleep(p3_device->cs_gpio, 1);
		udelay(1);
		gpio_set_value_cansleep(p3_device->cs_gpio, 0);
	}

	P3_DBG_MSG("%s cnt:%d ", __func__, cnt);
	return 0;
}

static int p3_swing_on_cs(struct p3_dev *p3_device, int cnt)
{
	int i;

	gpio_set_value_cansleep(p3_device->cs_gpio, 1);
	udelay(1);

	for(i=0; i< cnt ; i++)
	{
		udelay(1);
		gpio_set_value_cansleep(p3_device->cs_gpio, 0);
		udelay(1);
		gpio_set_value_cansleep(p3_device->cs_gpio, 1);
	}
	P3_DBG_MSG("%s cnt:%d ", __func__, cnt);

	udelay(1);
	gpio_set_value_cansleep(p3_device->cs_gpio, 0);

	return 0;
}

static int p3_dev_open(struct inode *inode, struct file *filp)
{
	struct p3_dev
	*p3_dev = container_of(filp->private_data,
			struct p3_dev, p3_device);
	/* GPIO ctrl for Power  */
	int ret = 0;

	s3c_gpio_cfgpin(p3_dev->cs_gpio, S3C_GPIO_OUTPUT);
	s3c_gpio_setpull(p3_dev->cs_gpio, S3C_GPIO_PULL_UP);
	s5p_gpio_set_drvstr(p3_dev->cs_gpio, S5P_GPIO_DRVSTR_LV3);

	s3c_gpio_cfgpin(p3_dev->clk_gpio, S3C_GPIO_OUTPUT);
	s3c_gpio_setpull(p3_dev->clk_gpio, S3C_GPIO_PULL_UP);
	s5p_gpio_set_drvstr(p3_dev->clk_gpio, S5P_GPIO_DRVSTR_LV3);

	s3c_gpio_cfgpin(p3_dev->mosi_gpio, S3C_GPIO_OUTPUT);
	s3c_gpio_setpull(p3_dev->mosi_gpio, S3C_GPIO_PULL_DOWN);
	s5p_gpio_set_drvstr(p3_dev->mosi_gpio, S5P_GPIO_DRVSTR_LV3);

	s3c_gpio_cfgpin(p3_dev->miso_gpio, S3C_GPIO_INPUT);
	s3c_gpio_setpull(p3_dev->miso_gpio, S3C_GPIO_PULL_NONE);
	s5p_gpio_set_drvstr(p3_dev->miso_gpio, S5P_GPIO_DRVSTR_LV3);

	/* for defence MULTI-OPEN */
	if (p3_dev->enabled_clk) {
		P3_ERR_MSG("%s - ALREADY opened!\n", __func__);
		return -EBUSY;
	}

	gpio_set_value_cansleep(p3_dev->cs_gpio, 0);
	gpio_set_value_cansleep(p3_dev->clk_gpio, 0);

	/* power ON at probe */
	ret = p3_regulator_onoff(p3_dev, 1);
	if(ret < 0)
		P3_ERR_MSG(" test: failed to turn on LDO()\n");
	usleep_range(5000, 5500);

	gpio_set_value_cansleep(p3_dev->cs_gpio, 1);
	gpio_set_value_cansleep(p3_dev->clk_gpio, 1);
	usleep_range(15, 20);

	gpio_set_value_cansleep(p3_dev->cs_gpio, 0);

	filp->private_data = p3_dev;
	P3_DBG_MSG("%s : Major No: %d, Minor No: %d\n", __func__,
		imajor(inode), iminor(inode));

	p3_dev->null_buffer =
		kmalloc(DEFAULT_BUFFER_SIZE, GFP_KERNEL);
	if (p3_dev->null_buffer == NULL) {
		P3_ERR_MSG("%s null_buffer == NULL, -ENOMEM\n", __func__);
		return -ENOMEM;
	}
	p3_dev->buffer =
		kmalloc(DEFAULT_BUFFER_SIZE, GFP_KERNEL);
	if (p3_dev->buffer == NULL) {
		kfree(p3_dev->null_buffer);
		P3_ERR_MSG("%s : buffer == NULL, -ENOMEM\n", __func__);
		return -ENOMEM;
	}

	return 0;
}

static int p3_dev_release(struct inode *inode, struct file *filp)
{
	struct p3_dev
	*p3_dev = container_of(filp->private_data,
			struct p3_dev, p3_device);

	/* GPIO ctrl for Power  */
	int ret = 0;

	p3_dev = filp->private_data;

	if (p3_dev->enabled_clk) {
		P3_DBG_MSG("%s disable CLK at release!!!\n", __func__);
		ret = p3_disable_clk(p3_dev);
		if(ret < 0)
			P3_ERR_MSG("failed to disable CLK\n");
	}
#ifdef FEATURE_ESE_WAKELOCK
	if (wake_lock_active(&p3_dev->ese_lock)) {
		P3_DBG_MSG("%s: wake unlock at release!!\n", __func__);
		wake_unlock(&p3_dev->ese_lock);
	}
#endif

	udelay(2);
	gpio_set_value_cansleep(p3_dev->cs_gpio, 0);

	/* Defence for bit shifting while CLOSE~OPEN */
	p3_swing_release_cs(p3_dev, 7);

	ret = p3_regulator_onoff(p3_dev, 0);
	if(ret < 0)
		P3_ERR_MSG(" test: failed to turn off LDO()\n");

	s3c_gpio_cfgpin(p3_dev->cs_gpio, S3C_GPIO_INPUT);
	s3c_gpio_setpull(p3_dev->cs_gpio, S3C_GPIO_PULL_DOWN);

	s3c_gpio_cfgpin(p3_dev->clk_gpio, S3C_GPIO_INPUT);
	s3c_gpio_setpull(p3_dev->clk_gpio, S3C_GPIO_PULL_DOWN);

	s3c_gpio_cfgpin(p3_dev->mosi_gpio, S3C_GPIO_INPUT);
	s3c_gpio_setpull(p3_dev->mosi_gpio, S3C_GPIO_PULL_DOWN);

	s3c_gpio_cfgpin(p3_dev->miso_gpio, S3C_GPIO_INPUT);
	s3c_gpio_setpull(p3_dev->miso_gpio, S3C_GPIO_PULL_DOWN);

	filp->private_data = p3_dev;
	P3_DBG_MSG("%s : Major No: %d, Minor No: %d\n", __func__,
			imajor(inode), iminor(inode));
	return 0;
}

static long p3_dev_ioctl(struct file *filp, unsigned int cmd,
        unsigned long arg)
{
	int ret = 0;
	struct p3_dev *p3_dev = NULL;
#ifdef P3_RST_ENABLE
	unsigned char buf[100];
#endif

	if (_IOC_TYPE(cmd) != P3_MAGIC) {
		P3_ERR_MSG("%s invalid magic. cmd=0x%X Received=0x%X Expected=0x%X\n",
			__func__, cmd, _IOC_TYPE(cmd), P3_MAGIC);
		return -ENOTTY;
	}
	/*P3_DBG_MSG(KERN_ALERT "p3_dev_ioctl-Enter %u arg = %ld\n", cmd, arg);*/
	p3_dev = filp->private_data;

	mutex_lock(&p3_dev->buffer_mutex);
	switch (cmd) {
	case P3_SET_PWR:
		if (arg == 2)
		{

		}
		break;

	case P3_SET_DBG:
		debug_level = (unsigned char )arg;
		P3_DBG_MSG(KERN_INFO"[NXP-P3] -  Debug level %d", debug_level);
		break;
	case P3_SET_POLL:
		p3_dev-> enable_poll_mode = (unsigned char )arg;
		if (p3_dev-> enable_poll_mode == 0)
		{
			P3_DBG_MSG(KERN_INFO"[NXP-P3] - IRQ Mode is set \n");
		}
		else
		{
			P3_DBG_MSG(KERN_INFO"[NXP-P3] - Poll Mode is set \n");
			p3_dev->enable_poll_mode = 1;
		}
		break;
	/*non TZ */
	case P3_SET_SPI_CONFIG:
		/*p3_ioctl_config_spi_gpio(p3_dev);*/
		break;
	case P3_ENABLE_SPI_CLK:
		P3_DBG_MSG("%s P3_ENABLE_SPI_CLK\n", __func__);
		ret = p3_enable_clk(p3_dev);
		break;
	case P3_DISABLE_SPI_CLK:
		P3_DBG_MSG("%s P3_DISABLE_SPI_CLK\n", __func__);
		ret = p3_disable_clk(p3_dev);
		break;
	case P3_ENABLE_SPI_CS:
		/*P3_DBG_MSG("%s P3_ENABLE_SPI_CS\n", __func__);*/
		ret = p3_enable_cs(p3_dev);
		break;
	case P3_DISABLE_SPI_CS:
		/*P3_DBG_MSG("%s P3_DISABLE_SPI_CS\n", __func__);*/
		ret = p3_disable_cs(p3_dev);
		break;

	case P3_RW_SPI_DATA:
		ret = p3_rw_spi_message(p3_dev, arg);
		if (ret < 0)
			P3_ERR_MSG("%s P3_RW_SPI_DATA failed [%d].\n",__func__,ret);
		break;
	case P3_SET_SPI_CLK: /* To change CLK */
		ret = p3_set_clk(p3_dev, arg);
		if (ret < 0)
			P3_ERR_MSG("%s P3_SET_SPI_CLK failed [%d].\n",__func__,ret);
		break;

	case P3_ENABLE_CLK_CS:
		P3_DBG_MSG("%s P3_ENABLE_CLK_CS\n", __func__);
		ret = p3_enable_clk_cs(p3_dev);
		break;
	case P3_DISABLE_CLK_CS:
		P3_DBG_MSG("%s P3_DISABLE_CLK_CS\n", __func__);
		ret = p3_disable_clk_cs(p3_dev);
		break;

	case P3_SWING_CS:
		p3_swing_on_cs(p3_dev, (int)arg);
		P3_DBG_MSG("%s P3_SWING_CS set %d\n", __func__, (int)arg);
		break;

	default:
		P3_DBG_MSG("%s no matching ioctl!\n", __func__);
		ret = -EINVAL;
	}
	mutex_unlock(&p3_dev->buffer_mutex);

	return ret;
}

static ssize_t p3_dev_write(struct file *filp, const char *buf, size_t count,
        loff_t *offset)
{
	int ret = count;
	struct p3_dev *p3_dev;
	unsigned char tx_buffer[MAX_BUFFER_SIZE];

	P3_DBG_MSG(KERN_ALERT "p3_dev_write -Enter count %zu\n", count);

	p3_dev = filp->private_data;

	mutex_lock(&p3_dev->buffer_mutex);
	if (count > MAX_BUFFER_SIZE)
		count = MAX_BUFFER_SIZE;

	memset(&tx_buffer[0], 0, sizeof(tx_buffer));
	if (copy_from_user(&tx_buffer[0], &buf[0], count))
	{
		P3_ERR_MSG("%s : failed to copy from user space\n", __func__);
		mutex_unlock(&p3_dev->buffer_mutex);
		return -EFAULT;
	}


	/* Write data */
	p3_spi_write(p3_dev, &tx_buffer[0], count);

	mutex_unlock(&p3_dev->buffer_mutex);
	P3_DBG_MSG(KERN_ALERT "p3_dev_write ret %d- Exit \n", ret);
	return ret;
}

#ifdef P3_IRQ_ENABLE
static void p3_disable_irq(struct p3_dev *p3_dev)
{
	unsigned long flags;

	P3_DBG_MSG("Entry : %s\n", __FUNCTION__);

	spin_lock_irqsave(&p3_dev->irq_enabled_lock, flags);
	if (p3_dev->irq_enabled)
	{
		disable_irq_nosync(p3_dev->spi->irq);
		p3_dev->irq_enabled = false;
	}
	spin_unlock_irqrestore(&p3_dev->irq_enabled_lock, flags);

	P3_DBG_MSG("Exit : %s\n", __FUNCTION__);
}

static irqreturn_t p3_dev_irq_handler(int irq, void *dev_id)
{
	struct p3_dev *p3_dev = dev_id;

	P3_DBG_MSG("Entry : %s\n", __FUNCTION__);
	p3_disable_irq(p3_dev);

	/* Wake up waiting readers */
	wake_up(&p3_dev->read_wq);

	P3_DBG_MSG("Exit : %s\n", __FUNCTION__);
	return IRQ_HANDLED;
}
#endif

static ssize_t p3_dev_read(struct file *filp, char *buf, size_t count,
        loff_t *offset)
{
	int ret = -EIO;
	struct p3_dev *p3_dev = filp->private_data;
	unsigned char rx_buffer[MAX_BUFFER_SIZE];

	P3_DBG_MSG("p3_dev_read count %zu - Enter \n", count);

	mutex_lock(&p3_dev->buffer_mutex);

	memset(&rx_buffer[0], 0x00, sizeof(rx_buffer));

	P3_DBG_MSG(KERN_INFO"Data Lenth = %zu", count);


	/* Read the availabe data along with one byte LRC */
	p3_spi_read(p3_dev, &rx_buffer[0], count);


	/*P3_DBG_MSG(KERN_INFO"total_count = %d", total_count);*/

	if (copy_to_user(buf, &rx_buffer[0], count))
	{
		P3_ERR_MSG("%s : failed to copy to user space\n", __func__);
		ret = -EFAULT;
		goto fail;
	}
	ret = count;
	P3_DBG_MSG("%s ret %d Exit\n",__func__, ret);

	mutex_unlock(&p3_dev->buffer_mutex);

	return ret;

	fail:
	P3_ERR_MSG("Error %s ret %d Exit\n", __func__, ret);
	mutex_unlock(&p3_dev->buffer_mutex);
	return ret;
}


#if 0
static ssize_t p3_test_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	unsigned char data;
	P3_DBG_MSG("%s\n", __func__);
	data='a';
	return sprintf(buf, "%d\n", data);
}

static ssize_t p3_test_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	unsigned long data;
	int error;

	error = kstrtoul(buf, 10, &data);
	if (error)
		return error;

	P3_DBG_MSG("%s [%lu]\n", __func__, data);
	/* Need to implement to verify chip*/
	return count;
}

static DEVICE_ATTR(test, S_IRUGO|S_IWUSR|S_IWGRP|S_IWOTH,
		p3_test_show, p3_test_store);
#endif

/* possible fops on the p3 device */
static const struct file_operations p3_dev_fops = {
	.owner = THIS_MODULE,
	.read = p3_dev_read,
	.write = p3_dev_write,
	.open = p3_dev_open,
	.release = p3_dev_release,
	.unlocked_ioctl = p3_dev_ioctl,
};

#ifdef CONFIG_OF
static int p3_parse_dt(struct device *dev, struct p3_dev *data)
{
	struct device_node *np = dev->of_node;
	int errorno = 0;

	data->cs_gpio = of_get_named_gpio(np, "p3-cs-gpio", 0);
	if (data->cs_gpio < 0) {
		P3_ERR_MSG("%s - get cs_gpio error\n", __func__);
		return -ENODEV;
	}

	data->clk_gpio = of_get_named_gpio(np,"p3-clk-gpio", 0);
	if (data->clk_gpio < 0) {
		P3_ERR_MSG("Cannot get clk gpio\n");
		return -ENODEV;
	}

	data->mosi_gpio = of_get_named_gpio(np,"p3-mosi-gpio", 0);
	if (data->mosi_gpio < 0) {
		P3_ERR_MSG("Cannot get mosi gpio\n");
		return -ENODEV;
	}

	data->miso_gpio = of_get_named_gpio(np,"p3-miso-gpio", 0);
	if (data->miso_gpio < 0) {
		P3_ERR_MSG("Cannot get miso gpio\n");
		return -ENODEV;
	}

	P3_DBG_MSG("%s: cs_gpio=%d, %d, %d, %d\n", __func__,
		data->cs_gpio, data->clkpin, data->mosi_gpio, data->miso_gpio);

	return errorno;
}
#endif

#if 0
static void p3_spi_gpio_init(struct p3_dev *p3dev) {
	gpio_direction_output(p3dev->cs_gpio, 0);
	gpio_direction_input(p3dev->miso_gpio);
	gpio_direction_output(p3dev->mosi_gpio, 0);
	gpio_direction_output(p3dev->clk_gpio, 0);
}
#endif

static int p3_probe(struct platform_device *dev)
{
	int ret = -1;
	struct p3_dev *p3_dev = NULL;
#ifndef CONFIG_OF
	struct p3_platform_data *pdata;
#endif

	p3_dev = kzalloc(sizeof(*p3_dev), GFP_KERNEL);
	if (p3_dev == NULL)
	{
		P3_ERR_MSG("failed to allocate memory for module data\n");
		ret = -ENOMEM;
		goto err_exit;
	}
#if 0
	ret = p3_regulator_onoff(p3_dev, 1);
	if (ret) {
		P3_ERR_MSG("%s - Failed to enable regulator\n", __func__);
		goto p3_parse_dt_failed;
	}
#endif

#ifdef CONFIG_OF
	if (dev->dev.of_node) {
	ret = p3_parse_dt(&dev, p3_dev);
		if (ret) {
			P3_ERR_MSG("%s - Failed to parse DT\n", __func__);
			goto p3_parse_dt_failed;
		}
		P3_DBG_MSG("%s: tz_mode=%d, isGpio_cfgDone:%d\n", __func__,
				p3_dev->tz_mode, p3_dev->isGpio_cfgDone);
	}
#else
	pdata = dev->dev.platform_data;
	if (pdata ==NULL) {
		P3_ERR_MSG("%s platform_data is null\n", __func__);
		ret = -ENOMEM;
		goto p3_parse_dt_failed;
	}

	p3_dev->rst_gpio = pdata->rst_gpio;
	p3_dev->irq_gpio = pdata->irq_gpio;
	p3_dev->cs_gpio = pdata->cs_gpio;
	p3_dev->clk_gpio = pdata->clk_gpio;
	p3_dev->mosi_gpio = pdata->mosi_gpio;
	p3_dev->miso_gpio = pdata->miso_gpio;
#endif

	//p3_spi_gpio_init(p3_dev);

	platform_set_drvdata(dev, p3_dev);

	p3_dev -> p3_device.minor = MISC_DYNAMIC_MINOR;
	p3_dev -> p3_device.name = "p3";
	p3_dev -> p3_device.fops = &p3_dev_fops;
	ret = misc_register(&p3_dev->p3_device);
	if (ret < 0)
	{
		P3_ERR_MSG("misc_register failed! %d\n", ret);
		goto err_exit0;
	}

	/* init mutex and queues */
	init_waitqueue_head(&p3_dev->read_wq);
	mutex_init(&p3_dev->buffer_mutex);
	spin_lock_init(&p3_dev->ese_spi_lock);
#ifdef FEATURE_ESE_WAKELOCK
	wake_lock_init(&p3_dev->ese_lock,
		WAKE_LOCK_SUSPEND, "ese_wake_lock");
#endif

#ifdef P3_IRQ_ENABLE
	spin_lock_init(&p3_dev->irq_enabled_lock);
#endif

#if 0 //test
{
	struct device *dev;

	p3_device_class = class_create(THIS_MODULE, "ese");
	if (IS_ERR(p3_device_class)) {
		P3_ERR_MSG("%s class_create() is failed:%lu\n",
			__func__,  PTR_ERR(p3_device_class));
		//status = PTR_ERR(p3_device_class);
		//goto vfsspi_probe_class_create_failed;
	}
	dev = device_create(p3_device_class, NULL,
			    0, p3_dev, "p3");
		P3_ERR_MSG("%s device_create() is failed:%lu\n",
			__func__,  PTR_ERR(dev));

	if ((device_create_file(dev, &dev_attr_test)) < 0)
		P3_ERR_MSG("%s device_create_file failed !!!\n", __func__); 
}
#endif

#if 0
	ret = p3_regulator_onoff(p3_dev, 0);
	if(ret < 0)
		P3_ERR_MSG(" test: failed to turn off LDO()\n");
#endif
	p3_dev-> enable_poll_mode = 1; /* Default IRQ read mode */
	P3_DBG_MSG("%s finished...\n", __FUNCTION__);
	return ret;

	/*err_exit1:*/
	misc_deregister(&p3_dev->p3_device);
	err_exit0:
#ifdef FEATURE_ESE_WAKELOCK
	wake_lock_destroy(&p3_dev->ese_lock);
#endif
	mutex_destroy(&p3_dev->buffer_mutex);
	p3_parse_dt_failed:
	kfree(p3_dev);
	err_exit:
	P3_DBG_MSG("ERROR: Exit : %s ret %d\n", __FUNCTION__, ret);
	return ret;
}

static int p3_remove(struct platform_device *pdev)
{
	struct p3_dev *p3_dev = platform_get_drvdata(pdev);

	P3_DBG_MSG("Entry : %s\n", __FUNCTION__);
	if (p3_dev == NULL)
	{
		P3_ERR_MSG("%s p3_dev is null!\n", __func__);
		return 0;
	}
#ifdef P3_HARD_RESET
	if (p3_regulator != NULL)
	{
		regulator_disable(p3_regulator);
		regulator_put(p3_regulator);
	}
	else
	{
		P3_ERR_MSG("ERROR %s regulator not enabled \n", __FUNCTION__);
	}
#endif
#ifdef P3_RST_ENABLE
	gpio_free(p3_dev->rst_gpio);
#endif

#ifdef P3_IRQ_ENABLE
	free_irq(p3_dev->spi->irq, p3_dev);
	gpio_free(p3_dev->irq_gpio);
#endif

#ifdef FEATURE_ESE_WAKELOCK
	wake_lock_destroy(&p3_dev->ese_lock);
#endif
	mutex_destroy(&p3_dev->buffer_mutex);
	misc_deregister(&p3_dev->p3_device);

	kfree(p3_dev);
	P3_DBG_MSG("Exit : %s\n", __FUNCTION__);
	return 0;
}

#ifdef CONFIG_OF
static struct of_device_id p3_match_table[] = {
	{ .compatible = "ese_p3",},
	{},
};
#else
#define ese_match_table NULL
#endif

static struct platform_driver p3_driver = {
	.driver = {
		.name = "p3",
		.owner = THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = p3_match_table,
#endif
	},
	.probe =  p3_probe,
	.remove = p3_remove,
};

static int __init p3_dev_init(void)
{
	debug_level = P3_FULL_DEBUG;

	P3_DBG_MSG("Entry : %s\n", __FUNCTION__);
#if 1 //(!defined(CONFIG_ESE_FACTORY_ONLY) || defined(CONFIG_SEC_FACTORY))
	return platform_driver_register(&p3_driver);
#else
	return -1;
#endif
}

static void __exit p3_dev_exit(void)
{
	P3_DBG_MSG("Entry : %s\n", __FUNCTION__);

	platform_driver_unregister(&p3_driver);
	P3_DBG_MSG("Exit : %s\n", __FUNCTION__);
}

module_init( p3_dev_init);
module_exit( p3_dev_exit);

MODULE_AUTHOR("Sec");
MODULE_DESCRIPTION("Oberthur ese platform driver");
MODULE_LICENSE("GPL");

/** @} */
