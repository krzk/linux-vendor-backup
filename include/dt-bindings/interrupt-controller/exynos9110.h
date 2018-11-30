/*
 * Copyright (c) 2017 Samsung Electronics Co., Ltd.
 *
 * Author: Kyungwoo Kang <kwoo.kang@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Device Tree binding constants for Exynos9110 interrupt controller.
*/

#ifndef _DT_BINDINGS_INTERRUPT_CONTROLLER_EXYNOS_9110_H
#define _DT_BINDINGS_INTERRUPT_CONTROLLER_EXYNOS_9110_H

#include <dt-bindings/interrupt-controller/arm-gic.h>

#define INTREQ_USI_CHUB_00_I2C				114
#define INTREQ_USI_CMGP_00_I2C				113
#define INTREQ_USI_CMGP_01_I2C				114
#define INTREQ_USI_CMGP_02_I2C				115
#define INTREQ_USI_CMGP_03_I2C				116
#define INTREQ_USI_CMGP_00					117
#define INTREQ_USI_CMGP_01					118
#define INTREQ_USI_CMGP_02					119
#define INTREQ_USI_CMGP_03					120
#define INTREQ_I2C_CMGP_4						148
#define INTREQ_I2C_CMGP_5						149
#define INTREQ_I2C_CMGP_6						150
#define INTREQ_USI_UART						245
#define INTREQ_I2C_0						246
#define INTREQ_I2C_1						247
#define INTREQ_I2C_2						248
#define INTREQ_USI_SPI						249
#define INTREQ_USI_I2C_00					250
#define INTREQ_USI_00						257
#define INTREQ_USI_00_I2C					258
#define INTREQ__WDT_CLUSTER0				232
#define INTREQ__RTC_ALARM_INT				101
#define INTREQ__RTC_TIC_INT_0				102
#define INTREQ__ADC_CMGP_S0				121
#define INTREQ__TMU							231
#define INTREQ__ABOX_GIC400				49

#endif	/* _DT_BINDINGS_INTERRUPT_CONTROLLER_EXYNOS_9110_H */
