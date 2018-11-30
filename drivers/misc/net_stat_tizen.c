/* drivers/misc/net_stat_tizen.c
 *
 * Copyright (C) 2018 SAMSUNG, Inc.
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
 *******************************************************************************
 *                                  HISTORY                                    *
 *******************************************************************************
 * ver   who                                         what                      *
 * ---- -------------------------------------------- ------------------------- *
 * 1.0   Junho Jang <vincent.jang@samsung.com>       <2018>                    *
 *                                                   Initial Release           *
 * ---- -------------------------------------------- ------------------------- *
 */

#include <linux/init.h>
#include <linux/debugfs.h>
#include <linux/spinlock.h>
#include <linux/slab.h>
#include <linux/suspend.h>
#include <linux/uaccess.h>
#include <linux/suspend.h>

#include <linux/net_stat_tizen.h>

#ifdef CONFIG_ENERGY_MONITOR
#include <linux/sort.h>
#include <linux/power/energy_monitor.h>
#endif

#define NET_STAT_TIZEN_PREFIX	"net_stat_tizen: "

/* TODO:  Currently we only support 9 net devices */
#define MAX_NET_DEV 10

struct net_stat_ndev {
	const char *ndev;
	int type;
};

struct net_stat_bt {
	ktime_t active_time;
	ktime_t tx_time;
	ktime_t rx_time;
};

struct net_stat_wifi {
	ktime_t total_scan_time;
	ktime_t last_time;
	unsigned int scan_req;
	unsigned int scan_done;
	unsigned int scan_abort;
	unsigned int roaming_done;
	unsigned int connection;
	unsigned int connection_fail;
	unsigned int disconnection;
};

struct net_stat_ndev_stat {
	struct net_device *ndev;
	unsigned up;
	unsigned down;
	struct net_stat_bt bt_stat;
	struct net_stat_wifi wifi_stat;

#ifdef CONFIG_ENERGY_MONITOR
	unsigned emon_up;
	unsigned emon_down;
	struct rtnl_link_stats64 emon_ndev_stats;
	struct net_stat_bt emon_bt_stat;
	struct net_stat_wifi emon_wifi_stat;
#endif
};

struct net_stat_tizen {
	unsigned int num_registered_ndev;

	unsigned int support_ndev_table_count;
	struct net_stat_ndev *support_ndev_table;

	struct net_stat_ndev_stat ndev_stat[MAX_NET_DEV];
};

static struct net_stat_tizen net_stat;

static DEFINE_SPINLOCK(net_stat_tizen_lock);


static struct net_stat_ndev_stat *net_stat_tizen_get_ndev_stat(
							struct net_device *dev)
{
	int i;

	for (i = 0; i < net_stat.num_registered_ndev; i++) {
		if (net_stat.ndev_stat[i].ndev)
			if (strcmp(net_stat.ndev_stat[i].ndev->name, dev->name) == 0)
				return &net_stat.ndev_stat[i];
	}

	return NULL;
}

static int net_stat_tizen_is_support_ndev(
							struct net_device *dev)
{
	int i;

	for (i = 0; i < net_stat.support_ndev_table_count; i++) {
		if (net_stat.support_ndev_table[i].ndev)
			if (strcmp(net_stat.support_ndev_table[i].ndev, dev->name) == 0)
				return 1;
	}

	return 0;
}

int net_stat_tizen_register(struct net_device *dev)
{
	struct net_stat_ndev_stat *stat;
	int err = 0;

	if (!dev) {
		err = -EINVAL;
		goto fail;
	}

	if (!net_stat_tizen_is_support_ndev(dev)) {
		err = -EPERM;
		goto fail;
	}

	if (net_stat.num_registered_ndev >= MAX_NET_DEV) {
		err = -ENOMEM;
		goto fail;
	}

	stat = &net_stat.ndev_stat[net_stat.num_registered_ndev];

	memset(stat, 0, sizeof(*stat));
	stat->ndev = dev;

	pr_info(NET_STAT_TIZEN_PREFIX
		"register success: index=%d\n", net_stat.num_registered_ndev);

	net_stat.num_registered_ndev++;

	return err;
fail:
	pr_info(NET_STAT_TIZEN_PREFIX"register failed: err=%d\n", err);

	return err;
}

