/* linux/drivers/modem/modem.c
 *
 * Copyright (C) 2010 Google, Inc.
 * Copyright (C) 2010 Samsung Electronics.
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

#include <linux/init.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/miscdevice.h>

#include <linux/uaccess.h>
#include <linux/fs.h>
#include <linux/io.h>
#include <linux/wait.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/mutex.h>
#include <linux/irq.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#ifdef CONFIG_HAS_WAKELOCK
#include <linux/wakelock.h>
#endif
#include <linux/rbtree.h>
#include <linux/of.h>
#include <linux/of_gpio.h>

#include <linux/platform_data/modem.h>
#include "modem_prj.h"
#include "modem_variation.h"
#include "modem_utils.h"

#define FMT_WAKE_TIME   (HZ/2)
#define RFS_WAKE_TIME   (HZ*3)
#define RAW_WAKE_TIME   (HZ*6)

/* pdp for SLP, rmnet for others */
#define NET_DEVICE_NAME(n) "pdp"#n

/* umts target platform data */
static struct modem_io_t umts_io_devices[] = {
	[0] = {
		.name = "umts_ipc0",
		.id = 0x1,
		.format = IPC_FMT,
		.io_type = IODEV_MISC,
		.links = LINKTYPE(LINKDEV_HSIC),
	},
	[1] = {
		.name = "umts_rfs0",
		.id = 0x41,
		.format = IPC_RFS,
		.io_type = IODEV_MISC,
		.links = LINKTYPE(LINKDEV_HSIC),
	},
	[2] = {
		.name = "umts_boot0",
		.id = 0x0,
		.format = IPC_BOOT,
		.io_type = IODEV_MISC,
		.links = LINKTYPE(LINKDEV_HSIC),
	},
	[3] = {
		.name = "multipdp",
		.id = 0x1,
		.format = IPC_MULTI_RAW,
		.io_type = IODEV_DUMMY,
		.links = LINKTYPE(LINKDEV_HSIC),
	},
	[4] = {
		.name = NET_DEVICE_NAME(0),
		.id = 0x2A,
		.format = IPC_RAW,
		.io_type = IODEV_NET,
		.links = LINKTYPE(LINKDEV_HSIC),
	},
	[5] = {
		.name = NET_DEVICE_NAME(1) ,
		.id = 0x2B,
		.format = IPC_RAW,
		.io_type = IODEV_NET,
		.links = LINKTYPE(LINKDEV_HSIC),
	},
	[6] = {
		.name = NET_DEVICE_NAME(2),
		.id = 0x2C,
		.format = IPC_RAW,
		.io_type = IODEV_NET,
		.links = LINKTYPE(LINKDEV_HSIC),
	},
	[7] = {
		.name = "umts_router",
		.id = 0x39,
		.format = IPC_RAW,
		.io_type = IODEV_MISC,
		.links = LINKTYPE(LINKDEV_HSIC),
	},
	[8] = {
		.name = "umts_csd",
		.id = 0x21,
		.format = IPC_RAW,
		.io_type = IODEV_MISC,
		.links = LINKTYPE(LINKDEV_HSIC),
	},
	[9] = {
		.name = "umts_ramdump0",
		.id = 0x0,
		.format = IPC_RAMDUMP,
		.io_type = IODEV_MISC,
		.links = LINKTYPE(LINKDEV_HSIC),
	},
	[10] = {
		.name = "umts_loopback0",
		.id = 0x3f,
		.format = IPC_RAW,
		.io_type = IODEV_MISC,
		.links = LINKTYPE(LINKDEV_HSIC),
	},
};

static struct modem_shared *create_modem_shared_data(void)
{
	struct modem_shared *msd;

	msd = kzalloc(sizeof(struct modem_shared), GFP_KERNEL);
	if (!msd)
		return NULL;

	/* initialize link device list */
	INIT_LIST_HEAD(&msd->link_dev_list);

