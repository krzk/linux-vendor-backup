/*
 * SAMSUNG NFC N2 Controller
 *
 * Copyright (C) 2013 Samsung Electronics Co.Ltd
 * Author: Woonki Lee <woonki84.lee@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the License, or (at your
 * option) any later version.
 *
 */

#include <linux/platform_device.h>

#define	SEC_NFC_DRIVER_NAME		"sec-nfc"
#define	SEC_NFC_FN_DRIVER_NAME		"sec-nfc-fn"

#define	SEC_NFC_MAX_BUFFER_SIZE	512

/* ioctl */
#define	SEC_NFC_MAGIC	'S'
#define	SEC_NFC_GET_MODE		_IOW(SEC_NFC_MAGIC, 0, unsigned int)
#define	SEC_NFC_SET_MODE		_IOW(SEC_NFC_MAGIC, 1, unsigned int)
#define	SEC_NFC_SLEEP			_IOW(SEC_NFC_MAGIC, 2, unsigned int)
#define	SEC_NFC_WAKEUP			_IOW(SEC_NFC_MAGIC, 3, unsigned int)

/* size */
#define	SEC_NFC_MSG_MAX_SIZE	(256 + 4)

/* wait for device stable */
#define	SEC_NFC_VEN_WAIT_TIME	(100)

/* gpio pin configuration */
struct sec_nfc_platform_data {
	unsigned int irq;
	unsigned int ven;
	unsigned int firm;
	unsigned int wake;
#ifdef	CONFIG_SEC_NFC_CLK_REQ
	unsigned int clk_req;
	unsigned int clk_enable;
#endif
	void	(*cfg_gpio)(void);
};

enum sec_nfc_mode {
	SEC_NFC_MODE_OFF = 0,
	SEC_NFC_MODE_FIRMWARE,
	SEC_NFC_MODE_BOOTLOADER,
	SEC_NFC_MODE_COUNT,
};

enum sec_nfc_power {
	SEC_NFC_PW_ON = 0,
	SEC_NFC_PW_OFF,
};
enum sec_nfc_firmpin {
	SEC_NFC_FW_OFF = 0,
	SEC_NFC_FW_ON,
};

enum sec_nfc_wake {
	SEC_NFC_WAKE_SLEEP = 0,
	SEC_NFC_WAKE_UP,
};
