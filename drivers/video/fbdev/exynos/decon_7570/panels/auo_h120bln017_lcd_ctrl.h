/*
	auo_h120bln017_lcd_ctrl.h
*/

#ifndef __AUO_H120BLN017_LCD_CTRL_H__
#define __AUO_H120BLN017_LCD_CTRL_H__

#include "decon_lcd.h"

void auo_h120bln017_lcd_init(struct decon_lcd *lcd);
void auo_h120bln017_lcd_enable(void);
void auo_h120bln017_lcd_disable(void);
void auo_h120bln017_lcd_lane_ctl(unsigned int lane_num);
int auo_h120bln017_lcd_gamma_ctrl(unsigned int backlightlevel);
int auo_h120bln017_lcd_gamma_update(void);
void auo_h120bln017_lcd_brightness_set(int brightness);
void auo_h120bln017_lcd_idle_mode(int on);
void auo_h120bln017_lcd_highbrightness_mode(int on);

#endif /* __AUO_H120BLN017_LCD_CTRL_H__ */
