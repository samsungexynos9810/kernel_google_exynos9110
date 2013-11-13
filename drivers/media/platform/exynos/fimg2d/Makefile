#
# Copyright (c) 2013 Samsung Electronics Co., Ltd.
#		http://www.samsung.com
#
# Licensed under GPLv2
#

ifeq ($(CONFIG_SOC_EXYNOS5430),y)
g2d-objs := g2d-core.o g2d-regs.o g2d-vb2.o
obj-$(CONFIG_VIDEO_EXYNOS_FIMG2D)	+= g2d.o
endif

ifeq ($(CONFIG_SOC_EXYNOS5422),y)
obj-$(CONFIG_VIDEO_EXYNOS_FIMG2D)	+= fimg2d_drv.o fimg2d_clk.o \
					fimg2d_ctx.o fimg2d_cache.o fimg2d_helper.o \
					fimg2d4x_blt.o fimg2d4x_hw.o fimg2d_cache_asm.o

ccflags-y += -DDEBUG
ccflags-y += -DBLIT_WORKQUE
ccflags-$(CONFIG_FIMG2D_CCI_SNOOP) += -DCCI_SNOOP
endif
