/*
 *  include/linux/net_stat_tizen.h
 *
 *  Copyright (C) 2018 Junho Jang <vincent.jang@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __NET_STAT_TIZEN_H
#define __NET_STAT_TIZEN_H

#include <linux/netdevice.h>

enum net_stat_wifi_event {
	WIFISTAT_SCAN_REQ,
	WIFISTAT_SCAN_DONE,
	WIFISTAT_SCAN_ABORT,
	WIFISTAT_ROAMING_DONE,
	WIFISTAT_CONNECTION,
	WIFISTAT_CONNECTION_FAIL,
	WIFISTAT_DISCONNECTION
};

struct net_stat_tizen_emon {
	unsigned int state;
	unsigned int up;
	unsigned int down;
	u64	rx_packets;
	u64	tx_packets;
	u64	rx_bytes;
	u64	tx_bytes;

	ktime_t bt_active_time;
	ktime_t bt_tx_time;
	ktime_t bt_rx_time;

	ktime_t scan_time;
	unsigned int scan_req;
	unsigned int scan_done;
	unsigned int scan_abort;
	unsigned int roaming_done;
	unsigned int wifi_state;
	unsigned int connection;
	unsigned int connection_fail;
	unsigned int disconnection;
};

#ifdef CONFIG_NET_STAT_TIZEN
extern int net_stat_tizen_register(struct net_device *dev);
extern int net_stat_tizen_unregister(struct net_device *dev);
extern int net_stat_tizen_update_wifi(struct net_device *dev,
					enum net_stat_wifi_event event);
extern int net_stat_tizen_get_stat(int type,
					struct net_stat_tizen_emon *emon_stat, size_t n);
#else
static inline int net_stat_tizen_register(struct net_device *dev)
{	return 0;}
static inline int net_stat_tizen_unregister(struct net_device *dev)
{	return 0;}
static inline int net_stat_tizen_update_wifi(struct net_device *dev,
					enum net_stat_wifi_event event)
{	return 0;}
static inline int net_stat_tizen_get_stat(int type,
					struct net_stat_tizen_emon *emon_stat, size_t n);
{	return 0;}
#endif
#endif /* __NET_STAT_TIZEN_H */

