/*
 * Copyright (c) 2016 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * IPs Traffic Monitor(ITM) Driver for Samsung Exynos8895 SOC
 * By Hosung Kim (hosung0.kim@samsung.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/io.h>
#include <linux/interrupt.h>
#include <linux/bitops.h>
#include <soc/samsung/exynos-itm.h>
#if defined(CONFIG_SEC_SIPC_MODEM_IF)
#include <linux/exynos-modem-ctrl.h>
#endif

/* S-NODE Only */
#define OFFSET_TMOUT_REG		(0x2000)
#define OFFSET_REQ_R			(0x0)
#define OFFSET_REQ_W			(0x20)
#define OFFSET_RESP_R			(0x40)
#define OFFSET_RESP_W			(0x60)
#define OFFSET_ERR_REPT 		(0x20)
#define OFFSET_NUM			(0x4)

#define REG_INT_MASK			(0x0)
#define REG_INT_CLR			(0x4)
#define REG_INT_INFO			(0x8)
#define REG_EXT_INFO_0			(0x10)
#define REG_EXT_INFO_1			(0x14)
#define REG_EXT_INFO_2			(0x18)

#define REG_DBG_CTL			(0x10)
#define REG_TMOUT_INIT_VAL		(0x14)
#define REG_TMOUT_FRZ_EN		(0x18)
#define REG_TMOUT_BUF_RD_ADDR		(0x20)
#define REG_TMOUT_BUF_RD_ID		(0x24)
#define REG_TMOUT_BUF_RD_PAYLOAD	(0x28)
#define REG_TMOUT_BUF_WR_ADDR		(0x30)
#define REG_TMOUT_BUF_WR_ID		(0x34)
#define REG_TMOUT_BUF_WR_PAYLOAD	(0x38)

#define BIT_ERR_CODE(x) 		(((x) & (0xF << 28)) >> 28)
#define BIT_ERR_OCCURRED(x)		(((x) & (0x1 << 27)) >> 27)
#define BIT_ERR_VALID(x)		(((x) & (0x1 << 26)) >> 26)
#define BIT_AXID(x)			(((x) & (0xFFFF)))
#define BIT_AXUSER(x)			(((x) & (0xFFFF << 16)) >> 16)

#define S_NODE				(0)
#define M_NODE				(1)
#define T_S_NODE			(2)
#define T_M_NODE			(3)

#define ERRCODE_SLVERR			(0)
#define ERRCODE_DECERR			(1)
#define ERRCODE_UNSUPORTED		(2)
#define ERRCODE_POWER_DOWN		(3)
#define ERRCODE_UNKNOWN_4		(4)
#define ERRCODE_UNKNOWN_5		(5)
#define ERRCODE_TMOUT			(6)

#define PATHTYPE_DATA			(0)
#define PATHTYPE_PERI			(1)

#define TMOUT				(0xFFFFF)
#define TMOUT_TEST			(0x1)

#define NEED_TO_CHECK			(0xCAFE)

static bool initial_multi_irq_enable = true;

struct itm_rpathinfo {
	unsigned int id;
	char *port_name;
	char *dest_name;
	unsigned int bits;
	unsigned int shift_bits;
};

struct itm_masterinfo {
	char *port_name;
	unsigned int user;
	char *master_name;
	unsigned int bits;
};

struct itm_nodeinfo {
	unsigned int type;
	char *name;
	unsigned int phy_regs;
	void __iomem *regs;
	unsigned int time_val;
	bool tmout_enabled;
	bool tmout_frz_enabled;
	bool err_enabled;
	bool retention;
};

const static char *itm_pathtype[] = {
	"DATA Path transaction (0x2000_0000 ~ 0xf_ffff_ffff)",
	"PERI(SFR) Path transaction (0x0 ~ 0x1fff_ffff)",
};

/* Error Code Description */
const static char *itm_errcode[] = {
	"Error Detect by the Slave(SLVERR)",
	"Decode error(DECERR)",
	"Unsupported transaction error",
	"Power Down access error",
	"Unsupported transaction",
	"Unsupported transaction",
	"Timeout error - response timeout in timeout value",
	"Invalid errorcode",
};

struct itm_nodegroup {
	int irq;
	char *name;
	unsigned int phy_regs;
	void __iomem *regs;
	struct itm_nodeinfo *nodeinfo;
	unsigned int nodesize;
	unsigned int irq_occurred;
	bool panic_delayed;
};

struct itm_platdata {
	const struct itm_rpathinfo	*rpathinfo;
	const struct itm_masterinfo	*masterinfo;
	struct itm_nodegroup		*nodegroup;
	bool probed;
};

