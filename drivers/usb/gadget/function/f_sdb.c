/*
 * Gadget Driver for Samsung SDB (based on Android ADB)
 *
 * Copyright (C) 2008 Google, Inc.
 * Author: Mike Lockwood <lockwood@android.com>
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/poll.h>
#include <linux/delay.h>
#include <linux/wait.h>
#include <linux/err.h>
#include <linux/interrupt.h>
#include <linux/sched.h>
#include <linux/types.h>
#include <linux/device.h>
#include <linux/miscdevice.h>

#define SDB_BULK_BUFFER_SIZE           4096

/* number of tx requests to allocate */
#define TX_REQ_MAX 4

static const char sdb_shortname[] = "samsung_sdb";

struct sdb_dev {
	struct usb_function function;
	struct usb_composite_dev *cdev;
	spinlock_t lock;

	struct usb_ep *ep_in;
	struct usb_ep *ep_out;

	int online;
	int error;

	atomic_t read_excl;
	atomic_t write_excl;
	atomic_t open_excl;

	struct list_head tx_idle;

	wait_queue_head_t read_wq;
	wait_queue_head_t write_wq;
	struct usb_request *rx_req;
	int rx_done;
};

static struct usb_interface_descriptor sdb_interface_desc = {
	.bLength                = USB_DT_INTERFACE_SIZE,
	.bDescriptorType        = USB_DT_INTERFACE,
	.bInterfaceNumber       = 0,
	.bNumEndpoints          = 2,
	.bInterfaceClass        = 0xFF,
	.bInterfaceSubClass     = 0x20,
	.bInterfaceProtocol     = 0x02,
};

static struct usb_endpoint_descriptor sdb_superspeed_in_desc = {
	.bLength                = USB_DT_ENDPOINT_SIZE,
	.bDescriptorType        = USB_DT_ENDPOINT,
	.bEndpointAddress       = USB_DIR_IN,
	.bmAttributes           = USB_ENDPOINT_XFER_BULK,
	.wMaxPacketSize         = __constant_cpu_to_le16(1024),
};

static struct usb_endpoint_descriptor sdb_superspeed_out_desc = {
	.bLength                = USB_DT_ENDPOINT_SIZE,
	.bDescriptorType        = USB_DT_ENDPOINT,
	.bEndpointAddress       = USB_DIR_OUT,
	.bmAttributes           = USB_ENDPOINT_XFER_BULK,
	.wMaxPacketSize         = __constant_cpu_to_le16(1024),
};

static struct usb_ss_ep_comp_descriptor sdb_superspeed_bulk_comp_desc = {
	.bLength =              sizeof sdb_superspeed_bulk_comp_desc,
	.bDescriptorType =      USB_DT_SS_ENDPOINT_COMP,

	/* the following 2 values can be tweaked if necessary */
	/* .bMaxBurst =         0, */
	/* .bmAttributes =      0, */
};

static struct usb_endpoint_descriptor sdb_highspeed_in_desc = {
	.bLength                = USB_DT_ENDPOINT_SIZE,
	.bDescriptorType        = USB_DT_ENDPOINT,
	.bEndpointAddress       = USB_DIR_IN,
	.bmAttributes           = USB_ENDPOINT_XFER_BULK,
	.wMaxPacketSize         = __constant_cpu_to_le16(512),
};

static struct usb_endpoint_descriptor sdb_highspeed_out_desc = {
	.bLength                = USB_DT_ENDPOINT_SIZE,
	.bDescriptorType        = USB_DT_ENDPOINT,
	.bEndpointAddress       = USB_DIR_OUT,
	.bmAttributes           = USB_ENDPOINT_XFER_BULK,
	.wMaxPacketSize         = __constant_cpu_to_le16(512),
};

static struct usb_endpoint_descriptor sdb_fullspeed_in_desc = {
	.bLength                = USB_DT_ENDPOINT_SIZE,
	.bDescriptorType        = USB_DT_ENDPOINT,
	.bEndpointAddress       = USB_DIR_IN,
	.bmAttributes           = USB_ENDPOINT_XFER_BULK,
};

