/*
 * ak4678.c  --  audio driver for ak4678
 *
 * Copyright (C) 2012 Asahi Kasei Microdevices Corporation
 *  Author                Date        Revistion
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *     Linfeng            12/09/28       1.0
 *to test:
 *     improve the register debug function  	2012-09-28
 *     Headphone DAPM 2012-10-9
 *    improve SDOD(SDTO Capture Switch DAPM)    2012-11-01
 *      add port A and port B pcm interface  	2012-10-9
 *     improve I/O control interface        	2012-10-30
 *
 * (ak4678_read ----- snd_soc_read)
 * (ak4678_write ----- snd_soc_write)
 * (ak4678_writeMask ----- snd_soc_update_bits)

 *  delete  {ak4678_codec
 *  enum snd_soc_control_type control_type
 *  static struct ak4678_priv *ak4678_data }  2013-05-14
 *
 *  Modified without DSP                      2013-12-26
 *
 *  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*
 *                       16/09/13         2.0 Kernel 3.18.XX
 *
 *  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*
 *      keisak@casio     18/06/06         2.0 Kernel 4.4
 *  limitted version only to mic usage
 *  No support kcontrol, dpam_widget
 *
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 *
 */

#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/of_gpio.h>
#include <linux/pm_runtime.h>
#include <linux/regmap.h>
#include <linux/regulator/consumer.h>
#include <linux/regulator/fixed.h>
#include <linux/slab.h>
#include <sound/initval.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/tlv.h>

#include "ak4678mic.h"

#ifdef CONFIG_SND_SOC_SAMSUNG_VERBOSE_DEBUG
#define gprintk(fmt, x...) printk("%s: " fmt, __FUNCTION__, ##x)
#else
#define gprintk(x...)                                                          \
	do {                                                                       \
	} while (0)
#endif

#define SYSFS_CODEC

/*
	to control regulatoer for i2c should be done in domain pm instead of here
	since codec driver is a client driver of i2c parent driver
*/
// #define REGLATORCTL_CODEC

/*
	control pm for i2c to disable interrupt. Before pincontorl for i2c is done.
	so it should be done on machine driver
*/
// #define PINCTL_I2C

/*
	runtime pm support
*/
//#define RUNTIME_PM_I2C


#define AK4678_PORTIIS 0

static int ak4678_runtime_suspend(struct device* dev);
static int ak4678_runtime_resume(struct device* dev);

static const char* ak4678_core_supply_names[] = {
#ifdef REGLATORCTL_CODEC
	"vdd_18",
#endif
	"vdd_30",
};
#define AK4678_NUM_CORE_SUPPLIES (sizeof(ak4678_core_supply_names)/sizeof(ak4678_core_supply_names[0]))

/* AK4678 Codec Private Data */
struct ak4678_priv {
	struct snd_soc_codec* codec;
	struct i2c_client* i2c;
	u16 externClkMode;
	u16 onStereoEF; /* Stereo Enpahsis Filter ON */
	u16 onDvlcDrc;  /* DVLC and DRC ON */
	u16 fsno; 		/* fs  0 : fs <= 12kHz,  1: 12kHz < fs <= 24kHz, 2: fs > 24kHz */
	u16 pllMode;
	int pdn;
	bool suspended;
	struct regulator_bulk_data core_supplies[AK4678_NUM_CORE_SUPPLIES];
#ifdef PINCTL_I2C
	struct pinctrl *pinctrl;
	struct pinctrl_state *pinctrl_state_active;
	struct pinctrl_state *pinctrl_state_suspend;
#endif
};

