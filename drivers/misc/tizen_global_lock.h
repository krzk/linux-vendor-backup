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

#ifndef __TIZEN_GLOBAL_LOOK_H__
#define __TIZEN_GLOBAL_LOOK_H__

#define TGL_IOCTL_BASE		0x32
#define TGL_IO(nr)		_IO(TGL_IOCTL_BASE, nr)
#define TGL_IOR(nr, type)	_IOR(TGL_IOCTL_BASE, nr, type)
#define TGL_IOW(nr, type)	_IOW(TGL_IOCTL_BASE, nr, type)
#define TGL_IOWR(nr, type)	_IOWR(TGL_IOCTL_BASE, nr, type)

/**
 * struct tgl_ver_data - tgl version data structure
 * @major: major version
 * @minor: minor version
 */
struct tgl_ver_data {
	unsigned int major;
	unsigned int minor;
};

/**
 * struct tgl_reg_data - tgl register data structure
 * @key: lookup key
 * @timeout_ms: timeout value for waiting event
 */
struct tgl_reg_data {
	unsigned int key;
	unsigned int timeout_ms;
};

enum tgl_type_data {
	TGL_TYPE_NONE = 0,
	TGL_TYPE_READ = (1 << 0),
	TGL_TYPE_WRITE = (1 << 1),
};

/**
 * struct tgl_lock_data - tgl lock data structure
 * @key: lookup key
 * @type: lock type that is in tgl_type_data
 */
struct tgl_lock_data {
	unsigned int key;
	enum tgl_type_data type;
};

enum tgl_status_data {
	TGL_STATUS_UNLOCKED,
	TGL_STATUS_LOCKED,
};

/**
 * struct tgl_usr_data - tgl user data structure
 * @key: lookup key
 * @data1: user data 1
 * @data2: user data 2
 * @status: lock status that is in tgl_status_data
 */
struct tgl_usr_data {
	unsigned int key;
	unsigned int data1;
	unsigned int data2;
	enum tgl_status_data status;
};

enum {
	_TGL_GET_VERSION,
	_TGL_REGISTER,
	_TGL_UNREGISTER,
	_TGL_LOCK,
	_TGL_UNLOCK,
	_TGL_SET_DATA,
	_TGL_GET_DATA,
};

/* get version information */
#define TGL_IOCTL_GET_VERSION	TGL_IOR(_TGL_GET_VERSION, struct tgl_ver_data)
/* register key */
#define TGL_IOCTL_REGISTER	TGL_IOW(_TGL_REGISTER, struct tgl_reg_data)
/* unregister key */
#define TGL_IOCTL_UNREGISTER	TGL_IOW(_TGL_UNREGISTER, struct tgl_reg_data)
/* lock with key */
#define TGL_IOCTL_LOCK		TGL_IOW(_TGL_LOCK, struct tgl_lock_data)
/* unlock with key */
#define TGL_IOCTL_UNLOCK	TGL_IOW(_TGL_UNLOCK, struct tgl_lock_data)
/* set user data with key */
#define TGL_IOCTL_SET_DATA	TGL_IOW(_TGL_SET_DATA, struct tgl_usr_data)
/* get user data with key */
#define TGL_IOCTL_GET_DATA	TGL_IOR(_TGL_GET_DATA, struct tgl_usr_data)

#endif	/* __TIZEN_GLOBAL_LOOK_H__ */
