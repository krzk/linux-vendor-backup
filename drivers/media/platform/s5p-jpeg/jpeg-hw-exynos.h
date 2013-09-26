/* Copyright (c) 2013 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com/
 *
 * Header file of the register interface for JPEG driver on Exynos4x12 and 5250.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#ifndef __JPEG_REGS_H__
#define __JPEG_REGS_H__

#include "jpeg-core.h"

void jpeg_sw_reset(void __iomem *base);
void jpeg_set_enc_dec_mode(void __iomem *base, unsigned int mode);
void jpeg_set_img_fmt(void __iomem *base, unsigned int img_fmt);
void jpeg_set_enc_out_fmt(void __iomem *base, unsigned int out_fmt);
void jpeg_set_enc_tbl(void __iomem *base);
void jpeg_set_interrupt(void __iomem *base);
unsigned int jpeg_get_int_status(void __iomem *base);
void jpeg_set_huf_table_enable(void __iomem *base, int value);
void jpeg_set_dec_scaling(void __iomem *base,
		enum exynos_jpeg_scale_value x_value,
		enum exynos_jpeg_scale_value y_value);
void jpeg_set_sys_int_enable(void __iomem *base, int value);
void jpeg_set_stream_buf_address(void __iomem *base, unsigned int address);
void jpeg_set_stream_size(void __iomem *base,
		unsigned int x_value, unsigned int y_value);
void jpeg_set_frame_buf_address(void __iomem *base,
		unsigned int fmt, unsigned int address,
		unsigned int address_2p, unsigned int address_3p);
void jpeg_set_encode_tbl_select(void __iomem *base,
		enum exynos_jpeg_img_quality_level level);
void jpeg_set_encode_hoff_cnt(void __iomem *base, unsigned int fmt);
void jpeg_set_dec_bitstream_size(void __iomem *base, unsigned int size);
unsigned int jpeg_get_stream_size(void __iomem *base);
void jpeg_get_frame_size(void __iomem *base,
			unsigned int *width, unsigned int *height);
unsigned int jpeg_get_frame_fmt(void __iomem *base);

#endif /* __JPEG_REGS_H__ */