	/* initialize tree of io devices */
	msd->iodevs_tree_chan = RB_ROOT;
	msd->iodevs_tree_fmt = RB_ROOT;

	msd->storage.cnt = 0;
	msd->storage.addr =
		kzalloc(MAX_MIF_BUFF_SIZE + MAX_MIF_SEPA_SIZE, GFP_KERNEL);
	if (!msd->storage.addr) {
		mif_err("IPC logger buff alloc failed!!\n");
		return NULL;
	}
	memset(msd->storage.addr, 0, MAX_MIF_BUFF_SIZE);
	memcpy(msd->storage.addr, MIF_SEPARATOR, MAX_MIF_SEPA_SIZE);
	msd->storage.addr += MAX_MIF_SEPA_SIZE;
	spin_lock_init(&msd->lock);

	return msd;
}

static struct modem_ctl *create_modemctl_device(struct platform_device *pdev,
		struct modem_shared *msd)
{
	int ret = 0;
	struct modem_data *pdata;
	struct modem_ctl *modemctl;
	struct device *dev = &pdev->dev;

	/* create modem control device */
	modemctl = kzalloc(sizeof(struct modem_ctl), GFP_KERNEL);
	if (!modemctl)
		return NULL;

	modemctl->msd = msd;
	modemctl->dev = dev;
	modemctl->phone_state = STATE_OFFLINE;

	pdata = pdev->dev.platform_data;
	modemctl->mdm_data = pdata;
	modemctl->name = pdata->name;

	/* init modemctl device for getting modemctl operations */
	ret = call_modem_init_func(modemctl, pdata);
	if (ret) {
		kfree(modemctl);
		return NULL;
	}

	mif_info("%s is created!!!\n", pdata->name);

	return modemctl;
}

static struct io_device *create_io_device(struct modem_io_t *io_t,
		struct modem_shared *msd, struct modem_ctl *modemctl,
		struct modem_data *pdata)
{
	int ret = 0;
	struct io_device *iod = NULL;

	iod = kzalloc(sizeof(struct io_device), GFP_KERNEL);
	if (!iod) {
		mif_err("iod == NULL\n");
		return NULL;
	}

	RB_CLEAR_NODE(&iod->node_chan);
	RB_CLEAR_NODE(&iod->node_fmt);

	iod->name = io_t->name;
	iod->id = io_t->id;
	iod->format = io_t->format;
	iod->io_typ = io_t->io_type;
	iod->link_types = io_t->links;
	iod->net_typ = pdata->modem_net;
	iod->use_handover = pdata->use_handover;
	iod->ipc_version = pdata->ipc_version;
	atomic_set(&iod->opened, 0);

	/* link between io device and modem control */
	iod->mc = modemctl;
	if (iod->format == IPC_FMT)
		modemctl->iod = iod;
	if (iod->format == IPC_BOOT) {
		modemctl->bootd = iod;
		mif_info("Bood device = %s\n", iod->name);
	}

	/* link between io device and modem shared */
	iod->msd = msd;

	/* add iod to rb_tree */
	if (iod->format != IPC_RAW)
		insert_iod_with_format(msd, iod->format, iod);

	if (sipc4_is_not_reserved_channel(iod->id))
		insert_iod_with_channel(msd, iod->id, iod);

	/* register misc device or net device */
	ret = sipc4_init_io_device(iod);
	if (ret) {
		kfree(iod);
		mif_err("sipc4_init_io_device fail (%d)\n", ret);
		return NULL;
	}

	mif_debug("%s is created!!!\n", iod->name);
	return iod;
}

