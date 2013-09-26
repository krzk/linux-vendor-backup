/* Copyright (c) 2013 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com/
 *
 * Register interface file for JPEG driver on Exynos4x12 and 5250.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/io.h>
#include <linux/delay.h>

#include "jpeg-hw-exynos.h"
#include "jpeg-regs.h"

void jpeg_sw_reset(void __iomem *base)
{
	unsigned int reg;

	reg = readl(base + S5P_JPEG_CNTL_REG);
	writel(reg & ~S5P_JPEG_SOFT_RESET_HI,
			base + S5P_JPEG_CNTL_REG);

	ndelay(100000);

	writel(reg | S5P_JPEG_SOFT_RESET_HI,
			base + S5P_JPEG_CNTL_REG);
}

void jpeg_set_enc_dec_mode(void __iomem *base, unsigned int mode)
{
	unsigned int reg;

	reg = readl(base + S5P_JPEG_CNTL_REG);
	/* set jpeg mod register */
	if (mode == SJPEG_DECODE) {
		writel((reg & S5P_JPEG_ENC_DEC_MODE_MASK) | S5P_JPEG_DEC_MODE,
			base + S5P_JPEG_CNTL_REG);
	} else {/* encode */
		writel((reg & S5P_JPEG_ENC_DEC_MODE_MASK) | S5P_JPEG_ENC_MODE,
			base + S5P_JPEG_CNTL_REG);
	}
}

void jpeg_set_img_fmt(void __iomem *base, unsigned int img_fmt)
{
	unsigned int reg;

	reg = readl(base + S5P_JPEG_IMG_FMT_REG) &
			S5P_JPEG_ENC_IN_FMT_MASK; /* clear except enc format */

	switch (img_fmt) {
	case V4L2_PIX_FMT_GREY:
		reg = reg | S5P_JPEG_ENC_GRAY_IMG | S5P_JPEG_GRAY_IMG_IP;
		break;
	case V4L2_PIX_FMT_RGB32:
		reg = reg | S5P_JPEG_ENC_RGB_IMG |
				S5P_JPEG_RGB_IP_RGB_32BIT_IMG;
		break;
	case V4L2_PIX_FMT_RGB565:
		reg = reg | S5P_JPEG_ENC_RGB_IMG |
				S5P_JPEG_RGB_IP_RGB_16BIT_IMG;
		break;
	case V4L2_PIX_FMT_NV24:
		reg = reg | S5P_JPEG_ENC_YUV_444_IMG |
				S5P_JPEG_YUV_444_IP_YUV_444_2P_IMG |
				S5P_JPEG_SWAP_CHROMA_CbCr;
		break;
	case V4L2_PIX_FMT_NV42:
		reg = reg | S5P_JPEG_ENC_YUV_444_IMG |
				S5P_JPEG_YUV_444_IP_YUV_444_2P_IMG |
				S5P_JPEG_SWAP_CHROMA_CrCb;
		break;
	case V4L2_PIX_FMT_YUYV:
		reg = reg | S5P_JPEG_DEC_YUV_422_IMG |
				S5P_JPEG_YUV_422_IP_YUV_422_1P_IMG |
				S5P_JPEG_SWAP_CHROMA_CbCr;
		break;

	case V4L2_PIX_FMT_YVYU:
		reg = reg | S5P_JPEG_DEC_YUV_422_IMG |
				S5P_JPEG_YUV_422_IP_YUV_422_1P_IMG |
				S5P_JPEG_SWAP_CHROMA_CrCb;
		break;
	case V4L2_PIX_FMT_NV16:
		reg = reg | S5P_JPEG_DEC_YUV_422_IMG |
				S5P_JPEG_YUV_422_IP_YUV_422_2P_IMG |
				S5P_JPEG_SWAP_CHROMA_CbCr;
		break;
	case V4L2_PIX_FMT_NV61:
		reg = reg | S5P_JPEG_DEC_YUV_422_IMG |
				S5P_JPEG_YUV_422_IP_YUV_422_2P_IMG |
				S5P_JPEG_SWAP_CHROMA_CrCb;
		break;
	case V4L2_PIX_FMT_NV12:
		reg = reg | S5P_JPEG_DEC_YUV_420_IMG |
				S5P_JPEG_YUV_420_IP_YUV_420_2P_IMG |
				S5P_JPEG_SWAP_CHROMA_CbCr;
		break;
	case V4L2_PIX_FMT_NV21:
		reg = reg | S5P_JPEG_DEC_YUV_420_IMG |
				S5P_JPEG_YUV_420_IP_YUV_420_2P_IMG |
				S5P_JPEG_SWAP_CHROMA_CrCb;
		break;
	case V4L2_PIX_FMT_YUV420:
		reg = reg | S5P_JPEG_DEC_YUV_420_IMG |
				S5P_JPEG_YUV_420_IP_YUV_420_3P_IMG |
				S5P_JPEG_SWAP_CHROMA_CbCr;
		break;
	case V4L2_PIX_FMT_YVU420:
		reg = reg | S5P_JPEG_DEC_YUV_420_IMG |
				S5P_JPEG_YUV_420_IP_YUV_420_3P_IMG |
				S5P_JPEG_SWAP_CHROMA_CrCb;
		break;
	default:
		break;

	}

	writel(reg, base + S5P_JPEG_IMG_FMT_REG);
}

