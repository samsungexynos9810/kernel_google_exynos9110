/* linux/drivers/video/decon_display/fimd_dt_exynos.c
 *
 * Copyright (c) 2013 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/of.h>
#include <linux/fb.h>
#include <linux/of_gpio.h>
#include <linux/platform_device.h>

#include "decon_display_driver.h"
#include "fimd_fb.h"
#include "decon_mipi_dsi.h"
#include "fimd_dt.h"

#define DISP_CTRL_NAME	"fimd_ctrl"
#define MIPI_DSI_NAME	"mipi_dsi"
#define MIPI_LCD_NAME	"mipi_lcd_info"

#define DT_READ_U32(node, key, value) do {\
		pprop = key; \
		if (of_property_read_u32((node), key, &temp)) \
			goto exception; \
		(value) = temp; \
	} while (0)

#define DT_READ_U32_OPTIONAL(node, key, value) do {\
		if (!of_property_read_u32((node), \
		key, &temp)) \
			(value) = temp; \
	} while (0)

#define DT_READ_U32_SETBITS(node, key, value, bit) do {\
		if (!of_property_read_u32((node), \
		key, &temp)) \
			(value) |= bit; \
	} while (0)


#define PARSE_FB_WIN_VARIANT(np, ...) do {\
		ret = parse_fb_win_variant(np, ##__VA_ARGS__); \
		if (ret < 0) \
			return ret; \
	} while (0)

#ifdef CONFIG_OF

static struct s3c_fb_driverdata g_fb_drvdata;
static struct s3c_fb_win_variant g_fb_win_variant[S3C_FB_MAX_WIN];
static struct s3c_fb_pd_win g_fb_win0;
static struct s3c_fb_platdata g_fimd_platdata;

struct mipi_dsim_config g_dsim_config = {
#if defined(CONFIG_DECON_LCD_S6E8AA0)
	.dsim_ddi_pd = &s6e8aa0_mipi_lcd_driver,
#elif defined(CONFIG_DECON_LCD_S6E3FA0)
	.dsim_ddi_pd = &s6e3fa0_mipi_lcd_driver,
#endif
};

#define FIMD_CONT_REG_INDEX	0
#define MIPI_DSI_REG_INDEX	1

static unsigned int g_dsi_power_gpio_num;
static unsigned int g_dsi_lcd_reset_gpio_num;
static unsigned int g_dp_hotplug_gpio_num;

struct mipi_dsim_lcd_config g_lcd_config;

static int parse_fimd_platdata(struct device_node *np)
{
	u32 temp;
	char *pprop;

	/* parse for fimd controller */
	DT_READ_U32(np, "samsung,vidcon0", g_fimd_platdata.vidcon0);
	DT_READ_U32(np, "samsung,vidcon1", g_fimd_platdata.vidcon1);
	DT_READ_U32(np, "samsung,default_win", g_fimd_platdata.default_win);
	DT_READ_U32(np, "samsung,left_margin",
		g_fb_win0.win_mode.left_margin);
	DT_READ_U32(np, "samsung,right_margin",
		g_fb_win0.win_mode.right_margin);
	DT_READ_U32(np, "samsung,upper_margin",
		g_fb_win0.win_mode.upper_margin);
	DT_READ_U32(np, "samsung,lower_margin",
		g_fb_win0.win_mode.lower_margin);
	DT_READ_U32(np, "samsung,hsync_len", g_fb_win0.win_mode.hsync_len);
	DT_READ_U32(np, "samsung,vsync_len", g_fb_win0.win_mode.vsync_len);
	DT_READ_U32(np, "samsung,xres", g_fb_win0.win_mode.xres);
	DT_READ_U32(np, "samsung,yres", g_fb_win0.win_mode.yres);
	DT_READ_U32(np, "samsung,virtual_x", g_fb_win0.virtual_x);
	DT_READ_U32(np, "samsung,virtual_y", g_fb_win0.virtual_y);
	DT_READ_U32(np, "samsung,width", g_fb_win0.width);
	DT_READ_U32(np, "samsung,height", g_fb_win0.height);
	DT_READ_U32(np, "samsung,max_bpp", g_fb_win0.max_bpp);
	DT_READ_U32(np, "samsung,default_bpp", g_fb_win0.default_bpp);

	g_fimd_platdata.win[0] = &g_fb_win0;
	g_fimd_platdata.win[1] = &g_fb_win0;
	g_fimd_platdata.win[2] = &g_fb_win0;
	g_fimd_platdata.win[3] = &g_fb_win0;
	g_fimd_platdata.win[4] = &g_fb_win0;

	/* parse for mipi-dsi driver */
	g_lcd_config.rgb_timing.left_margin = g_fb_win0.win_mode.left_margin;
	g_lcd_config.rgb_timing.right_margin = g_fb_win0.win_mode.right_margin;
	g_lcd_config.rgb_timing.upper_margin = g_fb_win0.win_mode.upper_margin;
	g_lcd_config.rgb_timing.lower_margin = g_fb_win0.win_mode.lower_margin;
	g_lcd_config.rgb_timing.hsync_len = g_fb_win0.win_mode.hsync_len;
	g_lcd_config.rgb_timing.vsync_len = g_fb_win0.win_mode.vsync_len;
	g_lcd_config.lcd_size.width = g_fb_win0.win_mode.xres;
	g_lcd_config.lcd_size.height = g_fb_win0.win_mode.yres;

	return 0;