int net_stat_tizen_unregister(struct net_device *dev)
{
	struct net_stat_ndev_stat *stat;
	int index = 0;
	int err = 0;

	if (!dev) {
		err = -EINVAL;
		goto fail;
	}

	stat = net_stat_tizen_get_ndev_stat(dev);
	if (!stat) {
		err = -ENODEV;
		goto fail;
	}

	stat->ndev = NULL;
	net_stat.num_registered_ndev--;

	pr_info(NET_STAT_TIZEN_PREFIX
		"unregister success: index=%d\n", index);

	return err;
fail:
	pr_info(NET_STAT_TIZEN_PREFIX"unregister failed: err=%d\n", err);

	return err;
}

int net_stat_tizen_update_wifi(struct net_device *dev,
	enum net_stat_wifi_event event)
{
	struct net_stat_ndev_stat *stat;
	struct net_stat_wifi *wifi_stat;
	ktime_t now;
	ktime_t duration;
	int err = 0;

	if (!dev) {
		err = -EINVAL;
		goto fail;
	}

	stat = net_stat_tizen_get_ndev_stat(dev);
	if (!stat) {
		err = -ENODEV;
		goto fail;
	}

	pr_info("%s: %s: %d\n", __func__, dev->name, event);

	wifi_stat = &stat->wifi_stat;

	now = ktime_get();
	switch (event) {
	case WIFISTAT_SCAN_REQ:
		wifi_stat->last_time = now;
		wifi_stat->scan_req++;
		break;
	case WIFISTAT_SCAN_DONE:
		duration = ktime_sub(now, wifi_stat->last_time);
		wifi_stat->total_scan_time =
			ktime_add(wifi_stat->total_scan_time, duration);
		wifi_stat->last_time = now;
		wifi_stat->scan_done++;
		break;
	case WIFISTAT_SCAN_ABORT:
		duration = ktime_sub(now, wifi_stat->last_time);
		wifi_stat->total_scan_time =
			ktime_add(wifi_stat->total_scan_time, duration);
		wifi_stat->last_time = now;
		wifi_stat->scan_abort++;
		break;
	case WIFISTAT_ROAMING_DONE:
		wifi_stat->roaming_done++;
		break;
	case WIFISTAT_CONNECTION:
		wifi_stat->connection++;
		break;
	case WIFISTAT_CONNECTION_FAIL:
		wifi_stat->connection_fail++;
		break;
	case WIFISTAT_DISCONNECTION:
		wifi_stat->disconnection++;
		break;
	default:
		break;
	}

	return err;
fail:
	pr_info(NET_STAT_TIZEN_PREFIX"%s: err=%d\n", __func__, err);

	return err;
}

static int net_stat_tizen_update_bt(struct net_stat_ndev_stat *stat)
{
	if (!stat)
		return -EINVAL;


	stat->bt_stat.active_time = pm_get_total_active_time("BT_sniff_wake");

	return 0;
}

static int net_stat_tizen_show(struct seq_file *m, void *v)
{
	struct net_stat_ndev_stat *stat;
	struct rtnl_link_stats64 temp;
	const struct rtnl_link_stats64 *ndev_stats;
	int i;

	seq_printf(m, "name          up          down        "
		"tx_bytes      tx_packets    rx_bytes      rx_packets    multicast     "
		"connection    conn_fail     disconnection "
		"roaming_done  scan_req      scan_done     scan_abort    "
		"scan_time     last_time\n");

	for (i = 0; i < net_stat.num_registered_ndev; i++) {
		stat = &net_stat.ndev_stat[i];
		if (stat->ndev) {
			if (stat->ndev->reg_state <= NETREG_REGISTERED) {
				ndev_stats = dev_get_stats(stat->ndev, &temp);

				seq_printf(m, "%-12s  %-10u  %-10u  "
							"%-12llu  %-12llu  %-12llu  %-12llu  %-12llu  "
							"%-12u  %-12u  %-12u  "
							"%-12u  %-12u  %-12u  %-12u  "
							"%-12llu  %-12llu\n",
							stat->ndev->name, stat->up, stat->down,
							ndev_stats->tx_bytes, ndev_stats->tx_packets,
							ndev_stats->rx_bytes, ndev_stats->rx_packets,
							ndev_stats->multicast,
							stat->wifi_stat.connection,
							stat->wifi_stat.connection_fail,
							stat->wifi_stat.disconnection,
							stat->wifi_stat.roaming_done,
							stat->wifi_stat.scan_req,
							stat->wifi_stat.scan_done,
							stat->wifi_stat.scan_abort,
							ktime_to_ms(stat->wifi_stat.total_scan_time),
							ktime_to_ms(stat->wifi_stat.last_time));
			} else
				seq_printf(m, "-\n");
		}
	}

	return 0;
}

