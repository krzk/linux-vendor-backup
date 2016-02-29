/*
 * Tizen Global Lock device driver
 *
 * Copyright (C) 2014 Samsung Electronics Co., Ltd.
 *
 * Author: YoungJun Cho <yj44.cho@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2, as published
 * by the Free Software Foundation.
 */

#ifndef __TGL__
#define __TGL__

#define TGL_IOCTL_BASE		0x32
#define TGL_IO(nr)		_IO(TGL_IOCTL_BASE, nr)
#define TGL_IOR(nr, type)	_IOR(TGL_IOCTL_BASE, nr, type)
#define TGL_IOW(nr, type)	_IOW(TGL_IOCTL_BASE, nr, type)
#define TGL_IOWR(nr, type)	_IOWR(TGL_IOCTL_BASE, nr, type)

/**
 * struct tgl_reg_data - tgl register data structure
 * @key: lookup key
 * @timeout_ms: timeout value for waiting event
 */
struct tgl_reg_data {
	unsigned int key;
	unsigned int timeout_ms;
};

enum {
	TGL_TYPE_NONE = 0,
	TGL_TYPE_READ = (1 << 0),
	TGL_TYPE_WRITE = (1 << 1),
};

/**
 * struct tgl_lock_data - tgl lock data structure
 * @key: lookup key
 * @type: lock type that is either TGL_TYPE_WRITE or TGL_TYPE_READ
 */
struct tgl_lock_data {
	unsigned int key;
	unsigned int type;
};

enum {
	TGL_STATUS_UNLOCKED,
	TGL_STATUS_LOCKED,
};

/**
 * struct tgl_usr_data - tgl user data structure
 * @key: lookup key
 * @data1: user data 1
 * @data2: user data 2
 * @status: lock status that is either TGL_STATUS_UNLOCKED or TGL_STATUS_LOCKED
 */
struct tgl_usr_data {
	unsigned int key;
	unsigned int data1;
	unsigned int data2;
	unsigned int status;
};

enum {
	_TGL_REGISTER = 1,
	_TGL_UNREGISTER,
	_TGL_LOCK,
	_TGL_UNLOCK,
	_TGL_SET_DATA,
	_TGL_GET_DATA,
};

/* register key */
#define TGL_IOCTL_REGISTER	TGL_IOW(_TGL_REGISTER, struct tgl_reg_data *)
/* unregister key */
#define TGL_IOCTL_UNREGISTER	TGL_IOW(_TGL_UNREGISTER, unsigned int)
/* lock with key */
#define TGL_IOCTL_LOCK		TGL_IOW(_TGL_LOCK, struct tgl_lock_data *)
/* unlock with key */
#define TGL_IOCTL_UNLOCK	TGL_IOW(_TGL_UNLOCK, unsigned int)
/* set user data with key */
#define TGL_IOCTL_SET_DATA	TGL_IOW(_TGL_SET_DATA, struct tgl_usr_data *)
/* get user data with key */
#define TGL_IOCTL_GET_DATA	TGL_IOR(_TGL_GET_DATA, struct tgl_usr_data *)

#endif	/* __TGL__ */
