/* include/video/exynos_mipi_dsim.h
 *
 * Platform data header for Samsung SoC MIPI-DSIM.
 *
 * Copyright (c) 2012 Samsung Electronics Co., Ltd
 *
 * InKi Dae <inki.dae@samsung.com>
 * Donghwa Lee <dh09.lee@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#ifndef _EXYNOS_MIPI_DSIM_H
#define _EXYNOS_MIPI_DSIM_H

#include <linux/device.h>
#include <linux/fb.h>

#define PANEL_NAME_SIZE		(32)

/*
 * Enumerate display interface type.
 *
 * DSIM_COMMAND means cpu interface and rgb interface for DSIM_VIDEO.
 *
 * P.S. MIPI DSI Master has two display controller intefaces, RGB Interface
 *	for main display and CPU Interface(same as I80 Interface) for main
 *	and sub display.
 */
enum mipi_dsim_interface_type {
	DSIM_COMMAND,
	DSIM_VIDEO
};

enum mipi_dsim_virtual_ch_no {
	DSIM_VIRTUAL_CH_0,
	DSIM_VIRTUAL_CH_1,
	DSIM_VIRTUAL_CH_2,
	DSIM_VIRTUAL_CH_3
};

enum mipi_dsim_burst_mode_type {
	DSIM_NON_BURST_SYNC_EVENT,
	DSIM_BURST_SYNC_EVENT,
	DSIM_NON_BURST_SYNC_PULSE,
	DSIM_BURST,
	DSIM_NON_VIDEO_MODE
};

enum mipi_dsim_no_of_data_lane {
	DSIM_DATA_LANE_1,
	DSIM_DATA_LANE_2,
	DSIM_DATA_LANE_3,
	DSIM_DATA_LANE_4
};

enum mipi_dsim_byte_clk_src {
	DSIM_PLL_OUT_DIV8,
	DSIM_EXT_CLK_DIV8,
	DSIM_EXT_CLK_BYPASS
};

enum mipi_dsim_pixel_format {
	DSIM_CMD_3BPP,
	DSIM_CMD_8BPP,
	DSIM_CMD_12BPP,
	DSIM_CMD_16BPP,
	DSIM_VID_16BPP_565,
	DSIM_VID_18BPP_666PACKED,
	DSIM_18BPP_666LOOSELYPACKED,
	DSIM_24BPP_888
};

enum mipi_dsim_version {
	DSIM_VER_1_33,
	DSIM_VER_1_34,
};

enum {
	DSIM_WRITE_CMD		= 1 << 8,
	DSIM_READ_CMD		= 1 << 9,
	DSIM_BEGIN_CMD_SET	= 1 << 10,
	DSIM_END_CMD_SET	= 1 << 11
};

enum {
	DSIM_STATE_LP,
	DSIM_STATE_HS,
	DSIM_STATE_ULPS
};

struct exynos_dsi_command {
	struct list_head	list;
	unsigned int		type;
	const unsigned char	*tx_data;
	unsigned char		rx_data;
	unsigned int		len;
	unsigned char		*rx_payload;
	struct completion	completed;
};