static int net_stat_tizen_open(struct inode *inode, struct file *filp)
{
	return single_open(filp, net_stat_tizen_show, NULL);
}

static const struct file_operations net_stat_tizen_fops = {
	.open       = net_stat_tizen_open,
	.read       = seq_read,
	.llseek     = seq_lseek,
	.release    = single_release,
};

static int net_stat_tizen_netdev_event(struct notifier_block *this,
			     unsigned long event, void *ptr)
{
	struct net_device *dev = netdev_notifier_info_to_dev(ptr);
	struct net_stat_ndev_stat *stat;

	stat = net_stat_tizen_get_ndev_stat(dev);
	if (!stat)
		return NOTIFY_DONE;

	pr_info("%s: %s: %lu\n", __func__, dev->name, event);

	switch (event) {
	case NETDEV_UP:
		stat->up++;
		break;
	case NETDEV_DOWN:
		stat->down++;
		break;
	default:
		break;
	}
	return NOTIFY_DONE;
}

static struct notifier_block netdev_notifier = {
	.notifier_call = net_stat_tizen_netdev_event,
};

#ifdef CONFIG_ENERGY_MONITOR
int net_stat_tizen_get_stat(int type,
 	struct net_stat_tizen_emon *emon_stat, size_t n)
{
	struct net_stat_ndev_stat *stat;
	struct rtnl_link_stats64 temp;
	const struct rtnl_link_stats64 *ndev_stats;
	unsigned long flags;
	int i;

	if (n > MAX_NET_DEV)
		return -EINVAL;

	if (!emon_stat)
		return -EINVAL;

	memset(emon_stat, 0, sizeof(struct net_stat_tizen_emon) * n);

	for (i = 0; i < n; i++) {
		stat = &net_stat.ndev_stat[i];
		if (!stat->ndev)
			continue;

		ndev_stats = dev_get_stats(stat->ndev, &temp);
		if (!ndev_stats)
			continue;

		if (strncmp(stat->ndev->name, "dummy0", 6) == 0)
			net_stat_tizen_update_bt(stat);

		emon_stat[i].state = stat->up - stat->down;
		emon_stat[i].up = stat->up - stat->emon_up;
		emon_stat[i].down = stat->down - stat->emon_down;

		emon_stat[i].rx_packets = ndev_stats->rx_packets -
			stat->emon_ndev_stats.rx_packets;
		emon_stat[i].tx_packets = ndev_stats->tx_packets -
			stat->emon_ndev_stats.tx_packets;
		emon_stat[i].rx_bytes = ndev_stats->rx_bytes -
			stat->emon_ndev_stats.rx_bytes;
		emon_stat[i].tx_bytes = ndev_stats->tx_bytes -
			stat->emon_ndev_stats.tx_bytes;

		emon_stat[i].bt_active_time = ktime_sub(stat->bt_stat.active_time,
			stat->emon_bt_stat.active_time);
		emon_stat[i].bt_tx_time = ktime_sub(stat->bt_stat.tx_time,
			stat->emon_bt_stat.tx_time);
		emon_stat[i].bt_rx_time = ktime_sub(stat->bt_stat.rx_time,
			stat->emon_bt_stat.rx_time);

		emon_stat[i].scan_time = ktime_sub(stat->wifi_stat.total_scan_time,
			stat->emon_wifi_stat.total_scan_time);
		emon_stat[i].scan_req = stat->wifi_stat.scan_req -
			stat->emon_wifi_stat.scan_req;
		emon_stat[i].scan_done = stat->wifi_stat.scan_done -
			stat->emon_wifi_stat.scan_done;
		emon_stat[i].scan_abort = stat->wifi_stat.scan_abort -
			stat->emon_wifi_stat.scan_abort;
		emon_stat[i].roaming_done = stat->wifi_stat.roaming_done -
			stat->emon_wifi_stat.roaming_done;

		emon_stat[i].wifi_state = stat->wifi_stat.connection -
			stat->wifi_stat.disconnection;
		emon_stat[i].connection = stat->wifi_stat.connection -
			stat->emon_wifi_stat.connection;
		emon_stat[i].connection_fail = stat->wifi_stat.connection_fail -
			stat->emon_wifi_stat.connection_fail;
		emon_stat[i].disconnection = stat->wifi_stat.disconnection -
			stat->emon_wifi_stat.disconnection;

		if (type != ENERGY_MON_TYPE_DUMP) {
			spin_lock_irqsave(&net_stat_tizen_lock, flags);
			stat->emon_up = stat->up;
			stat->emon_down = stat->down;

			stat->emon_bt_stat.active_time = stat->bt_stat.active_time;
			stat->emon_bt_stat.tx_time = stat->bt_stat.tx_time;
			stat->emon_bt_stat.rx_time = stat->bt_stat.rx_time;

			stat->emon_wifi_stat.total_scan_time = stat->wifi_stat.total_scan_time;
			stat->emon_wifi_stat.scan_req = stat->wifi_stat.scan_req;
			stat->emon_wifi_stat.scan_done = stat->wifi_stat.scan_done;
			stat->emon_wifi_stat.scan_abort = stat->wifi_stat.scan_abort;
			stat->emon_wifi_stat.roaming_done = stat->wifi_stat.roaming_done;
			stat->emon_wifi_stat.connection = stat->wifi_stat.connection;
			stat->emon_wifi_stat.connection_fail = stat->wifi_stat.connection_fail;
			stat->emon_wifi_stat.disconnection = stat->wifi_stat.disconnection;
			spin_unlock_irqrestore(&net_stat_tizen_lock, flags);
			memcpy(&stat->emon_ndev_stats, ndev_stats, sizeof(*ndev_stats));
		}
	}