exception:
	pr_err("%s: no property in the node, fb_variant.\n", pprop);
	return -EINVAL;
}

static int parse_fb_variant(struct device_node *np)
{
	u32 temp;
	char *pprop;

	DT_READ_U32(np, "nr_windows", g_fb_drvdata.variant.nr_windows);
	DT_READ_U32(np, "vidcon1", g_fb_drvdata.variant.vidcon1);
	DT_READ_U32(np, "vidtcon", g_fb_drvdata.variant.vidtcon);
	DT_READ_U32(np, "wincon", g_fb_drvdata.variant.wincon);
	DT_READ_U32(np, "winmap", g_fb_drvdata.variant.winmap);
	DT_READ_U32(np, "keycon", g_fb_drvdata.variant.keycon);
	DT_READ_U32(np, "osd", g_fb_drvdata.variant.osd);
	DT_READ_U32(np, "osd_stride", g_fb_drvdata.variant.osd_stride);
	DT_READ_U32(np, "buf_start", g_fb_drvdata.variant.buf_start);
	DT_READ_U32(np, "buf_size", g_fb_drvdata.variant.buf_size);
	DT_READ_U32(np, "buf_end", g_fb_drvdata.variant.buf_end);
	DT_READ_U32(np, "palette_0", g_fb_drvdata.variant.palette[0]);
	DT_READ_U32(np, "palette_1", g_fb_drvdata.variant.palette[1]);
	DT_READ_U32(np, "palette_2", g_fb_drvdata.variant.palette[2]);
	DT_READ_U32(np, "palette_3", g_fb_drvdata.variant.palette[3]);
	DT_READ_U32(np, "palette_4", g_fb_drvdata.variant.palette[4]);
	DT_READ_U32(np, "has_shadowcon", g_fb_drvdata.variant.has_shadowcon);
	DT_READ_U32(np, "has_blendcon", g_fb_drvdata.variant.has_blendcon);
	DT_READ_U32(np, "has_alphacon", g_fb_drvdata.variant.has_alphacon);
	DT_READ_U32(np, "has_fixvclk", g_fb_drvdata.variant.has_fixvclk);

	return 0;

exception:
	pr_err("%s: no property in the node, fb_variant.\n", pprop);
	return -EINVAL;
}

static int parse_fb_win_variant(struct device_node *np, char *node_name,
	struct s3c_fb_win_variant *pvar)
{
	u32 temp;
	int ret = 0;
	struct device_node *np_fbwin;

