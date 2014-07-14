/*
 * Samsung EXYNOS5 & EXYNOS3 FIMC-IS (Imaging Subsystem) driver
 *
 * Copyright (C) 2013-2014 Samsung Electronics Co., Ltd.
 * Author: Arun Kumar K <arun.kk@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#ifndef FIMC_IS_SENSOR_H_
#define FIMC_IS_SENSOR_H_

#include <linux/of.h>
#include <linux/types.h>

#define FIMC_IS_SENSOR_OPEN_TIMEOUT	2000 /* ms */

#define S5K6A3_SENSOR_WIDTH		1392
#define S5K6A3_SENSOR_HEIGHT		1392

#define S5K4E5_SENSOR_WIDTH		2560
#define S5K4E5_SENSOR_HEIGHT		1920

/* S5K8B1 sensor */
#define S5K8B1_SENSOR_WIDTH             1936
#define S5K8B1_SENSOR_HEIGHT            1096

#define SENSOR_WIDTH_PADDING		16
#define SENSOR_HEIGHT_PADDING		10

enum fimc_is_sensor_id {
	FIMC_IS_SENSOR_ID_S5K3H2	= 1,
	FIMC_IS_SENSOR_ID_S5K6A3,
	FIMC_IS_SENSOR_ID_S5K4E5,
	FIMC_IS_SENSOR_ID_S5K3H7,
	FIMC_IS_SENSOR_ID_CUSTOM,
	FIMC_IS_SENSOR_ID_S5K6B2	= 7,
	FIMC_IS_SENSOR_ID_S5K8B1	= 11,
	FIMC_IS_SENSOR_ID_END
};

struct sensor_drv_data {
	enum fimc_is_sensor_id id;
	/* sensor open timeout in ms */
	char *setfile_name;
        unsigned int pixel_width;
        unsigned int pixel_height;
};

/**
 * struct fimc_is_sensor - fimc-is sensor data structure
 * @drvdata: a pointer to the sensor's parameters data structure
 * @i2c_bus: ISP I2C bus index (0...1)
 * @i2c_slave_addr: I2C slave address
 * @width: sensor active width
 * @height: sensor active height
 * @pixel_width: sensor effective pixel width (width + padding)
 * @pixel_height: sensor effective pixel height (height + padding)
 */
struct fimc_is_sensor {
	const struct sensor_drv_data *drvdata;
	unsigned int i2c_bus;
        unsigned int i2c_slave_addr;
	unsigned int width;
	unsigned int height;
	unsigned int pixel_width;
	unsigned int pixel_height;
};

const struct sensor_drv_data *exynos5_is_sensor_get_drvdata(
			struct device_node *node);

#endif /* FIMC_IS_SENSOR_H_ */