/* ak4678 register cache & default register settings */
static const struct reg_default ak4678_reg[] = {
	{0x0, 0x00},  /*	0x00	AK4678_00_POWER_MANAGEMENT0	*/
	{0x1, 0x00},  /*	0x01	AK4678_01_POWER_MANAGEMENT1	*/
	{0x2, 0x00},  /*	0x02	AK4678_02_POWER_MANAGEMENT2	*/
	{0x3, 0xF6},  /*	0x03	AK4678_03_PLL_MODE_SELECT0	*/
	{0x4, 0x00},  /*	0x04	AK4678_04_PLL_MODE_SELECT1	*/
	{0x5, 0x02},  /*	0x05	AK4678_05_FORMAT_SELECT	*/
	{0x6, 0x00},  /*	0x06	AK4678_06_MIC_SIGNAL_SELECT	*/
	{0x7, 0xDD},  /*	0x07	AK4678_07_MIC_AMP_GAIN		*/
	{0x8, 0x00},  /*	0x08	AK4678_08_DIGITL_MIC	*/
	{0x9, 0x00},  /*	0x09	AK4678_09_DAC_PATH_SELECT	*/
	{0xA, 0x00},  /*	0x0A	AK4678_0A_LINE_MANAGEMENT	*/
	{0xB, 0x00},  /*	0x0B	AK4678_0B_HP_MANAGEMENT	*/
	{0xC, 0x50},  /*	0x0C	AK4678_0C_CP_CONTROL	*/
	{0xD, 0x00},  /*	0x0D	AK4678_0D_SPK_MANAGEMENT	*/
	{0xE, 0x03},  /*	0x0E	AK4678_0E_LINEOUT_VOLUME	*/
	{0xF, 0x19},  /*	0x0F	AK4678_0F_HP_VOLUME	*/
	{0x10, 0xDD}, /*	0x10	AK4678_10_SPRC_VOLUME	*/
	{0x11, 0xA9}, /*	0x11	AK4678_11_LIN_VOLUME	*/
	{0x12, 0xA9}, /*	0x12	AK4678_12_RIN_VOLUME	*/
	{0x13, 0xC9}, /*	0x13	AK4678_13_ALCREF_SELECT	*/
	{0x14, 0x00}, /*	0x14	AK4678_14_DIGMIX_CONTROL	*/
	{0x15, 0x10}, /*	0x15	AK4678_15_ALCTIMER_SELECT	*/
	{0x16, 0x01}, /*	0x16	AK4678_16_ALCMODE_CONTROL	*/
	{0x17, 0x02}, /*	0x17	AK4678_17_MODE0_CONTROL	*/
	{0x18, 0x43}, /*	0x18	AK4678_18_MODE1_CONTROL	*/
	{0x19, 0x12}, /*	0x19	AK4678_19_FILTER_SELECT0	*/
	{0x1A, 0x00}, /*	0x1A	AK4678_1A_FILTER_SELECT1	*/
	{0x1B, 0x00}, /*	0x1B	AK4678_1B_FILTER_SELECT2	*/
	{0x1C, 0x00}, /*	0x1C	AK4678_1C_SIDETONE_VOLUMEA	*/
	{0x1D, 0x0C}, /*	0x1D	AK4678_1D_LOUT_VOLUME	*/
	{0x1E, 0x0C}, /*	0x1E	AK4678_1E_ROUT_VOLUME	*/
	{0x1F, 0x00}, /*	0x1F	AK4678_1F_PCM_IF_MANAGEMENT	*/
	{0x20, 0x00}, /*	0x20	AK4678_20_PCM_IF_CONTROL0	*/
	{0x21, 0x00}, /*	0x21	AK4678_21_PCM_IF_CONTROL1	*/
	{0x22, 0x00}, /*	0x22	AK4678_22_SIDETONE_VOLUMEB	*/
	{0x23, 0x0C}, /*	0x23	AK4678_23_DVOLB_CONTROL	*/
	{0x24, 0x0C}, /*	0x24	AK4678_24_DVOLC_CONTROL	*/
	{0x25, 0x00}, /*	0x25	AK4678_25_DMIX_CONTROL0	*/
	{0x26, 0x00}, /*	0x26	AK4678_26_DMIX_CONTROL1	*/
	{0x27, 0x00}, /*	0x27	AK4678_27_DMIX_CONTROL2	*/
	{0x28, 0x00}, /*	0x28	AK4678_28_DMIX_CONTROL3	*/
	{0x29, 0x71}, /*	0x29	AK4678_29_FIL1_COEFFICIENT0	*/
	{0x2A, 0x1F}, /*	0x2A	AK4678_2A_FIL1_COEFFICIENT1	*/
	{0x2B, 0x1F}, /*	0x2B	AK4678_2B_FIL1_COEFFICIENT2	*/
	{0x2C, 0x21}, /*	0x2C	AK4678_2C_FIL1_COEFFICIENT3	*/
	{0x2D, 0x7F}, /*	0x2D	AK4678_2D_FIL2_COEFFICIENT0	*/
	{0x2E, 0x0C}, /*	0x2E	AK4678_2E_FIL2_COEFFICIENT1	*/
	{0x2F, 0xFF}, /*	0x2F	AK4678_2F_FIL2_COEFFICIENT2	*/
	{0x30, 0x38}, /*	0x30	AK4678_30_FIL2_COEFFICIENT3	*/
	{0x31, 0xA2}, /*	0x31	AK4678_31_FIL3_COEFFICIENT0	*/
	{0x32, 0x83}, /*	0x32	AK4678_32_FIL3_COEFFICIENT1	*/
	{0x33, 0x80}, /*	0x33	AK4678_33_FIL3_COEFFICIENT2	*/
	{0x34, 0x2E}, /*	0x34	AK4678_34_FIL3_COEFFICIENT3	*/
	{0x35, 0x5B}, /*	0x35	AK4678_35_EQ_COEFFICIENT0	*/
	{0x36, 0x23}, /*	0x36	AK4678_36_EQ_COEFFICIENT1	*/
	{0x37, 0x07}, /*	0x37	AK4678_37_EQ_COEFFICIENT2	*/
	{0x38, 0x28}, /*	0x38	AK4678_38_EQ_COEFFICIENT3	*/
	{0x39, 0xAA}, /*	0x39	AK4678_39_EQ_COEFFICIENT4	*/
	{0x3A, 0xEC}, /*	0x3A	AK4678_3A_EQ_COEFFICIENT5	*/
	{0x3B, 0x00}, /*	0x3B	AK4678_3B_E1_COEFFICIENT0	*/
	{0x3C, 0x00}, /*	0x3C	AK4678_3C_E1_COEFFICIENT1	*/
	{0x3D, 0x21}, /*	0x3D	AK4678_3D_E1_COEFFICIENT2	*/
	{0x3E, 0x35}, /*	0x3E	AK4678_3E_E1_COEFFICIENT3	*/
	{0x3F, 0xE6}, /*	0x3F	AK4678_3F_E1_COEFFICIENT4	*/
	{0x40, 0xE0}, /*	0x40	AK4678_40_E1_COEFFICIENT5	*/
	{0x41, 0x00}, /*	0x41	AK4678_41_E2_COEFFICIENT0	*/
	{0x42, 0x00}, /*	0x42	AK4678_42_E2_COEFFICIENT1	*/
	{0x43, 0xC1}, /*	0x43	AK4678_43_E2_COEFFICIENT2	*/
	{0x44, 0x2F}, /*	0x44	AK4678_44_E2_COEFFICIENT3	*/
	{0x45, 0xE6}, /*	0x45	AK4678_45_E2_COEFFICIENT4	*/
	{0x46, 0xE0}, /*	0x46	AK4678_46_E2_COEFFICIENT5	*/
	{0x47, 0x00}, /*	0x47	AK4678_47_E3_COEFFICIENT0	*/
	{0x48, 0x00}, /*	0x48	AK4678_48_E3_COEFFICIENT1	*/
	{0x49, 0x3C}, /*	0x49	AK4678_49_E3_COEFFICIENT2	*/
	{0x4A, 0x22}, /*	0x4A	AK4678_4A_E3_COEFFICIENT3	*/
	{0x4B, 0xE6}, /*	0x4B	AK4678_4B_E3_COEFFICIENT4	*/
	{0x4C, 0xE0}, /*	0x4C	AK4678_4C_E3_COEFFICIENT5	*/
	{0x4D, 0x00}, /*	0x4D	Not be used	*/
	{0x4E, 0x00}, /*	0x4E	Not be used	*/
	{0x4F, 0x00}, /*	0x4F	Not be used	*/
	{0x50, 0x48}, /*	0x50	AK4678_50_5BAND_E1_COEF0	*/
	{0x51, 0x00}, /*	0x51	AK4678_51_5BAND_E1_COEF1	*/
	{0x52, 0x91}, /*	0x52	AK4678_52_5BAND_E1_COEF2	*/
	{0x53, 0xE0}, /*	0x53	AK4678_53_5BAND_E1_COEF3	*/
	{0x54, 0xC9}, /*	0x54	AK4678_54_5BAND_E2_COEF0	*/
	{0x55, 0x00}, /*	0x55	AK4678_55_5BAND_E2_COEF1	*/
	{0x56, 0x8C}, /*	0x56	AK4678_56_5BAND_E2_COEF2	*/
	{0x57, 0x3D}, /*	0x57	AK4678_57_5BAND_E2_COEF3	*/
	{0x58, 0x6A}, /*	0x58	AK4678_58_5BAND_E2_COEF4	*/
	{0x59, 0xE2}, /*	0x59	AK4678_59_5BAND_E2_COEF5	*/
	{0x5A, 0x9C}, /*	0x5A	AK4678_5A_5BAND_E3_COEF0	*/
	{0x5B, 0x02}, /*	0x5B	AK4678_5B_5BAND_E3_COEF1	*/
	{0x5C, 0x67}, /*	0x5C	AK4678_5C_5BAND_E3_COEF2	*/
	{0x5D, 0x37}, /*	0x5D	AK4678_5D_5BAND_E3_COEF3	*/
	{0x5E, 0x07}, /*	0x5E	AK4678_5E_5BAND_E3_COEF4	*/
	{0x5F, 0xE8}, /*	0x5F	AK4678_5F_5BAND_E3_COEF5	*/
	{0x60, 0x20}, /*	0x60	AK4678_60_5BAND_E4_COEF0	*/
	{0x61, 0x08}, /*	0x61	AK4678_61_5BAND_E4_COEF1	*/
	{0x62, 0xD7}, /*	0x62	AK4678_62_5BAND_E4_COEF2	*/
	{0x63, 0x20}, /*	0x63	AK4678_63_5BAND_E4_COEF3	*/
	{0x64, 0xFF}, /*	0x64	AK4678_64_5BAND_E4_COEF4	*/
	{0x65, 0xF8}, /*	0x65	AK4678_65_5BAND_E4_COEF5	*/
	{0x66, 0x1B}, /*	0x66	AK4678_66_5BAND_E5_COEF0	*/
	{0x67, 0x14}, /*	0x67	AK4678_67_5BAND_E5_COEF1	*/
	{0x68, 0xCB}, /*	0x68	AK4678_68_5BAND_E5_COEF2	*/
	{0x69, 0xF7}, /*	0x69	AK4678_69_5BAND_E5_COEF3	*/
	{0x6A, 0x18}, /*	0x6A	AK4678_6A_5BAND_E1_GAIN	*/
	{0x6B, 0x18}, /*	0x6B	AK4678_6B_5BAND_E2_GAIN	*/
	{0x6C, 0x18}, /*	0x6C	AK4678_6C_5BAND_E3_GAIN	*/
	{0x6D, 0x18}, /*	0x6D	AK4678_6D_5BAND_E4_GAIN	*/
	{0x6E, 0x18}, /*	0x6E	AK4678_6R_5BAND_E5_GAIN	*/
	{0x6F, 0x00}, /*	0x6F	AK4678_6FH_RESERVED	*/
	{0x70, 0x00}, /*	0x70	AK4678_70_DRC_MODE_CONTROL	*/
	{0x71, 0x06}, /*	0x71	AK4678_71_NS_CONTROL	*/
	{0x72, 0x11}, /*	0x72	AK4678_72_NS_GAIN_AND_ATT_CONTROL	*/
	{0x73, 0x90}, /*	0x73	AK4678_73_NS_ON_LEVEL	*/
	{0x74, 0x8A}, /*	0x74	AK4678_74_NS_OFF_LEVEL	*/
	{0x75, 0x07}, /*	0x75	AK4678_75_NS_REFERENCE_SELECT	*/
	{0x76, 0x40}, /*	0x76	AK4678_76_NS_LPF_CO_EFFICIENT_0	*/
	{0x77, 0x07}, /*	0x77	AK4678_77_NS_LPF_CO_EFFICIENT_1	*/
	{0x78, 0x80}, /*	0x78	AK4678_78_NS_LPF_CO_EFFICIENT_2	*/
	{0x79, 0x2E}, /*	0x79	AK4678_79_NS_LPF_CO_EFFICIENT_3	*/
	{0x7A, 0xA9}, /*	0x7A	AK4678_7A_NS_HPF_CO_EFFICIENT_0	*/
	{0x7B, 0x1F}, /*	0x7B	AK4678_7B_NS_HPF_CO_EFFICIENT_1	*/
	{0x7C, 0xAD}, /*	0x7C	AK4678_7C_NS_HPF_CO_EFFICIENT_2	*/
	{0x7D, 0x20}, /*	0x7D	AK4678_7D_NS_HPF_CO_EFFICIENT_3	*/
	{0x7E, 0x00}, /*	0x7E	AK4678_7EH_RESERVED	*/
	{0x7F, 0x00}, /*	0x7F	AK4678_7FH_RESERVED	*/
	{0x80, 0x00}, /*	0x80	AK4678_80_DVLC_FILTER_SELECT	*/
	{0x81, 0x6F}, /*	0x81	AK4678_81_DVLC_MODE_CONTROL	*/
	{0x82, 0x18}, /*	0x82	AK4678_82_DVLCL_CURVE_X1	*/
	{0x83, 0x0C}, /*	0x83	AK4678_83_DVLCL_CURVE_Y1	*/
	{0x84, 0x10}, /*	0x84	AK4678_84_DVLCL_CURVE_X2	*/
	{0x85, 0x09}, /*	0x85	AK4678_85_DVLCL_CURVE_Y2	*/
	{0x86, 0x08}, /*	0x86	AK4678_86_DVLCL_CURVE_X3	*/
	{0x87, 0x08}, /*	0x87	AK4678_87_DVLCL_CURVE_Y3	*/
	{0x88, 0x7F}, /*	0x88	AK4678_88_DVLCL_SLOPE_1	*/
	{0x89, 0x1D}, /*	0x89	AK4678_89_DVLCL_SLOPE_2	*/
	{0x8A, 0x03}, /*	0x8A	AK4678_8A_DVLCL_SLOPE_3	*/
	{0x8B, 0x00}, /*	0x8B	AK4678_8B_DVLCL_SLOPE_4	*/
	{0x8C, 0x18}, /*	0x8C	AK4678_8C_DVLCM_CURVE_X1	*/
	{0x8D, 0x0C}, /*	0x8D	AK4678_8D_DVLCM_CURVE_Y1	*/
	{0x8E, 0x10}, /*	0x8E	AK4678_8E_DVLCM_CURVE_X2	*/
	{0x8F, 0x06}, /*	0x8F	AK4678_8F_DVLCM_CURVE_Y2	*/
	{0x90, 0x08}, /*	0x90	AK4678_90_DVLCM_CURVE_X3	*/
	{0x91, 0x04}, /*	0x91	AK4678_91_DVLCM_CURVE_Y3	*/
	{0x92, 0x7F}, /*	0x92	AK4678_92_DVLCM_SLOPE_1	*/
	{0x93, 0x4E}, /*	0x93	AK4678_93_DVLCM_SLOPE_2	*/
	{0x94, 0x0C}, /*	0x94	AK4678_94_DVLCM_SLOPE_3	*/
	{0x95, 0x00}, /*	0x95	AK4678_95_DVLCM_SLOPE_4	*/
	{0x96, 0x1C}, /*	0x96	AK4678_96_DVLCH_CURVE_X1	*/
	{0x97, 0x10}, /*	0x97	AK4678_97_DVLCH_CURVE_Y1	*/
	{0x98, 0x10}, /*	0x98	AK4678_98_DVLCH_CURVE_X2	*/
	{0x99, 0x0C}, /*	0x99	AK4678_99_DVLCH_CURVE_Y2	*/
	{0x9A, 0x08}, /*	0x9A	AK4678_9A_DVLCH_CURVE_X3	*/
	{0x9B, 0x09}, /*	0x9B	AK4678_9B_DVLCH_CURVE_Y3	*/
	{0x9C, 0x7F}, /*	0x9C	AK4678_9C_DVLCH_SLOPE_1	*/
	{0x9D, 0x12}, /*	0x9D	AK4678_9D_DVLCH_SLOPE_2	*/
	{0x9E, 0x07}, /*	0x9E	AK4678_9E_DVLCH_SLOPE_3	*/
	{0x9F, 0x01}, /*	0x9F	AK4678_9F_DVLCH_SLOPE_4	*/
	{0xA0, 0xAB}, /*	0xA0	AK4678_A0_DVLCL_LPF_CO_EFFICIENT_0	*/
	{0xA1, 0x00}, /*	0xA1	AK4678_A1_DVLCL_LPF_CO_EFFICIENT_1	*/
	{0xA2, 0x57}, /*	0xA2	AK4678_A2_DVLCL_LPF_CO_EFFICIENT_2	*/
	{0xA3, 0x21}, /*	0xA3	AK4678_A3_DVLCL_LPF_CO_EFFICIENT_3	*/
	{0xA4, 0x55}, /*	0xA4	AK4678_A4_DVLCM_HPF_CO_EFFICIENT_0	*/
	{0xA5, 0x1F}, /*	0xA5	AK4678_A5_DVLCM_HPF_CO_EFFICIENT_1	*/
	{0xA6, 0x57}, /*	0xA6	AK4678_A6_DVLCM_HPF_CO_EFFICIENT_2	*/
	{0xA7, 0x21}, /*	0xA7	AK4678_A7_DVLCM_HPF_CO_EFFICIENT_3	*/
	{0xA8, 0xB5}, /*	0xA8	AK4678_A8_DVLCM_LPF_CO_EFFICIENT_0	*/
	{0xA9, 0x05}, /*	0xA9	AK4678_A9_DVLCM_LPF_CO_EFFICIENT_1	*/
	{0xAA, 0x6A}, /*	0xAA	AK4678_AA_DVLCM_LPF_CO_EFFICIENT_2	*/
	{0xAB, 0x2B}, /*	0xAB	AK4678_AB_DVLCM_LPF_CO_EFFICIENT_3	*/
	{0xAC, 0x4B}, /*	0xAC	AK4678_AC_DVLCH_HPF_CO_EFFICIENT_0	*/
	{0xAD, 0x1A}, /*	0xAD	AK4678_AD_DVLCH_HPF_CO_EFFICIENT_1	*/
	{0xAE, 0x6A}, /*	0xAE	AK4678_AE_DVLCH_HPF_CO_EFFICIENT_2	*/
	{0xAF, 0x2B}, /*	0xAF	AK4678_AF_DVLCH_HPF_CO_EFFICIENT_3	*/
};