/*
 * struct mipi_dsim_config - interface for configuring mipi-dsi controller.
 *
 * @auto_flush: enable or disable Auto flush of MD FIFO using VSYNC pulse.
 * @eot_disable: enable or disable EoT packet in HS mode.
 * @auto_vertical_cnt: specifies auto vertical count mode.
 *	in Video mode, the vertical line transition uses line counter
 *	configured by VSA, VBP, and Vertical resolution.
 *	If this bit is set to '1', the line counter does not use VSA and VBP
 *	registers.(in command mode, this variable is ignored)
 * @hse: set horizontal sync event mode.
 *	In VSYNC pulse and Vporch area, MIPI DSI master transfers only HSYNC
 *	start packet to MIPI DSI slave at MIPI DSI spec1.1r02.
 *	this bit transfers HSYNC end packet in VSYNC pulse and Vporch area
 *	(in mommand mode, this variable is ignored)
 * @hfp: specifies HFP disable mode.
 *	if this variable is set, DSI master ignores HFP area in VIDEO mode.
 *	(in command mode, this variable is ignored)
 * @hbp: specifies HBP disable mode.
 *	if this variable is set, DSI master ignores HBP area in VIDEO mode.
 *	(in command mode, this variable is ignored)
 * @hsa: specifies HSA disable mode.
 *	if this variable is set, DSI master ignores HSA area in VIDEO mode.
 *	(in command mode, this variable is ignored)
 * @cma_allow: specifies the number of horizontal lines, where command packet
 *	transmission is allowed after Stable VFP period.
 * @e_interface: specifies interface to be used.(CPU or RGB interface)
 * @e_virtual_ch: specifies virtual channel number that main or
 *	sub diaplsy uses.
 * @e_pixel_format: specifies pixel stream format for main or sub display.
 * @e_burst_mode: selects Burst mode in Video mode.
 *	in Non-burst mode, RGB data area is filled with RGB data and NULL
 *	packets, according to input bandwidth of RGB interface.
 *	In Burst mode, RGB data area is filled with RGB data only.
 * @e_no_data_lane: specifies data lane count to be used by Master.
 * @e_byte_clk: select byte clock source. (it must be DSIM_PLL_OUT_DIV8)
 *	DSIM_EXT_CLK_DIV8 and DSIM_EXT_CLK_BYPASSS are not supported.
 * @pll_stable_time: specifies the PLL Timer for stability of the ganerated
 *	clock(System clock cycle base)
 *	if the timer value goes to 0x00000000, the clock stable bit of status
 *	and interrupt register is set.
 * @esc_clk: specifies escape clock frequency for getting the escape clock
 *	prescaler value.
 * @stop_holding_cnt: specifies the interval value between transmitting
 *	read packet(or write "set_tear_on" command) and BTA request.
 *	after transmitting read packet or write "set_tear_on" command,
 *	BTA requests to D-PHY automatically. this counter value specifies
 *	the interval between them.
 * @bta_timeout: specifies the timer for BTA.
 *	this register specifies time out from BTA request to change
 *	the direction with respect to Tx escape clock.
 * @rx_timeout: specifies the timer for LP Rx mode timeout.
 *	this register specifies time out on how long RxValid deasserts,
 *	after RxLpdt asserts with respect to Tx escape clock.
 *	- RxValid specifies Rx data valid indicator.
 *	- RxLpdt specifies an indicator that D-PHY is under RxLpdt mode.
 *	- RxValid and RxLpdt specifies signal from D-PHY.
 */
struct mipi_dsim_config {
	unsigned char			auto_flush;
	unsigned char			eot_disable;

	unsigned char			auto_vertical_cnt;
	unsigned char			hse;
	unsigned char			hfp;
	unsigned char			hbp;
	unsigned char			hsa;
	unsigned char			cmd_allow;

	enum mipi_dsim_interface_type	e_interface;
	enum mipi_dsim_virtual_ch_no	e_virtual_ch;
	enum mipi_dsim_pixel_format	e_pixel_format;
	enum mipi_dsim_burst_mode_type	e_burst_mode;
	enum mipi_dsim_no_of_data_lane	e_no_data_lane;
	enum mipi_dsim_byte_clk_src	e_byte_clk;

	/*
	 * ===========================================
	 * |    P    |    M    |    S    |    MHz    |
	 * -------------------------------------------
	 * |    3    |   100   |    3    |    100    |
	 * |    3    |   100   |    2    |    200    |
	 * |    3    |    63   |    1    |    252    |
	 * |    4    |   100   |    1    |    300    |
	 * |    4    |   110   |    1    |    330    |
	 * |   12    |   350   |    1    |    350    |
	 * |    3    |   100   |    1    |    400    |
	 * |    4    |   150   |    1    |    450    |
	 * |    6    |   118   |    1    |    472    |
	 * |	3    |   120   |    1    |    480    |
	 * |   12    |   250   |    0    |    500    |
	 * |    4    |   100   |    0    |    600    |
	 * |    3    |    81   |    0    |    648    |
	 * |    3    |    88   |    0    |    704    |
	 * |    3    |    90   |    0    |    720    |
	 * |    3    |   100   |    0    |    800    |
	 * |   12    |   425   |    0    |    850    |
	 * |    4    |   150   |    0    |    900    |
	 * |   12    |   475   |    0    |    950    |
	 * |    6    |   250   |    0    |   1000    |
	 * -------------------------------------------
	 */

	/*
	 * pms could be calculated as the following.
	 * M * 24 / P * 2 ^ S = MHz
	 */
	unsigned char			p;
	unsigned short			m;
	unsigned char			s;

	unsigned int			pll_stable_time;
	unsigned long			esc_clk;

	unsigned short			stop_holding_cnt;
	unsigned char			bta_timeout;
	unsigned short			rx_timeout;
};