	np_fbwin = of_find_node_by_name(np, node_name);
	if (!np_fbwin) {
		pr_err("%s: could not find fb_win_variant node\n",
			node_name);
		return -EINVAL;
	}
	DT_READ_U32_OPTIONAL(np_fbwin, "has_osd_c", pvar->has_osd_c);
	DT_READ_U32_OPTIONAL(np_fbwin, "has_osd_d", pvar->has_osd_d);
	DT_READ_U32_OPTIONAL(np_fbwin, "has_osd_alpha", pvar->has_osd_alpha);
	DT_READ_U32_OPTIONAL(np_fbwin, "osd_size_off", pvar->osd_size_off);
	DT_READ_U32_OPTIONAL(np_fbwin, "palette_size", pvar->palette_sz);

	DT_READ_U32_SETBITS(np_fbwin, "VALID_BPP_1248",
		pvar->valid_bpp, VALID_BPP1248);
	DT_READ_U32_SETBITS(np_fbwin, "VALID_BPP_13",
		pvar->valid_bpp, VALID_BPP(13));
	DT_READ_U32_SETBITS(np_fbwin, "VALID_BPP_15",
		pvar->valid_bpp, VALID_BPP(15));
	DT_READ_U32_SETBITS(np_fbwin, "VALID_BPP_16",
		pvar->valid_bpp, VALID_BPP(16));
	DT_READ_U32_SETBITS(np_fbwin, "VALID_BPP_18",
		pvar->valid_bpp, VALID_BPP(18));
	DT_READ_U32_SETBITS(np_fbwin, "VALID_BPP_19",
		pvar->valid_bpp, VALID_BPP(19));
	DT_READ_U32_SETBITS(np_fbwin, "VALID_BPP_24",
		pvar->valid_bpp, VALID_BPP(24));
	DT_READ_U32_SETBITS(np_fbwin, "VALID_BPP_25",
		pvar->valid_bpp, VALID_BPP(25));
	DT_READ_U32_SETBITS(np_fbwin, "VALID_BPP_32",
		pvar->valid_bpp, VALID_BPP(32));

	return ret;
}

static int parse_fb_win_variants(struct device_node *np)
{
	int ret = 0;
	PARSE_FB_WIN_VARIANT(np, "fb_win_variant_0", &g_fb_win_variant[0]);
	PARSE_FB_WIN_VARIANT(np, "fb_win_variant_1", &g_fb_win_variant[1]);
	PARSE_FB_WIN_VARIANT(np, "fb_win_variant_2", &g_fb_win_variant[2]);
	PARSE_FB_WIN_VARIANT(np, "fb_win_variant_3", &g_fb_win_variant[3]);
	PARSE_FB_WIN_VARIANT(np, "fb_win_variant_4", &g_fb_win_variant[4]);
	return ret;
}

/* parse_display_dt_exynos
 */
static int parse_display_dt_exynos(struct device_node *np)
{
	int ret = 0;
	struct device_node *decon_np;

	if (!np) {
		pr_err("%s: device node is NULL\n", of_node_full_name(np));
		return -EINVAL;
	}
	ret = parse_fimd_platdata(np);
	if (ret < 0) {
		pr_err("parsing fimd platdata is failed.\n");
		return -EINVAL;
	}

              g_dp_hotplug_gpio_num = of_get_gpio(np, 0);

	decon_np = of_find_node_by_name(np, "fb_variant");
	if (!decon_np) {
		pr_err("%s: could not find fb_variant node\n",
			of_node_full_name(np));
		return -EINVAL;
	}
	ret = parse_fb_variant(decon_np);
	if (ret < 0) {
		pr_err("parsing fb_variant is failed.\n");
		return -EINVAL;
	}
	ret = parse_fb_win_variants(np);
	if (ret < 0) {
		pr_err("parsing fb_win_variant is failed.\n");
		return -EINVAL;
	}
	g_fb_drvdata.win[0] = &g_fb_win_variant[0];
	g_fb_drvdata.win[1] = &g_fb_win_variant[1];
	g_fb_drvdata.win[2] = &g_fb_win_variant[2];
	g_fb_drvdata.win[3] = &g_fb_win_variant[3];
	g_fb_drvdata.win[4] = &g_fb_win_variant[4];

	return ret;
}

