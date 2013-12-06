/*
 * Copyright (c) 2013 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef __EXYNOS_BTS_H_
#define __EXYNOS_BTS_H_

#if defined(CONFIG_EXYNOS5430_BTS) || defined(CONFIG_EXYNOS5422_BTS)
void bts_initialize(const char *pd_name, bool on);
#else
#define bts_initialize(a, b) do {} while (0)
#endif

#if defined(CONFIG_EXYNOS5422_BTS)
void bts_change_g3d_state(unsigned int freq);
#else
#define bts_change_g3d_state(a) do {} while(0)
#endif


#endif