#define AK4678_FS_NUM 3
#define AK4678_FS_LOW 12000
#define AK4678_FS_MIDDLE 24000

#define AK4678__NUM 3

static u8 hpf2[AK4678_FS_NUM][4] = {
	{0xDE, 0x1D, 0x43, 0x24},
	{0xE6, 0x1E, 0x34, 0x22},
	{0x71, 0x1F, 0x1F, 0x21}
	};

static u8 fil3band[AK4678_FS_NUM][16] = {
	{0x87, 0x02, 0x0D, 0x25, 0x79, 0x1D, 0x0D, 0x25,
	 0x1D, 0x11, 0x3A, 0x02, 0xE3, 0x0E, 0x3A, 0x02},
	{0x50, 0x01, 0xA0, 0x22, 0xB0, 0x1E, 0xA0, 0x22,
	 0x04, 0x0A, 0x07, 0x34, 0xFC, 0x15, 0x07, 0x34},
	{0xAB, 0x00, 0x57, 0x21, 0x55, 0x1F, 0x57, 0x21,
	 0xB5, 0x05, 0x6A, 0x2B, 0x4B, 0x1A, 0x6A, 0x2B},
};

static u8 fil2ns[AK4678_FS_NUM][8] = {
	{0xEC, 0x15, 0xD7, 0x0B, 0xB0, 0x1E, 0xA0, 0x22},
	{0x7F, 0x0C, 0xFF, 0x38, 0x55, 0x1F, 0x57, 0x21},
	{0x40, 0x07, 0x80, 0x2E, 0xA9, 0x1F, 0xAD, 0x20},
};

