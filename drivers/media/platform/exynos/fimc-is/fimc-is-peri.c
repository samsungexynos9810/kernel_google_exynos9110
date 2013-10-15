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

static int fimc_is_peri_spi_read(struct spi_device *spi,
		void *buf, u32 rx_addr, size_t size)
{
	unsigned char req_data[4] = { 0x03,  };
	int ret;

	struct spi_transfer t_c;
	struct spi_transfer t_r;

	struct spi_message m;

	memset(&t_c, 0x00, sizeof(t_c));
	memset(&t_r, 0x00, sizeof(t_r));

	req_data[1] = (rx_addr & 0xFF00) >> 8;
	req_data[2] = (rx_addr & 0xFF);

	t_c.tx_buf = req_data;
	t_c.len = 4;
	t_c.cs_change = 1;
	t_c.bits_per_word = 32;

	t_r.rx_buf = buf;
	t_r.len = size;

	spi_message_init(&m);
	spi_message_add_tail(&t_c, &m);
	spi_message_add_tail(&t_r, &m);

	ret = spi_sync(spi, &m);
	if (ret) {
		err("spi sync error - can't read data");
		return -EIO;
	} else
		return 0;
}

static int fimc_is_peri_spi_single_write(struct spi_device *spi, u32 data)
{
	int ret = 0;
	u8 tx_buf[5];

	tx_buf[0] = 0x02; /* write cmd */
	tx_buf[1] = (data >> 24) & 0xFF; /* address */
	tx_buf[2] = (data >> 16) & 0xFF; /* address */
	tx_buf[3] = (data >>  8) & 0xFF; /* data */
	tx_buf[4] = (data >>  0) & 0xFF; /* data */

	ret = spi_write(spi, &tx_buf[0], 5);
	if (ret)
		err("spi sync error - can't read data");

	return ret;
}

/* Burst mode: <command><address><data data data ...>
 * Burst width: Maximun value is 512.
 */
static int fimc_is_peri_spi_burst_write(struct spi_device *spi,
	u8 *buf, size_t size, size_t burst_width)
{
	int ret = 0;
	u32 i, j;
	u8 tx_buf[256];
	size_t burst_size;

	/* check multiples of 4 */
	burst_width = (burst_width + 4 - 1) / 4 * 4;

	burst_size = size / burst_width * burst_width;

	for (i = 0; i < burst_size; i += burst_width) {
		tx_buf[0] = 0x02; /* write cmd */
		tx_buf[1] = *(buf + i + 3); /* address */
		tx_buf[2] = *(buf + i + 2); /* address */

		for (j = 0; j < burst_width / 4; j ++) {
			tx_buf[j*2 + 3] = *(buf + i + (j * 4) + 1 ); /* data */
			tx_buf[j*2 + 4] = *(buf + i + (j * 4) + 0 ); /* data */
		}

		ret = spi_write(spi, &tx_buf[0], j * 2 + 3);
		if (ret) {
			err("spi write error - can't write data");
			goto p_err;
			break;
		}
	}

	tx_buf[0] = 0x02; /* write cmd */
	tx_buf[1] = *(buf + i + 3); /* address */
	tx_buf[2] = *(buf + i + 2); /* address */

	for (j = 0; j < (size - burst_size) / 4; j ++) {
		tx_buf[j*2 + 3] = *(buf + i + (j * 4) + 1 ); /* data */
		tx_buf[j*2 + 4] = *(buf + i + (j * 4) + 0 ); /* data */
	}

	ret = spi_write(spi, &tx_buf[0], j * 2 + 3);
	if (ret)
		err("spi write error - can't write data");

p_err:
	return ret;
}

int fimc_is_peri_loadfirm(struct fimc_is_core *core)
{
	int ret = 0;
	u32 size;
	u8 *buf = NULL;
	u8 read_data[2];

	if (!core->spi1) {
		pr_debug("spi1 device is not available");
		goto p_err;
	}

	fimc_is_peri_spi_single_write(core->spi1, 0x00000000);
	fimc_is_peri_spi_single_write(core->spi1, 0x60480001);
	fimc_is_peri_spi_single_write(core->spi1, 0xFFFFFFF1);
	fimc_is_peri_spi_single_write(core->spi1, 0x64280000);
	fimc_is_peri_spi_single_write(core->spi1, 0x642A0000);

	size = sizeof(fw_73c1);
	buf = vmalloc(size);
	if (!buf) {
		err("failed to allocate memory");
		ret = -ENOMEM;
		goto p_err;
	}

	memcpy((void *)buf, (void *)&fw_73c1[0], size);

	ret = fimc_is_peri_spi_burst_write(core->spi1, buf, size, 512);
	if (ret) {
		err("fimc_is_peri_spi_write() fail");
		goto p_err;
	}

	fimc_is_peri_spi_single_write(core->spi1, 0x64280001);
	fimc_is_peri_spi_single_write(core->spi1, 0x642A0000);
	fimc_is_peri_spi_single_write(core->spi1, 0x60140001);
	fimc_is_peri_spi_single_write(core->spi1, 0xFFFFFFF3);

	/* check validation(Read data must be 0x73C1) */
	usleep_range(1000, 1000);
	fimc_is_peri_spi_read(core->spi1, (void *)read_data, 0x0000, 2);
	pr_info("Companion FW loaded: 0x%02x%02x\n", read_data[0], read_data[1]);

p_err:
	if (buf)
		vfree(buf);

	return ret;
}

int fimc_is_peri_loadsetf(struct fimc_is_core *core)
{
	int ret = 0;
	u32 i;
	u32 size;

	if (!core->spi1) {
		pr_debug("spi1 device is not available");
		goto p_err;
	}

	size = sizeof(setfile_73c1);

	for (i = 0; i < size / 4; i++) {
		ret = fimc_is_peri_spi_single_write(core->spi1, setfile_73c1[i]);
		if (ret) {
			err("fimc_is_peri_spi_setf_write() fail");
			break;
		}
	}

p_err:
	return ret;
}