const static struct itm_rpathinfo rpathinfo[] = {
	/* Target Address = 0x2000_0000 ~ 0xf_ffff_ffff */
	{0,	"DPU0", 	"DREX",		GENMASK(5, 0),	0},
	{1,	"DPU1", 	"DREX",		GENMASK(5, 0),	0},
	{2,	"DPU2", 	"DREX",		GENMASK(5, 0),	0},
	{3,	"CAM0", 	"DREX",		GENMASK(5, 0),	0},
	{4,	"CAM1", 	"DREX",		GENMASK(5, 0),	0},
	{5,	"ISPLP", 	"DREX",		GENMASK(5, 0),	0},
	{6,	"SRDZ", 	"DREX",		GENMASK(5, 0),	0},
	{7,	"IVA", 		"DREX",		GENMASK(5, 0),	0},
	{8,	"DSP", 		"DREX",		GENMASK(5, 0),	0},
	{9,	"VPU", 		"DREX",		GENMASK(5, 0),	0},
	{10,	"MFC0", 	"DREX",		GENMASK(5, 0),	0},
	{11,	"MFC1", 	"DREX",		GENMASK(5, 0),	0},
	{12,	"G2D0", 	"DREX",		GENMASK(5, 0),	0},
	{13,	"G2D1", 	"DREX",		GENMASK(5, 0),	0},
	{14,	"G2D2", 	"DREX",		GENMASK(5, 0),	0},
	{15,	"VTS", 		"DREX",		GENMASK(5, 0),	0},
	{16,	"FSYS0",	"DREX",		GENMASK(5, 0),	0},
	{17,	"CORESIGHT",	"DREX",		GENMASK(5, 0),	0},
	{18,	"PDMA",		"DREX",		GENMASK(5, 0),	0},
	{19,	"SPDMA",	"DREX",		GENMASK(5, 0),	0},
	{20,	"FSYS1",	"DREX",		GENMASK(5, 0),	0},
	{21,	"GNSS", 	"DREX",		GENMASK(5, 0),	0},
	{22,	"ALIVE",	"DREX",		GENMASK(5, 0),	0},
	{23,	"ABOX", 	"DREX",		GENMASK(5, 0),	0},
	{24,	"DPU0", 	"DREX",		GENMASK(5, 0),	0},
	{25,	"DPU1", 	"DREX",		GENMASK(5, 0),	0},
	{26,	"DPU2", 	"DREX",		GENMASK(5, 0),	0},
	{27,	"CAM0", 	"DREX",		GENMASK(5, 0),	0},
	{28,	"CAM1", 	"DREX",		GENMASK(5, 0),	0},
	{29,	"ISPLP",	"DREX",		GENMASK(5, 0),	0},
	{30,	"SRDZ",		"DREX",		GENMASK(5, 0),	0},
	{31,	"IVA",		"DREX",		GENMASK(5, 0),	0},
	{32,	"DSP",		"DREX",		GENMASK(5, 0),	0},
	{33,	"VPU",		"DREX",		GENMASK(5, 0),	0},
	{34,	"MFC0",		"DREX",		GENMASK(5, 0),	0},
	{35,	"MFC1",		"DREX",		GENMASK(5, 0),	0},
	{36,	"G2D0",		"DREX",		GENMASK(5, 0),	0},
	{37,	"G2D1",		"DREX",		GENMASK(5, 0),	0},
	{38,	"G2D2",		"DREX",		GENMASK(5, 0),	0},
	{39,	"VTS",		"DREX",		GENMASK(5, 0),	0},
	{40,	"FSYS0",	"DREX",		GENMASK(5, 0),	0},
	{41,	"CORESIGHT",	"DREX",		GENMASK(5, 0),	0},
	{42,	"PDMA",		"DREX",		GENMASK(5, 0),	0},
	{43,	"SPDMA",	"DREX",		GENMASK(5, 0),	0},
	{44,	"FSYS1",	"DREX",		GENMASK(5, 0),	0},
	{45,	"GNSS",		"DREX",		GENMASK(5, 0),	0},
	{46,	"ALIVE",	"DREX",		GENMASK(5, 0),	0},
	{47,	"ABOX",		"DREX",		GENMASK(5, 0),	0},
	{48,	"G3D0",		"DREX",		GENMASK(5, 0),	0},
	{49,	"G3D1",		"DREX",		GENMASK(5, 0),	0},
	{50,	"G3D2",		"DREX",		GENMASK(5, 0),	0},
	{51,	"G3D3",		"DREX",		GENMASK(5, 0),	0},
	{52,	"CP",		"DREX",		GENMASK(5, 0),	0},
	/* Target Address = 0x1800_0000 ~ 0x1fff_ffff */
	{0,	"DPU0",		"SP",		GENMASK(4, 0),	0},
	{1,	"DPU1",		"SP",		GENMASK(4, 0),	0},
	{2,	"DPU2",		"SP",		GENMASK(4, 0),	0},
	{3,	"CAM0",		"SP",		GENMASK(4, 0),	0},
	{4,	"CAM1",		"SP",		GENMASK(4, 0),	0},
	{5,	"ISPLP",	"SP",		GENMASK(4, 0),	0},
	{6,	"SRDZ",		"SP",		GENMASK(4, 0),	0},
	{7,	"IVA",		"SP",		GENMASK(4, 0),	0},
	{8,	"DSP",		"SP",		GENMASK(4, 0),	0},
	{9,	"VPU",		"SP",		GENMASK(4, 0),	0},
	{10,	"MFC0",		"SP",		GENMASK(4, 0),	0},
	{11,	"MFC1",		"SP",		GENMASK(4, 0),	0},
	{12,	"G2D0",		"SP",		GENMASK(4, 0),	0},
	{13,	"G2D1",		"SP",		GENMASK(4, 0),	0},
	{14,	"G2D2",		"SP",		GENMASK(4, 0),	0},
	{15,	"VTS",		"SP",		GENMASK(4, 0),	0},
	{16,	"FSYS0",	"SP",		GENMASK(4, 0),	0},
	{17,	"CORESIGHT",	"SP",		GENMASK(4, 0),	0},
	{18,	"PDMA",		"SP",		GENMASK(4, 0),	0},
	{19,	"SPDMA",	"SP",		GENMASK(4, 0),	0},
	{20,	"FSYS1",	"SP",		GENMASK(4, 0),	0},
	{21,	"GNSS",		"SP",		GENMASK(4, 0),	0},
	{22,	"ALIVE",	"SP",		GENMASK(4, 0),	0},
	{23,	"ABOX",		"SP",		GENMASK(4, 0),	0},
	/* Target Address = 0x0000_0000 ~ 0x17ff_ffff */
	{0,	"DPU0",		"PERI",		GENMASK(4, 0),	0},
	{1,	"DPU1",		"PERI",		GENMASK(4, 0),	0},
	{2,	"DPU2",		"PERI",		GENMASK(4, 0),	0},
	{3,	"CAM0",		"PERI",		GENMASK(4, 0),	0},
	{4,	"CAM1",		"PERI",		GENMASK(4, 0),	0},
	{5,	"ISPLP",	"PERI",		GENMASK(4, 0),	0},
	{6,	"SRDZ",		"PERI",		GENMASK(4, 0),	0},
	{7,	"IVA",		"PERI",		GENMASK(4, 0),	0},
	{8,	"DSP",		"PERI",		GENMASK(4, 0),	0},
	{9,	"VPU",		"PERI",		GENMASK(4, 0),	0},
	{10,	"MFC0",		"PERI",		GENMASK(4, 0),	0},
	{11,	"MFC1",		"PERI",		GENMASK(4, 0),	0},
	{12,	"G2D0",		"PERI",		GENMASK(4, 0),	0},
	{13,	"G2D1",		"PERI",		GENMASK(4, 0),	0},
	{14,	"G2D2",		"PERI",		GENMASK(4, 0),	0},
	{15,	"ABOX",		"PERI",		GENMASK(4, 0),	0},
	{16,	"VTS",		"PERI",		GENMASK(4, 0),	0},
	{17,	"FSYS0",	"PERI",		GENMASK(4, 0),	0},
	{18,	"CORESIGHT",	"PERI",		GENMASK(4, 0),	0},
	{19,	"PDMA",		"PERI",		GENMASK(4, 0),	0},
	{20,	"SPDMA",	"PERI",		GENMASK(4, 0),	0},
	{21,	"FSYS1",	"PERI",		GENMASK(4, 0),	0},
	{22,	"GNSS",		"PERI",		GENMASK(4, 0),	0},
	{22,	"ALIVE",	"PERI",		GENMASK(4, 0),	0},
};