static int attach_devices(struct io_device *iod, enum modem_link tx_link)
{
	struct modem_shared *msd = iod->msd;
	struct link_device *ld;

	/* find link type for this io device */
	list_for_each_entry(ld, &msd->link_dev_list, list) {
		if (IS_CONNECTED(iod, ld)) {
			/* The count 1 bits of iod->link_types is count
			 * of link devices of this iod.
			 * If use one link device,
			 * or, 2+ link devices and this link is tx_link,
			 * set iod's link device with ld
			 */
			if ((countbits(iod->link_types) <= 1) ||
					(tx_link == ld->link_type)) {
				mif_debug("set %s->%s\n", iod->name, ld->name);

				set_current_link(iod, ld);

				if (iod->ipc_version == SIPC_VER_42) {
					if (iod->format == IPC_FMT) {
						int ch = iod->id & 0x03;
						ld->fmt_iods[ch] = iod;
					}
				}
			}
		}
	}

	/* if use rx dynamic switch, set tx_link at modem_io_t of
	 * board-*-modems.c
	 */
	if (!get_current_link(iod)) {
		mif_err("%s->link == NULL\n", iod->name);
		BUG();
	}

	switch (iod->format) {
	case IPC_FMT:
#ifdef CONFIG_HAS_WAKELOCK
		wake_lock_init(&iod->wakelock, WAKE_LOCK_SUSPEND, iod->name);
#else
		device_init_wakeup(iod->miscdev.this_device, true);
#endif
		iod->waketime = FMT_WAKE_TIME;
		break;

	case IPC_RFS:
#ifdef CONFIG_HAS_WAKELOCK
		wake_lock_init(&iod->wakelock, WAKE_LOCK_SUSPEND, iod->name);
#else
		device_init_wakeup(iod->miscdev.this_device, true);
#endif
		iod->waketime = RFS_WAKE_TIME;
		break;

	case IPC_MULTI_RAW:
#ifdef CONFIG_HAS_WAKELOCK
		wake_lock_init(&iod->wakelock, WAKE_LOCK_SUSPEND, iod->name);
#else
		device_init_wakeup(iod->miscdev.this_device, true);
#endif
		iod->waketime = RAW_WAKE_TIME;
		break;
	case IPC_BOOT:
#ifdef CONFIG_HAS_WAKELOCK
		wake_lock_init(&iod->wakelock, WAKE_LOCK_SUSPEND, iod->name);
#else
		device_init_wakeup(iod->miscdev.this_device, true);
#endif
		iod->waketime = 3 * HZ;
	default:
		break;
	}

	return 0;
}

static struct modem_data *modem_parse_dt(struct platform_device *pdev, struct device_node *np)
{
	struct modem_data *mc;
	int err = 0;

	mc = kzalloc(sizeof(struct modem_data), GFP_KERNEL);
	if (!mc) {
		mif_err("Insufficent memory to allocate struct modem_data\n");
		err = -ENOMEM;
		goto dt_parse_err;
	}

	mc->link_pm_data = kzalloc(sizeof(struct modemlink_pm_data), GFP_KERNEL);
	if (!mc->link_pm_data) {
		mif_err("Insufficent memory to allocate struct modem_data\n");
		err = -ENOMEM;
		goto dt_parse_err;
	}

	mc->gpio_reset_req_n = of_get_named_gpio(np, "reset-req-gpio", 0);
	if (!gpio_is_valid(mc->gpio_reset_req_n)) {
		mif_err("failed to get reset-req gpio\n");
		err = -EINVAL;
		goto dt_parse_err;
	}

	mc->gpio_cp_on = of_get_named_gpio(np, "cp-on-gpio", 0);
	if (!gpio_is_valid(mc->gpio_cp_on)) {
		mif_err("failed to get cp-on gpio\n");
		err = -EINVAL;
		goto dt_parse_err;
	}

	mc->gpio_cp_reset = of_get_named_gpio(np, "cp-reset-gpio", 0);
	if (!gpio_is_valid(mc->gpio_cp_reset)) {
		mif_err("failed to get cp-reset gpio\n");
		err = -EINVAL;
		goto dt_parse_err;
	}

