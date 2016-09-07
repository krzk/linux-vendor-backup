/*
 * BlueZ - Bluetooth protocol stack for Linux
 *
 * Copyright (c) 2015-2016 Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef __MGMT_TIZEN_H
#define __MGMT_TIZEN_H

#define TIZEN_OP_CODE_BASE	0xff00
#define TIZEN_EV_BASE		0xff00

#define MGMT_OP_SET_ADVERTISING_PARAMS		(TIZEN_OP_CODE_BASE + 0x01)
struct mgmt_cp_set_advertising_params {
	__le16  interval_min;
	__le16  interval_max;
	__u8 filter_policy;
	__u8 type;
} __packed;
#define MGMT_SET_ADVERTISING_PARAMS_SIZE	6

#define MGMT_OP_SET_ADVERTISING_DATA		(TIZEN_OP_CODE_BASE + 0x02)
struct mgmt_cp_set_advertising_data {
	__u8    data[HCI_MAX_AD_LENGTH];
} __packed;
#define MGMT_SET_ADVERTISING_DATA_SIZE		HCI_MAX_AD_LENGTH
#define MGMT_SET_ADV_MIN_APP_DATA_SIZE		1

#define MGMT_OP_SET_SCAN_RSP_DATA		(TIZEN_OP_CODE_BASE + 0x03)
struct mgmt_cp_set_scan_rsp_data {
	__u8    data[HCI_MAX_AD_LENGTH];
} __packed;
#define MGMT_SET_SCAN_RSP_DATA_SIZE		HCI_MAX_AD_LENGTH
#define MGMT_SET_SCAN_RSP_MIN_APP_DATA_SIZE	1

#define MGMT_OP_ADD_DEV_WHITE_LIST		(TIZEN_OP_CODE_BASE + 0x04)
struct mgmt_cp_add_dev_white_list {
	__u8	bdaddr_type;
	bdaddr_t bdaddr;
} __packed;
#define MGMT_ADD_DEV_WHITE_LIST_SIZE		7

#define MGMT_OP_REMOVE_DEV_FROM_WHITE_LIST	(TIZEN_OP_CODE_BASE + 0x05)
struct mgmt_cp_remove_dev_from_white_list {
	__u8	bdaddr_type;
	bdaddr_t bdaddr;
} __packed;
#define MGMT_REMOVE_DEV_FROM_WHITE_LIST_SIZE	7

#define MGMT_OP_CLEAR_DEV_WHITE_LIST		(TIZEN_OP_CODE_BASE + 0x06)
#define MGMT_OP_CLEAR_DEV_WHITE_LIST_SIZE	0

/* For RSSI monitoring */
#define MGMT_OP_SET_RSSI_ENABLE			(TIZEN_OP_CODE_BASE + 0x07)
#define MGMT_SET_RSSI_ENABLE_SIZE		10

struct mgmt_cp_set_enable_rssi {
	__s8	low_th;
	__s8	in_range_th;
	__s8	high_th;
	bdaddr_t	bdaddr;
	__s8	link_type;
} __packed;

struct mgmt_cc_rsp_enable_rssi {
	__u8	status;
	__u8	le_ext_opcode;
	bdaddr_t	bt_address;
	__s8	link_type;
} __packed;

#define MGMT_OP_GET_RAW_RSSI			(TIZEN_OP_CODE_BASE + 0x08)
#define MGMT_GET_RAW_RSSI_SIZE			7

struct mgmt_cp_get_raw_rssi {
	bdaddr_t bt_address;
	__u8	link_type;
} __packed;

#define MGMT_OP_SET_RSSI_DISABLE		(TIZEN_OP_CODE_BASE + 0x09)
#define MGMT_SET_RSSI_DISABLE_SIZE		7
struct mgmt_cp_disable_rssi {
	bdaddr_t	bdaddr;
	__u8	link_type;
} __packed;
struct mgmt_cc_rp_disable_rssi {
	__u8	status;
	__u8	le_ext_opcode;
	bdaddr_t	bt_address;
	__s8	link_type;
} __packed;
/* RSSI monitoring */

