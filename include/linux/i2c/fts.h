#ifndef _LINUX_FTS_I2C_H_
#define _LINUX_FTS_I2C_H_

struct fts_i2c_platform_data {
	int max_x;
	int max_y;
	int max_width;

	struct regulator *vdd;
	struct regulator *avdd;

	int (*power)(void *data, bool on);
};

#endif /* __LINUX_FTS_I2C_H */