/* XIU ID Information */
const static struct itm_masterinfo masterinfo[] = {
	{"DPU0",	0,			/* 0XXXX0 */	"DPP/WBMUX",	BIT(5) | BIT(0)},
	{"DPU0",	BIT(0),			/* 0XXXX1 */	"SYSMMU_DPU0",	BIT(5) | BIT(0)},

	{"DPU1",	0,			/* 0XXXX0 */	"DPP/WBMUX",	BIT(5) | BIT(0)},
	{"DPU1",	BIT(0),			/* 0XXXX1 */	"SYSMMU_DPU1",	BIT(5) | BIT(0)},

	{"DPU2",	0,			/* 0XXXX0 */	"DPP/WBMUX",	BIT(5) | BIT(0)},
	{"DPU2",	BIT(0),			/* 0XXXX1 */	"SYSMMU_DPU2",	BIT(5) | BIT(0)},

	{"CAM0",	0,			/* 0XX000 */	"CSISx4",	BIT(5) | GENMASK(2, 0)},
	{"CAM0",	BIT(1),			/* 0XX010 */	"TPU0",		BIT(5) | GENMASK(2, 0)},
	{"CAM0",	BIT(2),			/* 0XX100 */	"VRA",		BIT(5) | GENMASK(2, 0)},
	{"CAM0",	GENMASK(2, 1),		/* 0XX110 */	"TPU1",		BIT(5) | GENMASK(2, 0)},
	{"CAM0",	BIT(0),			/* 0XXXX1 */	"SYSMMU_CAM0",	BIT(5) | BIT(0)},

	{"CAM1",	0,			/* 0XXXX0 */	"MC_SCALER",	BIT(5) | BIT(0)},
	{"CAM1",	BIT(0),			/* 0XXXX1 */	"SYSMMU_CAM1",	BIT(5) | BIT(0)},

	{"ISPLP",	0,			/* 0XX000 */	"3AAW",		BIT(5) | GENMASK(2, 0)},
	{"ISPLP",	BIT(1),			/* 0XX010 */	"ISPLP",	BIT(5) | GENMASK(2, 0)},
	{"ISPLP",	BIT(2),			/* 0XX100 */	"ISPHQ",	BIT(5) | GENMASK(2, 0)},
	{"ISPLP",	BIT(0),			/* 0XXXX1 */	"SYSMMU_ISPLP",	BIT(5) | BIT(0)},

	{"FSYS0",	0,			/* 0XXX00 */	"ETR",		BIT(5) | GENMASK(1, 0)},
	{"FSYS0",	BIT(1),			/* 0XXX10 */	"USB",		BIT(5) | GENMASK(1, 0)},
	{"FSYS0",	BIT(0),			/* 0XXXX1 */	"UFS",		BIT(5) | BIT(0)},

	{"MFC0",	0,			/* 0XXXX0 */	"MFC0",		BIT(5) | BIT(0)},
	{"MFC0",	BIT(0),			/* 0XXXX1 */	"SYSMMU_MFC0",	BIT(5) | BIT(0)},

	{"MFC1",	0,			/* 0XXXX0 */	"MFC1",		BIT(5) | BIT(0)},
	{"MFC1",	BIT(0),			/* 0XXXX1 */	"SYSMMU_MFC1",	BIT(5) | BIT(0)},

	{"G2D0",	0,			/* 0XXXX0 */	"G2D0",		BIT(5) | BIT(0)},
	{"G2D0",	BIT(0),			/* 0XXXX1 */	"SYSMMU_G2D0",	BIT(5) | BIT(0)},

	{"G2D1",	0,			/* 0XXXX0 */	"G2D1",		BIT(5) | BIT(0)},
	{"G2D1",	BIT(0),			/* 0XXXX1 */	"SYSMMU_G2D1",	BIT(5) | BIT(0)},

	{"G2D2",	0,			/* 0XXX00 */	"JPEG",		BIT(5) | GENMASK(1, 0)},
	{"G2D2",	BIT(1),			/* 0XXX10 */	"M2MSCALER",	BIT(5) | GENMASK(1, 0)},
	{"G2D2",	BIT(0),			/* 0XXXX1 */	"SYSMMU_G2D2",	BIT(5) | BIT(0)},

	{"IVA",		0,			/* 0XXX00 */	"DSP0",		BIT(5) | GENMASK(1, 0)},
	{"IVA",		BIT(1),			/* 0XXX10 */	"DSP1",		BIT(5) | GENMASK(1, 0)},
	{"IVA",		BIT(0),			/* 0XXXX1 */	"SYSMMU_DSP",	BIT(5) | BIT(0)},

	{"VPU",		0,			/* 0XXXX0 */	"VPU",		BIT(5) | BIT(0)},
	{"VPU",		BIT(0),			/* 0XXXX1 */	"SYSMMU_VPU",	BIT(5) | BIT(0)},

	{"FSYS1",	0,			/* 0XX000 */	"MMC2",		BIT(5) | GENMASK(2, 0)},
	{"FSYS1",	BIT(0),			/* 0XX001 */	"PCIE0",	BIT(5) | GENMASK(2, 0)},
	{"FSYS1",	BIT(1),			/* 0XX010 */	"PCIE1",	BIT(5) | GENMASK(2, 0)},
	{"FSYS1",	GENMASK(1, 0),		/* 0XX011 */	"SSS",		BIT(5) | GENMASK(2, 0)},
	{"FSYS1",	BIT(2),			/* 0XX100 */	"RTIC",		BIT(5) | GENMASK(2, 0)},
	{"FSYS1",	BIT(2) | BIT(0),	/* 0XX101 */	"MCOMP",	BIT(5) | GENMASK(2, 0)},

	{"AUD",		0,			/* 0XXXX0 */	"SPUS/SPUM/CA7",BIT(5) | BIT(0)},
	{"AUD",		BIT(0),			/* 0XXXX1 */	"SYSMMU_AUC",	BIT(5) | BIT(0)},

	{"GNSS",	0,			/* 0XXX00 */	"CM0+",		BIT(5) | GENMASK(1, 0)},
	{"GNSS",	BIT(0),			/* 0XXX01 */	"XDMA0",	BIT(5) | GENMASK(1, 0)},
	{"GNSS",	BIT(1),			/* 0XXX10 */	"XDMA1",	BIT(5) | GENMASK(1, 0)},

	{"ALIVE",	0,			/* 0XXX00 */	"CM3",		BIT(5) | GENMASK(1, 0)},
	{"ALIVE",	BIT(0),			/* 0XXX01 */	"SCAN2AXI",	BIT(5) | GENMASK(1, 0)},
	{"ALIVE",	BIT(1),			/* 0XXX10 */	"CM4F/DMIC",	BIT(5) | GENMASK(1, 0)},

	{"CP",		BIT(3),			/* X01XXX */	"CR7M",		GENMASK(4, 3)},
	{"CP",		BIT(2),			/* X001XX */	"CR4MtoL2",	GENMASK(4, 2)},
	{"CP",		GENMASK(4, 3),		/* X1100X */	"DMA",		GENMASK(4, 1)},
	{"CP",		GENMASK(4, 3) | BIT(1),	/* X1101X */	"MDMtoL2",	GENMASK(4, 1)},
	{"CP",		BIT(4),			/* X1000X */	"LMACtoL2",	GENMASK(4, 1)},
	{"CP",		BIT(1),			/* X00010 */	"CSXAP",	BIT(1)},
	{"CP",		BIT(4) | GENMASK(1, 0),	/* X10011 */	"HARQMOVERtoL2",GENMASK(4, 0)},

	/* Others */
	{"SRDZ",	0,					"SRDZ",		0},
	{"DSP",		0,					"DSP",		0},
	{"ABOX",	0,					"ABOX",		0},
	{"VTS",		0,					"VTS",		0},
	{"PDMA",	0,					"PDMA",		0},
	{"SPDMA",	0,					"SPDMA",	0},
};

