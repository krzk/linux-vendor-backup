/*
* Copyright (C) 2014 Samsung Electronics Co. Ltd. All Rights Reserved.
*
* This software is licensed under the terms of the GNU General Public
* License version 2, as published by the Free Software Foundation, and
* may be copied, distributed, and modified under those terms.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
*/
#ifndef _LINUX_SEC_VIBRATOR_H
#define _LINUX_SEC_VIBRATOR_H
struct isa1000a_vibrator_platform_data {	int duty;	int period;	int max_timeout;	char *regulator_name;	unsigned int pwm_id;
};
#if 0
ssize_t motor_control_show_motor_on(void);
ssize_t motor_control_show_motor_off(void);
#endif
void vibtonz_pwm(int nForce);

#define MOTOR_VDD 3300000

#define GPIO_MOTOR_EN	EXYNOS3_GPE1(4)
#define GPIO_VIBTONE_PWM		EXYNOS3_GPD0(0)
#endif
