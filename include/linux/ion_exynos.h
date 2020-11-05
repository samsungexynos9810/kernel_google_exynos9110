/*
 * include/linux/ion_exynos.h
 *
 * Copyright (C) 2018 Samsung Electronics Co., Ltd.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef __LINUX_ION_EXYNOS_H__
#define __LINUX_ION_EXYNOS_H__

#include <linux/dma-direction.h>

struct dma_buf_attachment;

#define ION_FLAG_CACHED 1
#define ION_FLAG_PROTECTED 16

#ifdef CONFIG_ION_EXYNOS
struct dma_buf *ion_alloc_dmabuf(const char *heap_name,
				 size_t len, unsigned int flags);
#else
static inline struct dma_buf *ion_alloc_dmabuf(const char *heap_name,
					       size_t len, unsigned int flags)
{
	return ERR_PTR(-ENODEV);
}
#endif

#if defined(CONFIG_EXYNOS_IOVMM) && defined(CONFIG_ION_EXYNOS)
dma_addr_t ion_iovmm_map(struct dma_buf_attachment *attachment,
			 off_t offset, size_t size,
			 enum dma_data_direction direction, int prop);
void ion_iovmm_unmap(struct dma_buf_attachment *attachment, dma_addr_t iova);
#else
static inline dma_addr_t ion_iovmm_map(struct dma_buf_attachment *attachment,
				       off_t offset, size_t size,
				       enum dma_data_direction direction,
				       int prop)
{
	return -ENODEV;
}
#define ion_iovmm_unmap(attachment, iova) do { } while (0)
#endif

#endif /* __LINUX_ION_EXYNOS_H__ */