static int parse_interrupt_dt_exynos(struct platform_device *pdev,
	struct display_driver *ddp)
{
	int ret = 0;
	struct resource *res;

	res = platform_get_resource(pdev, IORESOURCE_IRQ, 0);
	if (res == NULL) {
		pr_err("getting video irq resource failed\n");
		return -ENOENT;
	}
	ddp->decon_driver.irq_no = res->start;
	
	res = platform_get_resource(pdev, IORESOURCE_IRQ, 1);
	if (res == NULL) {
		pr_err("getting fifo irq resource failed\n");
		return -ENOENT;
	}
	ddp->decon_driver.fifo_irq_no = res->start;

	res = platform_get_resource(pdev, IORESOURCE_IRQ, 2);
	if (res == NULL) {
		pr_err("failed to get i80 frame done irq resource\n");
		return -ENOENT;
	}
	ddp->decon_driver.i80_irq_no = res->start;

#if 0 //DT structure needs to be fixed for supporting different parent types.
	res = platform_get_resource(pdev, IORESOURCE_IRQ, 3);
	if (res == NULL) {
		pr_err("failed to get mipi-dsi irq resource\n");
		return -ENOENT;
	}
	ddp->dsi_driver.dsi_irq_no = res->start;
#else
	//int-mipi-dsi0 = 113;
	//int-mipi-dsi1 = 114;
	ddp->dsi_driver.dsi_irq_no = 114; 
#endif
	return ret;
}

struct s3c_fb_driverdata *get_display_drvdata_exynos(void)
{
	return &g_fb_drvdata;
}

struct s3c_fb_platdata *get_display_platdata_exynos(void)
{
	return &g_fimd_platdata;
}

struct mipi_dsim_config *get_display_dsi_drvdata_exynos(void)
{
	return &g_dsim_config;
}

struct mipi_dsim_lcd_config *get_display_lcd_drvdata_exynos(void)
{
	return &g_lcd_config;
}

static int parse_dsi_drvdata(struct device_node *np)
{
	u32 temp;

	DT_READ_U32_OPTIONAL(np, "e_interface", g_dsim_config.e_interface);
	DT_READ_U32_OPTIONAL(np, "e_pixel_format",
		g_dsim_config.e_pixel_format);
	DT_READ_U32_OPTIONAL(np, "auto_flush", g_dsim_config.auto_flush);
	DT_READ_U32_OPTIONAL(np, "eot_disable", g_dsim_config.eot_disable);
	DT_READ_U32_OPTIONAL(np, "auto_vertical_cnt",
		g_dsim_config.auto_vertical_cnt);
	DT_READ_U32_OPTIONAL(np, "hse", g_dsim_config.hse);
	DT_READ_U32_OPTIONAL(np, "hfp", g_dsim_config.hfp);
	DT_READ_U32_OPTIONAL(np, "hbp", g_dsim_config.hbp);
	DT_READ_U32_OPTIONAL(np, "hsa", g_dsim_config.hsa);
	DT_READ_U32_OPTIONAL(np, "e_no_data_lane",
		g_dsim_config.e_no_data_lane);
	DT_READ_U32_OPTIONAL(np, "e_byte_clk", g_dsim_config.e_byte_clk);
	DT_READ_U32_OPTIONAL(np, "e_burst_mode", g_dsim_config.e_burst_mode);
	DT_READ_U32_OPTIONAL(np, "p", g_dsim_config.p);
	DT_READ_U32_OPTIONAL(np, "m", g_dsim_config.m);
	DT_READ_U32_OPTIONAL(np, "s", g_dsim_config.s);
	DT_READ_U32_OPTIONAL(np, "pll_stable_time",
		g_dsim_config.pll_stable_time);
	DT_READ_U32_OPTIONAL(np, "esc_clk", g_dsim_config.esc_clk);
	DT_READ_U32_OPTIONAL(np, "stop_holding_cnt",
		g_dsim_config.stop_holding_cnt);
	DT_READ_U32_OPTIONAL(np, "bta_timeout", g_dsim_config.bta_timeout);
	DT_READ_U32_OPTIONAL(np, "rx_timeout", g_dsim_config.rx_timeout);

	g_dsi_lcd_reset_gpio_num = of_get_gpio(np, 0);
	g_dsi_power_gpio_num = of_get_gpio(np, 1);

	return 0;
}

