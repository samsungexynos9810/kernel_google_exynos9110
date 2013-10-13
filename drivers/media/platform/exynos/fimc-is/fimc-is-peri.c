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

#include "fimc-is-peri.h"

#include "fw_73c1.h"
#include "setfile_73c1.h"

static int fimc_is_peri_spi_write(struct spi_device *spi, u8 *buf, size_t size)
{
	int ret = 0;
	u32 i;
	u8 tx_buf[5];

	for (i = 0; i < size; i += 4) {
		tx_buf[0] = 0x02; /* write cmd */
		tx_buf[1] = *(buf + i + 3); /* address */
		tx_buf[2] = *(buf + i + 2); /* address */
		tx_buf[3] = *(buf + i + 1); /* data */
		tx_buf[4] = *(buf + i + 0); /* data */

		ret = spi_write(spi, &tx_buf[0], 5);
		if (ret) {
			err("spi write error - can't write data");
			break;
		}
	}

	return ret;
}

int fimc_is_peri_loadfirm(struct fimc_is_core *core)
{
	int ret = 0;
	u32 size;
	u8 *buf = NULL;

	if (!core->spi1) {
		pr_debug("spi1 device is not available");
		goto p_err;
	}

	size = sizeof(fw_73c1);
	buf = vmalloc(size);
	if (!buf) {
		err("failed to allocate memory");
		ret = -ENOMEM;
		goto p_err;
	}

	memcpy((void *)buf, (void *)&fw_73c1[0], size);

	ret = fimc_is_peri_spi_write(core->spi1, buf, size);
	if (ret) {
		err("fimc_is_peri_spi_write() fail");
		goto p_err;
	}

p_err:
	if (buf)
		vfree(buf);

	return ret;
}

int fimc_is_peri_loadsetf(struct fimc_is_core *core)
{
	int ret = 0;
	u32 size;
	u8 *buf = NULL;

	if (!core->spi1) {
		pr_debug("spi1 device is not available");
		goto p_err;
	}

	size = sizeof(setfile_73c1);
	buf = vmalloc(size);
	if (!buf) {
		err("failed to allocate memory");
		ret = -ENOMEM;
		goto p_err;
	}

	memcpy((void *)buf, (void *)&setfile_73c1[0], size);

	ret = fimc_is_peri_spi_write(core->spi1, buf, size);
	if (ret) {
		err("fimc_is_peri_spi_write() fail");
		goto p_err;
	}

p_err:
	if (buf)
		vfree(buf);

	return ret;
}