static struct itm_nodeinfo data_bus_1[] = {
	{M_NODE,	"ALIVE",	0x15423000, NULL, 0,	   false, false, true,  false},
	{M_NODE,	"FSYS1",	0x15403000, NULL, 0,	   false, false, true,  false},
	{M_NODE,	"GNSS",		0x15413000, NULL, 0,	   false, false, true,  false},
	{T_S_NODE,	"BUS1_B0",	0x15433000, NULL, TMOUT,   true,  false, true,  false},
};

static struct itm_nodeinfo data_bus_c[] = {
	{M_NODE,	"ABOX",		0x15143000, NULL, 0,	   false, false, true,  false},
	{M_NODE,	"BUS1_B0",	0x15503000, NULL, 0,	   false, false, true,  false},
	{M_NODE,	"CAM0",		0x15043000, NULL, 0,	   false, false, true,  false},
	{M_NODE,	"CAM1",		0x15053000, NULL, 0,	   false, false, true,  false},
	{M_NODE,	"CORESIGHT",	0x15113000, NULL, 0,	   false, false, true,  false},
	{M_NODE,	"DPU0",		0x15013000, NULL, 0,	   false, false, true,  false},
	{M_NODE,	"DPU1",		0x15023000, NULL, 0,	   false, false, true,  false},
	{M_NODE,	"DPU2",		0x15033000, NULL, 0,	   false, false, true,  false},
	{M_NODE,	"DSP",		0x15093000, NULL, 0,	   false, false, true,  false},
	{M_NODE,	"FSYS0",	0x15103000, NULL, 0,	   false, false, true,  false},
	{M_NODE,	"G2D0",		0x150D3000, NULL, 0,	   false, false, true,  false},
	{M_NODE,	"G2D1",		0x150E3000, NULL, 0,	   false, false, true,  false},
	{M_NODE,	"G2D2",		0x150F3000, NULL, 0,	   false, false, true,  false},
	{M_NODE,	"ISPLP",	0x15063000, NULL, 0,	   false, false, true,  false},
	{M_NODE,	"IVA",		0x15083000, NULL, 0,	   false, false, true,  false},
	{M_NODE,	"MFC0",		0x150B3000, NULL, 0,	   false, false, true,  false},
	{M_NODE,	"MFC1",		0x150C3000, NULL, 0,	   false, false, true,  false},
	{M_NODE,	"PDMA",		0x15123000, NULL, 0,	   false, false, true,  false},
	{M_NODE,	"SPDMA",	0x15133000, NULL, 0,	   false, false, true,  false},
	{M_NODE,	"SRDZ",		0x15073000, NULL, 0,	   false, false, true,  false},
	{M_NODE,	"VPU",		0x150A3000, NULL, 0,	   false, false, true,  false},
	{M_NODE,	"VTS",		0x15153000, NULL, 0,	   false, false, true,  false},
	{T_S_NODE,	"BUSC_M0",	0x15163000, NULL, 0,	   false,  false, true,  false},
	{T_S_NODE,	"BUSC_M1",	0x15173000, NULL, 0,	   false,  false, true,  false},
	{T_S_NODE,	"BUSC_M2",	0x15183000, NULL, 0,	   false,  false, true,  false},
	{T_S_NODE,	"BUSC_M3",	0x15193000, NULL, 0,	   false,  false, true,  false},
	{S_NODE,	"PERI",		0x151B3000, NULL, TMOUT,   true,  false, true,  false},
	{S_NODE,	"SP",		0x151A3000, NULL, TMOUT,   true,  false, true,  false},
};

static struct itm_nodeinfo data_core[] = {
	{T_M_NODE,	"BUSC_M0",	0x14A03000, NULL, 0,	   false, false, true,  false},
	{T_M_NODE,	"BUSC_M1",	0x14A13000, NULL, 0,	   false, false, true,  false},
	{T_M_NODE,	"BUSC_M2",	0x14A23000, NULL, 0,	   false, false, true,  false},
	{T_M_NODE,	"BUSC_M3",	0x14A33000, NULL, 0,	   false, false, true,  false},
	{M_NODE,	"CP",		0x14A83000, NULL, 0,	   false, false, true,  false},
	{M_NODE,	"G3D0",		0x14A43000, NULL, 0,	   false, false, true,  false},
	{M_NODE,	"G3D1",		0x14A53000, NULL, 0,	   false, false, true,  false},
	{M_NODE,	"G3D2",		0x14A63000, NULL, 0,	   false, false, true,  false},
	{M_NODE,	"G3D3",		0x14A73000, NULL, 0,	   false, false, true,  false},
	{S_NODE,	"DREX",		0x14A93000, NULL, TMOUT,   true,  false, true,  false},
	{S_NODE,	"DREX",		0x14AA3000, NULL, TMOUT,   true,  false, true,  false},
};