static int set_fschgpara(struct snd_soc_codec* codec, int fsno)
{
	u16 i, nAddr, nWtm;
	gprintk("\n");
	nAddr = AK4678_29_FIL1_COEFFICIENT0;
	for (i = 0; i < 4; i++) {
		snd_soc_write(codec, nAddr, hpf2[fsno][i]);
		nAddr++;
	}

	nAddr = AK4678_A0_DVLCL_LPF_CO_EFFICIENT_0;
	for (i = 0; i < 16; i++) {
		snd_soc_write(codec, nAddr, fil3band[fsno][i]);
		nAddr++;
	}

	nAddr = AK4678_76_NS_LPF_CO_EFFICIENT_0;
	for (i = 0; i < 8; i++) {
		snd_soc_write(codec, nAddr, fil2ns[fsno][i]);
		nAddr++;
	}

	switch (fsno) {
	case 0:
		nWtm = AK4678_WTM_FS11;
		break;
	case 1:
		nWtm = AK4678_WTM_FS22;
		break;
	case 2:
	default:
		nWtm = AK4678_WTM_FS44;
		break;
	}

	snd_soc_update_bits(codec, AK4678_15_ALCTIMER_SELECT, AK4678_WTM, nWtm);
	return (0);
}

static int ak4678_hw_params(struct snd_pcm_substream* substream,
	struct snd_pcm_hw_params* params, struct snd_soc_dai* dai)
{
	struct snd_soc_codec* codec = dai->codec;
	struct ak4678_priv* ak4678 = snd_soc_codec_get_drvdata(codec);
	u8 fs;
	u8 mode = 0;
	u16 fsno2 = ak4678->fsno;

