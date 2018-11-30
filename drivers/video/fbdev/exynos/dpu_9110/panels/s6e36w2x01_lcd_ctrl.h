/* drivers/video/fbdev/exynos/dpu_9110/panels/s6e36w2x01_lcd_ctrl.h
 *
 * Copyright (c) 2018 Samsung Electronics Co., Ltd.
 *
 * Haowe Li <haowei.li@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#ifndef __S6E36W2X01_LCD_CTRL_H__
#define __S6E36W2X01_LCD_CTRL_H__

#include "decon_lcd.h"
#include "mdnie_lite.h"

void s6e36w2x01_init(int id, struct decon_lcd *lcd);
void s6e36w2x01_enable(int id);
void s6e36w2x01_disable(int id);
int s6e36w2x01_gamma_ctrl(int id, unsigned int backlightlevel);
int s6e36w2x01_gamma_update(int id);

#endif /*__S6E36W2X01_LCD_CTRL_H__*/