	return 0;
}
#endif

static int __init net_stat_tizen_dt_init(void)
{
	struct device_node *np;
	int i;
	int num_support_dev;
	unsigned int alloc_size;
	const char *ndev_name[MAX_NET_DEV];

	np = of_find_node_by_name(NULL, "net_stat_tizen");
	if (!np) {
		pr_err("%s:missing net_stat_tizen in DT\n", __func__);
		goto fail;
	}

	/* alloc memory for support_ndev_table */
	num_support_dev = of_property_count_strings(np, "support-dev");
	if (num_support_dev < 0) {
		pr_err("%s: missing disp_stat,support-dev in DT\n", __func__);
		goto fail;
	}
	if (num_support_dev > MAX_NET_DEV) {
		pr_err("%s: support-dev should be smaller than %d\n",
			__func__, MAX_NET_DEV);
		goto fail;
	} else
		net_stat.support_ndev_table_count = num_support_dev;

	alloc_size = num_support_dev * sizeof(struct net_stat_ndev);
	net_stat.support_ndev_table = kzalloc(alloc_size, GFP_KERNEL);
	if (!net_stat.support_ndev_table)
		goto fail;

	/* get support ndev */
	of_property_read_string_array(np, "support-dev", ndev_name, num_support_dev);
	for (i = 0; i < num_support_dev; i++) {
		net_stat.support_ndev_table[i].ndev = ndev_name[i];
		pr_info(NET_STAT_TIZEN_PREFIX"%s: support-dev[%d]=%s\n",
			__func__, i, net_stat.support_ndev_table[i].ndev);
	}

	return 0;
fail:
	kfree(net_stat.support_ndev_table);
	net_stat.support_ndev_table_count = 0;
	net_stat.support_ndev_table = NULL;
	pr_err(NET_STAT_TIZEN_PREFIX"%s: parsing fail\n", __func__);

	return -1;
}

static int __init net_stat_tizen_init(void)
{
	struct dentry *root;
	int err;

	err = net_stat_tizen_dt_init();
	if (err < 0)
		return err;

	err = register_netdevice_notifier(&netdev_notifier);
	if (err)
		return err;

	root = debugfs_create_dir("net_stat_tizen", NULL);
	if (!root) {
		pr_err(NET_STAT_TIZEN_PREFIX
			"failed to create net_stat_tizen debugfs directory\n");
		return -ENOMEM;
	}

	/* Make interface to read the wifi statistic */
	if (!debugfs_create_file("stat", 0400, root, NULL, &net_stat_tizen_fops))
		goto error_debugfs;

	return 0;

error_debugfs:
	debugfs_remove_recursive(root);

	return -1;
}

module_init(net_stat_tizen_init);
