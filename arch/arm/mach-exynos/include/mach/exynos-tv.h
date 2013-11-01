/*
 * arch/arm/mach-exynos/include/mach/exynos-tv.h
 *
 * Exynos TV driver core functions
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef _EXYNOS_TV_H
#define _EXYNOS_TV_H

/*
 * These functions are only for use with the core support code, such as
 * the CPU-specific initialization code.
 */

enum tv_ip_version {
	IP_VER_TV_5H = 1,
	IP_VER_TV_5A2,
};

#define is_ip_ver_5h	(pdata->ip_ver == IP_VER_TV_5H)

/* Re-define device name to differentiate the subsystem in various SoCs. */
static inline void s5p_hdmi_setname(char *name)
{
#ifdef CONFIG_S5P_DEV_TV
	s5p_device_hdmi.name = name;
#endif
}

struct s5p_platform_cec {
#ifdef CONFIG_S5P_DEV_TV
	void    (*cfg_gpio)(struct platform_device *pdev);
#endif
};

struct s5p_hdmi_platdata {
	enum tv_ip_version ip_ver;
};

struct s5p_dex_platdata {
	enum tv_ip_version ip_ver;
};

extern void s5p_hdmi_set_platdata(struct s5p_hdmi_platdata *pd);

#endif /* _EXYNOS_TV_H */
