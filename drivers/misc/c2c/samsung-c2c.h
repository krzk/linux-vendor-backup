/*
 * Samsung C2C driver
 *
 * Copyright (C) 2011 Samsung Electronics Co.Ltd
 * Author: Kisang Lee <kisang80.lee@samsung.com>
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */
#ifndef SAMSUNG_C2C_H
#define SAMSUNG_C2C_H

#include <mach/c2c.h>

/* This timer will be only used for debugging
#define ENABLE_C2CSTATE_TIMER
*/
#define C2C_DEV_NAME "c2c_dev"
#define C2C_SYSREG_DEFAULT 0x832AA803

enum c2c_set_clear {
	C2C_CLEAR = 0,
	C2C_SET = 1,
};

enum c2c_interrupt {
	C2C_INT_TOGGLE = 0,
	C2C_INT_HIGH = 1,
	C2C_INT_LOW = 2,
};

struct c2c_state_control {
	void __iomem *ap_sscm_addr;
	void __iomem *cp_sscm_addr;
#ifdef CONFIG_C2C_IPC_ENABLE
	void *shd_pages;
	struct c2c_ipc_handler hd;
#endif
	struct device *c2c_dev;

	u32 rx_width;
	u32 tx_width;

	u32 clk_opp100;
	u32 clk_opp50;
	u32 clk_opp25;

	struct clk *c2c_sclk;
	struct clk *c2c_aclk;

	enum c2c_opp_mode opp_mode;
	/* Below variables are needed in reset for retention */
	u32 retention_reg;
	void __iomem *c2c_sysreg;
};

#endif
