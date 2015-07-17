#ifndef FIMC_IS_DEVICE_CSI_H
#define FIMC_IS_DEVICE_CSI_H

#include <linux/phy/phy.h>
#include <media/v4l2-device.h>
#include "fimc-is-type.h"

struct fimc_is_device_csi {
	/* channel information */
	u32				instance;
	void __iomem			*base_reg;
	struct phy			*phy;

	/* for settle time */
	u32				sensor_cfgs;
	struct fimc_is_sensor_cfg	*sensor_cfg;

	/* for vci setting */
	u32				vcis;
	struct fimc_is_vci		*vci;

	/* image configuration */
	u32				mode;
	u32				lanes;
	struct fimc_is_image		image;
};

int __must_check fimc_is_csi_probe(void *parent, u32 instance);
int __must_check fimc_is_csi_open(struct v4l2_subdev *subdev);
int __must_check fimc_is_csi_close(struct v4l2_subdev *subdev);

void s5pcsis_enable_interrupts(void __iomem *base_reg, struct fimc_is_image *image, bool on);
void s5pcsis_set_hsync_settle(void __iomem *base_reg, int settle);
void s5pcsis_set_params(void __iomem *base_reg, struct fimc_is_image *image, u32 lanes);
void s5pcsis_reset(void __iomem *base_reg);
void s5pcsis_system_enable(void __iomem *base_reg, int on, u32 lanes);

#endif
