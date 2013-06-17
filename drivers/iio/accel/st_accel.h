/*
 * STMicroelectronics accelerometers driver
 *
 * Copyright 2012-2013 STMicroelectronics Inc.
 *
 * Denis Ciocca <denis.ciocca@st.com>
 * v. 1.0.0
 * Licensed under the GPL-2.
 */

#ifndef ST_ACCEL_H
#define ST_ACCEL_H

#include <linux/types.h>
#include <linux/iio/common/st_sensors.h>

#define LSM303DLHC_ACCEL_DEV_NAME	"lsm303dlhc-accel"
#define LIS3DH_ACCEL_DEV_NAME		"lis3dh"
#define LSM330D_ACCEL_DEV_NAME		"lsm330d-accel"
#define LSM330DL_ACCEL_DEV_NAME		"lsm330dl-accel"
#define LSM330DLC_ACCEL_DEV_NAME	"lsm330dlc-accel"
#define LIS331DLH_ACCEL_DEV_NAME	"lis331dlh"
#define LSM303DL_ACCEL_DEV_NAME		"lsm303dl-accel"
#define LSM303DLH_ACCEL_DEV_NAME	"lsm303dlh-accel"
#define LSM303DLM_ACCEL_DEV_NAME	"lsm303dlm-accel"
#define LSM330_ACCEL_DEV_NAME		"lsm330-accel"

int st_accel_common_probe(struct iio_dev *indio_dev);
void st_accel_common_remove(struct iio_dev *indio_dev);

#ifdef CONFIG_IIO_BUFFER
int st_accel_allocate_ring(struct iio_dev *indio_dev);
void st_accel_deallocate_ring(struct iio_dev *indio_dev);
int st_accel_trig_set_state(struct iio_trigger *trig, bool state);
#define ST_ACCEL_TRIGGER_SET_STATE (&st_accel_trig_set_state)
#else /* CONFIG_IIO_BUFFER */
static inline int st_accel_allocate_ring(struct iio_dev *indio_dev)
{
	return 0;
}
static inline void st_accel_deallocate_ring(struct iio_dev *indio_dev)
{
}
#define ST_ACCEL_TRIGGER_SET_STATE NULL
#endif /* CONFIG_IIO_BUFFER */

#endif /* ST_ACCEL_H */
