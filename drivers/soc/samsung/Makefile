#
# Exynos SoC Specific drivers
#

#obj-$(CONFIG_ARCH_EXYNOS)	+= exynos-powermode.o
#obj-$(CONFIG_ARCH_EXYNOS)	+= exynos-pm.o

#obj-$(CONFIG_ARCH_EXYNOS)	+= exynos-smc.o

# CAL
obj-$(CONFIG_PWRCAL)            += pwrcal/
obj-$(CONFIG_CAL_IF)		+= cal-if/
obj-$(CONFIG_EXYNOS_PMU)	+= exynos-pmu.o
obj-$(CONFIG_EXYNOS_CHIPID)	+= exynos-chipid.o
obj-$(CONFIG_EXYNOS_REBOOT)	+= exynos-reboot.o
obj-$(CONFIG_EXYNOS_PD)        += pm_domains-cal.o pm_domains_sysfs.o

obj-$(CONFIG_ECT)		+= ect_parser.o

# USI
obj-$(CONFIG_USI) += usi.o
