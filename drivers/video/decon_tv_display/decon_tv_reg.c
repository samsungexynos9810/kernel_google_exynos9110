/*
 * Samsung DECON_TV driver
 *
 * Copyright (c) 2010-2011 Samsung Electronics Co., Ltd.
 *
 * Tomasz Stanislawski, <t.stanislaws@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published
 * by the Free Software Foundiation. either version 2 of the License,
 * or (at your option) any later version
 */

#include "decon_tv.h"
#include "regs-decon_tv.h"

#include <linux/fb.h>
#include <plat/cpu.h>
#include <linux/delay.h>
#include <linux/ratelimit.h>
#include <linux/debugfs.h>
#include <linux/seq_file.h>

/* Register access subroutines */

static inline u32 dex_read(struct dex_device *dex, u32 reg_id)
{
	return readl(dex->res.dex_regs + reg_id);
}

static inline void dex_write(struct dex_device *dex, u32 reg_id, u32 val)
{
	writel(val, dex->res.dex_regs + reg_id);
}

static inline void dex_write_mask(struct dex_device *dex, u32 reg_id,
	u32 val, u32 mask)
{
	u32 old = dex_read(dex, reg_id);

	val = (val & mask) | (old & ~mask);
	writel(val, dex->res.dex_regs + reg_id);
}

void dex_shadow_protect(struct dex_device *dex, int idx, int en)
{
	if (en)
		dex_write_mask(dex, SHADOWCON, ~0, SHADOWCON_WINx_PROTECT(idx));
	else
		dex_write_mask(dex, SHADOWCON, 0, SHADOWCON_WINx_PROTECT(idx));
}

void dex_reg_reset(struct dex_device *dex)
{
	u32 val;
	unsigned long flags;

	spin_lock_irqsave(&dex->reg_slock, flags);

	/* s/w reset */
	dex_write_mask(dex, VIDCON0, ~0, VIDCON0_SWRESET);

	/* clock gating : pass */
	val = DECON_CMU_CLKGATE_MODE_SFR_F |
	      DECON_CMU_CLKGATE_MODE_MEM_F;
	dex_write_mask(dex, DECON_CMU, val, DECON_CMU_CLKGATE_MASK);

	/* blending mode : specifies the width of alpha value */
	dex_write_mask(dex, BLENDCON, ~0, BLENDCON_NEW_8BIT_ALPHA_VALUE);

	/* set output on for HDMI */
	val = VIDOUTCON0_LCD_F;
	/*
	 * TODO: if interlaced
	if (interlaced)
		val |= VIDOUTCON0_INTERLACE_EN_F;
	else
		val |= VIDOUTCON0_PROGRESSIVE_EN_F;
	*/
	dex_write_mask(dex, VIDOUTCON0, val, VIDOUTCON0_TV_MASK);

	/* enable interrupts */
	val = dex_read(dex, VIDINTCON0);
	val |= VIDINTCON0_INTEXTRAEN;
	val |= VIDINTCON0_INT_ENABLE;
	val |= VIDINTCON0_INT_FRAME;
	val |= VIDINTCON0_FRAMESEL0_VSYNC;
	val |= VIDINTCON0_FIFOLEVEL_EMPTY;
	val |= VIDINTCON0_INT_FIFO;
	val |= VIDINTCON0_FIFOSEL_MAIN_EN;
	dex_write_mask(dex, VIDINTCON0, val, VIDINTCON0_INT_MASK);

	/* specifies the VCLK hold scheme at underr-run */
	dex_write_mask(dex, VIDCON1, VIDCON1_VCLK_HOLD, VIDCON1_VCLK_MASK);

	/* enable CRC, CRCCLK */
	val = dex_read(dex, CRCCTRL);
	val |= CRCCTRL_CRCEN;
	val |= CRCCTRL_CRCSTART_F;
	val |= CRCCTRL_CRCCLKEN;
	dex_write_mask(dex, CRCCTRL, val, CRCCTRL_MASK);

	spin_unlock_irqrestore(&dex->reg_slock, flags);
}

void dex_update_regs(struct dex_device *dex, struct dex_reg_data *regs)
{
	unsigned short i;
	unsigned long flags;
	u32 val = 0;

	spin_lock_irqsave(&dex->reg_slock, flags);

	for (i = 1; i < DEX_MAX_WINDOWS; i++)
		if (!dex->windows[i]->local)
			dex_shadow_protect(dex, i, DEX_ENABLE);

	for (i = 1; i < DEX_MAX_WINDOWS; i++) {
		if (!dex->windows[i]->local) {
			dex_write(dex, WINCON(i), regs->wincon[i]);
			dex_write(dex, VIDOSD_A(i), regs->vidosd_a[i]);
			dex_write(dex, VIDOSD_B(i), regs->vidosd_b[i]);
			dex_write(dex, VIDOSD_C(i), regs->vidosd_c[i]);
			dex_write(dex, VIDOSD_D(i), regs->vidosd_d[i]);
			dex_write(dex, VIDW_BUF_START(i), regs->buf_start[i]);
			dex_write(dex, VIDW_BUF_END(i), regs->buf_end[i]);
			dex_write(dex, VIDW_BUF_SIZE(i), regs->buf_size[i]);
			dex_write(dex, BLENDEQ(i), regs->blendeq[i]);
			dex->windows[i]->dma_buf_data =
				regs->dma_buf_data[i];
		}
	}

	for (i = 1; i < DEX_MAX_WINDOWS; i++)
		if (!dex->windows[i]->local)
			dex_shadow_protect(dex, i, DEX_DISABLE);

	/* TODO: SLAVE_SYMC and STADALONE_F */
	val |= DECON_UPDATE_STANDALONE_F;
	val |= DECON_UPDATE_SLAVE_SYNC;
	dex_write_mask(dex, DECON_UPDATE, val, DECON_UPDATE_MASK);

	spin_unlock_irqrestore(&dex->reg_slock, flags);
}