static struct itm_nodeinfo peri_bus_1[] = {
	{T_M_NODE,	"BUSC_BUS1",	0x15643000, NULL, 0,	   false, false, true,  false},
	{S_NODE,	"ALIVE",	0x15633000, NULL, TMOUT,   true,  false, true,  false},
	{S_NODE,	"BUS1_SFR",	0x15613000, NULL, TMOUT,   true,  false, true,  false},
	{S_NODE,	"FSYS1",	0x15623000, NULL, TMOUT,   true,  false, true,  false},
	{S_NODE,	"TREX_BUS1",	0x15603000, NULL, TMOUT,   true,  false, true,  false},
};

static struct itm_nodeinfo peri_bus_c[] = {
	{M_NODE,	"BUSC_PERI_M",	0x153A3000, NULL, 0,	   false, false, true,  false},
	{T_M_NODE,	"CORE_BUSC",	0x153B3000, NULL, 0,	   false, false, true,  false},
	{S_NODE,	"ABOX",		0x15383000, NULL, TMOUT,   true,  false, true,  false},
	{T_S_NODE,	"BUSC_BUS1",	0x15243000, NULL, 0,	   true,  false, true,  false},
	{S_NODE,	"BUSC_CORE",	0x15233000, NULL, TMOUT,   true,  false, true,  false},
	{S_NODE,	"BUSC_SFR0",	0x15213000, NULL, TMOUT,   true,  false, true,  false},
	{S_NODE,	"BUSC_SFR1",	0x15223000, NULL, TMOUT,   true,  false, true,  false},
	{S_NODE,	"CAM",		0x152E3000, NULL, TMOUT,   true,  false, true,  false},
	{S_NODE,	"DPU0",		0x152C3000, NULL, TMOUT,   true,  false, true,  false},
	{S_NODE,	"DPU1",		0x152D3000, NULL, TMOUT,   true,  false, true,  false},
	{S_NODE,	"DSP",		0x15363000, NULL, TMOUT,   true,  false, true,  false},
	{S_NODE,	"FSYS0",	0x15333000, NULL, TMOUT,   true,  false, true,  false},
	{S_NODE,	"G2D",		0x15343000, NULL, TMOUT,   true,  false, true,  false},
	{S_NODE,	"ISPHQ",	0x15313000, NULL, TMOUT,   true,  false, true,  false},
	{S_NODE,	"ISPLP",	0x15303000, NULL, TMOUT,   true,  false, true,  false},
	{S_NODE,	"IVA",		0x15353000, NULL, TMOUT,   true,  false, true,  false},
	{S_NODE,	"MFC",		0x15323000, NULL, TMOUT,   true,  false, true,  false},
	{S_NODE,	"MIF0",		0x15253000, NULL, TMOUT,   true,  false, true,  false},
	{S_NODE,	"MIF1",		0x15263000, NULL, TMOUT,   true,  false, true,  false},
	{S_NODE,	"MIF2",		0x15273000, NULL, TMOUT,   true,  false, true,  false},
	{S_NODE,	"MIF3",		0x15283000, NULL, TMOUT,   true,  false, true,  false},
	{S_NODE,	"PERIC0",	0x152A3000, NULL, TMOUT,   true,  false, true,  false},
	{S_NODE,	"PERIC1",	0x152B3000, NULL, TMOUT,   true,  false, true,  false},
	{S_NODE,	"PERIS",	0x15293000, NULL, TMOUT,   true,  false, true,  false},
	{S_NODE,	"SRDZ",		0x152F3000, NULL, TMOUT,   true,  false, true,  false},
	{S_NODE,	"TREX_BUSC",	0x15203000, NULL, TMOUT,   true,  false, true,  false},
	{S_NODE,	"VPU",		0x15373000, NULL, TMOUT,   true,  false, true,  false},
	{S_NODE,	"VTS",		0x15393000, NULL, TMOUT,   true,  false, true,  false},
};

static struct itm_nodeinfo peri_core_0[] = {
	{M_NODE,	"CP_PERI_M",	0x14C63000, NULL, 0,	   false, false, true,  false},
	{M_NODE,	"SCI_CCM0",	0x14C33000, NULL, 0,	   false, false, true,  false},
	{M_NODE,	"SCI_CCM1",	0x14C43000, NULL, 0,	   false, false, true,  false},
	{M_NODE,	"SCI_IRPM",	0x14C53000, NULL, 0,	   false, false, true,  false},
	{T_S_NODE,	"CORE0_CORE1",	0x14C03000, NULL, 0,	   true,  false, true,  false},
	{T_S_NODE,	"CORE_BUSC",	0x14C13000, NULL, 0,	   true,  false, true,  false},
};

static struct itm_nodeinfo peri_core_1[] = {
	{T_M_NODE,	"BUSC_CORE",	0x14E13000, NULL, 0,	   false, false, true,  false},
	{T_M_NODE,	"CORE0_CORE1",	0x14E03000, NULL, 0,	   false, false, true,  false},
	{S_NODE,	"CORESIGHT",	0x14E83000, NULL, TMOUT,   true,  false, true,  false},
	{S_NODE,	"CORE_SFR",	0x14E33000, NULL, TMOUT,   true,  false, true,  false},
	{S_NODE,	"CPUCL0",	0x14E53000, NULL, TMOUT,   true,  false, true,  false},
	{S_NODE,	"CPUCL1",	0x14E63000, NULL, TMOUT,   true,  false, true,  false},
	{S_NODE,	"G3D",		0x14E73000, NULL, TMOUT,   true,  false, true,  false},
	{S_NODE,	"IMEM",		0x14E03000, NULL, TMOUT,   true,  false, true,  false},
	{S_NODE,	"TREX_CORE",	0x14E23000, NULL, TMOUT,   true,  false, true,  false},
};

static struct itm_nodegroup nodegroup[] = {
	{311,		"DATA_CORE",	0x14AB3000, NULL, data_core,	ARRAY_SIZE(data_core),	0, false},
	{ 92,		"DATA_BUS_C",	0x151C3000, NULL, data_bus_c,	ARRAY_SIZE(data_bus_c), 0, false},
	{ 72,		"DATA_BUS_1",	0x15443000, NULL, data_bus_1,	ARRAY_SIZE(data_bus_1), 0, false},
	{315,		"PERI_CORE_0",	0x14C73000, NULL, peri_core_0,	ARRAY_SIZE(peri_core_0),0, false},
	{316,		"PERI_CORE_1",	0x14E93000, NULL, peri_core_1,	ARRAY_SIZE(peri_core_1),0, false},
	{ 77,		"PERI_BUS_1",	0x15653000, NULL, peri_bus_1,	ARRAY_SIZE(peri_bus_1), 0, false},
	{ 93,		"PERI_BUS_C",	0x153D3000, NULL, peri_bus_c,	ARRAY_SIZE(peri_bus_c), 0, false},
};

