/*
	sharp_lcd_ctrl.h
*/

#ifndef __SHARP_LCD_CTRL_H__
#define __SHARP_LCD_CTRL_H__

#include "decon_lcd.h"

int sharp_lcd_init(struct decon_lcd *lcd);
int sharp_lcd_enable(void);
int sharp_lcd_disable(void);
int sharp_lcd_lane_ctl(unsigned int lane_num);
int sharp_lcd_gamma_ctrl(unsigned int backlightlevel);
int sharp_lcd_gamma_update(void);
int sharp_lcd_brightness_set(int brightness);
int sharp_lcd_idle_mode(int on);

#endif /* __SHARP_LCD_CTRL_H__ */
