/*
 * Copyright (c) 2015 CASIO COMPUTER CO., LTD.
 *
 * EXINOS - GPIO lib support
 * EXINOS is the product by Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/


#ifndef __GPIO_PLUS_H
#define __GPIO_PLUS_H

#include <linux/interrupt.h>
#include <asm-generic/gpio.h>
#include <mach/irqs.h>



/* GPIO TLMM (Top Level Multiplexing) Definitions */

/* GPIO TLMM: Function -- GPIO specific */

/* GPIO TLMM: Direction */
enum {
	GPIO_CFG_INPUT,
	GPIO_CFG_OUTPUT,
};

/* GPIO TLMM: Pullup/Pulldown */
enum {
	GPIO_CFG_NO_PULL,
	GPIO_CFG_PULL_DOWN,
	GPIO_CFG_KEEPER,
	GPIO_CFG_PULL_UP,
};

/* GPIO TLMM: Drive Strength */
enum {
	GPIO_CFG_2MA,
	GPIO_CFG_4MA,
	GPIO_CFG_6MA,
	GPIO_CFG_8MA,
	GPIO_CFG_10MA,
	GPIO_CFG_12MA,
	GPIO_CFG_14MA,
	GPIO_CFG_16MA,
};

enum {
	GPIO_CFG_ENABLE,
	GPIO_CFG_DISABLE,
};

#define GPIO_CFG(gpio, func, dir, pull, drvstr) \
	((((gpio) & 0x3FF) << 4)        |	  \
	 ((func) & 0xf)                  |	  \
	 (((dir) & 0x1) << 14)           |	  \
	 (((pull) & 0x3) << 15)          |	  \
	 (((drvstr) & 0xF) << 17))

/**
 * extract GPIO pin from bit-field used for gpio_tlmm_config
 */
#define GPIO_PIN(gpio_cfg)    (((gpio_cfg) >>  4) & 0x3ff)
#define GPIO_FUNC(gpio_cfg)   (((gpio_cfg) >>  0) & 0xf)
#define GPIO_DIR(gpio_cfg)    (((gpio_cfg) >> 14) & 0x1)
#define GPIO_PULL(gpio_cfg)   (((gpio_cfg) >> 15) & 0x3)
#define GPIO_DRVSTR(gpio_cfg) (((gpio_cfg) >> 17) & 0xf)

int gpio_tlmm_config(unsigned config, unsigned disable);

#endif /* __GPIO_PLUS_H */