	gprintk("\n");

	/* exynos requiure codec to run 48KHz */
	fs = snd_soc_read(codec, AK4678_03_PLL_MODE_SELECT0);
	fs &= ~AK4678_FS;
	fs |= AK4678_FS_48KHZ;

	snd_soc_write(codec, AK4678_03_PLL_MODE_SELECT0, fs);

	if (ak4678->externClkMode == 1) {
		mode = snd_soc_read(codec, AK4678_04_PLL_MODE_SELECT1);
		mode &= ~AK4678_PLL_MODE_SELECT_1_CM;

		/* CONFIG_LINF MCLK usually is 256fs. */
		if (params_rate(params) <= AK4678_FS_MIDDLE) {
			fsno2 = 1;
			mode |= AK4678_PLL_MODE_SELECT_1_CM;
		} else {
			fsno2 = 2;
			mode |= AK4678_PLL_MODE_SELECT_1_CM0;
		}

		snd_soc_write(codec, AK4678_04_PLL_MODE_SELECT1, mode);
	}

	if (fsno2 != ak4678->fsno) {
		ak4678->fsno = fsno2;
		set_fschgpara(codec, fsno2);
	}

	return 0;
}

static int ak4678_set_dai_sysclk(
	struct snd_soc_dai* dai, int clk_id, unsigned int freq, int dir)
{
	gprintk("\n");
	return 0;
}

static int ak4678_set_dai_fmt(struct snd_soc_dai* dai, unsigned int fmt)
{
	struct snd_soc_codec* codec = dai->codec;
	u8 mode;
	u8 format;

	gprintk("\n");

	/* set master/slave audio interface */
	mode = snd_soc_read(codec, AK4678_04_PLL_MODE_SELECT1);

	switch (fmt & SND_SOC_DAIFMT_MASTER_MASK) {
	case SND_SOC_DAIFMT_CBS_CFS:
		gprintk("(Slave)\n");
		mode &= ~(AK4678_M_S);
		mode &= ~(AK4678_BCKO);
		break;
	case SND_SOC_DAIFMT_CBM_CFM:
		gprintk("(Master)\n");
		mode |= (AK4678_M_S);
		// mode |= (AK4678_BCKO);  //CONFIG_LINF  BICKO 64FS
		break;
	case SND_SOC_DAIFMT_CBS_CFM:
	case SND_SOC_DAIFMT_CBM_CFS:
	default:
		dev_err(codec->dev, "Clock mode unsupported");
		return -EINVAL;
	}

	format = snd_soc_read(codec, AK4678_05_FORMAT_SELECT);
	format &= ~AK4678_DIF;

	switch (fmt & SND_SOC_DAIFMT_FORMAT_MASK) {
	case SND_SOC_DAIFMT_I2S:
		format |= AK4678_DIF_I2S_MODE;
		break;
	case SND_SOC_DAIFMT_LEFT_J:
		format |= AK4678_DIF_MSB_MODE;
		break;
	case SND_SOC_DAIFMT_DSP_A:
		format |= AK4678_DIF_DSP_MODE;
		format |= AK4678_BCKP;
		format |= AK4678_MSBS;
		if ((fmt & SND_SOC_DAIFMT_INV_MASK) == SND_SOC_DAIFMT_IB_NF)
			format &= ~AK4678_BCKP;
		break;
	default:
		return -EINVAL;
	}

	/* set mode and format */
	snd_soc_write(codec, AK4678_05_FORMAT_SELECT, format);
	snd_soc_write(codec, AK4678_04_PLL_MODE_SELECT1, mode);

	return 0;
}

static int ak4678_set_dai_pll(struct snd_soc_dai* dai, int pll_id, int source,
	unsigned int freq_in, unsigned int freq_out)
{
	struct snd_soc_codec* codec = dai->codec;

	// pll slave mode only 64fs, 48K
	gprintk("\n");
	snd_soc_update_bits( codec, AK4678_03_PLL_MODE_SELECT0, 0xF, 0x3);
	snd_soc_write(codec, AK4678_04_PLL_MODE_SELECT1, AK4678_PMPLL | AK4678_BCKO);
	return 0;
}

static int ak4678_set_dai_clkdiv(struct snd_soc_dai* dai, int div_id, int div)
{
	u8 ret = 0;

	gprintk("\n");

	return ret;
}

static int ak4678_hw_free(
	struct snd_pcm_substream* substream, struct snd_soc_dai* dai)
{
	int ret = 0;
	gprintk("\n");
	/* we must stop power here before stop supplying codec power
	 at i2s_shutdown */
	ret = ak4678_runtime_suspend(dai->codec->dev);
	return ret;
}

void ak4678_set_dai_shutdown(
	struct snd_pcm_substream* substream, struct snd_soc_dai* dai)
{
	gprintk("\n");
	/* this function is called after i2s_shut down
	 so we can do nothing here */

}

static int ak4678_set_prepare(
	struct snd_pcm_substream* substream, struct snd_soc_dai* dai)
{
	int ret = 0;
	struct snd_soc_codec* codec = dai->codec;
	gprintk("\n");
	/* record prepare */
	/*  <ctl name="PFSEL" value="ADC"/> */
	/*  <ctl name="PFSDO" value="PFSEL"/> */
	snd_soc_update_bits( codec, AK4678_19_FILTER_SELECT0, 3, 2);
	/*  <ctl name="SDOL" value="PFSDO Lch"/>
        <ctl name="SDOR" value="PFSDO Rch"/> */
	snd_soc_update_bits( codec, AK4678_28_DMIX_CONTROL3, 0xF, 0);
	/* <ctl name="SDTO Capture Switch" value="Enable"/> */
	// snd_soc_update_bits(codec, AK4678_05_FORMAT_SELECT, 0x10, 0);
	snd_soc_update_bits(codec, AK4678_00_POWER_MANAGEMENT0, 0x032, 0x012);
	gprintk("done \n");

	return ret;
}

static int ak4678_set_dai_startup(
	struct snd_pcm_substream* substream, struct snd_soc_dai* dai)
{
	struct snd_soc_codec* codec = dai->codec;
	int ret = 0;

	gprintk("\n");
	ak4678_runtime_resume(codec->dev);

	ret = snd_soc_update_bits(
		codec, AK4678_00_POWER_MANAGEMENT0, AK4678_PMVCM, AK4678_PMVCM);