void jpeg_set_enc_out_fmt(void __iomem *base, unsigned int out_fmt)
{
	unsigned int reg;

	reg = readl(base + S5P_JPEG_IMG_FMT_REG) &
			~S5P_JPEG_ENC_FMT_MASK; /* clear enc format */

	switch (out_fmt) {
	case V4L2_JPEG_CHROMA_SUBSAMPLING_GRAY:
		reg = reg | S5P_JPEG_ENC_FMT_GRAY;
		break;

	case V4L2_JPEG_CHROMA_SUBSAMPLING_444:
		reg = reg | S5P_JPEG_ENC_FMT_YUV_444;
		break;

	case V4L2_JPEG_CHROMA_SUBSAMPLING_422:
		reg = reg | S5P_JPEG_ENC_FMT_YUV_422;
		break;

	case V4L2_JPEG_CHROMA_SUBSAMPLING_420:
		reg = reg | S5P_JPEG_ENC_FMT_YUV_420;
		break;

	default:
		break;
	}

	writel(reg, base + S5P_JPEG_IMG_FMT_REG);
}

void jpeg_set_interrupt(void __iomem *base)
{
	unsigned int reg;

	reg = readl(base + S5P_JPEG_INT_EN_REG) & ~S5P_JPEG_INT_EN_MASK;
	writel(S5P_JPEG_INT_EN_ALL, base + S5P_JPEG_INT_EN_REG);
}

unsigned int jpeg_get_int_status(void __iomem *base)
{
	unsigned int	int_status;

	int_status = readl(base + S5P_JPEG_INT_STATUS_REG);

	return int_status;
}

void jpeg_set_huf_table_enable(void __iomem *base, int value)
{
	unsigned int	reg;

	reg = readl(base + S5P_JPEG_CNTL_REG) & ~S5P_JPEG_HUF_TBL_EN;

	if (value == 1)
		writel(reg | S5P_JPEG_HUF_TBL_EN, base + S5P_JPEG_CNTL_REG);
	else
		writel(reg | ~S5P_JPEG_HUF_TBL_EN, base + S5P_JPEG_CNTL_REG);
}

void jpeg_set_dec_scaling(void __iomem *base,
		enum exynos_jpeg_scale_value x_value, enum exynos_jpeg_scale_value y_value)
{
	unsigned int	reg;

	reg = readl(base + S5P_JPEG_CNTL_REG) &
			~(S5P_JPEG_HOR_SCALING_MASK |
				S5P_JPEG_VER_SCALING_MASK);

	writel(reg | S5P_JPEG_HOR_SCALING(x_value) |
			S5P_JPEG_VER_SCALING(y_value),
				base + S5P_JPEG_CNTL_REG);
}

void jpeg_set_sys_int_enable(void __iomem *base, int value)
{
	unsigned int	reg;

	reg = readl(base + S5P_JPEG_CNTL_REG) & ~(S5P_JPEG_SYS_INT_EN);

	if (value == 1)
		writel(S5P_JPEG_SYS_INT_EN, base + S5P_JPEG_CNTL_REG);
	else
		writel(~S5P_JPEG_SYS_INT_EN, base + S5P_JPEG_CNTL_REG);
}

void jpeg_set_stream_buf_address(void __iomem *base, unsigned int address)
{
	writel(address, base + S5P_JPEG_OUT_MEM_BASE_REG);
}

void jpeg_set_stream_size(void __iomem *base,
		unsigned int x_value, unsigned int y_value)
{
	writel(0x0, base + S5P_JPEG_IMG_SIZE_REG); /* clear */
	writel(S5P_JPEG_X_SIZE(x_value) | S5P_JPEG_Y_SIZE(y_value),
			base + S5P_JPEG_IMG_SIZE_REG);
}

void jpeg_set_frame_buf_address(void __iomem *base,
		unsigned int fmt, unsigned int address_1p,
		unsigned int address_2p, unsigned int address_3p)
{
	switch (fmt) {
	case V4L2_PIX_FMT_GREY:
	case V4L2_PIX_FMT_RGB565:
	case V4L2_PIX_FMT_RGB32:
	case V4L2_PIX_FMT_YUYV:
	case V4L2_PIX_FMT_YVYU:
		writel(address_1p, base + S5P_JPEG_IMG_BA_PLANE_1_REG);
		writel(0, base + S5P_JPEG_IMG_BA_PLANE_2_REG);
		writel(0, base + S5P_JPEG_IMG_BA_PLANE_3_REG);
		break;
	case V4L2_PIX_FMT_NV24:
	case V4L2_PIX_FMT_NV42:
	case V4L2_PIX_FMT_NV16:
	case V4L2_PIX_FMT_NV61:
	case V4L2_PIX_FMT_NV12:
	case V4L2_PIX_FMT_NV21:
		writel(address_1p, base + S5P_JPEG_IMG_BA_PLANE_1_REG);
		writel(address_2p, base + S5P_JPEG_IMG_BA_PLANE_2_REG);
		writel(0, base + S5P_JPEG_IMG_BA_PLANE_3_REG);
		break;
	case V4L2_PIX_FMT_YUV420:
	case V4L2_PIX_FMT_YVU420:
		writel(address_1p, base + S5P_JPEG_IMG_BA_PLANE_1_REG);
		writel(address_2p, base + S5P_JPEG_IMG_BA_PLANE_2_REG);
		writel(address_3p, base + S5P_JPEG_IMG_BA_PLANE_3_REG);
		break;
	default:
		break;
	}
}
void jpeg_set_encode_tbl_select(void __iomem *base,
		enum exynos_jpeg_img_quality_level level)
{
	unsigned int	reg;