int get_display_dsi_lcd_reset_gpio_exynos(void)
{
	return g_dsi_lcd_reset_gpio_num;
}

int get_display_dsi_lcd_power_gpio_exynos(void)
{
        return g_dsi_power_gpio_num;
}

int get_display_dp_hotplug_gpio_exynos(void)
{
        return g_dp_hotplug_gpio_num;
}

int parse_display_dsi_dt_exynos(struct device_node *np)
{
	int ret = 0;

	if (!np) {
		pr_err("%s: no devicenode given\n", of_node_full_name(np));
		return -EINVAL;
	}
	ret = parse_dsi_drvdata(np);
	if (ret < 0) {
		pr_err("parsing MIPI DSI drvdata is failed.\n");
		goto end;
	}
end:
	return 0;
}

/* parse_all_dt_exynos -
 * parses TOP device tree for display subsystem */
static int parse_all_dt_exynos(struct device_node *parent)
{
	int ret = 0;
	struct device_node *node;

	/* display controller parsing */
	node = of_get_child_by_name(parent, DISP_CTRL_NAME);
	if (!node) {
		pr_err("device tree errror : empty fimd controller dt\n");
		return -EINVAL;
	}
	ret = parse_display_dt_exynos(node);
	if (ret < 0) {
		pr_err("parsing DT for fimd controller is failed.\n");
		return ret;
	}
	/* mipi dsi parsing */
	node = of_get_child_by_name(parent, MIPI_DSI_NAME);
	if (!node) {
		pr_err("device tree errror : empty mipi dsi dt\n");
		return -EINVAL;
	}
	ret = parse_display_dsi_dt_exynos(node);
	if (ret < 0) {
		pr_err("parsing for display controller was failed.\n");
		return ret;
	}

	return ret;
}
void dump_driver_data(void);
/* parse_display_driver_dt_exynos
 * Parses the DiviceTree data and initilaizes the display H/W info.
 * Initializes the base address and IRQ numbers of all the display system IPs. */
int parse_display_driver_dt_exynos(struct platform_device *pdev,
	struct display_driver *ddp)
{
	int ret = 0;
	struct device *dev = &pdev->dev;

	ddp->display_driver = dev;

	/* now get display controller resources */
	ddp->decon_driver.regs = platform_get_resource(pdev,
		IORESOURCE_MEM, FIMD_CONT_REG_INDEX);
	if (!ddp->decon_driver.regs) {
		pr_err("failed to find FIMD registers\n");
		return -ENOENT;
	}
	ddp->dsi_driver.regs = platform_get_resource(pdev,
		IORESOURCE_MEM, MIPI_DSI_REG_INDEX);
	if (!ddp->dsi_driver.regs) {
		pr_err("failed to find MIPI-DSI registers\n");
		return -ENOENT;
	}
             printk("###%s: decon_driver.regs %x dsi_driver.regs %x \n\n\n", __func__, *(unsigned int*)ddp->decon_driver.regs, *(unsigned int*)ddp->dsi_driver.regs);

	/* starts to parse device tree */
	ret = parse_interrupt_dt_exynos(pdev, ddp);
	if (ret < 0) {
		pr_err("interrupt parse error system\n");
		return -EINVAL;
	}

	ret = parse_all_dt_exynos(dev->of_node);
	if (ret < 0) {
		pr_err("device tree parse error system\n");
		return -EINVAL;
	}

dump_driver_data();
	return ret;
}