	return ret;
}

static int ak4678_trigger(
	struct snd_pcm_substream* substream, int cmd, struct snd_soc_dai* dai)
{
	int ret = 0;
//	struct snd_soc_codec* codec = dai->codec;

	gprintk("cmd = %d\n", cmd);
	switch(cmd) {
		case SNDRV_PCM_TRIGGER_START:
//			ret = snd_soc_update_bits(codec, AK4678_00_POWER_MANAGEMENT0, 0x02, 0x02);
			break;

		case SNDRV_PCM_TRIGGER_STOP:
	        /*	<ctl name="ADCL Mux" value="OFF"/>
        		<ctl name="ADCR Mux" value="OFF"/> */
			break;
	}


	return ret;
}

static int ak4678_set_bias_level(
	struct snd_soc_codec* codec, enum snd_soc_bias_level level)
{
	struct ak4678_priv* ak4678 = snd_soc_codec_get_drvdata(codec);

	gprintk("BIAS LEVLE =%d\n", level);

	switch (level) {
	case SND_SOC_BIAS_ON:
	case SND_SOC_BIAS_PREPARE:
	case SND_SOC_BIAS_STANDBY:
		snd_soc_update_bits(
			codec, AK4678_00_POWER_MANAGEMENT0, AK4678_PMVCM, AK4678_PMVCM);
		break;
	case SND_SOC_BIAS_OFF:
		if (ak4678->pllMode == 1)
			snd_soc_update_bits(codec, AK4678_04_PLL_MODE_SELECT1, 0x01, 0x0);

		snd_soc_update_bits(
			codec, AK4678_00_POWER_MANAGEMENT0, AK4678_PMVCM, 0);
		break;
	}
	return 0;
}

static int ak4678_set_dai_mute(struct snd_soc_dai* dai, int mute)
{
	struct snd_soc_codec* codec = dai->codec;
	int ret = 0;
	u32 mute_reg;

	gprintk("mute[%s]\n", mute ? "ON" : "OFF");

	mute_reg = snd_soc_read(codec, AK4678_18_MODE1_CONTROL) & 0x4;

	if (mute_reg == mute)
		return 0;

	if (mute)
		ret |= snd_soc_update_bits(codec, AK4678_18_MODE1_CONTROL, 0x4, 0x4);
	else
		ret |= snd_soc_update_bits(codec, AK4678_18_MODE1_CONTROL, 0x4, 0);

	return ret;
}

static struct snd_soc_dai_ops ak4678_dai_ops = {
	.hw_params = ak4678_hw_params,
	.set_sysclk = ak4678_set_dai_sysclk,
	.set_fmt = ak4678_set_dai_fmt,
	.trigger = ak4678_trigger,
	.set_clkdiv = ak4678_set_dai_clkdiv,
	.set_pll = ak4678_set_dai_pll,
	.hw_free = ak4678_hw_free,
	.shutdown = ak4678_set_dai_shutdown,
	.startup = ak4678_set_dai_startup,
	.digital_mute = ak4678_set_dai_mute,
	.prepare = ak4678_set_prepare,
};

struct snd_soc_dai_driver ak4678_dai[] = {
	{
		.name = "ak4678-mic",
		.id = AK4678_PORTIIS,
		.capture =
			{
				.stream_name = "Capture",
				.channels_min = 1,
				.channels_max = 2,
				.rate_min = 8000,
				.rate_max = 52000,
				.rates = SNDRV_PCM_RATE_CONTINUOUS,
				// .rates = SNDRV_PCM_RATE_48000,
				.formats = SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S24_LE,
			},
		.ops = &ak4678_dai_ops,
	},
};

static int ak4678_write_def_reg(
	struct snd_soc_codec* codec, unsigned int regs, unsigned int rege)
{
	unsigned int reg;
	reg = regs;
	do {
		snd_soc_write(codec, reg, ak4678_reg[reg].def);
		reg++;
	} while (reg <= rege);
	return (0);
}

static int ak4678_set_reg_digital_effect(struct snd_soc_codec* codec)
{
	ak4678_write_def_reg(
		codec, AK4678_13_ALCREF_SELECT, AK4678_16_ALCMODE_CONTROL);
	ak4678_write_def_reg(
		codec, AK4678_29_FIL1_COEFFICIENT0, AK4678_3A_EQ_COEFFICIENT5);
	ak4678_write_def_reg(
		codec, AK4678_50_5BAND_E1_COEF0, AK4678_69_5BAND_E5_COEF3);
	ak4678_write_def_reg(codec, AK4678_76_NS_LPF_CO_EFFICIENT_0,
		AK4678_7D_NS_HPF_CO_EFFICIENT_3);
	ak4678_write_def_reg(
		codec, AK4678_82_DVLCL_CURVE_X1, AK4678_AF_DVLCH_HPF_CO_EFFICIENT_3);

	return (0);
}

static int ak4678_init_reg(struct snd_soc_codec* codec)
{
	struct ak4678_priv* ak4678 = snd_soc_codec_get_drvdata(codec);

	gprintk("\n");

	if (ak4678->pdn > 0) {
		/* need 2usec low sustained level in advance */
		usleep_range(100, 10000);
		gpio_set_value(ak4678->pdn, 1);
	}

	udelay(10);

	/* Dummy command */
	snd_soc_write(codec, AK4678_00_POWER_MANAGEMENT0, 0x0);
	ak4678_set_bias_level(codec, SND_SOC_BIAS_STANDBY);

	// Set SPK VOLUME 3db , default 0db(0x10 == 0xB)
	snd_soc_write(codec, AK4678_10_SPRC_VOLUME, 0xc);

	// set mic gain 12 dB
	snd_soc_write(codec, AK4678_07_MIC_AMP_GAIN, 0x99);

	ak4678_set_reg_digital_effect(codec);

	return 0;
}

#ifdef PINCTL_I2C
static int i2c_pinctrl_init(struct ak4678_priv* ak4678)
{
	int err;
	struct device *dev = &ak4678->i2c->dev;

	ak4678->pinctrl = devm_pinctrl_get(&(ak4678->i2c->dev));
	if (IS_ERR_OR_NULL(ak4678->pinctrl)) {
		err = PTR_ERR(ak4678->pinctrl);
		dev_err(dev, "Target does not use pinctrl %d\n", err);
		return err;
	}

	ak4678->pinctrl_state_active = pinctrl_lookup_state(ak4678->pinctrl,
			"i2c_active");
	if (IS_ERR_OR_NULL(ak4678->pinctrl_state_active)) {
		err = PTR_ERR(ak4678->pinctrl_state_active);
		dev_err(dev, "Can not lookup active pinctrl state %d\n", err);
		return err;
	}

	ak4678->pinctrl_state_suspend = pinctrl_lookup_state(ak4678->pinctrl,
			"i2c_suspend");
	if (IS_ERR_OR_NULL(ak4678->pinctrl_state_suspend)) {
		err = PTR_ERR(ak4678->pinctrl_state_suspend);
		dev_err(dev, "Can not lookup suspend pinctrl state %d\n", err);
		return err;
	}
	return 0;
}
#endif