static struct usb_endpoint_descriptor sdb_fullspeed_out_desc = {
	.bLength                = USB_DT_ENDPOINT_SIZE,
	.bDescriptorType        = USB_DT_ENDPOINT,
	.bEndpointAddress       = USB_DIR_OUT,
	.bmAttributes           = USB_ENDPOINT_XFER_BULK,
};

static struct usb_descriptor_header *fs_sdb_descs[] = {
	(struct usb_descriptor_header *) &sdb_interface_desc,
	(struct usb_descriptor_header *) &sdb_fullspeed_in_desc,
	(struct usb_descriptor_header *) &sdb_fullspeed_out_desc,
	NULL,
};

static struct usb_descriptor_header *hs_sdb_descs[] = {
	(struct usb_descriptor_header *) &sdb_interface_desc,
	(struct usb_descriptor_header *) &sdb_highspeed_in_desc,
	(struct usb_descriptor_header *) &sdb_highspeed_out_desc,
	NULL,
};

static struct usb_descriptor_header *ss_sdb_descs[] = {
	(struct usb_descriptor_header *) &sdb_interface_desc,
	(struct usb_descriptor_header *) &sdb_superspeed_in_desc,
	(struct usb_descriptor_header *) &sdb_superspeed_bulk_comp_desc,
	(struct usb_descriptor_header *) &sdb_superspeed_out_desc,
	(struct usb_descriptor_header *) &sdb_superspeed_bulk_comp_desc,
	NULL,
};

static void sdb_ready_callback(void);
static void sdb_closed_callback(void);

/* temporary variable used between sdb_open() and sdb_gadget_bind() */
static struct sdb_dev *_sdb_dev;

static inline struct sdb_dev *func_to_sdb(struct usb_function *f)
{
	return container_of(f, struct sdb_dev, function);
}


static struct usb_request *sdb_request_new(struct usb_ep *ep, int buffer_size)
{
	struct usb_request *req = usb_ep_alloc_request(ep, GFP_KERNEL);
	if (!req)
		return NULL;

	/* now allocate buffers for the requests */
	req->buf = kmalloc(buffer_size, GFP_KERNEL);
	if (!req->buf) {
		usb_ep_free_request(ep, req);
		return NULL;
	}

	return req;
}

static void sdb_request_free(struct usb_request *req, struct usb_ep *ep)
{
	if (req) {
		kfree(req->buf);
		usb_ep_free_request(ep, req);
	}
}

static inline int sdb_lock(atomic_t *excl)
{
	if (atomic_inc_return(excl) == 1) {
		return 0;
	} else {
		atomic_dec(excl);
		return -1;
	}
}

static inline void sdb_unlock(atomic_t *excl)
{
	atomic_dec(excl);
}

/* add a request to the tail of a list */
void sdb_req_put(struct sdb_dev *dev, struct list_head *head,
		struct usb_request *req)
{
	unsigned long flags;

	spin_lock_irqsave(&dev->lock, flags);
	list_add_tail(&req->list, head);
	spin_unlock_irqrestore(&dev->lock, flags);
}

/* remove a request from the head of a list */
struct usb_request *sdb_req_get(struct sdb_dev *dev, struct list_head *head)
{
	unsigned long flags;
	struct usb_request *req;

	spin_lock_irqsave(&dev->lock, flags);
	if (list_empty(head)) {
		req = 0;
	} else {
		req = list_first_entry(head, struct usb_request, list);
		list_del(&req->list);
	}
	spin_unlock_irqrestore(&dev->lock, flags);
	return req;
}

static void sdb_complete_in(struct usb_ep *ep, struct usb_request *req)
{
	struct sdb_dev *dev = _sdb_dev;

	if (req->status != 0)
		dev->error = 1;

	sdb_req_put(dev, &dev->tx_idle, req);

	wake_up(&dev->write_wq);
}

static void sdb_complete_out(struct usb_ep *ep, struct usb_request *req)
{
	struct sdb_dev *dev = _sdb_dev;

	dev->rx_done = 1;
	if (req->status != 0 && req->status != -ECONNRESET)
		dev->error = 1;

	wake_up(&dev->read_wq);
}

