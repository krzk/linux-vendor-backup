/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 * Copyright (C) Samsung Electronics, 2017
 *
 *    2017      Vyacheslav Cherkashin <v.cherkashin@samsung.com>
 */


#include <kprobe/swap_kprobes.h>
#include <linux/backlight.h>
#include "lcd_base.h"


static const char path_bl[] = "/sys/class/backlight/panel/brightness";
static const char path_bl_max[] = "/sys/class/backlight/panel/max_brightness";
static const char path_power[] = "/sys/class/lcd/panel/lcd_power";

static const char * const all_path[] = {
	path_bl,
	path_bl_max,
	path_power
};


static int sprdfb_check(struct lcd_ops *ops)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(all_path); ++i) {
		int ret = read_val(all_path[i]);

		if (IS_ERR_VALUE(ret))
			return 0;
	}

	return 1;
}

static unsigned long sprdfb_get_parameter(struct lcd_ops *ops,
					  enum lcd_parameter_type type)
{
	switch (type) {
	case LPD_MIN_BRIGHTNESS:
		return 0;
	case LPD_MAX_BRIGHTNESS:
		return read_val(path_bl_max);
	case LPD_BRIGHTNESS:
		return read_val(path_bl);
	case LPD_POWER:
		return read_val(path_power);
	}

	return -EINVAL;
}


static int entry_handler_set_power(struct kretprobe_instance *ri,
				   struct pt_regs *regs);
static int ret_handler_set_power(struct kretprobe_instance *ri,
				 struct pt_regs *regs);

static struct kretprobe set_power_krp = {
	.kp.symbol_name = "sprdfb_set_power",
	.entry_handler = entry_handler_set_power,
	.handler = ret_handler_set_power,
	.data_size = sizeof(int)
};


static int entry_handler_set_backlight(struct kretprobe_instance *ri,
				       struct pt_regs *regs);
static int ret_handler_set_backlight(struct kretprobe_instance *ri,
				     struct pt_regs *regs);

static struct kretprobe set_backlight_krp = {
	.kp.symbol_name = "panel_update_brightness",
	.entry_handler = entry_handler_set_backlight,
	.handler = ret_handler_set_backlight,
	.data_size = sizeof(int)
};

int sprdfb_set(struct lcd_ops *ops)
{
	int ret;

	ret = swap_register_kretprobe(&set_power_krp);
	if (ret)
		return ret;

	ret = swap_register_kretprobe(&set_backlight_krp);
	if (ret)
		swap_unregister_kretprobe(&set_power_krp);

	return ret;
}

int sprdfb_unset(struct lcd_ops *ops)
{
	swap_unregister_kretprobe(&set_backlight_krp);
	swap_unregister_kretprobe(&set_power_krp);

	return 0;
}

static struct lcd_ops sprdfb_ops = {
	.name = "sprdfb_panel",
	.check = sprdfb_check,
	.set = sprdfb_set,
	.unset = sprdfb_unset,
	.get = sprdfb_get_parameter
};

struct lcd_ops *LCD_MAKE_FNAME(sprdfb_panel)(void)
{
	return &sprdfb_ops;
}


/* ============================================================================
 * ===                               POWER                                  ===
 * ============================================================================
 */
static int entry_handler_set_power(struct kretprobe_instance *ri,
				   struct pt_regs *regs)
{
	int *power = (int *)ri->data;

	*power = (int)swap_get_karg(regs, 1);

	return 0;
}

static int ret_handler_set_power(struct kretprobe_instance *ri,
				 struct pt_regs *regs)
{
	int *power = (int *)ri->data;
	int ret = regs_return_value(regs);

	if (!ret && sprdfb_ops.notifier)
		sprdfb_ops.notifier(&sprdfb_ops, LAT_POWER, INT2VOIDP(*power));

	return 0;
}


/* ============================================================================
 * ===                              BACKLIGHT                               ===
 * ============================================================================
 */
static int entry_handler_set_backlight(struct kretprobe_instance *ri,
				       struct pt_regs *regs)
{
	int *br = (int *)ri->data;
	struct backlight_device *bd;

	bd = (struct backlight_device *)swap_get_karg(regs, 0);
	*br = bd->props.brightness;

	return 0;
}

static int ret_handler_set_backlight(struct kretprobe_instance *ri,
				     struct pt_regs *regs)
{
	int *br = (int *)ri->data;
	int ret = regs_return_value(regs);

	if (!ret && sprdfb_ops.notifier)
		sprdfb_ops.notifier(&sprdfb_ops, LAT_BRIGHTNESS,
				    INT2VOIDP(*br));

	return 0;
}
