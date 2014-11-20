/*
 * STMicroelectronics gyroscopes driver
 *
 * Copyright 2012-2013 STMicroelectronics Inc.
 *
 * Denis Ciocca <denis.ciocca@st.com>
 *
 * Licensed under the GPL-2.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/spi/spi.h>
#include <linux/iio/iio.h>

#include <linux/iio/common/st_sensors.h>
#include <linux/iio/common/st_sensors_spi.h>
#include "st_gyro.h"

static int st_gyro_spi_probe(struct spi_device *spi)
{
	struct iio_dev *indio_dev;
	struct st_sensor_data *gdata;
	int err;

	indio_dev = iio_device_alloc(sizeof(*gdata));
	if (indio_dev == NULL) {
		err = -ENOMEM;
		goto iio_device_alloc_error;
	}

	gdata = iio_priv(indio_dev);
	gdata->dev = &spi->dev;

	st_sensors_spi_configure(indio_dev, spi, gdata);

	err = st_gyro_common_probe(indio_dev);
	if (err < 0)
		goto st_gyro_common_probe_error;

	return 0;

st_gyro_common_probe_error:
	iio_device_free(indio_dev);
iio_device_alloc_error:
	return err;
}

static int st_gyro_spi_remove(struct spi_device *spi)
{
	st_gyro_common_remove(spi_get_drvdata(spi));

	return 0;
}

static const struct spi_device_id st_gyro_id_table[] = {
	{ L3G4200D_GYRO_DEV_NAME },
	{ LSM330D_GYRO_DEV_NAME },
	{ LSM330DL_GYRO_DEV_NAME },
	{ LSM330DLC_GYRO_DEV_NAME },
	{ L3GD20_GYRO_DEV_NAME },
	{ L3G4IS_GYRO_DEV_NAME },
	{ LSM330_GYRO_DEV_NAME },
	{},
};
MODULE_DEVICE_TABLE(spi, st_gyro_id_table);

#ifdef CONFIG_OF
static struct of_device_id st_gyro_dt_match[] = {
	{ .compatible = "st,l3g4200d" },
	{ .compatible = "st,lsm330d-gyro" },
	{ .compatible = "st,lsm330dl-gyro" },
	{ .compatible = "st,lsm330dlc-gyro" },
	{ .compatible = "st,l3gd20" },
	{ .compatible = "st,l3gd20h" },
	{ .compatible = "st,l3g4is-ui" },
	{ .compatible = "st,lsm330-gyro" },
	{ },
};
#endif

static struct spi_driver st_gyro_driver = {
	.driver = {
		.owner = THIS_MODULE,
		.name = "st-gyro-spi",
		.of_match_table = of_match_ptr(st_gyro_dt_match),
	},
	.probe = st_gyro_spi_probe,
	.remove = st_gyro_spi_remove,
	.id_table = st_gyro_id_table,
};
module_spi_driver(st_gyro_driver);

MODULE_AUTHOR("Denis Ciocca <denis.ciocca@st.com>");
MODULE_DESCRIPTION("STMicroelectronics gyroscopes spi driver");
MODULE_LICENSE("GPL v2");