static int sdb_create_bulk_endpoints(struct sdb_dev *dev,
				struct usb_endpoint_descriptor *in_desc,
				struct usb_endpoint_descriptor *out_desc)
{
	struct usb_composite_dev *cdev = dev->cdev;
	struct usb_request *req;
	struct usb_ep *ep;
	int i;

	DBG(cdev, "create_bulk_endpoints dev: %p\n", dev);

	ep = usb_ep_autoconfig(cdev->gadget, in_desc);
	if (!ep) {
		DBG(cdev, "usb_ep_autoconfig for ep_in failed\n");
		return -ENODEV;
	}
	DBG(cdev, "usb_ep_autoconfig for ep_in got %s\n", ep->name);
	ep->driver_data = dev;		/* claim the endpoint */
	dev->ep_in = ep;

	ep = usb_ep_autoconfig(cdev->gadget, out_desc);
	if (!ep) {
		DBG(cdev, "usb_ep_autoconfig for ep_out failed\n");
		return -ENODEV;
	}
	DBG(cdev, "usb_ep_autoconfig for sdb ep_out got %s\n", ep->name);
	ep->driver_data = dev;		/* claim the endpoint */
	dev->ep_out = ep;

	/* now allocate requests for our endpoints */
	req = sdb_request_new(dev->ep_out, SDB_BULK_BUFFER_SIZE);
	if (!req)
		goto fail;
	req->complete = sdb_complete_out;
	dev->rx_req = req;

	for (i = 0; i < TX_REQ_MAX; i++) {
		req = sdb_request_new(dev->ep_in, SDB_BULK_BUFFER_SIZE);
		if (!req)
			goto fail;
		req->complete = sdb_complete_in;
		sdb_req_put(dev, &dev->tx_idle, req);
	}

	return 0;

fail:
	printk(KERN_ERR "sdb_bind() could not allocate requests\n");
	return -1;
}

static ssize_t sdb_read(struct file *fp, char __user *buf,
				size_t count, loff_t *pos)
{
	struct sdb_dev *dev = fp->private_data;
	struct usb_request *req;
	int r = count, xfer;
	int maxp;
	int ret;

	pr_debug("sdb_read(%d)\n", (int)count);
	if (!_sdb_dev)
		return -ENODEV;

	if (sdb_lock(&dev->read_excl))
		return -EBUSY;

	/* we will block until we're online */
	while (!(dev->online || dev->error)) {
		pr_debug("sdb_read: waiting for online state\n");
		ret = wait_event_interruptible(dev->read_wq,
				(dev->online || dev->error));
		if (ret < 0) {
			sdb_unlock(&dev->read_excl);
			return ret;
		}
	}
	if (dev->error) {
		r = -EIO;
		goto done;
	}

	maxp = usb_endpoint_maxp(dev->ep_out->desc);
	count = round_up(count, maxp);

	if (count > SDB_BULK_BUFFER_SIZE)
		return -EINVAL;

requeue_req:
	/* queue a request */
	req = dev->rx_req;
	req->length = count;
	dev->rx_done = 0;
	ret = usb_ep_queue(dev->ep_out, req, GFP_ATOMIC);
	if (ret < 0) {
		pr_debug("sdb_read: failed to queue req %p (%d)\n", req, ret);
		r = -EIO;
		dev->error = 1;
		goto done;
	} else {
		pr_debug("rx %p queue\n", req);
	}

	/* wait for a request to complete */
	ret = wait_event_interruptible(dev->read_wq, dev->rx_done);
	if (ret < 0) {
		if (ret != -ERESTARTSYS)
			dev->error = 1;
		r = ret;
		usb_ep_dequeue(dev->ep_out, req);
		goto done;
	}
	if (!dev->error) {
		/* If we got a 0-len packet, throw it back and try again. */
		if (req->actual == 0)
			goto requeue_req;

		pr_debug("rx %p %d\n", req, req->actual);
		xfer = (req->actual < count) ? req->actual : count;
		if (copy_to_user(buf, req->buf, xfer))
			r = -EFAULT;

	} else
		r = -EIO;

done:
	sdb_unlock(&dev->read_excl);
	pr_debug("sdb_read returning %d\n", r);
	return r;
}

