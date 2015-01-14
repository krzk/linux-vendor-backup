/*
 * Copyright (C) 2015 Samsung Electronics Co., Ltd.
 *	Inha Song <ideal.song@samsung.com>
 *
 * Low Power Audio SubSystem driver for Samsung Exynos
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */
#ifndef __SND_SOC_SAMSUNG_LPASS_H
#define __SND_SOC_SAMSUNG_LPASS_H

/* SFR */
#define SFR_LPASS_INTR_CA5_MASK	(0x48)
#define SFR_LPASS_INTR_CPU_MASK	(0x58)

/* SW_RESET */
#define LPASS_SW_RESET_CA5	(1 << 0)
#define LPASS_SW_RESET_SB	(1 << 11)

/* Interrupt mask */
#define LPASS_INTR_APM		(1 << 9)
#define LPASS_INTR_MIF		(1 << 8)
#define LPASS_INTR_TIMER	(1 << 7)
#define LPASS_INTR_DMA		(1 << 6)
#define LPASS_INTR_GPIO		(1 << 5)
#define LPASS_INTR_I2S		(1 << 4)
#define LPASS_INTR_PCM		(1 << 3)
#define LPASS_INTR_SB		(1 << 2)
#define LPASS_INTR_UART		(1 << 1)
#define LPASS_INTR_SFR		(1 << 0)

#endif /* __SND_SOC_SAMSUNG_LPASS_H */