struct itm_dev {
	struct device			*dev;
	struct itm_platdata		*pdata;
	struct of_device_id		*match;
	int				irq;
	int				id;
	void __iomem			*regs;
	spinlock_t			ctrl_lock;
	struct itm_notifier		notifier_info;
};

struct itm_panic_block {
	struct notifier_block nb_panic_block;
	struct itm_dev *pdev;
};

/* declare notifier_list */
static ATOMIC_NOTIFIER_HEAD(itm_notifier_list);

static const struct of_device_id itm_dt_match[] = {
	{ .compatible = "samsung,exynos-itm",
	  .data = NULL, },
	{},
};
MODULE_DEVICE_TABLE(of, itm_dt_match);

static struct itm_rpathinfo* itm_get_rpathinfo
					(struct itm_dev *itm,
					 unsigned int id,
					 char *dest_name)
{
	struct itm_platdata *pdata = itm->pdata;
	struct itm_rpathinfo *rpath = NULL;
	int i;

	for (i = 0; i < ARRAY_SIZE(rpathinfo); i++) {
		if (pdata->rpathinfo[i].id == (id & pdata->rpathinfo[i].bits)) {
			if (dest_name && !strncmp(pdata->rpathinfo[i].dest_name,
				dest_name, strlen(pdata->rpathinfo[i].dest_name))) {
				rpath = (struct itm_rpathinfo *)&pdata->rpathinfo[i];
				break;
			}
		}
	}
	return rpath;
}

static struct itm_masterinfo* itm_get_masterinfo
					(struct itm_dev *itm,
					 char *port_name,
					 unsigned int user)
{
	struct itm_platdata *pdata = itm->pdata;
	struct itm_masterinfo *master = NULL;
	unsigned int val;
	int i;

	for (i = 0; i < ARRAY_SIZE(masterinfo); i++) {
		if (!strncmp(pdata->masterinfo[i].port_name, port_name,
				strlen(port_name))) {
			val = user & pdata->masterinfo[i].bits;
			if (val == pdata->masterinfo[i].user) {
				master = (struct itm_masterinfo *)&pdata->masterinfo[i];
				break;
			}
		}
	}
	return master;
}

static void itm_init(struct itm_dev *itm, bool enabled)
{
	struct itm_platdata *pdata = itm->pdata;
	struct itm_nodeinfo *node;
	unsigned int offset;
	int i, j;

	for (i = 0; i < ARRAY_SIZE(nodegroup); i++) {
		node = pdata->nodegroup[i].nodeinfo;
		for (j = 0; j < pdata->nodegroup[i].nodesize; j++) {
			if (node[j].type == S_NODE && node[j].tmout_enabled) {
				offset = OFFSET_TMOUT_REG;
				/* Enable Timeout setting */
				__raw_writel(enabled, node[j].regs + offset + REG_DBG_CTL);
				/* set tmout interval value */
				__raw_writel(node[j].time_val,
					node[j].regs + offset + REG_TMOUT_INIT_VAL);
				pr_debug("Exynos IPM - %s timeout enabled\n", node[j].name);
				if (node[j].tmout_frz_enabled) {
					/* Enable freezing */
					__raw_writel(enabled,
						node[j].regs + offset + REG_TMOUT_FRZ_EN);
				}
			}
			if (node[j].err_enabled) {
				/* clear previous interrupt of req_read */
				offset = OFFSET_REQ_R;
				if (!pdata->probed || !node->retention)
					__raw_writel(1, node[j].regs + offset + REG_INT_CLR);
				/* enable interrupt */
				__raw_writel(enabled, node[j].regs + offset + REG_INT_MASK);

				/* clear previous interrupt of req_write */
				offset = OFFSET_REQ_W;
				if (pdata->probed || !node->retention)
					__raw_writel(1, node[j].regs + offset + REG_INT_CLR);
				/* enable interrupt */
				__raw_writel(enabled, node[j].regs + offset + REG_INT_MASK);

				/* clear previous interrupt of response_read */
				offset = OFFSET_RESP_R;
				if (!pdata->probed || !node->retention)
					__raw_writel(1, node[j].regs + offset + REG_INT_CLR);
				/* enable interrupt */
				__raw_writel(enabled, node[j].regs + offset + REG_INT_MASK);

				/* clear previous interrupt of response_write */
				offset = OFFSET_RESP_W;
				if (!pdata->probed || !node->retention)
					__raw_writel(1, node[j].regs + offset + REG_INT_CLR);
				/* enable interrupt */
				__raw_writel(enabled, node[j].regs + offset + REG_INT_MASK);
				pr_debug("Exynos IPM - %s error reporting enabled\n", node[j].name);
			}
		}
	}
}

static void itm_post_handler_by_master(struct itm_dev *itm,
					struct itm_nodegroup *group,
					char *port, char *master, bool read)
{
	/* After treatment by port */
	if (!port || strlen(port) < 1)
		return;

	if (!strncmp(port, "CP", strlen(port))) {
		/* if master is DSP and operation is read, we don't care this */
		if (master && !strncmp(master, "TL3MtoL2",strlen(master)) && read == true) {
			group->panic_delayed = true;
			group->irq_occurred = 0;
			pr_info("ITM skips CP's DSP(TL3MtoL2) detected\n");
		} else {
			/* Disable busmon all interrupts */
			itm_init(itm, false);
			group->panic_delayed = true;
#if defined(CONFIG_SEC_SIPC_MODEM_IF)
			ss310ap_force_crash_exit_ext();
#endif
		}
	}
}