static ssize_t sdb_write(struct file *fp, const char __user *buf,
				 size_t count, loff_t *pos)
{
	struct sdb_dev *dev = fp->private_data;
	struct usb_request *req = 0;
	int r = count, xfer;
	int ret;

	if (!_sdb_dev)
		return -ENODEV;
	pr_debug("sdb_write(%d)\n", (int)count);

	if (sdb_lock(&dev->write_excl))
		return -EBUSY;

	while (count > 0) {
		if (dev->error) {
			pr_debug("sdb_write dev->error\n");
			r = -EIO;
			break;
		}

		/* get an idle tx request to use */
		req = 0;
		ret = wait_event_interruptible(dev->write_wq,
			(req = sdb_req_get(dev, &dev->tx_idle)) || dev->error);

		if (ret < 0) {
			r = ret;
			break;
		}

		if (req != 0) {
			if (count > SDB_BULK_BUFFER_SIZE)
				xfer = SDB_BULK_BUFFER_SIZE;
			else
				xfer = count;
			if (copy_from_user(req->buf, buf, xfer)) {
				r = -EFAULT;
				break;
			}

			req->length = xfer;
			ret = usb_ep_queue(dev->ep_in, req, GFP_ATOMIC);
			if (ret < 0) {
				pr_debug("sdb_write: xfer error %d\n", ret);
				dev->error = 1;
				r = -EIO;
				break;
			}

			buf += xfer;
			count -= xfer;

			/* zero this so we don't try to free it on error exit */
			req = 0;
		}
	}

	if (req)
		sdb_req_put(dev, &dev->tx_idle, req);

	sdb_unlock(&dev->write_excl);
	pr_debug("sdb_write returning %d\n", r);
	return r;
}

static int sdb_open(struct inode *ip, struct file *fp)
{
	pr_info("sdb_open\n");
	if (!_sdb_dev)
		return -ENODEV;

	if (sdb_lock(&_sdb_dev->open_excl))
		return -EBUSY;

	fp->private_data = _sdb_dev;

	/* clear the error latch */
	_sdb_dev->error = 0;

	sdb_ready_callback();

	return 0;
}

static int sdb_release(struct inode *ip, struct file *fp)
{
	pr_info("sdb_release\n");

	sdb_closed_callback();

	sdb_unlock(&_sdb_dev->open_excl);
	return 0;
}

/* file operations for sdb device /dev/samsung_sdb */
static const struct file_operations sdb_fops = {
	.owner = THIS_MODULE,
	.read = sdb_read,
	.write = sdb_write,
	.open = sdb_open,
	.release = sdb_release,
};

static struct miscdevice sdb_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = sdb_shortname,
	.fops = &sdb_fops,
};




static int
sdb_function_bind(struct usb_configuration *c, struct usb_function *f)
{
	struct usb_composite_dev *cdev = c->cdev;
	struct sdb_dev	*dev = func_to_sdb(f);
	int			id;
	int			ret;

	dev->cdev = cdev;
	DBG(cdev, "sdb_function_bind dev: %p\n", dev);

	/* allocate interface ID(s) */
	id = usb_interface_id(c, f);
	if (id < 0)
		return id;
	sdb_interface_desc.bInterfaceNumber = id;

	/* allocate endpoints */
	ret = sdb_create_bulk_endpoints(dev, &sdb_fullspeed_in_desc,
			&sdb_fullspeed_out_desc);
	if (ret)
		return ret;

	/* support high speed hardware */
	if (gadget_is_dualspeed(c->cdev->gadget)) {
		sdb_highspeed_in_desc.bEndpointAddress =
			sdb_fullspeed_in_desc.bEndpointAddress;
		sdb_highspeed_out_desc.bEndpointAddress =
			sdb_fullspeed_out_desc.bEndpointAddress;
	}

	/* support super speed hardware */
	if (gadget_is_superspeed(c->cdev->gadget)) {
		sdb_superspeed_in_desc.bEndpointAddress =
			sdb_fullspeed_in_desc.bEndpointAddress;
		sdb_superspeed_out_desc.bEndpointAddress =
			sdb_fullspeed_out_desc.bEndpointAddress;
	}

	DBG(cdev, "%s speed %s: IN/%s, OUT/%s\n",
			gadget_is_superspeed(c->cdev->gadget) ? "super" :
			gadget_is_dualspeed(c->cdev->gadget) ? "dual" : "full",
			f->name, dev->ep_in->name, dev->ep_out->name);
	return 0;
}