	mc->gpio_pda_active = of_get_named_gpio(np, "pda-active-gpio", 0);
	if (!gpio_is_valid(mc->gpio_pda_active)) {
		mif_err("failed to get pda-active gpio\n");
		err = -EINVAL;
		goto dt_parse_err;
	}

	mc->gpio_phone_active = of_get_named_gpio(np, "phone-active-gpio", 0);
	if (!gpio_is_valid(mc->gpio_phone_active)) {
		mif_err("failed to get phone-active gpio\n");
		err = -EINVAL;
		goto dt_parse_err;
	}

	mc->gpio_cp_dump_int = of_get_named_gpio(np, "cp-dump-int-gpio", 0);
	if (!gpio_is_valid(mc->gpio_cp_dump_int)) {
		mif_err("failed to get cp-dump-int gpio\n");
		err = -EINVAL;
		goto dt_parse_err;
	}

	mc->link_pm_data->gpio_link_slavewake = of_get_named_gpio(np,
						"link-slavewake-gpio", 0);
	if (!gpio_is_valid(mc->link_pm_data->gpio_link_slavewake)) {
		mif_err("failed to get link-slavewak gpio\n");
		err = -EINVAL;
		goto dt_parse_err;
	}

	mc->link_pm_data->gpio_link_hostwake = of_get_named_gpio(np,
						"link-hostwake-gpio", 0);
	if (!gpio_is_valid(mc->link_pm_data->gpio_link_hostwake)) {
		mif_err("failed to get link-hostwake gpio\n");
		err = -EINVAL;
		goto dt_parse_err;
	}

	mc->link_pm_data->gpio_link_active = of_get_named_gpio(np, "link-active-gpio", 0);
	if (!gpio_is_valid(mc->link_pm_data->gpio_link_active)) {
		mif_err("failed to get link-active gpio\n");
		err = -EINVAL;
		goto dt_parse_err;
	}

	mc->link_pm_data->gpio_link_enable = of_get_named_gpio(np, "link-enable-gpio", 0);

	/* link enable is optional */
	if (gpio_is_valid(mc->link_pm_data->gpio_link_enable))
		devm_gpio_request(&pdev->dev, mc->link_pm_data->gpio_link_enable,
				"gpio_link_enable");
	else
		mc->link_pm_data->gpio_link_enable = 0;

	devm_gpio_request(&pdev->dev, mc->gpio_reset_req_n, "gpio_reset_req_n");
	devm_gpio_request(&pdev->dev, mc->gpio_cp_on, "gpio_cp_on");
	devm_gpio_request(&pdev->dev, mc->gpio_cp_reset, "gpio_cp_reset");
	devm_gpio_request(&pdev->dev, mc->gpio_pda_active, "gpio_pda_active");
	devm_gpio_request(&pdev->dev, mc->gpio_phone_active, "gpio_phone_active");
	devm_gpio_request(&pdev->dev, mc->gpio_cp_dump_int, "gpio_cp_dump_int");
	devm_gpio_request(&pdev->dev, mc->link_pm_data->gpio_link_slavewake, "gpio_link_slavewake");
	devm_gpio_request(&pdev->dev, mc->link_pm_data->gpio_link_hostwake, "gpio_link_hostwake");
	devm_gpio_request(&pdev->dev, mc->link_pm_data->gpio_link_active, "gpio_link_active");

	platform_set_drvdata(pdev, mc);

	return mc;

dt_parse_err:
	if (mc && mc->link_pm_data)
		kfree(mc->link_pm_data);
	if (mc)
		kfree(mc);

	return ERR_PTR(err);
}

