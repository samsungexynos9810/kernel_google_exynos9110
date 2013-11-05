/*
 * include/linux/exynos_ion.h
 *
 * Copyright (C) 2011 Google, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef _LINUX_EXYNOS_ION_H
#define _LINUX_EXYNOS_ION_H

#include <linux/ion.h>

enum {
	ION_HEAP_TYPE_EXYNOS_CONTIG = ION_HEAP_TYPE_CUSTOM + 1,
	ION_HEAP_TYPE_EXYNOS,
};

#define EXYNOS_ION_HEAP_SYSTEM_ID 0
#define EXYNOS_ION_HEAP_EXYNOS_CONTIG_ID 4
#define EXYNOS_ION_HEAP_EXYNOS_ID 5
#define EXYNOS_ION_HEAP_CHUNK_ID 6

#define EXYNOS_ION_HEAP_SYSTEM_MASK	(1 << EXYNOS_ION_HEAP_SYSTEM_ID)
#define EXYNOS_ION_HEAP_EXYNOS_CONTIG_MASK	\
				(1 << EXYNOS_ION_HEAP_EXYNOS_CONTIG_ID)
#define EXYNOS_ION_HEAP_EXYNOS_MASK	(1 << EXYNOS_ION_HEAP_EXYNOS_ID)

#define ION_EXYNOS_ID_COMMON		0
#define ION_EXYNOS_ID_MFC_SH		2
#define ION_EXYNOS_ID_MSGBOX_SH		3
#define ION_EXYNOS_ID_FIMD_VIDEO	4
#define ION_EXYNOS_ID_GSC		5
#define ION_EXYNOS_ID_MFC_OUTPUT	6
#define ION_EXYNOS_ID_MFC_INPUT		7
#define ION_EXYNOS_ID_MFC_FW		8
#define ION_EXYNOS_ID_SECTBL		9
#define ION_EXYNOS_ID_G2D_WFD		10

#ifndef BITS_PER_BYTE
#define BITS_PER_BYTE 8
#endif

#ifndef BITS_PER_INT
#define BITS_PER_INT (sizeof(int) * BITS_PER_BYTE)
#endif

/* contiguous region IDs are on top 4 bits ( 0 ~ 15) */
#define EXYNOS_ION_CONTIG_ID_BITS     4
#define EXYNOS_ION_CONTIG_ID_NUM      (1 << EXYNOS_ION_CONTIG_ID_BITS)
#define EXYNOS_ION_CONTIG_ID_SHIFT    (BITS_PER_INT - EXYNOS_ION_CONTIG_ID_BITS)
#define EXYNOS_ION_CONTIG_ID_MASK     ((EXYNOS_ION_CONTIG_ID_NUM - 1) << \
					EXYNOS_ION_CONTIG_ID_SHIFT)

#define MAKE_CONTIG_ID(flag)		(flag >> EXYNOS_ION_CONTIG_ID_SHIFT)
#define MAKE_CONTIG_FLAG(id)		(id << EXYNOS_ION_CONTIG_ID_SHIFT)

#define ION_EXYNOS_MFC_SH_MASK	     MAKE_CONTIG_FLAG(ION_EXYNOS_ID_MFC_SH)
#define ION_EXYNOS_MSGBOX_SH_MASK    MAKE_CONTIG_FLAG(ION_EXYNOS_ID_MSGBOX_SH)
#define ION_EXYNOS_FIMD_VIDEO_MASK   MAKE_CONTIG_FLAG(ION_EXYNOS_ID_FIMD_VIDEO)
#define ION_EXYNOS_GSC_MASK	     MAKE_CONTIG_FLAG(ION_EXYNOS_ID_GSC)
#define ION_EXYNOS_MFC_OUTPUT_MASK   MAKE_CONTIG_FLAG(ION_EXYNOS_ID_MFC_OUTPUT)
#define ION_EXYNOS_MFC_INPUT_MASK    MAKE_CONTIG_FLAG(ION_EXYNOS_ID_MFC_INPUT)
#define ION_EXYNOS_MFC_FW_MASK	     MAKE_CONTIG_FLAG(ION_EXYNOS_ID_MFC_FW)
#define ION_EXYNOS_SECTBL_MASK	     MAKE_CONTIG_FLAG(ION_EXYNOS_ID_SECTBL)
#define ION_EXYNOS_G2D_WFD_MASK	     MAKE_CONTIG_FLAG(ION_EXYNOS_ID_G2D_WFD)

#ifdef CONFIG_ION_EXYNOS
unsigned int ion_exynos_contig_region_mask(char *region_name);
#else
static inline int ion_exynos_contig_region_mask(char *region_name)
{
	return 0;
}
#endif

#if defined(CONFIG_ION_EXYNOS)
int init_exynos_ion_contig_heap(void);
#else
static inline int fdt_init_exynos_ion(void)
{
	return 0;
}
#endif

#endif /* _LINUX_ION_H */
