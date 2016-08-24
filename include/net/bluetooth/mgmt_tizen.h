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

#endif	/* __MGMT_TIZEN_H */