/*
 * struct mipi_dsim_device - global interface for mipi-dsi driver.
 *
 * @dev: driver model representation of the device.
 * @id: unique device id.
 * @clock: pointer to MIPI-DSI clock of clock framework.
 * @irq: interrupt number to MIPI-DSI controller.
 * @reg_base: base address to memory mapped SRF of MIPI-DSI controller.
 *	(virtual address)
 * @lock: the mutex protecting this data structure.
 * @dsim_info: infomation for configuring mipi-dsi controller.
 * @master_ops: callbacks to mipi-dsi operations.
 * @dsim_lcd_dev: pointer to activated ddi device.
 *	(it would be registered by mipi-dsi driver.)
 * @dsim_lcd_drv: pointer to activated_ddi driver.
 *	(it would be registered by mipi-dsi driver.)
 * @lcd_info: pointer to mipi_lcd_info structure.
 * @data_lane: specifiec enabled data lane number.
 *	this variable would be set by driver according to e_no_data_lane
 *	automatically.
 * @e_clk_src: select byte clock source.
 * @ddata: pointer to MIPI-DSI driver data.
 * @pd: pointer to MIPI-DSI driver platform data.
 */
struct mipi_dsim_device {
	struct device			*dev;
	int				id;
	struct resource			*res;
	struct clk			*clock;
	int			irq;
	void __iomem			*reg_base;
	struct mutex			lock;
	spinlock_t			atomic_lock;
	spinlock_t			bus_lock;
	spinlock_t			cmd_lock;
	struct list_head		cmd_list;

	struct mipi_dsim_config		*dsim_config;
	struct mipi_dsim_master_ops	*master_ops;
	struct mipi_dsim_lcd_device	*dsim_lcd_dev;
	struct mipi_dsim_lcd_driver	*dsim_lcd_drv;
	void				*src_pdev;

	unsigned int			data_lane;
	unsigned int			e_clk_src;
	unsigned int			clk_status;
	unsigned int			dpms;
	bool			lp_mode;
	atomic_t			in_trigger;
	atomic_t			bus_img_req_cnt;
	atomic_t			bus_cmd_req_cnt;
	atomic_t			pwr_gate;
	atomic_t			dpms_on;
	atomic_t			te_skip;
	unsigned int			state;
	bool			probed;
	unsigned int			dbg_cnt;
	bool			secure_mode;
	struct mipi_dsim_driverdata	*ddata;
	struct mipi_dsim_platform_data	*pd;
};

/*
 * struct mipi_dsim_driverdata - interface to driver data
 *	for mipi-dsi driver.
 *
 * @clk_name: specifies clock name of SoC.
 * @num_supply: number of supplies.
 * @supplies: list of supplies.
 */
struct mipi_dsim_driverdata {
	char *clk_name;
	enum mipi_dsim_version e_version;
	int num_supply;
	struct regulator_bulk_data *supplies;
};

/*
 * struct mipi_dsim_platform_data - interface to platform data
 *	for mipi-dsi driver.
 *
 * @lcd_panel_name: specifies lcd panel name registered to mipi-dsi driver.
 *	lcd panel driver searched would be actived.
 * @dsim_config: pointer of structure for configuring mipi-dsi controller.
 * @enabled: indicate whether mipi controller got enabled or not.
 * @lcd_panel_info: pointer for lcd panel specific structure.
 *	this structure specifies width, height, timing and polarity and so on.
 * @src_pdev: display controller platform dev;
 * @phy_enable: pointer to a callback controlling D-PHY enable/reset
 */
struct mipi_dsim_platform_data {
	char				lcd_panel_name[PANEL_NAME_SIZE];

	struct mipi_dsim_config		*dsim_config;
	unsigned int			enabled;
	void				*lcd_panel_info;
	void				*src_pdev;

	int (*phy_enable)(struct platform_device *pdev, bool on);
};

/*
 * struct mipi_dsim_master_ops - callbacks to mipi-dsi operations.
 *
 * @cmd_write: transfer command to lcd panel at LP mode.
 * @cmd_read: read command from rx register.
 * @get_fb_frame_done: get frame done status of display controller.
 *  @set_early_blank_mode: set framebuffer blank mode.
 *	- this callback should be called prior to fb_blank() by a client driver
 *	only if needing.
 *  @set_blank_mode: set framebuffer blank mode.
 *	- this callback should be called after fb_blank() by a client driver
 *	only if needing.
 *  @wait_for_frame_done: wait for until whole image frame is transferred.
 *  @set_refresh_rate: set refresh rate to support dynamic refresh feature.
 *  @update_panel_refresh: update the panel self refresh rate change.
 */