static void
sdb_function_unbind(struct usb_configuration *c, struct usb_function *f)
{
	struct sdb_dev	*dev = func_to_sdb(f);
	struct usb_request *req;


	dev->online = 0;
	dev->error = 1;

	wake_up(&dev->read_wq);

	sdb_request_free(dev->rx_req, dev->ep_out);
	while ((req = sdb_req_get(dev, &dev->tx_idle)))
		sdb_request_free(req, dev->ep_in);
}

static int sdb_function_set_alt(struct usb_function *f,
		unsigned intf, unsigned alt)
{
	struct sdb_dev	*dev = func_to_sdb(f);
	struct usb_composite_dev *cdev = f->config->cdev;
	int ret;

	DBG(cdev, "sdb_function_set_alt intf: %d alt: %d\n", intf, alt);

	ret = config_ep_by_speed(cdev->gadget, f, dev->ep_in);
	if (ret)
		return ret;

	ret = usb_ep_enable(dev->ep_in);
	if (ret)
		return ret;

	ret = config_ep_by_speed(cdev->gadget, f, dev->ep_out);
	if (ret)
		return ret;

	ret = usb_ep_enable(dev->ep_out);
	if (ret) {
		usb_ep_disable(dev->ep_in);
		return ret;
	}
	dev->online = 1;

	/* readers may be blocked waiting for us to go online */
	wake_up(&dev->read_wq);
	return 0;
}

static void sdb_function_disable(struct usb_function *f)
{
	struct sdb_dev	*dev = func_to_sdb(f);
	struct usb_composite_dev	*cdev = dev->cdev;

	DBG(cdev, "sdb_function_disable cdev %p\n", cdev);
	dev->online = 0;
	dev->error = 1;
	usb_ep_disable(dev->ep_in);
	usb_ep_disable(dev->ep_out);

	/* readers may be blocked waiting for us to go online */
	wake_up(&dev->read_wq);

	VDBG(cdev, "%s disabled\n", dev->function.name);
}

static int sdb_bind_config(struct usb_configuration *c)
{
	struct sdb_dev *dev = _sdb_dev;

	printk(KERN_INFO "sdb_bind_config\n");

	dev->cdev = c->cdev;
	dev->function.name = "sdb";
	dev->function.fs_descriptors = fs_sdb_descs;
	dev->function.hs_descriptors = hs_sdb_descs;
	dev->function.ss_descriptors = ss_sdb_descs;
	dev->function.bind = sdb_function_bind;
	dev->function.unbind = sdb_function_unbind;
	dev->function.set_alt = sdb_function_set_alt;
	dev->function.disable = sdb_function_disable;

	return usb_add_function(c, &dev->function);
}

static int sdb_setup(void)
{
	struct sdb_dev *dev;
	int ret;

	dev = kzalloc(sizeof(*dev), GFP_KERNEL);
	if (!dev)
		return -ENOMEM;

	spin_lock_init(&dev->lock);

	init_waitqueue_head(&dev->read_wq);
	init_waitqueue_head(&dev->write_wq);

	atomic_set(&dev->open_excl, 0);
	atomic_set(&dev->read_excl, 0);
	atomic_set(&dev->write_excl, 0);

	INIT_LIST_HEAD(&dev->tx_idle);

	_sdb_dev = dev;

	ret = misc_register(&sdb_device);
	if (ret)
		goto err;

	return 0;

err:
	kfree(dev);
	printk(KERN_ERR "sdb gadget driver failed to initialize\n");
	return ret;
}

static void sdb_cleanup(void)
{
	misc_deregister(&sdb_device);

	kfree(_sdb_dev);
	_sdb_dev = NULL;
}
