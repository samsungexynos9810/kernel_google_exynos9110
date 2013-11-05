/*
 * Samsung Exynos5 SoC series FIMC-IS driver
 *
 *
 * Copyright (c) 2011 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef FIMC_IS_DEVICE_FLITE_H
#define FIMC_IS_DEVICE_FLITE_H

#include "fimc-is-type.h"

#define EXPECT_FRAME_START	0
#define EXPECT_FRAME_END	1

#define FLITE_NOTIFY_FSTART	0
#define FLITE_NOTIFY_FEND	1

#define FLITE_ENABLE_FLAG	1
#define FLITE_ENABLE_MASK	0xFFFF
#define FLITE_ENABLE_SHIFT	0

#define FLITE_NOWAIT_FLAG	1
#define FLITE_NOWAIT_MASK	0xFFFF0000
#define FLITE_NOWAIT_SHIFT	16

struct fimc_is_device_sensor;

enum fimc_is_flite_state {
	/* buffer state*/
	FLITE_A_SLOT_VALID = 0,
	FLITE_B_SLOT_VALID,
	/* finish state */
	FLITE_LAST_CAPTURE,
	/* one the fly output */
	FLITE_OTF_WITH_3AA,
};

struct fimc_is_device_flite {
	u32				instance;
	unsigned long __iomem		*base_reg;
	unsigned long			state;
	wait_queue_head_t		wait_queue;

	struct fimc_is_image		image;
	struct fimc_is_framemgr		*framemgr;

	/* which 3aa gorup is connected when otf is enable */
	u32				group;
	u32				sw_checker;
	u32				sw_trigger;
	atomic_t			bcount;
	atomic_t			fcount;
	u32				tasklet_param_str;
	struct tasklet_struct		tasklet_flite_str;
	u32				tasklet_param_end;
	struct tasklet_struct		tasklet_flite_end;
};

int fimc_is_flite_probe(struct fimc_is_device_sensor *device,
	u32 instance);
int fimc_is_flite_open(struct v4l2_subdev *subdev,
	struct fimc_is_framemgr *framemgr);
int fimc_is_flite_close(struct v4l2_subdev *subdev);

extern u32 __iomem *notify_fcount_sen0;
extern u32 __iomem *notify_fcount_sen1;
extern u32 __iomem *notify_fcount_sen2;
extern u32 __iomem *last_fcount0;
extern u32 __iomem *last_fcount1;

#endif