static int ak4678_probe(struct snd_soc_codec* codec)
{
	struct ak4678_priv* ak4678 = snd_soc_codec_get_drvdata(codec);
	int ret = 0;

	gprintk("\n");

#ifdef PINCTL_I2C
	if (i2c_pinctrl_init(ak4678) != 0)
		gprintk("failed pin control\n");
#endif
	if (ak4678->pdn > 0) {
		ret = gpio_request(ak4678->pdn, "ak4678 pdn");
		if (ret) {
			gprintk("failed to request reset gpio ak4678 pdn\n");
			return ret;
		}
		gpio_direction_output(ak4678->pdn, 0);
		gpio_set_value(ak4678->pdn, 0);
	}

	ak4678->codec = codec;

	// ak4678_init_reg(codec);

	ak4678->externClkMode = 0;
	ak4678->onStereoEF = 0;
	ak4678->onDvlcDrc = 0;
	ak4678->fsno = 2;

	ak4678->pllMode = 0;

	return ret;
}

static int ak4678_remove(struct snd_soc_codec* codec)
{
	struct ak4678_priv* ak4678 = snd_soc_codec_get_drvdata(codec);
	gprintk("\n");

	ak4678_set_bias_level(codec, SND_SOC_BIAS_OFF);

	if (ak4678->pdn > 0) {
		gpio_set_value(ak4678->pdn, 0);
		gpio_free(ak4678->pdn);
	}

	return 0;
}

static int ak4678_suspend(struct snd_soc_codec* codec)
{
	gprintk("\n");
	return 0;
}

static int ak4678_resume(struct snd_soc_codec* codec)
{
	gprintk("\n");
	return 0;
}

static int ak4678_runtime_suspend(struct device* dev)
{
	int ret;
	struct ak4678_priv* ak4678 = dev_get_drvdata(dev);

	gprintk("\n");

	if (ak4678->suspended) {
		gprintk("Already in suspend state\n");
		return 0;
	}

	if (ak4678->pdn > 0) {
		gpio_set_value(ak4678->pdn, 0);
	}

#ifdef PINCTL_I2C
	if (ak4678->pinctrl) {
		ret = pinctrl_select_state(ak4678->pinctrl,	ak4678->pinctrl_state_suspend);
		if (ret < 0)
			dev_err(dev, "Could not set pin to suspend state %d\n", ret);
	}
#endif

	ret = regulator_bulk_disable(
		ARRAY_SIZE(ak4678->core_supplies), ak4678->core_supplies);
	if (ret != 0) {
		dev_err(dev, "Failed to disable supplies: %d\n", ret);
		return ret;
	}

	ak4678->suspended = true;

	return 0;
}

static int ak4678_runtime_resume(struct device* dev)
{

	struct ak4678_priv* ak4678 = dev_get_drvdata(dev);
	int ret;

	gprintk("\n");

	if (!ak4678->suspended) {
		gprintk("Not in suspend state\n");
		return 0;
	}
	ret = regulator_bulk_enable(
		ARRAY_SIZE(ak4678->core_supplies), ak4678->core_supplies);
	if (ret != 0) {
		dev_err(dev, "Failed to enable supplies: %d\n", ret);
	}

#ifdef PINCTL_I2C
	ret = pinctrl_select_state(ak4678->pinctrl, ak4678->pinctrl_state_active);
	if (ret != 0)
		dev_err(dev, "Could not set pin to active state %d\n", ret);
#endif
	ak4678_init_reg(ak4678->codec);

	ak4678->suspended = false;

	return 0;
}

#ifdef RUNTIME_PM_I2C
static int ak4678_runtime_idle(struct device* dev)
{
	gprintk("\n");
	return 0;
}

static const struct dev_pm_ops ak4678_pm_ops = {
	.runtime_suspend = ak4678_runtime_suspend,
	.runtime_resume = ak4678_runtime_resume,
	.runtime_idle = ak4678_runtime_idle,
};
#endif

static struct snd_soc_codec_driver soc_codec_dev_ak4678 = {
	.probe = ak4678_probe,
	.remove = ak4678_remove,
	.suspend = ak4678_suspend,
	.resume = ak4678_resume,
	.set_bias_level = ak4678_set_bias_level,
};

static const struct regmap_config ak4678_regmap = {
	.reg_bits = 8,
	.val_bits = 8,

	.max_register = AK4678_MAX_REGISTERS,
	.reg_defaults = ak4678_reg,
	.num_reg_defaults = ARRAY_SIZE(ak4678_reg),
	.cache_type = REGCACHE_RBTREE,
};


#ifdef SYSFS_CODEC

static u32 addr1, addr2;

/*
 * store format :
 * 		wrte data :	"w <address> <data>" => @(paddres + offset) = data
 * 		read data :	"r <address> <data-data>" => @(paddres + offset) | "echo mixer"
 */

static ssize_t codec_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	char s[16];
	char *string = s;
	char *argv[3];
	int argc;
	unsigned int data1;
	struct ak4678_priv* ak4678 = dev_get_drvdata(dev);
	unsigned int result;

	strncpy(s, buf, sizeof(s));
	for (argc=0; argc<3 ;argc++) {
		argv[argc] = strsep(&string, " ");
		if (argv[argc] == NULL) break;
	}

	if (count < 2) 	return count;

	switch (*argv[0]) {
	case 'w':
		if (argc >= 3) {
			result = kstrtouint(argv[1], 16, &addr1);
			result = kstrtouint(argv[2], 16, &data1);
			addr2 = addr1;
			snd_soc_write(ak4678->codec, addr1, (u8)data1);
		}
		break;
	case 'r':
		if (argc >= 2) {
			gprintk("argv[1] %s\n", argv[1]);
			argv[2] = strchr(argv[1],'-');
			if (argv[2] != NULL)
				*argv[2]++ = '\0';
			gprintk("argv %s-%s\n", argv[1], argv[2]);
			result = kstrtouint(argv[1], 16, &addr1);
			if (argv[2] != NULL)
				result = kstrtouint(argv[2], 16, &addr2);
			else
				addr2 = addr1;
			gprintk("r 0x%02x-0x%02x\n", addr1, addr2);
		}
		break;
	}
	return count;
}