void dump_s3c_fb_variant(struct s3c_fb_variant *p_fb_variant)
{
	pr_err("[INFO] is_2443:1: 0x%0X\n", p_fb_variant->is_2443);
	pr_err("[INFO] nr_windows: 0x%0X\n", p_fb_variant->nr_windows);
	pr_err("[INFO] vidtcon: 0x%0X\n", p_fb_variant->vidtcon);
	pr_err("[INFO] wincon: 0x%0X\n", p_fb_variant->wincon);
	pr_err("[INFO] winmap: 0x%0X\n", p_fb_variant->winmap);
	pr_err("[INFO] keycon: 0x%0X\n", p_fb_variant->keycon);
	pr_err("[INFO] buf_start: 0x%0X\n", p_fb_variant->buf_start);
	pr_err("[INFO] buf_end: 0x%0X\n", p_fb_variant->buf_end);
	pr_err("[INFO] buf_size: 0x%0X\n", p_fb_variant->buf_size);
	pr_err("[INFO] osd: 0x%0X\n", p_fb_variant->osd);
	pr_err("[INFO] osd_stride: 0x%0X\n", p_fb_variant->osd_stride);
	pr_err("[INFO] palette[0]: 0x%0X\n", p_fb_variant->palette[0]);
	pr_err("[INFO] palette[1]: 0x%0X\n", p_fb_variant->palette[1]);
	pr_err("[INFO] palette[2]: 0x%0X\n", p_fb_variant->palette[2]);
	pr_err("[INFO] palette[3]: 0x%0X\n", p_fb_variant->palette[3]);
	pr_err("[INFO] palette[4]: 0x%0X\n", p_fb_variant->palette[4]);

	pr_err("[INFO] has_prtcon:1: 0x%0X\n", p_fb_variant->has_prtcon);
	pr_err("[INFO] has_shadowcon:1: 0x%0X\n", p_fb_variant->has_shadowcon);
	pr_err("[INFO] has_blendcon:1: 0x%0X\n", p_fb_variant->has_blendcon);
	pr_err("[INFO] has_alphacon:1: 0x%0X\n", p_fb_variant->has_alphacon);
	pr_err("[INFO] has_clksel:1: 0x%0X\n", p_fb_variant->has_clksel);
	pr_err("[INFO] has_fixvclk:1: 0x%0X\n", p_fb_variant->has_fixvclk);
};

void dump_s3c_fb_win_variant(struct s3c_fb_win_variant *p_fb_win_variant)
{
	pr_err("[INFO] has_osd_c:1: 0x%0X\n", p_fb_win_variant->has_osd_c);
	pr_err("[INFO] has_osd_d:1: 0x%0X\n", p_fb_win_variant->has_osd_d);
	pr_err("[INFO] has_osd_alpha:1: 0x%0X\n",
		p_fb_win_variant->has_osd_alpha);
	pr_err("[INFO] palette_16bpp:1: 0x%0X\n",
		p_fb_win_variant->palette_16bpp);
	pr_err("[INFO] osd_size_off: 0x%0X\n", p_fb_win_variant->osd_size_off);
	pr_err("[INFO] palette_sz: 0x%0X\n", p_fb_win_variant->palette_sz);
	pr_err("[INFO] valid_bpp: 0x%0X\n", p_fb_win_variant->valid_bpp);
}

void dump_s3c_fb_win_variants(struct s3c_fb_win_variant p_fb_win_variant[],
	int num)
{
	int i;
	for (i = 0; i < num; ++i) {
		pr_err("[INFO] -------- %d --------\n", i);
		dump_s3c_fb_win_variant(&g_fb_win_variant[i]);
	}
}


void dump_driver_data()
{
	dump_s3c_fb_variant(&g_fb_drvdata.variant);
	dump_s3c_fb_win_variants(g_fb_win_variant, S3C_FB_MAX_WIN);
}

#endif
