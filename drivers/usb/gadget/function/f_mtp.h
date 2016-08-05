/*
 * Gadget Driver for MTP
 *
 * Copyright (C) 2009 Samsung Electronics.
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

#ifndef __F_MTP_H
#define __F_MTP_H

#define MTP_MAX_PACKET_LEN_FROM_APP 16

#define MTP_IOCTL_LETTER	'Z'
#define GET_HIGH_FULL_SPEED	_IOR(MTP_IOCTL_LETTER, 1, int)
#define MTP_DISABLE			_IO(MTP_IOCTL_LETTER, 2)
#define MTP_CLEAR_HALT		_IO(MTP_IOCTL_LETTER, 3)
#define MTP_WRITE_INT_DATA	_IOW(MTP_IOCTL_LETTER, 4, char *)
#define SET_MTP_USER_PID	_IOW(MTP_IOCTL_LETTER, 5, int)
#define GET_SETUP_DATA		_IOR(MTP_IOCTL_LETTER, 6, char *)
#define SET_SETUP_DATA		_IOW(MTP_IOCTL_LETTER, 7, char *)
#define SEND_RESET_ACK		_IO(MTP_IOCTL_LETTER, 8)
#define SET_ZLP_DATA		_IO(MTP_IOCTL_LETTER, 9)
#define GET_MAX_PKT_SIZE	_IOR(MTP_IOCTL_LETTER, 10, void *)
#define SIG_SETUP			44

/*PIMA15740-2000 spec*/
#define USB_PTPREQUEST_CANCELIO   0x64    /* Cancel request */
#define USB_PTPREQUEST_GETEVENT   0x65    /* Get extended event data */
#define USB_PTPREQUEST_RESET      0x66    /* Reset Device */
#define USB_PTPREQUEST_GETSTATUS  0x67    /* Get Device Status */
#define USB_PTPREQUEST_CANCELIO_SIZE 6
#define USB_PTPREQUEST_GETSTATUS_SIZE 12

struct mtp_event {
	size_t length;
	void *data;
};

int mtp_function_add(struct usb_configuration *c);
int mtp_function_config_changed(struct usb_composite_dev *cdev,
		struct usb_configuration *c);
int mtp_enable(void);
void mtp_function_enable(int enable);

struct max_pkt_size {
	unsigned int txPktSize;
	unsigned int rxPktSize;
};

struct usb_mtp_ctrlrequest {
	struct usb_ctrlrequest	setup;
};

struct usb_container_header {
	uint32_t  Length;/* the valid size, in BYTES, of the container  */
	uint16_t   Type;/* Container type */
	uint16_t   Code;/* Operation code, response code, or Event code */
	uint32_t  TransactionID;/* host generated number */
};

struct read_send_info {
	int	Fd;/* Media File fd */
	uint64_t Length;/* the valid size, in BYTES, of the container  */
	uint16_t Code;/* Operation code, response code, or Event code */
	uint32_t TransactionID;/* host generated number */
};

#endif /* __F_MTP_H */