/* For le discovery */
#define MGMT_OP_START_LE_DISCOVERY		(TIZEN_OP_CODE_BASE + 0x0a)
struct mgmt_cp_start_le_discovery {
	__u8	type;
} __packed;
#define MGMT_START_LE_DISCOVERY_SIZE		1

#define MGMT_OP_STOP_LE_DISCOVERY		(TIZEN_OP_CODE_BASE + 0x0b)
struct mgmt_cp_stop_le_discovery {
	__u8	type;
} __packed;
#define MGMT_STOP_LE_DISCOVERY_SIZE		1
/* le discovery */

/* For LE auto connection */
#define MGMT_OP_DISABLE_LE_AUTO_CONNECT		(TIZEN_OP_CODE_BASE + 0x0c)
#define MGMT_DISABLE_LE_AUTO_CONNECT_SIZE	0
/* LE auto connection */

/* For Add LE connection parameter update procedure */
#define MGMT_LE_CONN_UPDATE_SIZE		14
#define MGMT_OP_LE_CONN_UPDATE			(TIZEN_OP_CODE_BASE + 0x0d)
struct mgmt_cp_le_conn_update {
	__le16	conn_interval_min;
	__le16	conn_interval_max;
	__le16	conn_latency;
	__le16	supervision_timeout;
	bdaddr_t	bdaddr;
} __packed;
/* Add LE connection parameter update procedure */

/* For Set Manufacturer Data */
#define MGMT_OP_SET_MANUFACTURER_DATA		(TIZEN_OP_CODE_BASE + 0x0e)
struct mgmt_cp_set_manufacturer_data {
	__u8	data[100];
} __packed;
#define MGMT_SET_MANUFACTURER_DATA_SIZE		100
/* Set Manufacturer Data */

#define MGMT_OP_LE_SET_SCAN_PARAMS		(TIZEN_OP_CODE_BASE + 0x0f)
struct mgmt_cp_le_set_scan_params {
	__u8	type;	/* le scan type */
	__le16	interval;
	__le16	window;
} __packed;
#define MGMT_LE_SET_SCAN_PARAMS_SIZE		5

/* EVENTS */

/* For device name update changes */
#define MGMT_EV_DEVICE_NAME_UPDATE		(TIZEN_EV_BASE + 0x01)
struct mgmt_ev_device_name_update {
	struct mgmt_addr_info addr;
	__le16	eir_len;
	__u8	eir[0];
} __packed;
/* Device name update changes */

/* For handling of hardware error event */
#define MGMT_EV_HARDWARE_ERROR			(TIZEN_EV_BASE + 0x02)
struct mgmt_ev_hardware_error {
	__u8	error_code;
} __packed;
/* handling of hardware error event */

/* For handling of RSSI Events */
#define MGMT_EV_RSSI_ALERT			(TIZEN_EV_BASE + 0x04)
struct mgmt_ev_vendor_specific_rssi_alert {
	bdaddr_t	bdaddr;
	__s8	link_type;
	__s8	alert_type;
	__s8	rssi_dbm;
} __packed;

#define MGMT_EV_RAW_RSSI			(TIZEN_EV_BASE + 0x05)
struct mgmt_cc_rp_get_raw_rssi {
	__u8	status;
	__s8	rssi_dbm;
	__u8	link_type;
	bdaddr_t	bt_address;
} __packed;

#define MGMT_EV_RSSI_ENABLED			(TIZEN_EV_BASE + 0x06)

#define MGMT_EV_RSSI_DISABLED			(TIZEN_EV_BASE + 0x07)
/* Handling of RSSI Events */

/* For Add LE connection update Events */
#define MGMT_EV_CONN_UPDATED			(TIZEN_EV_BASE + 0x08)
struct mgmt_ev_conn_updated {
	struct	mgmt_addr_info addr;
	__le16	conn_interval;
	__le16	conn_latency;
	__le16	supervision_timeout;
} __packed;

#define MGMT_EV_CONN_UPDATE_FAILED		(TIZEN_EV_BASE + 0x09)
struct mgmt_ev_conn_update_failed {
	struct	mgmt_addr_info addr;
	__u8	status;
} __packed;
/* Add LE connection update Events */

#endif	/* __MGMT_TIZEN_H */