	switch (level) {
	case QUALITY_LEVEL_1:
		reg = S5P_JPEG_Q_TBL_COMP1_0 | S5P_JPEG_Q_TBL_COMP2_0 |
			S5P_JPEG_Q_TBL_COMP3_0 |
			S5P_JPEG_HUFF_TBL_COMP1_AC_0_DC_1 |
			S5P_JPEG_HUFF_TBL_COMP2_AC_0_DC_0 |
			S5P_JPEG_HUFF_TBL_COMP3_AC_1_DC_1;
		break;
	case QUALITY_LEVEL_2:
		reg = S5P_JPEG_Q_TBL_COMP1_1 | S5P_JPEG_Q_TBL_COMP2_1 |
			S5P_JPEG_Q_TBL_COMP3_1 |
			S5P_JPEG_HUFF_TBL_COMP1_AC_0_DC_1 |
			S5P_JPEG_HUFF_TBL_COMP2_AC_0_DC_0 |
			S5P_JPEG_HUFF_TBL_COMP3_AC_1_DC_1;
		break;
	case QUALITY_LEVEL_3:
		reg = S5P_JPEG_Q_TBL_COMP1_2 | S5P_JPEG_Q_TBL_COMP2_2 |
			S5P_JPEG_Q_TBL_COMP3_2 |
			S5P_JPEG_HUFF_TBL_COMP1_AC_0_DC_1 |
			S5P_JPEG_HUFF_TBL_COMP2_AC_0_DC_0 |
			S5P_JPEG_HUFF_TBL_COMP3_AC_1_DC_1;
		break;
	case QUALITY_LEVEL_4:
		reg = S5P_JPEG_Q_TBL_COMP1_3 | S5P_JPEG_Q_TBL_COMP2_3 |
			S5P_JPEG_Q_TBL_COMP3_3 |
			S5P_JPEG_HUFF_TBL_COMP1_AC_0_DC_1 |
			S5P_JPEG_HUFF_TBL_COMP2_AC_0_DC_0 |
			S5P_JPEG_HUFF_TBL_COMP3_AC_1_DC_1;
		break;
	default:
		reg = S5P_JPEG_Q_TBL_COMP1_0 | S5P_JPEG_Q_TBL_COMP2_0 |
			S5P_JPEG_Q_TBL_COMP3_1 |
			S5P_JPEG_HUFF_TBL_COMP1_AC_0_DC_1 |
			S5P_JPEG_HUFF_TBL_COMP2_AC_0_DC_0 |
			S5P_JPEG_HUFF_TBL_COMP3_AC_1_DC_1;
		break;
	}
	writel(reg, base + S5P_JPEG_TBL_SEL_REG);
}

void jpeg_set_encode_hoff_cnt(void __iomem *base, unsigned int fmt)
{
	if (fmt == V4L2_PIX_FMT_GREY)
		writel(0xd2, base + S5P_JPEG_HUFF_CNT_REG);
	else
		writel(0x1a2, base + S5P_JPEG_HUFF_CNT_REG);
}

unsigned int jpeg_get_stream_size(void __iomem *base)
{
	unsigned int size;

	size = readl(base + S5P_JPEG_BITSTREAM_SIZE_REG);
	return size;
}

void jpeg_set_dec_bitstream_size(void __iomem *base, unsigned int size)
{
	writel(size, base + S5P_JPEG_BITSTREAM_SIZE_REG);
}

void jpeg_get_frame_size(void __iomem *base,
			unsigned int *width, unsigned int *height)
{
	*width = (readl(base + S5P_JPEG_DECODE_XY_SIZE_REG) &
				S5P_JPEG_DECODED_SIZE_MASK);
	*height = (readl(base + S5P_JPEG_DECODE_XY_SIZE_REG) >> 16) &
				S5P_JPEG_DECODED_SIZE_MASK;
}

unsigned int jpeg_get_frame_fmt(void __iomem *base)
{
	return readl(base + S5P_JPEG_DECODE_IMG_FMT_REG) &
				EXYNOS_JPEG_DECODED_IMG_FMT_MASK;
}
