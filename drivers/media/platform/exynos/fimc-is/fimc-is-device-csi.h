#ifndef FIMC_IS_DEVICE_CSI_H
#define FIMC_IS_DEVICE_CSI_H

#include "fimc-is-type.h"

struct fimc_is_device_sensor;

struct fimc_is_settle {
	u32 width;
	u32 height;
	u32 framerate;
	u32 settle;
};

struct fimc_is_device_csi {
	/* channel information */
	u32				instance;
	unsigned long __iomem		*base_reg;

	/* settle time */
	u32				settle_max;
	struct fimc_is_settle		*settle_table;

	/* image configuration */
	struct fimc_is_image		image;
};

int __must_check fimc_is_csi_probe(struct fimc_is_device_sensor *device,
	u32 instance);
int __must_check fimc_is_csi_open(struct v4l2_subdev *subdev);
int __must_check fimc_is_csi_close(struct v4l2_subdev *subdev);

#endif