static ssize_t codec_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	unsigned int count=0;
	struct ak4678_priv* ak4678 = dev_get_drvdata(dev);
	u8 reg_data;
	int i=0, imax;

	if (addr2 > 255) addr2 = 255;
	imax = addr2 - addr1 + 1;

	for (i=0; i<imax ;i++) {
		if (i%16 == 0)
			count += sprintf(buf+count, "@0x%x: ", addr1+i);
		reg_data = snd_soc_read(ak4678->codec, addr1+i);
		count += sprintf(buf+count, "0x%02x", reg_data);
		if (i == imax-1 || i%16 == 15)
			count += sprintf(buf+count, "\n");
		else
			count += sprintf(buf+count, ",");
	}
	return count;
}


static DEVICE_ATTR(codec, S_IRUGO | S_IWUSR | S_IWGRP,
		codec_show, codec_store);

static struct attribute *kaudioc_attr_list[] = {
	&dev_attr_codec.attr,
	NULL,
};

static struct attribute_group kaudioc_attr_grp = {
	.attrs = kaudioc_attr_list,
};

#endif

static int ak4678_i2c_probe(
	struct i2c_client* i2c, const struct i2c_device_id* id)
{
	struct ak4678_priv* ak4678;
	struct regmap* regmap;
	struct device_node* np = i2c->dev.of_node;
	int ret, i;

	gprintk("\n");

	ak4678 = devm_kzalloc(&i2c->dev, sizeof(struct ak4678_priv), GFP_KERNEL);
	if (ak4678 == NULL)
		return -ENOMEM;

	if (np) {
		ak4678->pdn = of_get_named_gpio(np, "ak4678,pdn-gpio", 0);
		if (ak4678->pdn < 0) {
			gprintk("Looking up %s property in node %s failed %d\n",
				"ak4678,pdn-gpio", np->full_name, ak4678->pdn);
			ak4678->pdn = -1;
			dev_err(&i2c->dev, "ak4678 PDN pin error\n");
			goto err;
		}

		if (!gpio_is_valid(ak4678->pdn)) {
			gprintk("gpio is unvalid\n");
			dev_err(&i2c->dev, "ak4678 pdn pin(%u) is invalid\n", ak4678->pdn);
			goto err;
		}
	}

	for (i = 0; i < ARRAY_SIZE(ak4678->core_supplies); i++)
		ak4678->core_supplies[i].supply = ak4678_core_supply_names[i];

	ret = devm_regulator_bulk_get(
		&i2c->dev, ARRAY_SIZE(ak4678->core_supplies), ak4678->core_supplies);
	if (ret != 0) {
		dev_err(&i2c->dev, "Failed to request core supplies: %d\n", ret);
		gprintk("Fialed to core supply\n");
		goto err;
	}

	ret = regulator_bulk_enable(
		ARRAY_SIZE(ak4678->core_supplies), ak4678->core_supplies);
	if (ret != 0) {
		gprintk("Fialed to bulk enable\n");
		dev_err(&i2c->dev, "Failed to enable core supplies: %d\n", ret);
		goto err;
	}

	regmap = devm_regmap_init_i2c(i2c, &ak4678_regmap);
	if (IS_ERR(regmap)) {
		ret = PTR_ERR(regmap);
		gprintk("regmap i2c error\n");
		goto err_regx;
	}

	gprintk("client data\n");
	i2c_set_clientdata(i2c, ak4678);

	ak4678->i2c = i2c;

	pm_runtime_set_active(&i2c->dev);
	pm_runtime_enable(&i2c->dev);
	pm_request_idle(&i2c->dev);

	gprintk("snd_soc_register_codec\n");
	ret = snd_soc_register_codec(
		&i2c->dev, &soc_codec_dev_ak4678, ak4678_dai, 1);
	if (ret != 0) {
		gprintk("failed probe\n");
err_regx:
		regulator_bulk_disable(
			ARRAY_SIZE(ak4678->core_supplies), ak4678->core_supplies);
err:
		gprintk("failed probe\n");
		return ret;
	}

#ifdef SYSFS_CODEC
	gprintk("kobj.name=%s, p.name=%s", i2c->dev.kobj.name, i2c->dev.kobj.parent->name);
	ret = sysfs_create_group(&(i2c->dev.kobj), &kaudioc_attr_grp);
	if (ret) {
		dev_err(&i2c->dev, "Failed to create sysfs group, errno:%d\n", ret);
	}
#endif

	gprintk("exit\n");
	return 0;
}

static int ak4678_i2c_remove(struct i2c_client* client)
{
	struct ak4678_priv* ak4678 = i2c_get_clientdata(client);

	gprintk("\n");

#ifdef SYSFS_CODEC
	sysfs_remove_group(&(client->dev.kobj), &kaudioc_attr_grp);
#endif
	snd_soc_unregister_codec(&client->dev);
	kfree(ak4678);

	return 0;
}

static struct of_device_id ak4678_i2c_dt_ids[] = {
	{.compatible = "akm,ak4678"}, {}};
MODULE_DEVICE_TABLE(of, ak4678_i2c_dt_ids);

static const struct i2c_device_id ak4678_i2c_id[] = {{"ak4678", 0}, {}};
MODULE_DEVICE_TABLE(i2c, ak4678_i2c_id);

static struct i2c_driver ak4678_i2c_driver = {
	.driver =
		{
			.name = "ak4678mic",
			.owner = THIS_MODULE,
			.of_match_table = of_match_ptr(ak4678_i2c_dt_ids),
#ifdef RUNTIME_PM_I2C
			.pm = &ak4678_pm_ops,
#endif
		},
	.probe = ak4678_i2c_probe,
	.remove = ak4678_i2c_remove,
	.id_table = ak4678_i2c_id,
};

static int __init ak4678_modinit(void)
{
	int ret;

	gprintk("\n");
	ret = i2c_add_driver(&ak4678_i2c_driver);
	if (ret)
		pr_err("Failed to register ak4678 I2C driver: %d\n", ret);

	return ret;
}
module_init(ak4678_modinit);

static void __exit ak4678_exit(void) { i2c_del_driver(&ak4678_i2c_driver); }
module_exit(ak4678_exit);

MODULE_DESCRIPTION("ASoC ak4678 mic codec driver");
MODULE_AUTHOR("AKM Corporation");
MODULE_LICENSE("GPL");