int dex_reg_compare(struct dex_device *dex, int i, dma_addr_t addr)
{
	u32 val = dex_read(dex, SHD_VIDW_BUF_START(i));

	if (val != addr)
		return -EINVAL;
	else
		return 0;
}

irqreturn_t dex_irq_handler(int irq, void *dev_data)
{
	struct dex_device *dex = dev_data;
	ktime_t timestamp = ktime_get();
	u32 val;

	spin_lock(&dex->reg_slock);

	val = dex_read(dex, VIDINTCON1);
	if (val & VIDINTCON1_INT_FRAME) {
		/* VSYNC interrupt, accept it */
		dex_write_mask(dex, VIDINTCON1, ~0, VIDINTCON1_INT_FRAME);
		dex->vsync_timestamp = timestamp;
		wake_up_interruptible_all(&dex->vsync_wait);
	}
	if (val & VIDINTCON1_INT_FIFO) {
		dex_err("DECONTV FIFO underrun\n");
		dex_write_mask(dex, VIDINTCON1, ~0, VIDINTCON1_INT_FIFO);
	}
	if (val & VIDINTCON1_INT_I80) {
		dex_write_mask(dex, VIDINTCON1, ~0, VIDINTCON1_INT_I80);
#ifdef CONFIG_MACH_UNIVERSAL5430
		dex->vsync_timestamp = timestamp;
		wake_up_interruptible_all(&dex->vsync_wait);
#endif
	}

	spin_unlock(&dex->reg_slock);
	return IRQ_HANDLED;
}

void dex_reg_local_on(struct dex_device *dex, int idx)
{
	dex_write_mask(dex, WINCON(idx), ~0, WINCONx_ENLOCAL_F);
}

void dex_reg_local_off(struct dex_device *dex, int idx)
{
	dex_write_mask(dex, WINCON(idx), 0, WINCONx_ENLOCAL_F);
}

void dex_reg_streamon(struct dex_device *dex)
{
	unsigned long flags;

	spin_lock_irqsave(&dex->reg_slock, flags);

	dex_write_mask(dex, VIDCON0, ~0, VIDCON0_ENVID_F);
	dex_write_mask(dex, VIDCON0, ~0, VIDCON0_ENVID);

	spin_unlock_irqrestore(&dex->reg_slock, flags);
}

void dex_reg_streamoff(struct dex_device *dex)
{
	unsigned long flags;

	spin_lock_irqsave(&dex->reg_slock, flags);

	dex_write_mask(dex, VIDCON0, 0, VIDCON0_ENVID_F);
	dex_write_mask(dex, VIDCON0, 0, VIDCON0_ENVID);

	spin_unlock_irqrestore(&dex->reg_slock, flags);
}

int dex_reg_wait4update(struct dex_device *dex)
{
	ktime_t timestamp = dex->vsync_timestamp;
	int ret;

	ret = wait_event_interruptible_timeout(dex->vsync_wait,
		!ktime_equal(timestamp, dex->vsync_timestamp),
		msecs_to_jiffies(100));

	if (ret > 0)
		return 0;

	if (ret < 0)
		return ret;

	dex_warn("no vsync detected - timeout\n");
	return -ETIME;
}

static int dex_debugfs_show(struct seq_file *s, void *unused)
{
	struct dex_device *dex = s->private;

	mutex_lock(&dex->s_mutex);

	if (!dex->n_streamer) {
		seq_printf(s, "Not streaming\n");
		mutex_unlock(&dex->s_mutex);
		return 0;
	}

#define DUMPREG(reg_id) \
do { \
	seq_printf(s, "%-17s %08x\n", #reg_id, \
		(u32)readl(dex->res.dex_regs + reg_id)); \
} while (0)

	DUMPREG(VIDCON0);
	DUMPREG(VIDOUTCON0);
	DUMPREG(WINCON(0));
	DUMPREG(WINCON(1));
	DUMPREG(WINCON(2));
	DUMPREG(WINCON(3));
	DUMPREG(WINCON(4));

#undef DUMPREG

	mutex_unlock(&dex->s_mutex);
	return 0;
}

static int dex_debugfs_open(struct inode *inode, struct file *file)
{
	return single_open(file, dex_debugfs_show, inode->i_private);
}

static const struct file_operations dex_debugfs_fops = {
	.open           = dex_debugfs_open,
	.read           = seq_read,
	.llseek         = seq_lseek,
	.release        = single_release,
};

void dex_debugfs_init(struct dex_device *dex)
{
	debugfs_create_file(dev_name(dex->dev), S_IRUGO, NULL,
			    dex, &dex_debugfs_fops);
}

static void dex_reg_dex_dump(struct dex_device *dex)
{
#define DUMPREG(reg_id) \
do { \
	dex_dbg(#reg_id " = %08x\n", \
		(u32)readl(dex->res.dex_regs + reg_id)); \
} while (0)

	DUMPREG(VIDCON0);
	DUMPREG(VIDOUTCON0);
	DUMPREG(WINCON(0));
	DUMPREG(WINCON(1));
	DUMPREG(WINCON(2));
	DUMPREG(WINCON(3));
	DUMPREG(WINCON(4));

#undef DUMPREG
}

void dex_reg_dump(struct dex_device *dex)
{
	dex_reg_dex_dump(dex);
}