struct mipi_dsim_master_ops {
	int (*cmd_write)(struct mipi_dsim_device *dsim, unsigned int data_id,
		const unsigned char *data0, unsigned int data1);
	int (*atomic_cmd_write)(struct mipi_dsim_device *dsim,
				unsigned int data_id,
				const unsigned char *data0, unsigned int data1);
	int (*cmd_read)(struct mipi_dsim_device *dsim, unsigned int data_id,
		unsigned int data0, unsigned int req_size, u8 *rx_buf);
	int (*te_handler)(struct mipi_dsim_device *dsim);
	int (*cmd_set_begin)(struct mipi_dsim_device *dsim);
	int (*cmd_set_end)(struct mipi_dsim_device *dsim);
	int (*get_fb_frame_done)(struct fb_info *info);
	int (*set_early_blank_mode)(struct mipi_dsim_device *dsim, int power);
	int (*set_blank_mode)(struct mipi_dsim_device *dsim, int power);
	int (*set_refresh_rate)(struct mipi_dsim_device *dsim, int refresh);
	int (*prepare)(struct mipi_dsim_device *dsim);
	int (*set_lp_mode)(struct mipi_dsim_device *dsim, bool enable);
	int (*set_runtime_active)(struct mipi_dsim_device *dsim);
	int (*set_smies_active)(struct mipi_dsim_device *dsim, bool enable);
	int (*set_smies_mode)(struct mipi_dsim_device *dsim, int mode);
	void (*update_panel_refresh)(struct mipi_dsim_device *dsim, unsigned int rate);
	int (*set_partial_region)(struct mipi_dsim_device *dsim, void *pos);
	int (*set_secure_mode)(struct mipi_dsim_device *dsim, bool enable);
	void (*set_dbg_en)(struct mipi_dsim_device *dsim, bool enable);
};

/*
 * device structure for mipi-dsi based lcd panel.
 *
 * @name: name of the device to use with this device, or an
 *	alias for that name.
 * @dev: driver model representation of the device.
 * @id: id of device to be registered.
 * @bus_id: bus id for identifing connected bus
 *	and this bus id should be same as id of mipi_dsim_device.
 * @irq: irq number for signaling when framebuffer transfer of
 *	lcd panel module is completed.
 *	this irq would be used only for MIPI-DSI based CPU mode lcd panel.
 * @master: pointer to mipi-dsi master device object.
 * @platform_data: lcd panel specific platform data.
 */
struct mipi_dsim_lcd_device {
	char			*name;
	struct device		dev;
	int			id;
	int			bus_id;
	int			irq;

	struct mipi_dsim_device *master;
	void			*platform_data;
};

/*
 * driver structure for mipi-dsi based lcd panel.
 *
 * this structure should be registered by lcd panel driver.
 * mipi-dsi driver seeks lcd panel registered through name field
 * and calls these callback functions in appropriate time.
 *
 * @name: name of the driver to use with this device, or an
 *	alias for that name.
 * @id: id of driver to be registered.
 *	this id would be used for finding device object registered.
 */
struct mipi_dsim_lcd_driver {
	char			*name;
	int			id;
	unsigned int		if_type;

	void	(*power_on)(struct mipi_dsim_lcd_device *dsim_dev, int enable);
	void	(*reset)(struct mipi_dsim_lcd_device *dsim_dev, int enable);
	int	(*check_mtp)(struct mipi_dsim_lcd_device *dsim_dev);
	void	(*set_sequence)(struct mipi_dsim_lcd_device *dsim_dev);
	void	(*display_on)(struct mipi_dsim_lcd_device *dsim_dev,
				unsigned int enable);
	int	(*probe)(struct mipi_dsim_lcd_device *dsim_dev);
	int	(*remove)(struct mipi_dsim_lcd_device *dsim_dev);
	void	(*shutdown)(struct mipi_dsim_lcd_device *dsim_dev);
	int	(*suspend)(struct mipi_dsim_lcd_device *dsim_dev);
	int	(*resume)(struct mipi_dsim_lcd_device *dsim_dev);
	int (*set_refresh_rate)(struct mipi_dsim_lcd_device *dsim_dev,
		int refresh);
	int (*set_partial_region)(struct mipi_dsim_lcd_device *dsim_dev,
					int offset_x, int offset_y,
					int width, int height);
	void (*panel_pm_check)(struct mipi_dsim_lcd_device *dsim_dev,
							bool *pm_op);
	void (*te_active)(struct mipi_dsim_lcd_device *dsim_dev,
							bool enable);
};

/*
 * register mipi_dsim_lcd_device to mipi-dsi master.
 */
int exynos_mipi_dsi_register_lcd_device(struct mipi_dsim_lcd_device
						*lcd_dev);
/**
 * register mipi_dsim_lcd_driver object defined by lcd panel driver
 * to mipi-dsi driver.
 */
int exynos_mipi_dsi_register_lcd_driver(struct mipi_dsim_lcd_driver
						*lcd_drv);
int s5p_dsim_phy_enable(struct platform_device *pdev, bool on);
#endif /* _EXYNOS_MIPI_DSIM_H */
