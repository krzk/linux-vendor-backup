/*
 *  drivers/usb/gadget/menu_setting.h
 *
 * Copyright (C) 2009 Samsung Electronics Co. Ltd.
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


/////////////////////////////////////////////////////////////////////////////////
//
// Definitions
//

#define USBSTATUS_USB 					(0x1 << 0)
#define USBSTATUS_UART 					(0x1 << 1)
#define USBSTATUS_SAMSUNG_KIES			(0x1 << 2)
#define USBSTATUS_MTPONLY 				(0x1 << 3)
#define USBSTATUS_UMS					(0x1 << 4)
#define USBSTATUS_ASKON 				(0x1 << 5)
#define USBSTATUS_VTP					(0x1 << 6)
#define USBSTATUS_ADB					(0x1 << 7)
#define USBSTATUS_DM					(0x1 << 8)
#define USBSTATUS_ACMONLY				(0x1 << 9)
#define USBSTATUS_ADB_REMOVE			(0x1 << 10)