static int modem_probe(struct platform_device *pdev)
{
	int i;
	struct modem_data *pdata = pdev->dev.platform_data;
	struct modem_shared *msd = NULL;
	struct modem_ctl *modemctl = NULL;
	struct io_device *iod[ARRAY_SIZE(umts_io_devices)];
	struct link_device *ld;

	mif_err("%s\n", pdev->name);
	memset(iod, 0, sizeof(iod));

	msd = create_modem_shared_data();
	if (!msd) {
		mif_err("msd == NULL\n");
		goto err_free_modemctl;
	}


	pdata = modem_parse_dt(pdev, pdev->dev.of_node);
	if (IS_ERR(pdata)) {
		mif_err("Failed to parse device tree\n");
		kfree(msd);
		return -ENOMEM;
	}

	pdev->dev.platform_data = pdata;

	pdata->num_iodevs = ARRAY_SIZE(umts_io_devices);
	pdata->iodevs = umts_io_devices;
	pdata->modem_type = 1;
	pdata->name = "xmm6262";
	pdata->link_types = LINKTYPE(LINKDEV_HSIC);

	modemctl = create_modemctl_device(pdev, msd);
	if (!modemctl) {
		mif_err("modemctl == NULL\n");
		goto err_free_modemctl;
	}

	/* create link device */
	/* support multi-link device */
	for (i = 0; i < LINKDEV_MAX ; i++) {
		/* find matching link type */
		if (pdata->link_types & LINKTYPE(i)) {
			ld = call_link_init_func(pdev, i);
			if (!ld)
				goto err_free_modemctl;

			mif_err("link created: %s\n", ld->name);
			ld->link_type = i;
			ld->mc = modemctl;
			ld->msd = msd;
			list_add(&ld->list, &msd->link_dev_list);
		}
	}

	/* create io deivces and connect to modemctl device */
	for (i = 0; i < pdata->num_iodevs; i++) {
		iod[i] = create_io_device(&pdata->iodevs[i], msd, modemctl,
				pdata);
		if (!iod[i]) {
			mif_err("iod[%d] == NULL\n", i);
			goto err_free_modemctl;
		}

		attach_devices(iod[i], pdata->iodevs[i].tx_link);
	}

	platform_set_drvdata(pdev, modemctl);

	mif_info("Complete!!!\n");
	return 0;

err_free_modemctl:
	for (i = 0; i < pdata->num_iodevs; i++)
		if (iod[i] != NULL)
			kfree(iod[i]);

	if (modemctl != NULL)
		kfree(modemctl);

	if (msd != NULL)
		kfree(msd);

	return -ENOMEM;
}

static void modem_shutdown(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct modem_ctl *mc = dev_get_drvdata(dev);
	mc->ops.modem_off(mc);
	mc->phone_state = STATE_OFFLINE;
}

static int modem_suspend(struct device *pdev)
{
#ifndef CONFIG_LINK_DEVICE_HSIC
	struct modem_ctl *mc = dev_get_drvdata(pdev);

	if (mc->gpio_pda_active)
		gpio_set_value(mc->gpio_pda_active, 0);
#endif

	return 0;
}

static int modem_resume(struct device *pdev)
{
#ifndef CONFIG_LINK_DEVICE_HSIC
	struct modem_ctl *mc = dev_get_drvdata(pdev);

	if (mc->gpio_pda_active)
		gpio_set_value(mc->gpio_pda_active, 1);
#endif

	return 0;
}

static const struct dev_pm_ops modem_pm_ops = {
	.suspend    = modem_suspend,
	.resume     = modem_resume,
};

static const struct of_device_id modem_of_match[] = {
	{ .compatible = "samsung,modem_if" },
	{},
};
MODULE_DEVICE_TABLE(of, modem_of_match);

static struct platform_driver modem_driver = {
	.probe = modem_probe,
	.shutdown = modem_shutdown,
	.driver = {
		.of_match_table = modem_of_match,
		.name = "modem_if",
		.pm   = &modem_pm_ops,
	},
};

static int __init modem_init(void)
{
	return platform_driver_register(&modem_driver);
}

module_init(modem_init);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Samsung Modem Interface Driver");