static void itm_report_route(struct itm_dev *itm,
				struct itm_nodegroup *group,
				struct itm_nodeinfo *node,
				unsigned int offset, bool read)
{
	struct itm_masterinfo *master = NULL;
	struct itm_rpathinfo *rpath = NULL;
	unsigned int val, id, user;
	char *port = NULL, *source = NULL, *dest = NULL;

	val = __raw_readl(node->regs + offset + REG_INT_INFO);
	id = BIT_AXID(val);
	val = __raw_readl(node->regs + offset + REG_EXT_INFO_2);
	user = BIT_AXUSER(val);

	if (!strncmp(group->name, "DATA", strlen("DATA"))) {
		/* Data Path */
		if (node->type == S_NODE) {
			/*
			 * If detected S_NODE are SP or PERI,
			 * it is for peri access. In this case, we can
			 * find port and source. But if we want to know
			 * target more, we should PERI's S_NODE or M_NODE
			 */
			rpath = itm_get_rpathinfo(itm, id, node->name);
			if (!rpath) {
				pr_info("failed to get route path - %s, id:%x\n",
						node->name, id);
				return;
			}
			master = itm_get_masterinfo(itm, rpath->port_name, user);
			if (!master) {
				pr_info("failed to get master IP with "
					"port:%s, user:%x\n", rpath->port_name, user);
				return;
			}
			port = rpath->port_name;
			source = master->master_name;
			dest = rpath->dest_name;
			if (!strncmp(port, "PERI", strlen("PERI")) ||
				!strncmp(port, "SP", strlen("SP")))
				val = PATHTYPE_PERI;
			else
				val = PATHTYPE_DATA;
		} else {
			val = PATHTYPE_DATA;
			master = itm_get_masterinfo(itm, node->name, user);
			if (!master) {
				pr_info("failed to get master IP with "
					"port:%s, id:%x\n", node->name, user);
				port = node->name;
			} else {
				port = node->name;
				source = master->master_name;
			}
		}
	} else {
		/*
		 * PERI_PATH
		 * In this case, we will get the route table from DATA_BACKBONE interrupt
		 * of PERI port
		 */
		val = PATHTYPE_PERI;
		if (node->type == S_NODE) {
			if ((user & GENMASK(4, 0)) & BIT(3)) {
				/* Master is CP */
				port = "CP";
			} else {
				/* Master is CPU cluster */
				/* user & GENMASK(1, 0) = core number */
				port = "CPU";
				/* TODO: Core Number */
			}
			dest = node->name;
		} else {
			master = itm_get_masterinfo(itm, node->name, user);
			if (!master) {
				pr_info("failed to get master IP with "
					"port:%s, id:%x\n", node->name, user);
				port = node->name;
			} else {
				port = node->name;
				source = master->master_name;
			}
		}
	}
	pr_info("--------------------------------------------------------------------------------\n"
		"ROUTE INFORMATION - %s group\n"
		"> Path Type  : %s\n"
		"> Master     : %s %s\n"
		"> Target     : %s\n",
		group->name,
		itm_pathtype[val],
		port ? port : "See other ROUTE Information(Master)",
		source ? source : "",
		dest ? dest : "See other ROUTE Information(Target)");

	itm_post_handler_by_master(itm, group, port, source, read);
}

static void itm_report_info(struct itm_dev *itm,
			       struct itm_nodegroup *group,
			       struct itm_nodeinfo *node,
			       unsigned int offset)
{
	unsigned int errcode, int_info, info0, info1, info2;
	bool read = false, req = false;

	int_info = __raw_readl(node->regs + offset + REG_INT_INFO);
	if (!BIT_ERR_VALID(int_info)) {
		pr_info("%s(0x%08X) is stopover\n",
			node->name, node->phy_regs + offset);
		return;
	}

	errcode = BIT_ERR_CODE(int_info);
	info0 = __raw_readl(node->regs + offset + REG_EXT_INFO_0);
	info1 = __raw_readl(node->regs + offset + REG_EXT_INFO_1);
	info2 = __raw_readl(node->regs + offset + REG_EXT_INFO_2);

	switch(offset) {
	case OFFSET_REQ_R:
		read = true;
		/* fall down */
	case OFFSET_REQ_W:
		req = true;
		if (node->type == S_NODE) {
			/* Only S-Node is able to make log to registers */
			pr_info("invalid logging, see more following information\n");
			goto out;
		}
		break;
	case OFFSET_RESP_R:
		read = true;
		/* fall down */
	case OFFSET_RESP_W:
		req = false;
		if (node->type != S_NODE) {
			/* Only S-Node is able to make log to registers */
			pr_info("invalid logging, see more following information\n");
			goto out;
		}
		break;
	default:
		pr_info("Unknown Error - offset:%u\n", offset);
		goto out;
	}
	/* Normally fall down to here */
	itm_report_route(itm, group, node, offset, read);

	pr_info("--------------------------------------------------------------------------------\n"
		"TRANSACTION INFORMATION\n"
		"> Transaction     : %s %s\n"
		"> Target address  : 0x%04X_%08X\n"
		"> Error type      : %s\n",
		read ? "READ" : "WRITE",
		req ? "REQUEST" : "RESPONSE",
		(unsigned int)(info1 & GENMASK(3, 0)),
		info0,
		itm_errcode[errcode]);

out:
	/* report extention raw information of register */
	pr_info("--------------------------------------------------------------------------------\n"
		"NODE RAW INFORMATION\n"
		"> NODE INFO       : %s(%s, 0x%08X)\n"
		"> INTERRUPT_INFO  : 0x%08X\n"
		"> EXT_INFO_0      : 0x%08X\n"
		"> EXT_INFO_1      : 0x%08X\n"
		"> EXT_INFO_2      : 0x%08X\n\n",
		node->name,
		node->type ? "M_NODE" : "S_NODE",
		node->phy_regs + offset,
		int_info,
		info0,
		info1,
		info2);
}

static int itm_parse_info(struct itm_dev *itm,
			      struct itm_nodegroup *group,
			      bool clear)
{
	struct itm_nodeinfo *node = NULL;
	unsigned int val, offset, vec;
	unsigned long flags, bit = 0;
	int i, j, ret = 0;

	spin_lock_irqsave(&itm->ctrl_lock, flags);
	if (group) {
		/* Processing only this group and select detected node */
		vec = __raw_readl(group->regs);
		node = group->nodeinfo;
		if (!vec)
			goto exit;

		for_each_set_bit(bit, (unsigned long *)&vec, group->nodesize) {
			/* exist array */
			for (i = 0; i < OFFSET_NUM; i++) {
				offset = i * OFFSET_ERR_REPT;
				/* Check Request information */
				val = __raw_readl(node[bit].regs + offset + REG_INT_INFO);
				if (BIT_ERR_OCCURRED(val)) {
					/* This node occurs the error */
					itm_report_info(itm, group, &node[bit], offset);
					if (clear)
						__raw_writel(1, node[bit].regs + offset + REG_INT_CLR);
					ret = true;
				}
			}

		}
	} else {
		/* Processing all group & nodes */
		for (i = 0; i < ARRAY_SIZE(nodegroup); i++) {
			group = &nodegroup[i];
			vec = __raw_readl(group->regs);
			node = group->nodeinfo;
			bit = 0;

			for_each_set_bit(bit, (unsigned long *)&vec, group->nodesize) {
				for (j = 0; j < OFFSET_NUM; j++) {
					offset = j * OFFSET_ERR_REPT;
					/* Check Request information */
					val = __raw_readl(node[bit].regs + offset + REG_INT_INFO);
					if (BIT_ERR_OCCURRED(val)) {
						/* This node occurs the error */
						itm_report_info(itm, group, &node[bit], offset);
						if (clear)
							__raw_writel(1,
								node[j].regs + offset + REG_INT_CLR);
						ret = true;
					}
				}
			}
		}
	}
exit:
	spin_unlock_irqrestore(&itm->ctrl_lock, flags);
	return ret;
}

