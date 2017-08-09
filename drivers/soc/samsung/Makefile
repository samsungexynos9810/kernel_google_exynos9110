#
# Exynos SoC Specific drivers
#

obj-$(CONFIG_ARCH_EXYNOS)	+= exynos-powermode.o
obj-$(CONFIG_ARCH_EXYNOS)	+= exynos-pm.o

obj-$(CONFIG_ARM64_EXYNOS_CPUIDLE)	+= cpuidle_profiler.o

obj-$(CONFIG_ARCH_EXYNOS)	+= exynos-smc.o
obj-$(CONFIG_EXYNOS_RGT)	+= exynos-rgt.o

# CAL
obj-$(CONFIG_PWRCAL)            += pwrcal/
obj-$(CONFIG_CAL_IF)		+= cal-if/
obj-$(CONFIG_EXYNOS_PMU)	+= exynos-pmu.o
obj-$(CONFIG_EXYNOS_CHIPID)	+= exynos-chipid.o
obj-$(CONFIG_EXYNOS_REBOOT)	+= exynos-reboot.o
obj-$(CONFIG_EXYNOS_PD)		+= exynos-pd.o exynos-pd-dbg.o
# ACPM
obj-$(CONFIG_EXYNOS_ACPM)    	+= acpm/

obj-$(CONFIG_ECT)		+= ect_parser.o

# USI
obj-$(CONFIG_USI) += usi.o
