# arch/arm/mach-exynos/pwrcal/Makefile
#
# Copyright (c) 2014-2014 Samsung Electronics Co., Ltd.
#		http://www.samsung.com/
#
# Licensed under GPLv2


obj-y				+= power-cal.o pwrcal-cmu.o pwrcal-pll.o pwrcal-dfs.o \
				pwrcal-pmu.o pwrcal-rae.o pwrcal-vclk.o pwrcal-dbg.o
obj-$(CONFIG_PMUCAL_MOD)	+= pmucal_mod/pmucal_system.o pmucal_mod/pmucal_local.o \
				   pmucal_mod/pmucal_cpu.o pmucal_mod/pmucal_rae.o

ifeq ($(CONFIG_SOC_EXYNOS8890), y)
obj-y				+= S5E8890/S5E8890-cmu.o S5E8890/S5E8890-pmu.o \
				S5E8890/S5E8890-dfs.o S5E8890/S5E8890-asv.o \
				S5E8890/S5E8890-syspwr.o S5E8890/S5E8890-vclk.o \
				S5E8890/S5E8890-drampara.o S5E8890/S5E8890-pll.o \
				S5E8890/S5E8890-rae.o
endif
ifeq ($(CONFIG_SOC_EXYNOS7270), y)
obj-y				+= S5E7270/S5E7270-cmu.o \
				S5E7270/S5E7270-dfs.o S5E7270/S5E7270-asv.o \
				S5E7270/S5E7270-vclk.o \
				S5E7270/S5E7270-drampara.o S5E7270/S5E7270-pll.o \
				S5E7270/S5E7270-rae.o
endif