static irqreturn_t itm_irq_handler(int irq, void *data)
{
	struct itm_dev *itm = (struct itm_dev *)data;
	struct itm_platdata *pdata = itm->pdata;
	struct itm_nodegroup *group = NULL;
	bool ret;
	int i;

	/* convert from raw irq source to SPI irq number */
	irq = irq - 32;

	/* Search itm group */
	for (i = 0; i < ARRAY_SIZE(nodegroup); i++) {
		if (irq == nodegroup[i].irq) {
			group = &pdata->nodegroup[i];
			pr_info("\n--------------------------------------------------------------------------------\n");
			pr_info("ITM: Abnormal Traffic Detected: %d irq, %s group, 0x%x vector\n",
				irq, group->name, __raw_readl(group->regs));
			break;
		}
	}
	if (group) {
		ret = itm_parse_info(itm, group, true);
		if (!ret) {
			pr_info("ITM: parsing is failed: %s, %d irq, 0x%x vector\n",
				group->name, irq, __raw_readl(group->regs));
		} else {
			if (group->irq_occurred && !group->panic_delayed)
				panic("STOP infinite output by Exynos ITM");
			else
				group->irq_occurred++;
		}
	} else {
		pr_info("interrupt group is not valid: %d irq\n", irq);
	}

	return IRQ_HANDLED;
}

void itm_notifier_chain_register(struct notifier_block *block)
{
	atomic_notifier_chain_register(&itm_notifier_list, block);
}

static int itm_logging_panic_handler(struct notifier_block *nb,
				   unsigned long l, void *buf)
{
	struct itm_panic_block *itm_panic = (struct itm_panic_block *)nb;
	struct itm_dev *itm = itm_panic->pdev;
	int ret;

	if (!IS_ERR_OR_NULL(itm)) {
		/* Check error has been logged */
		ret = itm_parse_info(itm, NULL, false);
		if (!ret)
			pr_info("No found error in %s\n", __func__);
		else
			pr_info("Found errors in %s\n", __func__);
	}
	return 0;
}

static int itm_probe(struct platform_device *pdev)
{
	struct itm_dev *itm;
	struct itm_panic_block *itm_panic = NULL;
	struct itm_platdata *pdata;
	struct itm_nodeinfo *node;
	unsigned int irq_option = 0;
	char *dev_name;
	int ret, i, j;

	itm = devm_kzalloc(&pdev->dev, sizeof(struct itm_dev), GFP_KERNEL);
	if (!itm) {
		dev_err(&pdev->dev, "failed to allocate memory for driver's "
				"private data\n");
		return -ENOMEM;
	}
	itm->dev = &pdev->dev;

	spin_lock_init(&itm->ctrl_lock);

	pdata = devm_kzalloc(&pdev->dev, sizeof(struct itm_platdata), GFP_KERNEL);
	if (!pdata) {
		dev_err(&pdev->dev, "failed to allocate memory for driver's "
				"platform data\n");
		return -ENOMEM;
	}
	itm->pdata = pdata;
	itm->pdata->masterinfo = masterinfo;
	itm->pdata->rpathinfo = rpathinfo;
	itm->pdata->nodegroup = nodegroup;

	for (i = 0; i < ARRAY_SIZE(nodegroup); i++)
	{
		dev_name = nodegroup[i].name;
		node = nodegroup[i].nodeinfo;

		nodegroup[i].regs = devm_ioremap_nocache(&pdev->dev, nodegroup[i].phy_regs, SZ_16K);
		if (nodegroup[i].regs == NULL) {
			dev_err(&pdev->dev, "failed to claim register region - %s\n", dev_name);
			return -ENOENT;
		}


		if (initial_multi_irq_enable)
			irq_option = IRQF_GIC_MULTI_TARGET;

		ret = devm_request_irq(&pdev->dev, nodegroup[i].irq + 32,
					itm_irq_handler, irq_option,
					dev_name, itm);

		for (j = 0; j < nodegroup[i].nodesize; j++) {
			node[j].regs = devm_ioremap_nocache(&pdev->dev, node[j].phy_regs, SZ_16K);
			if (node[j].regs == NULL) {
				dev_err(&pdev->dev, "failed to claim register region - %s\n", dev_name);
				return -ENOENT;
			}
		}
	}

	itm_panic = devm_kzalloc(&pdev->dev,
			sizeof(struct itm_panic_block), GFP_KERNEL);

	if (!itm_panic) {
		dev_err(&pdev->dev, "failed to allocate memory for driver's "
				"panic handler data\n");
	} else {
		itm_panic->nb_panic_block.notifier_call =
					itm_logging_panic_handler;
		itm_panic->pdev = itm;
		atomic_notifier_chain_register(&panic_notifier_list,
					&itm_panic->nb_panic_block);
	}

	platform_set_drvdata(pdev, itm);

	itm_init(itm, true);
	pdata->probed = true;

	dev_info(&pdev->dev, "success to probe ITM driver\n");

	return 0;
}

static int itm_remove(struct platform_device *pdev)
{
	platform_set_drvdata(pdev, NULL);
	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int itm_suspend(struct device *dev)
{
	return 0;
}

static int itm_resume(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct itm_dev *itm = platform_get_drvdata(pdev);

	itm_init(itm, true);

	return 0;
}

static SIMPLE_DEV_PM_OPS(itm_pm_ops,
			 itm_suspend,
			 itm_resume);
#define ITM_PM	(itm_pm_ops)
#else
#define ITM_PM	NULL
#endif

static struct platform_driver exynos_itm_driver = {
	.probe		= itm_probe,
	.remove		= itm_remove,
	.driver		= {
		.name		= "exynos-itm",
		.of_match_table	= itm_dt_match,
		.pm		= &itm_pm_ops,
	},
};

module_platform_driver(exynos_itm_driver);

MODULE_DESCRIPTION("Samsung Exynos ITM DRIVER");
MODULE_AUTHOR("Hosung Kim <hosung0.kim@samsung.com");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:exynos-itm");
