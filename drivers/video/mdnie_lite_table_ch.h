#ifndef __MDNIE_TABLE_H__
#define __MDNIE_TABLE_H__

/* 2014.03.04 */

#define MDNIE_USE_SET_ADDR			1
#define MDNIE_SEQUENCE_OFFSET_1		0x0982
#define MDNIE_SEQUENCE_OFFSET_2		0x09D4

/* SCR Position can be different each panel */
#define ASCR_CMD		MDNIE_CMD2
/* NMR7 Feature */
#define ASCR_BIT_SHIFT	2

#define MDNIE_RED_R		29		/* ASCR_WIDE_CR[7:0] */

#define MDNIE_WHITE_R		47		/* ASCR_WIDE_WR[7:0] */
#define MDNIE_WHITE_G		49		/* ASCR_WIDE_WG[7:0] */
#define MDNIE_WHITE_B		51		/* ASCR_WIDE_WB[7:0] */

#define MDNIE_COLOR_BLIND_OFFSET	MDNIE_RED_R

#define COLOR_OFFSET_F1(x, y)		(((y << 10) - (((x << 10) * 99) / 91) - (6 << 10)) >> 10)
#define COLOR_OFFSET_F2(x, y)		(((y << 10) - (((x << 10) * 164) / 157) - (8 << 10)) >> 10)
#define COLOR_OFFSET_F3(x, y)		(((y << 10) + (((x << 10) * 218) / 39) - (20166 << 10)) >> 10)
#define COLOR_OFFSET_F4(x, y)		(((y << 10) + (((x << 10) * 23) / 8) - (11610 << 10)) >> 10)

/* color coordination order is WR, WG, WB */
static unsigned char coordinate_data[][3] = {
	{0xff, 0xff, 0xff}, /* dummy */
	{0xff, 0xf8, 0xf9}, /* Tune_1 */
	{0xff, 0xfa, 0xfe}, /* Tune_2 */
	{0xfb, 0xf9, 0xff}, /* Tune_3 */
	{0xff, 0xfe, 0xfb}, /* Tune_4 */
	{0xff, 0xff, 0xff}, /* Tune_5 */
	{0xf9, 0xfb, 0xff}, /* Tune_6 */
	{0xfc, 0xff, 0xf9}, /* Tune_7 */
	{0xfb, 0xff, 0xfb}, /* Tune_8 */
	{0xfa, 0xff, 0xff}, /* Tune_9 */
};

////////////////// UI /// /////////////////////
static unsigned char SCREEN_CURTAIN_1[] = {
	/* start */
	0xBA, // Start offset 0x0982, base B1h
	0x00, // data_width 00  lce_module 0  lcd_bypass 0  lcd_roi 00  algo_module 0  algo_bypass 0
	0x30, // algo_roi 00  ascr_module 1  ascr_bypass 1  ascr_roi 00  roi_type 00
	0x00, // roi0_x_start 00000000
	0x00, // roi0_x_start 0000  roi0_x_end 0000
	0x00, // roi0_x_end 00000000
	0x00, // roi0_y_start 00000000
	0x00, // roi0_y_start 0000  roi0_y_end 0000
	0x00, // roi0_y_end 00000000
	0x00, // roi1_x_start 00000000
	0x00, // roi1_x_start 0000  roi1_x_end 0000
	0x00, // roi1_x_end 00000000
	0x00, // roi1_y_start 00000000
	0x00, // roi1_y_start 0000  roi1_y_end 0000
	0x00, // roi1_y_end 00000000
	0xa0, // hsize 10100000
	0x06, // hsize 0000  vsize 0110
	0x40, // vsize 01000000
	0x62, // gain 110000 cgain 10
	0x48, // cgain 0100  scene_on 1  scene_tran 000
	0x14, // scene_tran 0  min_diff 0010100
	0xb3, // illum_gain 10110011
	0x87, // ref_offset 10000111
	0x40, // ref_offset 0  ref_gain 1000000
	0x36, // ref_gain 00  hbsize 110  vbsize 110
	0xfa, // bth 11111010
	0x5a, // bin_size_ratio 0101101  dth 0
	0xe5, // dth 11  min_ref_offset 100101
	0x83, // min_ref_offset 10  gamma 0  cs 0 sharpen 0 nr 0 mask_th 11
	0xfc, // mask_th 111111  sharpen_weight 00
	0x00, // sharpen_weight 00000000
	0xff, // max_plus 11111111
	0xff, // max_plus 111  max_minus 11111
	0xfd, // max_minus 111111  cs_gain 01
	0x00, // cs_gain 00000000
	0x00, // curve_1_b
	0x20, // curve_1_a
	0x00, // curve_2_b
	0x20, // curve_2_a
	0x00, // curve_3_b
	0x20, // curve_3_a
	0x00, // curve_4_b
	0x20, // curve_4_a
	0x00, // curve_5_b
	0x20, // curve_5_a
	0x00, // curve_6_b
	0x20, // curve_6_a
	0x00, // curve_7_b
	0x20, // curve_7_a
	0x00, // curve_8_b
	0x20, // curve_8_a
	0x00, // curve_9_b
	0x20, // curve_9_a
	0x00, // curve_10_b
	0x20, // curve_10_a
	0x00, // curve_11_b
	0x20, // curve_11_a
	0x00, // curve_12_b
	0x20, // curve_12_a
	0x00, // curve_13_b
	0x20, // curve_13_a
	0x00, // curve_14_b
	0x20, // curve_14_a
	0x00, // curve_15_b
	0x20, // curve_15_a
	0x00, // curve_16_b
	0x20, // curve_16_a
	0x00, // curve_17_b
	0x20, // curve_17_a
	0x00, // curve_18_b
	0x20, // curve_18_a
	0x00, // curve_19_b
	0x20, // curve_19_a
	0x00, // curve_20_b
	0x20, // curve_20_a
	0x00, // curve_21_b
	0x20, // curve_21_a
	0x00, // curve_22_b
	0x20, // curve_22_a
	0x00, // curve_23_b
	0x20, // curve_23_a
	0x00, // curve_24_b
	0xff, // curve_24_a
};

static unsigned char SCREEN_CURTAIN_2[] = {
	0xBA, // Start offset 0x09D4, base B1h
	0x05, // ascr_strength 00000  ascr_on 1  skin_cb 01
	0x9e, // skin_cb 100111  skin_cr 10
	0xa4, // skin_cr 101001  up_distance 00
	0x30, // up_distance 001100  down_distance 00                                       
	0x30, // down_distance 001100  right_distance 00                                    
	0x30, // right_distance 001100  left_distance 00                                    
	0x30, // left_distance 001100  up_divided_distance 00
	0x2a, // up_divided_distance 00101010
	0xaa, // up_divided_distance 10101010
	0xc2, // up_divided_distance 11  down_divided_distance 000010
	0xaa, // down_divided_distance 10101010
	0xac, // down_divided_distance 101011 right_divided_distance 00
	0x2a, // right_divided_distance 00101010
	0xaa, // right_divided_distance 10101010
	0xc2, // right_divided_distance 11  left_divided_distance 000010
	0xaa, // left_divided_distance 10101010
	0xaf, // left_divided_distance 101011  ascr_skin_Rr 11
	0xfc, // ascr_skin_Rr 111111  ascr_skin_Rg 00
	0x00, // ascr_skin_Rg 000000  ascr_skin_Rb 00
	0x03, // ascr_skin_Rb 000000  ascr_skin_Yr 11
	0xff, // ascr_skin_Yr 111111  ascr_skin_Yg 11
	0xfc, // ascr_skin_Yg 111111  ascr_skin_Yb 00
	0x03, // ascr_skin_Yb 000000  ascr_skin_Mr 11
	0xfc, // ascr_skin_Mr 111111  ascr_skin_Mg 00
	0x03, // ascr_skin_Mg 000000  ascr_skin_Mb 11
	0xff, // ascr_skin_Mb 111111  ascr_skin_Wr 11
	0xff, // ascr_skin_Wr 111111  ascr_skin_Wg 11
	0xff, // ascr_skin_Wg 111111  ascr_skin_Wb 11
	0xfc, // ascr_skin_Wb 111111  ascr_Cr 00
	0x00, // ascr_Cr 000000  ascr_Rr 00
	0x00, // ascr_Rr 000000  ascr_Cg 00
	0x00, // ascr_Cg 000000  ascr_Rg 00
	0x00, // ascr_Rg 000000  ascr_Cb 00
	0x00, // ascr_Cb 000000  ascr_Rb 00
	0x00, // ascr_Rb 000000  ascr_Mr 00
	0x00, // ascr_Mr 000000  ascr_Gr 00
	0x00, // ascr_Gr 000000  ascr_Mg 00
	0x00, // ascr_Mg 000000  ascr_Gg 00
	0x00, // ascr_Gg 000000  ascr_Mb 00
	0x00, // ascr_Mb 000000  ascr_Gb 00
	0x00, // ascr_Gb 000000  ascr_Yr 00
	0x00, // ascr_Yr 000000  ascr_Br 00
	0x00, // ascr_Br 000000  ascr_Yg 00
	0x00, // ascr_Yg 000000  ascr_Bg 00
	0x00, // ascr_Bg 000000  ascr_Yb 00
	0x00, // ascr_Yb 000000  ascr_Bb 00
	0x00, // ascr_Bb 000000  ascr_Wr 00
	0x00, // ascr_Wr 000000  ascr_Kr 00
	0x00, // ascr_Kr 000000  ascr_Wg 00
	0x00, // ascr_Wg 000000  ascr_Kg 00
	0x00, // ascr_Kg 000000  ascr_Wb 00
	0x00, // ascr_Wb 000000  ascr_Kb 00
	0x00, // ascr_Kb 000000  reserved 00
	0x0f, // reserved 0000  vsync_mask 1111
	/* end */
};

static unsigned char STANDARD_UI_1[] = {
	/* start */
	0xBA, // Start offset 0x0982, base B1h
	0x00, // data_width 00  lce_module 0  lcd_bypass 0  lcd_roi 00  algo_module 0  algo_bypass 0
	0x30, // algo_roi 00  ascr_module 1  ascr_bypass 1  ascr_roi 00  roi_type 00
	0x00, // roi0_x_start 00000000
	0x00, // roi0_x_start 0000  roi0_x_end 0000
	0x00, // roi0_x_end 00000000
	0x00, // roi0_y_start 00000000
	0x00, // roi0_y_start 0000  roi0_y_end 0000
	0x00, // roi0_y_end 00000000
	0x00, // roi1_x_start 00000000
	0x00, // roi1_x_start 0000  roi1_x_end 0000
	0x00, // roi1_x_end 00000000
	0x00, // roi1_y_start 00000000
	0x00, // roi1_y_start 0000  roi1_y_end 0000
	0x00, // roi1_y_end 00000000
	0xa0, // hsize 10100000
	0x06, // hsize 0000  vsize 0110
	0x40, // vsize 01000000
	0x62, // gain 110000 cgain 10
	0x48, // cgain 0100  scene_on 1  scene_tran 000
	0x14, // scene_tran 0  min_diff 0010100
	0xb3, // illum_gain 10110011
	0x87, // ref_offset 10000111
	0x40, // ref_offset 0  ref_gain 1000000
	0x36, // ref_gain 00  hbsize 110  vbsize 110
	0xfa, // bth 11111010
	0x5a, // bin_size_ratio 0101101  dth 0
	0xe5, // dth 11  min_ref_offset 100101
	0x83, // min_ref_offset 10  gamma 0  cs 0 sharpen 0 nr 0 mask_th 11
	0xfc, // mask_th 111111  sharpen_weight 00
	0x40, // sharpen_weight 01000000
	0x14, // max_plus 00010100
	0x02, // max_plus 000  max_minus 00010
	0x81, // max_minus 100000  cs_gain 01
	0x00, // cs_gain 00000000
	0x00, // curve_1_b
	0x20, // curve_1_a
	0x00, // curve_2_b
	0x20, // curve_2_a
	0x00, // curve_3_b
	0x20, // curve_3_a
	0x00, // curve_4_b
	0x20, // curve_4_a
	0x00, // curve_5_b
	0x20, // curve_5_a
	0x00, // curve_6_b
	0x20, // curve_6_a
	0x00, // curve_7_b
	0x20, // curve_7_a
	0x00, // curve_8_b
	0x20, // curve_8_a
	0x00, // curve_9_b
	0x20, // curve_9_a
	0x00, // curve_10_b
	0x20, // curve_10_a
	0x00, // curve_11_b
	0x20, // curve_11_a
	0x00, // curve_12_b
	0x20, // curve_12_a
	0x00, // curve_13_b
	0x20, // curve_13_a
	0x00, // curve_14_b
	0x20, // curve_14_a
	0x00, // curve_15_b
	0x20, // curve_15_a
	0x00, // curve_16_b
	0x20, // curve_16_a
	0x00, // curve_17_b
	0x20, // curve_17_a
	0x00, // curve_18_b
	0x20, // curve_18_a
	0x00, // curve_19_b
	0x20, // curve_19_a
	0x00, // curve_20_b
	0x20, // curve_20_a
	0x00, // curve_21_b
	0x20, // curve_21_a
	0x00, // curve_22_b
	0x20, // curve_22_a
	0x00, // curve_23_b
	0x20, // curve_23_a
	0x00, // curve_24_b
	0xff, // curve_24_a
};

static unsigned char STANDARD_UI_2[] = {
	0xBA, // Start offset 0x09D4, base B1h
	0x05, // ascr_strength 00000  ascr_on 1  skin_cb 01
	0x9e, // skin_cb 100111  skin_cr 10
	0xa4, // skin_cr 101001  up_distance 00
	0x5c, // up_distance 010111  down_distance 00                                       
	0xa4, // down_distance 101001  right_distance 00                                    
	0x64, // right_distance 011001  left_distance 00                                    
	0x9c, // left_distance 100111  up_divided_distance 00
	0x16, // up_divided_distance 00010110
	0x42, // up_divided_distance 01000010
	0xc0, // up_divided_distance 11  down_divided_distance 000000
	0xc7, // down_divided_distance 11000111
	0xd0, // down_divided_distance 110100 right_divided_distance 00
	0x14, // right_divided_distance 00010100
	0x7b, // right_divided_distance 01111011
	0x00, // right_divided_distance 00  left_divided_distance 000000
	0xd2, // left_divided_distance 11010010
	0x0f, // left_divided_distance 000011  ascr_skin_Rr 11
	0xfc, // ascr_skin_Rr 111111  ascr_skin_Rg 00
	0x00, // ascr_skin_Rg 000000  ascr_skin_Rb 00
	0x03, // ascr_skin_Rb 000000  ascr_skin_Yr 11
	0xff, // ascr_skin_Yr 111111  ascr_skin_Yg 11
	0xfc, // ascr_skin_Yg 111111  ascr_skin_Yb 00
	0x03, // ascr_skin_Yb 000000  ascr_skin_Mr 11
	0xfc, // ascr_skin_Mr 111111  ascr_skin_Mg 00
	0x03, // ascr_skin_Mg 000000  ascr_skin_Mb 11
	0xff, // ascr_skin_Mb 111111  ascr_skin_Wr 11
	0xff, // ascr_skin_Wr 111111  ascr_skin_Wg 11
	0xff, // ascr_skin_Wg 111111  ascr_skin_Wb 11
	0xfc, // ascr_skin_Wb 111111  ascr_Cr 00
	0x03, // ascr_Cr 000000  ascr_Rr 11
	0xff, // ascr_Rr 111111  ascr_Cg 11
	0xfc, // ascr_Cg 111111  ascr_Rg 00
	0x03, // ascr_Rg 000000  ascr_Cb 11
	0xfc, // ascr_Cb 111111  ascr_Rb 00
	0x03, // ascr_Rb 000000  ascr_Mr 11
	0xfc, // ascr_Mr 111111  ascr_Gr 00
	0x00, // ascr_Gr 000000  ascr_Mg 00
	0x03, // ascr_Mg 000000  ascr_Gg 11
	0xff, // ascr_Gg 111111  ascr_Mb 11
	0xfc, // ascr_Mb 111111  ascr_Gb 00
	0x03, // ascr_Gb 000000  ascr_Yr 11
	0xfc, // ascr_Yr 111111  ascr_Br 00
	0x03, // ascr_Br 000000  ascr_Yg 11
	0xfc, // ascr_Yg 111111  ascr_Bg 00
	0x00, // ascr_Bg 000000  ascr_Yb 00
	0x03, // ascr_Yb 000000  ascr_Bb 11
	0xff, // ascr_Bb 111111  ascr_Wr 11
	0xfc, // ascr_Wr 111111  ascr_Kr 00
	0x03, // ascr_Kr 000000  ascr_Wg 11
	0xfc, // ascr_Wg 111111  ascr_Kg 00
	0x03, // ascr_Kg 000000  ascr_Wb 11
	0xfc, // ascr_Wb 111111  ascr_Kb 00
	0x00, // ascr_Kb 000000  reserved 00
	0x0f, // reserved 0000  vsync_mask 1111
	/* end */
};

static unsigned char NATURAL_UI_1[] = {
	/* start */
	0xBA, // Start offset 0x0982, base B1h
	0x00, // data_width 00  lce_module 0  lcd_bypass 0  lcd_roi 00  algo_module 0  algo_bypass 0
	0x30, // algo_roi 00  ascr_module 1  ascr_bypass 1  ascr_roi 00  roi_type 00
	0x00, // roi0_x_start 00000000
	0x00, // roi0_x_start 0000  roi0_x_end 0000
	0x00, // roi0_x_end 00000000
	0x00, // roi0_y_start 00000000
	0x00, // roi0_y_start 0000  roi0_y_end 0000
	0x00, // roi0_y_end 00000000
	0x00, // roi1_x_start 00000000
	0x00, // roi1_x_start 0000  roi1_x_end 0000
	0x00, // roi1_x_end 00000000
	0x00, // roi1_y_start 00000000
	0x00, // roi1_y_start 0000  roi1_y_end 0000
	0x00, // roi1_y_end 00000000
	0xa0, // hsize 10100000
	0x06, // hsize 0000  vsize 0110
	0x40, // vsize 01000000
	0x62, // gain 110000 cgain 10
	0x48, // cgain 0100  scene_on 1  scene_tran 000
	0x14, // scene_tran 0  min_diff 0010100
	0xb3, // illum_gain 10110011
	0x87, // ref_offset 10000111
	0x40, // ref_offset 0  ref_gain 1000000
	0x36, // ref_gain 00  hbsize 110  vbsize 110
	0xfa, // bth 11111010
	0x5a, // bin_size_ratio 0101101  dth 0
	0xe5, // dth 11  min_ref_offset 100101
	0x83, // min_ref_offset 10  gamma 0  cs 0 sharpen 0 nr 0 mask_th 11
	0xfc, // mask_th 111111  sharpen_weight 00
	0x10, // sharpen_weight 00010000
	0x14, // max_plus 00010100
	0x02, // max_plus 000  max_minus 00010
	0x81, // max_minus 100000  cs_gain 01
	0x00, // cs_gain 00000000
	0x00, // curve_1_b
	0x20, // curve_1_a
	0x00, // curve_2_b
	0x20, // curve_2_a
	0x00, // curve_3_b
	0x20, // curve_3_a
	0x00, // curve_4_b
	0x20, // curve_4_a
	0x00, // curve_5_b
	0x20, // curve_5_a
	0x00, // curve_6_b
	0x20, // curve_6_a
	0x00, // curve_7_b
	0x20, // curve_7_a
	0x00, // curve_8_b
	0x20, // curve_8_a
	0x00, // curve_9_b
	0x20, // curve_9_a
	0x00, // curve_10_b
	0x20, // curve_10_a
	0x00, // curve_11_b
	0x20, // curve_11_a
	0x00, // curve_12_b
	0x20, // curve_12_a
	0x00, // curve_13_b
	0x20, // curve_13_a
	0x00, // curve_14_b
	0x20, // curve_14_a
	0x00, // curve_15_b
	0x20, // curve_15_a
	0x00, // curve_16_b
	0x20, // curve_16_a
	0x00, // curve_17_b
	0x20, // curve_17_a
	0x00, // curve_18_b
	0x20, // curve_18_a
	0x00, // curve_19_b
	0x20, // curve_19_a
	0x00, // curve_20_b
	0x20, // curve_20_a
	0x00, // curve_21_b
	0x20, // curve_21_a
	0x00, // curve_22_b
	0x20, // curve_22_a
	0x00, // curve_23_b
	0x20, // curve_23_a
	0x00, // curve_24_b
	0xff, // curve_24_a
};

static unsigned char NATURAL_UI_2[] = {
	0xBA, // Start offset 0x09D4, base B1h
	0x05, // ascr_strength 00000  ascr_on 1  skin_cb 01
	0x9e, // skin_cb 100111  skin_cr 10
	0xa4, // skin_cr 101001  up_distance 00
	0x5c, // up_distance 010111  down_distance 00                                       
	0xa4, // down_distance 101001  right_distance 00                                    
	0x64, // right_distance 011001  left_distance 00                                    
	0x9c, // left_distance 100111  up_divided_distance 00
	0x16, // up_divided_distance 00010110
	0x42, // up_divided_distance 01000010
	0xc0, // up_divided_distance 11  down_divided_distance 000000
	0xc7, // down_divided_distance 11000111
	0xd0, // down_divided_distance 110100 right_divided_distance 00
	0x14, // right_divided_distance 00010100
	0x7b, // right_divided_distance 01111011
	0x00, // right_divided_distance 00  left_divided_distance 000000
	0xd2, // left_divided_distance 11010010
	0x0f, // left_divided_distance 000011  ascr_skin_Rr 11
	0xfc, // ascr_skin_Rr 111111  ascr_skin_Rg 00
	0x00, // ascr_skin_Rg 000000  ascr_skin_Rb 00
	0x03, // ascr_skin_Rb 000000  ascr_skin_Yr 11
	0xff, // ascr_skin_Yr 111111  ascr_skin_Yg 11
	0xfc, // ascr_skin_Yg 111111  ascr_skin_Yb 00
	0x03, // ascr_skin_Yb 000000  ascr_skin_Mr 11
	0xfc, // ascr_skin_Mr 111111  ascr_skin_Mg 00
	0x03, // ascr_skin_Mg 000000  ascr_skin_Mb 11
	0xff, // ascr_skin_Mb 111111  ascr_skin_Wr 11
	0xff, // ascr_skin_Wr 111111  ascr_skin_Wg 11
	0xff, // ascr_skin_Wg 111111  ascr_skin_Wb 11
	0xfc, // ascr_skin_Wb 111111  ascr_Cr 00
	0x03, // ascr_Cr 000000  ascr_Rr 11
	0xff, // ascr_Rr 111111  ascr_Cg 11
	0xfc, // ascr_Cg 111111  ascr_Rg 00
	0x5b, // ascr_Rg 010110  ascr_Cb 11
	0x94, // ascr_Cb 100101  ascr_Rb 00
	0x3b, // ascr_Rb 001110  ascr_Mr 11
	0xfc, // ascr_Mr 111111  ascr_Gr 00
	0x00, // ascr_Gr 000000  ascr_Mg 00
	0x53, // ascr_Mg 010100  ascr_Gg 11
	0xff, // ascr_Gg 111111  ascr_Mb 11
	0x64, // ascr_Mb 011001  ascr_Gb 00
	0x13, // ascr_Gb 000100  ascr_Yr 11
	0xf0, // ascr_Yr 111100  ascr_Br 00
	0x9b, // ascr_Br 100110  ascr_Yg 11
	0xfc, // ascr_Yg 111111  ascr_Bg 00
	0x6c, // ascr_Bg 011011  ascr_Yb 00
	0x8f, // ascr_Yb 100011  ascr_Bb 11
	0xff, // ascr_Bb 111111  ascr_Wr 11
	0xfc, // ascr_Wr 111111  ascr_Kr 00
	0x03, // ascr_Kr 000000  ascr_Wg 11
	0xe0, // ascr_Wg 111000  ascr_Kg 00
	0x03, // ascr_Kg 000000  ascr_Wb 11
	0xbc, // ascr_Wb 101111  ascr_Kb 00
	0x00, // ascr_Kb 000000  reserved 00
	0x0f, // reserved 0000  vsync_mask 1111
	/* end */
};

static unsigned char DYNAMIC_UI_1[] = {
	/* start */
	0xBA, // Start offset 0x0982, base B1h
	0x03, // data_width 00  lce_module 0  lcd_bypass 0  lcd_roi 00  algo_module 1  algo_bypass 1
	0x30, // algo_roi 00  ascr_module 1  ascr_bypass 1  ascr_roi 00  roi_type 00
	0x00, // roi0_x_start 00000000
	0x00, // roi0_x_start 0000  roi0_x_end 0000
	0x00, // roi0_x_end 00000000
	0x00, // roi0_y_start 00000000
	0x00, // roi0_y_start 0000  roi0_y_end 0000
	0x00, // roi0_y_end 00000000
	0x00, // roi1_x_start 00000000
	0x00, // roi1_x_start 0000  roi1_x_end 0000
	0x00, // roi1_x_end 00000000
	0x00, // roi1_y_start 00000000
	0x00, // roi1_y_start 0000  roi1_y_end 0000
	0x00, // roi1_y_end 00000000
	0xa0, // hsize 10100000
	0x06, // hsize 0000  vsize 0110
	0x40, // vsize 01000000
	0x62, // gain 110000 cgain 10
	0x48, // cgain 0100  scene_on 1  scene_tran 000
	0x14, // scene_tran 0  min_diff 0010100
	0xb3, // illum_gain 10110011
	0x87, // ref_offset 10000111
	0x40, // ref_offset 0  ref_gain 1000000
	0x36, // ref_gain 00  hbsize 110  vbsize 110
	0xfa, // bth 11111010
	0x5a, // bin_size_ratio 0101101  dth 0
	0xe5, // dth 11  min_ref_offset 100101
	0xb3, // min_ref_offset 10  gamma 1  cs 1 sharpen 0 nr 0 mask_th 11
	0xfc, // mask_th 111111  sharpen_weight 00
	0x50, // sharpen_weight 01010000
	0x14, // max_plus 00010100
	0x02, // max_plus 000  max_minus 00010
	0x81, // max_minus 100000  cs_gain 01
	0x20, // cs_gain 00100000
	0x00, // curve_1_b 
	0x0f, // curve_1_a 
	0x00, // curve_2_b 
	0x0f, // curve_2_a 
	0x00, // curve_3_b 
	0x0f, // curve_3_a 
	0x00, // curve_4_b 
	0x0f, // curve_4_a 
	0x09, // curve_5_b 
	0xa2, // curve_5_a 
	0x09, // curve_6_b 
	0xa2, // curve_6_a 
	0x09, // curve_7_b 
	0xa2, // curve_7_a 
	0x09, // curve_8_b 
	0xa2, // curve_8_a 
	0x09, // curve_9_b 
	0xa2, // curve_9_a 
	0x09, // curve_10_b 
	0xa2, // curve_10_a 
	0x0a, // curve_11_b 
	0xa2, // curve_11_a 
	0x0a, // curve_12_b 
	0xa2, // curve_12_a 
	0x0a, // curve_13_b 
	0xa2, // curve_13_a 
	0x0a, // curve_14_b 
	0xa2, // curve_14_a 
	0x0a, // curve_15_b 
	0xa2, // curve_15_a 
	0x0a, // curve_16_b 
	0xa2, // curve_16_a 
	0x0a, // curve_17_b 
	0xa2, // curve_17_a 
	0x0a, // curve_18_b 
	0xa2, // curve_18_a 
	0x0f, // curve_19_b 
	0xa4, // curve_19_a 
	0x0f, // curve_20_b 
	0xa4, // curve_20_a 
	0x0f, // curve_21_b 
	0xa4, // curve_21_a 
	0x23, // curve_22_b 
	0x1c, // curve_22_a 
	0x48, // curve_23_b 
	0x17, // curve_23_a 
	0x00, // curve_24_b 
	0xff, // curve_24_a 
};

static unsigned char DYNAMIC_UI_2[] = {
	0xBA, // Start offset 0x09D4, base B1h
	0x05, // ascr_strength 00000  ascr_on 1  skin_cb 01
	0x9e, // skin_cb 100111  skin_cr 10
	0xa4, // skin_cr 101001  up_distance 00
	0x5c, // up_distance 010111  down_distance 00                                       
	0xa4, // down_distance 101001  right_distance 00                                    
	0x64, // right_distance 011001  left_distance 00                                    
	0x9c, // left_distance 100111  up_divided_distance 00
	0x16, // up_divided_distance 00010110
	0x42, // up_divided_distance 01000010
	0xc0, // up_divided_distance 11  down_divided_distance 000000
	0xc7, // down_divided_distance 11000111
	0xd0, // down_divided_distance 110100 right_divided_distance 00
	0x14, // right_divided_distance 00010100
	0x7b, // right_divided_distance 01111011
	0x00, // right_divided_distance 00  left_divided_distance 000000
	0xd2, // left_divided_distance 11010010
	0x0f, // left_divided_distance 000011  ascr_skin_Rr 11
	0xfc, // ascr_skin_Rr 111111  ascr_skin_Rg 00
	0x00, // ascr_skin_Rg 000000  ascr_skin_Rb 00
	0x03, // ascr_skin_Rb 000000  ascr_skin_Yr 11
	0xff, // ascr_skin_Yr 111111  ascr_skin_Yg 11
	0xfc, // ascr_skin_Yg 111111  ascr_skin_Yb 00
	0x03, // ascr_skin_Yb 000000  ascr_skin_Mr 11
	0xfc, // ascr_skin_Mr 111111  ascr_skin_Mg 00
	0x03, // ascr_skin_Mg 000000  ascr_skin_Mb 11
	0xff, // ascr_skin_Mb 111111  ascr_skin_Wr 11
	0xff, // ascr_skin_Wr 111111  ascr_skin_Wg 11
	0xff, // ascr_skin_Wg 111111  ascr_skin_Wb 11
	0xfc, // ascr_skin_Wb 111111  ascr_Cr 00
	0x03, // ascr_Cr 000000  ascr_Rr 11
	0xff, // ascr_Rr 111111  ascr_Cg 11
	0xfc, // ascr_Cg 111111  ascr_Rg 00
	0x03, // ascr_Rg 000000  ascr_Cb 11
	0xfc, // ascr_Cb 111111  ascr_Rb 00
	0x03, // ascr_Rb 000000  ascr_Mr 11
	0xfc, // ascr_Mr 111111  ascr_Gr 00
	0x00, // ascr_Gr 000000  ascr_Mg 00
	0x03, // ascr_Mg 000000  ascr_Gg 11
	0xff, // ascr_Gg 111111  ascr_Mb 11
	0xfc, // ascr_Mb 111111  ascr_Gb 00
	0x03, // ascr_Gb 000000  ascr_Yr 11
	0xfc, // ascr_Yr 111111  ascr_Br 00
	0x03, // ascr_Br 000000  ascr_Yg 11
	0xfc, // ascr_Yg 111111  ascr_Bg 00
	0x00, // ascr_Bg 000000  ascr_Yb 00
	0x03, // ascr_Yb 000000  ascr_Bb 11
	0xff, // ascr_Bb 111111  ascr_Wr 11
	0xfc, // ascr_Wr 111111  ascr_Kr 00
	0x03, // ascr_Kr 000000  ascr_Wg 11
	0xfc, // ascr_Wg 111111  ascr_Kg 00
	0x03, // ascr_Kg 000000  ascr_Wb 11
	0xfc, // ascr_Wb 111111  ascr_Kb 00
	0x00, // ascr_Kb 000000  reserved 00
	0x0f, // reserved 0000  vsync_mask 1111
	/* end */
};

static unsigned char MOVIE_UI_1[] = {
	/* start */
	0xBA, // Start offset 0x0982, base B1h
	0x03, // data_width 00  lce_module 0  lcd_bypass 0  lcd_roi 00  algo_module 1  algo_bypass 1
	0x30, // algo_roi 00  ascr_module 1  ascr_bypass 1  ascr_roi 00  roi_type 00
	0x00, // roi0_x_start 00000000
	0x00, // roi0_x_start 0000  roi0_x_end 0000
	0x00, // roi0_x_end 00000000
	0x00, // roi0_y_start 00000000
	0x00, // roi0_y_start 0000  roi0_y_end 0000
	0x00, // roi0_y_end 00000000
	0x00, // roi1_x_start 00000000
	0x00, // roi1_x_start 0000  roi1_x_end 0000
	0x00, // roi1_x_end 00000000
	0x00, // roi1_y_start 00000000
	0x00, // roi1_y_start 0000  roi1_y_end 0000
	0x00, // roi1_y_end 00000000
	0xa0, // hsize 10100000
	0x06, // hsize 0000  vsize 0110
	0x40, // vsize 01000000
	0x62, // gain 110000 cgain 10
	0x48, // cgain 0100  scene_on 1  scene_tran 000
	0x14, // scene_tran 0  min_diff 0010100
	0xb3, // illum_gain 10110011
	0x87, // ref_offset 10000111
	0x40, // ref_offset 0  ref_gain 1000000
	0x36, // ref_gain 00  hbsize 110  vbsize 110
	0xfa, // bth 11111010
	0x5a, // bin_size_ratio 0101101  dth 0
	0xe5, // dth 11  min_ref_offset 100101
	0x93, // min_ref_offset 10  gamma 0  cs 1 sharpen 0 nr 0 mask_th 11
	0xfc, // mask_th 111111  sharpen_weight 00
	0x10, // sharpen_weight 00010000
	0x14, // max_plus 00010100
	0x02, // max_plus 000  max_minus 00010
	0x81, // max_minus 100000  cs_gain 01
	0x20, // cs_gain 00100000
	0x00, // curve_1_b
	0x20, // curve_1_a
	0x00, // curve_2_b
	0x20, // curve_2_a
	0x00, // curve_3_b
	0x20, // curve_3_a
	0x00, // curve_4_b
	0x20, // curve_4_a
	0x00, // curve_5_b
	0x20, // curve_5_a
	0x00, // curve_6_b
	0x20, // curve_6_a
	0x00, // curve_7_b
	0x20, // curve_7_a
	0x00, // curve_8_b
	0x20, // curve_8_a
	0x00, // curve_9_b
	0x20, // curve_9_a
	0x00, // curve_10_b
	0x20, // curve_10_a
	0x00, // curve_11_b
	0x20, // curve_11_a
	0x00, // curve_12_b
	0x20, // curve_12_a
	0x00, // curve_13_b
	0x20, // curve_13_a
	0x00, // curve_14_b
	0x20, // curve_14_a
	0x00, // curve_15_b
	0x20, // curve_15_a
	0x00, // curve_16_b
	0x20, // curve_16_a
	0x00, // curve_17_b
	0x20, // curve_17_a
	0x00, // curve_18_b
	0x20, // curve_18_a
	0x00, // curve_19_b
	0x20, // curve_19_a
	0x00, // curve_20_b
	0x20, // curve_20_a
	0x00, // curve_21_b
	0x20, // curve_21_a
	0x00, // curve_22_b
	0x20, // curve_22_a
	0x00, // curve_23_b
	0x20, // curve_23_a
	0x00, // curve_24_b
	0xff, // curve_24_a
};

static unsigned char MOVIE_UI_2[] = {
	0xBA, // Start offset 0x09D4, base B1h
	0x05, // ascr_strength 00000  ascr_on 1  skin_cb 01
	0x9e, // skin_cb 100111  skin_cr 10
	0xa4, // skin_cr 101001  up_distance 00
	0x5c, // up_distance 010111  down_distance 00                                       
	0xa4, // down_distance 101001  right_distance 00                                    
	0x64, // right_distance 011001  left_distance 00                                    
	0x9c, // left_distance 100111  up_divided_distance 00
	0x16, // up_divided_distance 00010110
	0x42, // up_divided_distance 01000010
	0xc0, // up_divided_distance 11  down_divided_distance 000000
	0xc7, // down_divided_distance 11000111
	0xd0, // down_divided_distance 110100 right_divided_distance 00
	0x14, // right_divided_distance 00010100
	0x7b, // right_divided_distance 01111011
	0x00, // right_divided_distance 00  left_divided_distance 000000
	0xd2, // left_divided_distance 11010010
	0x0f, // left_divided_distance 000011  ascr_skin_Rr 11
	0xfc, // ascr_skin_Rr 111111  ascr_skin_Rg 00
	0x00, // ascr_skin_Rg 000000  ascr_skin_Rb 00
	0x03, // ascr_skin_Rb 000000  ascr_skin_Yr 11
	0xff, // ascr_skin_Yr 111111  ascr_skin_Yg 11
	0xfc, // ascr_skin_Yg 111111  ascr_skin_Yb 00
	0x03, // ascr_skin_Yb 000000  ascr_skin_Mr 11
	0xfc, // ascr_skin_Mr 111111  ascr_skin_Mg 00
	0x03, // ascr_skin_Mg 000000  ascr_skin_Mb 11
	0xff, // ascr_skin_Mb 111111  ascr_skin_Wr 11
	0xff, // ascr_skin_Wr 111111  ascr_skin_Wg 11
	0xff, // ascr_skin_Wg 111111  ascr_skin_Wb 11
	0xfd, // ascr_skin_Wb 111111  ascr_Cr 01
	0xeb, // ascr_Cr 111010  ascr_Rr 11
	0xff, // ascr_Rr 111111  ascr_Cg 11
	0xfc, // ascr_Cg 111111  ascr_Rg 00
	0xaf, // ascr_Rg 101011  ascr_Cb 11
	0xc4, // ascr_Cb 110001  ascr_Rb 00
	0xb7, // ascr_Rb 101101  ascr_Mr 11
	0xfd, // ascr_Mr 111111  ascr_Gr 01
	0xa0, // ascr_Gr 101000  ascr_Mg 00
	0x57, // ascr_Mg 010101  ascr_Gg 11
	0xff, // ascr_Gg 111111  ascr_Mb 11
	0xe8, // ascr_Mb 111010  ascr_Gb 00
	0xc7, // ascr_Gb 110001  ascr_Yr 11
	0xe0, // ascr_Yr 111000  ascr_Br 00
	0xd3, // ascr_Br 110100  ascr_Yg 11
	0xfc, // ascr_Yg 111111  ascr_Bg 00
	0x81, // ascr_Bg 100000  ascr_Yb 01
	0x2f, // ascr_Yb 001011  ascr_Bb 11
	0xff, // ascr_Bb 111111  ascr_Wr 11
	0xfc, // ascr_Wr 111111  ascr_Kr 00
	0x03, // ascr_Kr 000000  ascr_Wg 11
	0xe0, // ascr_Wg 111000  ascr_Kg 00
	0x03, // ascr_Kg 000000  ascr_Wb 11
	0xbc, // ascr_Wb 101111  ascr_Kb 00
	0x00, // ascr_Kb 000000  reserved 00
	0x0f, // reserved 0000  vsync_mask 1111
	/* end */
};

static unsigned char AUTO_UI_1[] = {
	/* start */
	0xBA, // Start offset 0x0982, base B1h
	0x00, // data_width 00  lce_module 0  lcd_bypass 0  lcd_roi 00  algo_module 0  algo_bypass 0
	0x30, // algo_roi 00  ascr_module 1  ascr_bypass 1  ascr_roi 00  roi_type 00
	0x00, // roi0_x_start 00000000
	0x00, // roi0_x_start 0000  roi0_x_end 0000
	0x00, // roi0_x_end 00000000
	0x00, // roi0_y_start 00000000
	0x00, // roi0_y_start 0000  roi0_y_end 0000
	0x00, // roi0_y_end 00000000
	0x00, // roi1_x_start 00000000
	0x00, // roi1_x_start 0000  roi1_x_end 0000
	0x00, // roi1_x_end 00000000
	0x00, // roi1_y_start 00000000
	0x00, // roi1_y_start 0000  roi1_y_end 0000
	0x00, // roi1_y_end 00000000
	0xa0, // hsize 10100000
	0x06, // hsize 0000  vsize 0110
	0x40, // vsize 01000000
	0x62, // gain 110000 cgain 10
	0x48, // cgain 0100  scene_on 1  scene_tran 000
	0x14, // scene_tran 0  min_diff 0010100
	0xb3, // illum_gain 10110011
	0x87, // ref_offset 10000111
	0x40, // ref_offset 0  ref_gain 1000000
	0x36, // ref_gain 00  hbsize 110  vbsize 110
	0xfa, // bth 11111010
	0x5a, // bin_size_ratio 0101101  dth 0
	0xe5, // dth 11  min_ref_offset 100101
	0x83, // min_ref_offset 10  gamma 0  cs 0 sharpen 0 nr 0 mask_th 11
	0xfc, // mask_th 111111  sharpen_weight 00
	0x40, // sharpen_weight 01000000
	0x14, // max_plus 00010100
	0x02, // max_plus 000  max_minus 00010
	0x81, // max_minus 100000  cs_gain 01
	0x00, // cs_gain 00000000
	0x00, // curve_1_b
	0x20, // curve_1_a
	0x00, // curve_2_b
	0x20, // curve_2_a
	0x00, // curve_3_b
	0x20, // curve_3_a
	0x00, // curve_4_b
	0x20, // curve_4_a
	0x00, // curve_5_b
	0x20, // curve_5_a
	0x00, // curve_6_b
	0x20, // curve_6_a
	0x00, // curve_7_b
	0x20, // curve_7_a
	0x00, // curve_8_b
	0x20, // curve_8_a
	0x00, // curve_9_b
	0x20, // curve_9_a
	0x00, // curve_10_b
	0x20, // curve_10_a
	0x00, // curve_11_b
	0x20, // curve_11_a
	0x00, // curve_12_b
	0x20, // curve_12_a
	0x00, // curve_13_b
	0x20, // curve_13_a
	0x00, // curve_14_b
	0x20, // curve_14_a
	0x00, // curve_15_b
	0x20, // curve_15_a
	0x00, // curve_16_b
	0x20, // curve_16_a
	0x00, // curve_17_b
	0x20, // curve_17_a
	0x00, // curve_18_b
	0x20, // curve_18_a
	0x00, // curve_19_b
	0x20, // curve_19_a
	0x00, // curve_20_b
	0x20, // curve_20_a
	0x00, // curve_21_b
	0x20, // curve_21_a
	0x00, // curve_22_b
	0x20, // curve_22_a
	0x00, // curve_23_b
	0x20, // curve_23_a
	0x00, // curve_24_b
	0xff, // curve_24_a
};

static unsigned char AUTO_UI_2[] = {
	0xBA, // Start offset 0x09D4, base B1h
	0x05, // ascr_strength 00000  ascr_on 1  skin_cb 01
	0x9e, // skin_cb 100111  skin_cr 10
	0xa4, // skin_cr 101001  up_distance 00
	0x5c, // up_distance 010111  down_distance 00                                       
	0xa4, // down_distance 101001  right_distance 00                                    
	0x64, // right_distance 011001  left_distance 00                                    
	0x9c, // left_distance 100111  up_divided_distance 00
	0x16, // up_divided_distance 00010110
	0x42, // up_divided_distance 01000010
	0xc0, // up_divided_distance 11  down_divided_distance 000000
	0xc7, // down_divided_distance 11000111
	0xd0, // down_divided_distance 110100 right_divided_distance 00
	0x14, // right_divided_distance 00010100
	0x7b, // right_divided_distance 01111011
	0x00, // right_divided_distance 00  left_divided_distance 000000
	0xd2, // left_divided_distance 11010010
	0x0f, // left_divided_distance 000011  ascr_skin_Rr 11
	0xfc, // ascr_skin_Rr 111111  ascr_skin_Rg 00
	0x00, // ascr_skin_Rg 000000  ascr_skin_Rb 00
	0x03, // ascr_skin_Rb 000000  ascr_skin_Yr 11
	0xff, // ascr_skin_Yr 111111  ascr_skin_Yg 11
	0xfc, // ascr_skin_Yg 111111  ascr_skin_Yb 00
	0x03, // ascr_skin_Yb 000000  ascr_skin_Mr 11
	0xfc, // ascr_skin_Mr 111111  ascr_skin_Mg 00
	0x03, // ascr_skin_Mg 000000  ascr_skin_Mb 11
	0xff, // ascr_skin_Mb 111111  ascr_skin_Wr 11
	0xff, // ascr_skin_Wr 111111  ascr_skin_Wg 11
	0xff, // ascr_skin_Wg 111111  ascr_skin_Wb 11
	0xfc, // ascr_skin_Wb 111111  ascr_Cr 00
	0x03, // ascr_Cr 000000  ascr_Rr 11
	0xff, // ascr_Rr 111111  ascr_Cg 11
	0xfc, // ascr_Cg 111111  ascr_Rg 00
	0x03, // ascr_Rg 000000  ascr_Cb 11
	0xfc, // ascr_Cb 111111  ascr_Rb 00
	0x03, // ascr_Rb 000000  ascr_Mr 11
	0xfc, // ascr_Mr 111111  ascr_Gr 00
	0x00, // ascr_Gr 000000  ascr_Mg 00
	0x03, // ascr_Mg 000000  ascr_Gg 11
	0xff, // ascr_Gg 111111  ascr_Mb 11
	0xfc, // ascr_Mb 111111  ascr_Gb 00
	0x03, // ascr_Gb 000000  ascr_Yr 11
	0xfc, // ascr_Yr 111111  ascr_Br 00
	0x03, // ascr_Br 000000  ascr_Yg 11
	0xfc, // ascr_Yg 111111  ascr_Bg 00
	0x00, // ascr_Bg 000000  ascr_Yb 00
	0x03, // ascr_Yb 000000  ascr_Bb 11
	0xff, // ascr_Bb 111111  ascr_Wr 11
	0xfc, // ascr_Wr 111111  ascr_Kr 00
	0x03, // ascr_Kr 000000  ascr_Wg 11
	0xfc, // ascr_Wg 111111  ascr_Kg 00
	0x03, // ascr_Kg 000000  ascr_Wb 11
	0xfc, // ascr_Wb 111111  ascr_Kb 00
	0x00, // ascr_Kb 000000  reserved 00
	0x0f, // reserved 0000  vsync_mask 1111
	/* end */
};

////////////////// GALLERY /////////////////////
static unsigned char STANDARD_GALLERY_1[] = {
	/* start */
	0xBA, // Start offset 0x0982, base B1h
	0x03, // data_width 00  lce_module 0  lcd_bypass 0  lcd_roi 00  algo_module 1  algo_bypass 1
	0x30, // algo_roi 00  ascr_module 1  ascr_bypass 1  ascr_roi 00  roi_type 00
	0x00, // roi0_x_start 00000000
	0x00, // roi0_x_start 0000  roi0_x_end 0000
	0x00, // roi0_x_end 00000000
	0x00, // roi0_y_start 00000000
	0x00, // roi0_y_start 0000  roi0_y_end 0000
	0x00, // roi0_y_end 00000000
	0x00, // roi1_x_start 00000000
	0x00, // roi1_x_start 0000  roi1_x_end 0000
	0x00, // roi1_x_end 00000000
	0x00, // roi1_y_start 00000000
	0x00, // roi1_y_start 0000  roi1_y_end 0000
	0x00, // roi1_y_end 00000000
	0xa0, // hsize 10100000
	0x06, // hsize 0000  vsize 0110
	0x40, // vsize 01000000
	0x62, // gain 110000 cgain 10
	0x48, // cgain 0100  scene_on 1  scene_tran 000
	0x14, // scene_tran 0  min_diff 0010100
	0xb3, // illum_gain 10110011
	0x87, // ref_offset 10000111
	0x40, // ref_offset 0  ref_gain 1000000
	0x36, // ref_gain 00  hbsize 110  vbsize 110
	0xfa, // bth 11111010
	0x5a, // bin_size_ratio 0101101  dth 0
	0xe5, // dth 11  min_ref_offset 100101
	0x8b, // min_ref_offset 10  gamma 0  cs 0 sharpen 1 nr 0 mask_th 11
	0xfc, // mask_th 111111  sharpen_weight 00
	0x15, // sharpen_weight 00010101
	0x14, // max_plus 00010100
	0x02, // max_plus 000  max_minus 00010
	0x81, // max_minus 100000  cs_gain 01
	0x00, // cs_gain 00000000
	0x00, // curve_1_b
	0x20, // curve_1_a
	0x00, // curve_2_b
	0x20, // curve_2_a
	0x00, // curve_3_b
	0x20, // curve_3_a
	0x00, // curve_4_b
	0x20, // curve_4_a
	0x00, // curve_5_b
	0x20, // curve_5_a
	0x00, // curve_6_b
	0x20, // curve_6_a
	0x00, // curve_7_b
	0x20, // curve_7_a
	0x00, // curve_8_b
	0x20, // curve_8_a
	0x00, // curve_9_b
	0x20, // curve_9_a
	0x00, // curve_10_b
	0x20, // curve_10_a
	0x00, // curve_11_b
	0x20, // curve_11_a
	0x00, // curve_12_b
	0x20, // curve_12_a
	0x00, // curve_13_b
	0x20, // curve_13_a
	0x00, // curve_14_b
	0x20, // curve_14_a
	0x00, // curve_15_b
	0x20, // curve_15_a
	0x00, // curve_16_b
	0x20, // curve_16_a
	0x00, // curve_17_b
	0x20, // curve_17_a
	0x00, // curve_18_b
	0x20, // curve_18_a
	0x00, // curve_19_b
	0x20, // curve_19_a
	0x00, // curve_20_b
	0x20, // curve_20_a
	0x00, // curve_21_b
	0x20, // curve_21_a
	0x00, // curve_22_b
	0x20, // curve_22_a
	0x00, // curve_23_b
	0x20, // curve_23_a
	0x00, // curve_24_b
	0xff, // curve_24_a
};

static unsigned char STANDARD_GALLERY_2[] = {
	0xBA, // Start offset 0x09D4, base B1h
	0x05, // ascr_strength 00000  ascr_on 1  skin_cb 01
	0x9e, // skin_cb 100111  skin_cr 10
	0xa4, // skin_cr 101001  up_distance 00
	0x5c, // up_distance 010111  down_distance 00                                       
	0xa4, // down_distance 101001  right_distance 00                                    
	0x64, // right_distance 011001  left_distance 00                                    
	0x9c, // left_distance 100111  up_divided_distance 00
	0x16, // up_divided_distance 00010110
	0x42, // up_divided_distance 01000010
	0xc0, // up_divided_distance 11  down_divided_distance 000000
	0xc7, // down_divided_distance 11000111
	0xd0, // down_divided_distance 110100 right_divided_distance 00
	0x14, // right_divided_distance 00010100
	0x7b, // right_divided_distance 01111011
	0x00, // right_divided_distance 00  left_divided_distance 000000
	0xd2, // left_divided_distance 11010010
	0x0f, // left_divided_distance 000011  ascr_skin_Rr 11
	0xfc, // ascr_skin_Rr 111111  ascr_skin_Rg 00
	0x00, // ascr_skin_Rg 000000  ascr_skin_Rb 00
	0x03, // ascr_skin_Rb 000000  ascr_skin_Yr 11
	0xff, // ascr_skin_Yr 111111  ascr_skin_Yg 11
	0xfc, // ascr_skin_Yg 111111  ascr_skin_Yb 00
	0x03, // ascr_skin_Yb 000000  ascr_skin_Mr 11
	0xfc, // ascr_skin_Mr 111111  ascr_skin_Mg 00
	0x03, // ascr_skin_Mg 000000  ascr_skin_Mb 11
	0xff, // ascr_skin_Mb 111111  ascr_skin_Wr 11
	0xff, // ascr_skin_Wr 111111  ascr_skin_Wg 11
	0xff, // ascr_skin_Wg 111111  ascr_skin_Wb 11
	0xfc, // ascr_skin_Wb 111111  ascr_Cr 00
	0x03, // ascr_Cr 000000  ascr_Rr 11
	0xff, // ascr_Rr 111111  ascr_Cg 11
	0xfc, // ascr_Cg 111111  ascr_Rg 00
	0x03, // ascr_Rg 000000  ascr_Cb 11
	0xfc, // ascr_Cb 111111  ascr_Rb 00
	0x03, // ascr_Rb 000000  ascr_Mr 11
	0xfc, // ascr_Mr 111111  ascr_Gr 00
	0x00, // ascr_Gr 000000  ascr_Mg 00
	0x03, // ascr_Mg 000000  ascr_Gg 11
	0xff, // ascr_Gg 111111  ascr_Mb 11
	0xfc, // ascr_Mb 111111  ascr_Gb 00
	0x03, // ascr_Gb 000000  ascr_Yr 11
	0xfc, // ascr_Yr 111111  ascr_Br 00
	0x03, // ascr_Br 000000  ascr_Yg 11
	0xfc, // ascr_Yg 111111  ascr_Bg 00
	0x00, // ascr_Bg 000000  ascr_Yb 00
	0x03, // ascr_Yb 000000  ascr_Bb 11
	0xff, // ascr_Bb 111111  ascr_Wr 11
	0xfc, // ascr_Wr 111111  ascr_Kr 00
	0x03, // ascr_Kr 000000  ascr_Wg 11
	0xfc, // ascr_Wg 111111  ascr_Kg 00
	0x03, // ascr_Kg 000000  ascr_Wb 11
	0xfc, // ascr_Wb 111111  ascr_Kb 00
	0x00, // ascr_Kb 000000  reserved 00
	0x0f, // reserved 0000  vsync_mask 1111
	/* end */
};

static unsigned char NATURAL_GALLERY_1[] = {
	/* start */
	0xBA, // Start offset 0x0982, base B1h
	0x03, // data_width 00  lce_module 0  lcd_bypass 0  lcd_roi 00  algo_module 1  algo_bypass 1
	0x30, // algo_roi 00  ascr_module 1  ascr_bypass 1  ascr_roi 00  roi_type 00
	0x00, // roi0_x_start 00000000
	0x00, // roi0_x_start 0000  roi0_x_end 0000
	0x00, // roi0_x_end 00000000
	0x00, // roi0_y_start 00000000
	0x00, // roi0_y_start 0000  roi0_y_end 0000
	0x00, // roi0_y_end 00000000
	0x00, // roi1_x_start 00000000
	0x00, // roi1_x_start 0000  roi1_x_end 0000
	0x00, // roi1_x_end 00000000
	0x00, // roi1_y_start 00000000
	0x00, // roi1_y_start 0000  roi1_y_end 0000
	0x00, // roi1_y_end 00000000
	0xa0, // hsize 10100000
	0x06, // hsize 0000  vsize 0110
	0x40, // vsize 01000000
	0x62, // gain 110000 cgain 10
	0x48, // cgain 0100  scene_on 1  scene_tran 000
	0x14, // scene_tran 0  min_diff 0010100
	0xb3, // illum_gain 10110011
	0x87, // ref_offset 10000111
	0x40, // ref_offset 0  ref_gain 1000000
	0x36, // ref_gain 00  hbsize 110  vbsize 110
	0xfa, // bth 11111010
	0x5a, // bin_size_ratio 0101101  dth 0
	0xe5, // dth 11  min_ref_offset 100101
	0x8b, // min_ref_offset 10  gamma 0  cs 0 sharpen 1 nr 0 mask_th 11
	0xfc, // mask_th 111111  sharpen_weight 00
	0x10, // sharpen_weight 00010000
	0x14, // max_plus 00010100
	0x02, // max_plus 000  max_minus 00010
	0x81, // max_minus 100000  cs_gain 01
	0x00, // cs_gain 00000000
	0x00, // curve_1_b
	0x20, // curve_1_a
	0x00, // curve_2_b
	0x20, // curve_2_a
	0x00, // curve_3_b
	0x20, // curve_3_a
	0x00, // curve_4_b
	0x20, // curve_4_a
	0x00, // curve_5_b
	0x20, // curve_5_a
	0x00, // curve_6_b
	0x20, // curve_6_a
	0x00, // curve_7_b
	0x20, // curve_7_a
	0x00, // curve_8_b
	0x20, // curve_8_a
	0x00, // curve_9_b
	0x20, // curve_9_a
	0x00, // curve_10_b
	0x20, // curve_10_a
	0x00, // curve_11_b
	0x20, // curve_11_a
	0x00, // curve_12_b
	0x20, // curve_12_a
	0x00, // curve_13_b
	0x20, // curve_13_a
	0x00, // curve_14_b
	0x20, // curve_14_a
	0x00, // curve_15_b
	0x20, // curve_15_a
	0x00, // curve_16_b
	0x20, // curve_16_a
	0x00, // curve_17_b
	0x20, // curve_17_a
	0x00, // curve_18_b
	0x20, // curve_18_a
	0x00, // curve_19_b
	0x20, // curve_19_a
	0x00, // curve_20_b
	0x20, // curve_20_a
	0x00, // curve_21_b
	0x20, // curve_21_a
	0x00, // curve_22_b
	0x20, // curve_22_a
	0x00, // curve_23_b
	0x20, // curve_23_a
	0x00, // curve_24_b
	0xff, // curve_24_a
};

static unsigned char NATURAL_GALLERY_2[] = {
	0xBA, // Start offset 0x09D4, base B1h
	0x05, // ascr_strength 00000  ascr_on 1  skin_cb 01
	0x9e, // skin_cb 100111  skin_cr 10
	0xa4, // skin_cr 101001  up_distance 00
	0x5c, // up_distance 010111  down_distance 00                                       
	0xa4, // down_distance 101001  right_distance 00                                    
	0x64, // right_distance 011001  left_distance 00                                    
	0x9c, // left_distance 100111  up_divided_distance 00
	0x16, // up_divided_distance 00010110
	0x42, // up_divided_distance 01000010
	0xc0, // up_divided_distance 11  down_divided_distance 000000
	0xc7, // down_divided_distance 11000111
	0xd0, // down_divided_distance 110100 right_divided_distance 00
	0x14, // right_divided_distance 00010100
	0x7b, // right_divided_distance 01111011
	0x00, // right_divided_distance 00  left_divided_distance 000000
	0xd2, // left_divided_distance 11010010
	0x0f, // left_divided_distance 000011  ascr_skin_Rr 11
	0xfc, // ascr_skin_Rr 111111  ascr_skin_Rg 00
	0x00, // ascr_skin_Rg 000000  ascr_skin_Rb 00
	0x03, // ascr_skin_Rb 000000  ascr_skin_Yr 11
	0xff, // ascr_skin_Yr 111111  ascr_skin_Yg 11
	0xfc, // ascr_skin_Yg 111111  ascr_skin_Yb 00
	0x03, // ascr_skin_Yb 000000  ascr_skin_Mr 11
	0xfc, // ascr_skin_Mr 111111  ascr_skin_Mg 00
	0x03, // ascr_skin_Mg 000000  ascr_skin_Mb 11
	0xff, // ascr_skin_Mb 111111  ascr_skin_Wr 11
	0xff, // ascr_skin_Wr 111111  ascr_skin_Wg 11
	0xff, // ascr_skin_Wg 111111  ascr_skin_Wb 11
	0xfc, // ascr_skin_Wb 111111  ascr_Cr 00
	0x03, // ascr_Cr 000000  ascr_Rr 11
	0xff, // ascr_Rr 111111  ascr_Cg 11
	0xfc, // ascr_Cg 111111  ascr_Rg 00
	0x5b, // ascr_Rg 010110  ascr_Cb 11
	0x94, // ascr_Cb 100101  ascr_Rb 00
	0x3b, // ascr_Rb 001110  ascr_Mr 11
	0xfc, // ascr_Mr 111111  ascr_Gr 00
	0x00, // ascr_Gr 000000  ascr_Mg 00
	0x53, // ascr_Mg 010100  ascr_Gg 11
	0xff, // ascr_Gg 111111  ascr_Mb 11
	0x64, // ascr_Mb 011001  ascr_Gb 00
	0x13, // ascr_Gb 000100  ascr_Yr 11
	0xf0, // ascr_Yr 111100  ascr_Br 00
	0x9b, // ascr_Br 100110  ascr_Yg 11
	0xfc, // ascr_Yg 111111  ascr_Bg 00
	0x6c, // ascr_Bg 011011  ascr_Yb 00
	0x8f, // ascr_Yb 100011  ascr_Bb 11
	0xff, // ascr_Bb 111111  ascr_Wr 11
	0xfc, // ascr_Wr 111111  ascr_Kr 00
	0x03, // ascr_Kr 000000  ascr_Wg 11
	0xe0, // ascr_Wg 111000  ascr_Kg 00
	0x03, // ascr_Kg 000000  ascr_Wb 11
	0xbc, // ascr_Wb 101111  ascr_Kb 00
	0x00, // ascr_Kb 000000  reserved 00
	0x0f, // reserved 0000  vsync_mask 1111
	/* end */
};

static unsigned char DYNAMIC_GALLERY_1[] = {
	/* start */
	0xBA, // Start offset 0x0982, base B1h
	0x03, // data_width 00  lce_module 0  lcd_bypass 0  lcd_roi 00  algo_module 1  algo_bypass 1
	0x30, // algo_roi 00  ascr_module 1  ascr_bypass 1  ascr_roi 00  roi_type 00
	0x00, // roi0_x_start 00000000
	0x00, // roi0_x_start 0000  roi0_x_end 0000
	0x00, // roi0_x_end 00000000
	0x00, // roi0_y_start 00000000
	0x00, // roi0_y_start 0000  roi0_y_end 0000
	0x00, // roi0_y_end 00000000
	0x00, // roi1_x_start 00000000
	0x00, // roi1_x_start 0000  roi1_x_end 0000
	0x00, // roi1_x_end 00000000
	0x00, // roi1_y_start 00000000
	0x00, // roi1_y_start 0000  roi1_y_end 0000
	0x00, // roi1_y_end 00000000
	0xa0, // hsize 10100000
	0x06, // hsize 0000  vsize 0110
	0x40, // vsize 01000000
	0x62, // gain 110000 cgain 10
	0x48, // cgain 0100  scene_on 1  scene_tran 000
	0x14, // scene_tran 0  min_diff 0010100
	0xb3, // illum_gain 10110011
	0x87, // ref_offset 10000111
	0x40, // ref_offset 0  ref_gain 1000000
	0x36, // ref_gain 00  hbsize 110  vbsize 110
	0xfa, // bth 11111010
	0x5a, // bin_size_ratio 0101101  dth 0
	0xe5, // dth 11  min_ref_offset 100101
	0xbb, // min_ref_offset 10  gamma 1  cs 1 sharpen 1 nr 0 mask_th 11
	0xfc, // mask_th 111111  sharpen_weight 00
	0x20, // sharpen_weight 00100000
	0x14, // max_plus 00010100
	0x02, // max_plus 000  max_minus 00010
	0x81, // max_minus 100000  cs_gain 01
	0x20, // cs_gain 00100000
	0x00, // curve_1_b 
	0x0f, // curve_1_a 
	0x00, // curve_2_b 
	0x0f, // curve_2_a 
	0x00, // curve_3_b 
	0x0f, // curve_3_a 
	0x00, // curve_4_b 
	0x0f, // curve_4_a 
	0x09, // curve_5_b 
	0xa2, // curve_5_a 
	0x09, // curve_6_b 
	0xa2, // curve_6_a 
	0x09, // curve_7_b 
	0xa2, // curve_7_a 
	0x09, // curve_8_b 
	0xa2, // curve_8_a 
	0x09, // curve_9_b 
	0xa2, // curve_9_a 
	0x09, // curve_10_b 
	0xa2, // curve_10_a 
	0x0a, // curve_11_b 
	0xa2, // curve_11_a 
	0x0a, // curve_12_b 
	0xa2, // curve_12_a 
	0x0a, // curve_13_b 
	0xa2, // curve_13_a 
	0x0a, // curve_14_b 
	0xa2, // curve_14_a 
	0x0a, // curve_15_b 
	0xa2, // curve_15_a 
	0x0a, // curve_16_b 
	0xa2, // curve_16_a 
	0x0a, // curve_17_b 
	0xa2, // curve_17_a 
	0x0a, // curve_18_b 
	0xa2, // curve_18_a 
	0x0f, // curve_19_b 
	0xa4, // curve_19_a 
	0x0f, // curve_20_b 
	0xa4, // curve_20_a 
	0x0f, // curve_21_b 
	0xa4, // curve_21_a 
	0x23, // curve_22_b 
	0x1c, // curve_22_a 
	0x48, // curve_23_b 
	0x17, // curve_23_a 
	0x00, // curve_24_b 
	0xff, // curve_24_a 
};

static unsigned char DYNAMIC_GALLERY_2[] = {
	0xBA, // Start offset 0x09D4, base B1h
	0x05, // ascr_strength 00000  ascr_on 1  skin_cb 01
	0x9e, // skin_cb 100111  skin_cr 10
	0xa4, // skin_cr 101001  up_distance 00
	0x5c, // up_distance 010111  down_distance 00                                       
	0xa4, // down_distance 101001  right_distance 00                                    
	0x64, // right_distance 011001  left_distance 00                                    
	0x9c, // left_distance 100111  up_divided_distance 00
	0x16, // up_divided_distance 00010110
	0x42, // up_divided_distance 01000010
	0xc0, // up_divided_distance 11  down_divided_distance 000000
	0xc7, // down_divided_distance 11000111
	0xd0, // down_divided_distance 110100 right_divided_distance 00
	0x14, // right_divided_distance 00010100
	0x7b, // right_divided_distance 01111011
	0x00, // right_divided_distance 00  left_divided_distance 000000
	0xd2, // left_divided_distance 11010010
	0x0f, // left_divided_distance 000011  ascr_skin_Rr 11
	0xfc, // ascr_skin_Rr 111111  ascr_skin_Rg 00
	0x00, // ascr_skin_Rg 000000  ascr_skin_Rb 00
	0x03, // ascr_skin_Rb 000000  ascr_skin_Yr 11
	0xff, // ascr_skin_Yr 111111  ascr_skin_Yg 11
	0xfc, // ascr_skin_Yg 111111  ascr_skin_Yb 00
	0x03, // ascr_skin_Yb 000000  ascr_skin_Mr 11
	0xfc, // ascr_skin_Mr 111111  ascr_skin_Mg 00
	0x03, // ascr_skin_Mg 000000  ascr_skin_Mb 11
	0xff, // ascr_skin_Mb 111111  ascr_skin_Wr 11
	0xff, // ascr_skin_Wr 111111  ascr_skin_Wg 11
	0xff, // ascr_skin_Wg 111111  ascr_skin_Wb 11
	0xfc, // ascr_skin_Wb 111111  ascr_Cr 00
	0x03, // ascr_Cr 000000  ascr_Rr 11
	0xff, // ascr_Rr 111111  ascr_Cg 11
	0xfc, // ascr_Cg 111111  ascr_Rg 00
	0x03, // ascr_Rg 000000  ascr_Cb 11
	0xfc, // ascr_Cb 111111  ascr_Rb 00
	0x03, // ascr_Rb 000000  ascr_Mr 11
	0xfc, // ascr_Mr 111111  ascr_Gr 00
	0x00, // ascr_Gr 000000  ascr_Mg 00
	0x03, // ascr_Mg 000000  ascr_Gg 11
	0xff, // ascr_Gg 111111  ascr_Mb 11
	0xfc, // ascr_Mb 111111  ascr_Gb 00
	0x03, // ascr_Gb 000000  ascr_Yr 11
	0xfc, // ascr_Yr 111111  ascr_Br 00
	0x03, // ascr_Br 000000  ascr_Yg 11
	0xfc, // ascr_Yg 111111  ascr_Bg 00
	0x00, // ascr_Bg 000000  ascr_Yb 00
	0x03, // ascr_Yb 000000  ascr_Bb 11
	0xff, // ascr_Bb 111111  ascr_Wr 11
	0xfc, // ascr_Wr 111111  ascr_Kr 00
	0x03, // ascr_Kr 000000  ascr_Wg 11
	0xfc, // ascr_Wg 111111  ascr_Kg 00
	0x03, // ascr_Kg 000000  ascr_Wb 11
	0xfc, // ascr_Wb 111111  ascr_Kb 00
	0x00, // ascr_Kb 000000  reserved 00
	0x0f, // reserved 0000  vsync_mask 1111
	/* end */
};

static unsigned char MOVIE_GALLERY_1[] = {
	/* start */
	0xBA, // Start offset 0x0982, base B1h
	0x03, // data_width 00  lce_module 0  lcd_bypass 0  lcd_roi 00  algo_module 1  algo_bypass 1
	0x30, // algo_roi 00  ascr_module 1  ascr_bypass 1  ascr_roi 00  roi_type 00
	0x00, // roi0_x_start 00000000
	0x00, // roi0_x_start 0000  roi0_x_end 0000
	0x00, // roi0_x_end 00000000
	0x00, // roi0_y_start 00000000
	0x00, // roi0_y_start 0000  roi0_y_end 0000
	0x00, // roi0_y_end 00000000
	0x00, // roi1_x_start 00000000
	0x00, // roi1_x_start 0000  roi1_x_end 0000
	0x00, // roi1_x_end 00000000
	0x00, // roi1_y_start 00000000
	0x00, // roi1_y_start 0000  roi1_y_end 0000
	0x00, // roi1_y_end 00000000
	0xa0, // hsize 10100000
	0x06, // hsize 0000  vsize 0110
	0x40, // vsize 01000000
	0x62, // gain 110000 cgain 10
	0x48, // cgain 0100  scene_on 1  scene_tran 000
	0x14, // scene_tran 0  min_diff 0010100
	0xb3, // illum_gain 10110011
	0x87, // ref_offset 10000111
	0x40, // ref_offset 0  ref_gain 1000000
	0x36, // ref_gain 00  hbsize 110  vbsize 110
	0xfa, // bth 11111010
	0x5a, // bin_size_ratio 0101101  dth 0
	0xe5, // dth 11  min_ref_offset 100101
	0x9b, // min_ref_offset 10  gamma 0  cs 1 sharpen 1 nr 0 mask_th 11
	0xfc, // mask_th 111111  sharpen_weight 00
	0x10, // sharpen_weight 00010000
	0x14, // max_plus 00010100
	0x02, // max_plus 000  max_minus 00010
	0x81, // max_minus 100000  cs_gain 01
	0x20, // cs_gain 00100000
	0x00, // curve_1_b
	0x20, // curve_1_a
	0x00, // curve_2_b
	0x20, // curve_2_a
	0x00, // curve_3_b
	0x20, // curve_3_a
	0x00, // curve_4_b
	0x20, // curve_4_a
	0x00, // curve_5_b
	0x20, // curve_5_a
	0x00, // curve_6_b
	0x20, // curve_6_a
	0x00, // curve_7_b
	0x20, // curve_7_a
	0x00, // curve_8_b
	0x20, // curve_8_a
	0x00, // curve_9_b
	0x20, // curve_9_a
	0x00, // curve_10_b
	0x20, // curve_10_a
	0x00, // curve_11_b
	0x20, // curve_11_a
	0x00, // curve_12_b
	0x20, // curve_12_a
	0x00, // curve_13_b
	0x20, // curve_13_a
	0x00, // curve_14_b
	0x20, // curve_14_a
	0x00, // curve_15_b
	0x20, // curve_15_a
	0x00, // curve_16_b
	0x20, // curve_16_a
	0x00, // curve_17_b
	0x20, // curve_17_a
	0x00, // curve_18_b
	0x20, // curve_18_a
	0x00, // curve_19_b
	0x20, // curve_19_a
	0x00, // curve_20_b
	0x20, // curve_20_a
	0x00, // curve_21_b
	0x20, // curve_21_a
	0x00, // curve_22_b
	0x20, // curve_22_a
	0x00, // curve_23_b
	0x20, // curve_23_a
	0x00, // curve_24_b
	0xff, // curve_24_a
};

static unsigned char MOVIE_GALLERY_2[] = {
	0xBA, // Start offset 0x09D4, base B1h
	0x05, // ascr_strength 00000  ascr_on 1  skin_cb 01
	0x9e, // skin_cb 100111  skin_cr 10
	0xa4, // skin_cr 101001  up_distance 00
	0x5c, // up_distance 010111  down_distance 00                                       
	0xa4, // down_distance 101001  right_distance 00                                    
	0x64, // right_distance 011001  left_distance 00                                    
	0x9c, // left_distance 100111  up_divided_distance 00
	0x16, // up_divided_distance 00010110
	0x42, // up_divided_distance 01000010
	0xc0, // up_divided_distance 11  down_divided_distance 000000
	0xc7, // down_divided_distance 11000111
	0xd0, // down_divided_distance 110100 right_divided_distance 00
	0x14, // right_divided_distance 00010100
	0x7b, // right_divided_distance 01111011
	0x00, // right_divided_distance 00  left_divided_distance 000000
	0xd2, // left_divided_distance 11010010
	0x0f, // left_divided_distance 000011  ascr_skin_Rr 11
	0xfc, // ascr_skin_Rr 111111  ascr_skin_Rg 00
	0x00, // ascr_skin_Rg 000000  ascr_skin_Rb 00
	0x03, // ascr_skin_Rb 000000  ascr_skin_Yr 11
	0xff, // ascr_skin_Yr 111111  ascr_skin_Yg 11
	0xfc, // ascr_skin_Yg 111111  ascr_skin_Yb 00
	0x03, // ascr_skin_Yb 000000  ascr_skin_Mr 11
	0xfc, // ascr_skin_Mr 111111  ascr_skin_Mg 00
	0x03, // ascr_skin_Mg 000000  ascr_skin_Mb 11
	0xff, // ascr_skin_Mb 111111  ascr_skin_Wr 11
	0xff, // ascr_skin_Wr 111111  ascr_skin_Wg 11
	0xff, // ascr_skin_Wg 111111  ascr_skin_Wb 11
	0xfd, // ascr_skin_Wb 111111  ascr_Cr 01
	0xeb, // ascr_Cr 111010  ascr_Rr 11
	0xff, // ascr_Rr 111111  ascr_Cg 11
	0xfc, // ascr_Cg 111111  ascr_Rg 00
	0xaf, // ascr_Rg 101011  ascr_Cb 11
	0xc4, // ascr_Cb 110001  ascr_Rb 00
	0xb7, // ascr_Rb 101101  ascr_Mr 11
	0xfd, // ascr_Mr 111111  ascr_Gr 01
	0xa0, // ascr_Gr 101000  ascr_Mg 00
	0x57, // ascr_Mg 010101  ascr_Gg 11
	0xff, // ascr_Gg 111111  ascr_Mb 11
	0xe8, // ascr_Mb 111010  ascr_Gb 00
	0xc7, // ascr_Gb 110001  ascr_Yr 11
	0xe0, // ascr_Yr 111000  ascr_Br 00
	0xd3, // ascr_Br 110100  ascr_Yg 11
	0xfc, // ascr_Yg 111111  ascr_Bg 00
	0x81, // ascr_Bg 100000  ascr_Yb 01
	0x2f, // ascr_Yb 001011  ascr_Bb 11
	0xff, // ascr_Bb 111111  ascr_Wr 11
	0xfc, // ascr_Wr 111111  ascr_Kr 00
	0x03, // ascr_Kr 000000  ascr_Wg 11
	0xe0, // ascr_Wg 111000  ascr_Kg 00
	0x03, // ascr_Kg 000000  ascr_Wb 11
	0xbc, // ascr_Wb 101111  ascr_Kb 00
	0x00, // ascr_Kb 000000  reserved 00
	0x0f, // reserved 0000  vsync_mask 1111
	/* end */
};

static unsigned char AUTO_GALLERY_1[] = {
	/* start */
	0xBA, // Start offset 0x0982, base B1h
	0x03, // data_width 00  lce_module 0  lcd_bypass 0  lcd_roi 00  algo_module 1  algo_bypass 1
	0x30, // algo_roi 00  ascr_module 1  ascr_bypass 1  ascr_roi 00  roi_type 00
	0x00, // roi0_x_start 00000000
	0x00, // roi0_x_start 0000  roi0_x_end 0000
	0x00, // roi0_x_end 00000000
	0x00, // roi0_y_start 00000000
	0x00, // roi0_y_start 0000  roi0_y_end 0000
	0x00, // roi0_y_end 00000000
	0x00, // roi1_x_start 00000000
	0x00, // roi1_x_start 0000  roi1_x_end 0000
	0x00, // roi1_x_end 00000000
	0x00, // roi1_y_start 00000000
	0x00, // roi1_y_start 0000  roi1_y_end 0000
	0x00, // roi1_y_end 00000000
	0xa0, // hsize 10100000
	0x06, // hsize 0000  vsize 0110
	0x40, // vsize 01000000
	0x62, // gain 110000 cgain 10
	0x48, // cgain 0100  scene_on 1  scene_tran 000
	0x14, // scene_tran 0  min_diff 0010100
	0xb3, // illum_gain 10110011
	0x87, // ref_offset 10000111
	0x40, // ref_offset 0  ref_gain 1000000
	0x36, // ref_gain 00  hbsize 110  vbsize 110
	0xfa, // bth 11111010
	0x5a, // bin_size_ratio 0101101  dth 0
	0xe5, // dth 11  min_ref_offset 100101
	0x8b, // min_ref_offset 10  gamma 0  cs 0 sharpen 1 nr 0 mask_th 11
	0xfc, // mask_th 111111  sharpen_weight 00
	0x15, // sharpen_weight 00010101
	0x14, // max_plus 00010100
	0x02, // max_plus 000  max_minus 00010
	0x81, // max_minus 100000  cs_gain 01
	0x00, // cs_gain 00000000
	0x00, // curve_1_b
	0x20, // curve_1_a
	0x00, // curve_2_b
	0x20, // curve_2_a
	0x00, // curve_3_b
	0x20, // curve_3_a
	0x00, // curve_4_b
	0x20, // curve_4_a
	0x00, // curve_5_b
	0x20, // curve_5_a
	0x00, // curve_6_b
	0x20, // curve_6_a
	0x00, // curve_7_b
	0x20, // curve_7_a
	0x00, // curve_8_b
	0x20, // curve_8_a
	0x00, // curve_9_b
	0x20, // curve_9_a
	0x00, // curve_10_b
	0x20, // curve_10_a
	0x00, // curve_11_b
	0x20, // curve_11_a
	0x00, // curve_12_b
	0x20, // curve_12_a
	0x00, // curve_13_b
	0x20, // curve_13_a
	0x00, // curve_14_b
	0x20, // curve_14_a
	0x00, // curve_15_b
	0x20, // curve_15_a
	0x00, // curve_16_b
	0x20, // curve_16_a
	0x00, // curve_17_b
	0x20, // curve_17_a
	0x00, // curve_18_b
	0x20, // curve_18_a
	0x00, // curve_19_b
	0x20, // curve_19_a
	0x00, // curve_20_b
	0x20, // curve_20_a
	0x00, // curve_21_b
	0x20, // curve_21_a
	0x00, // curve_22_b
	0x20, // curve_22_a
	0x00, // curve_23_b
	0x20, // curve_23_a
	0x00, // curve_24_b
	0xff, // curve_24_a
};

static unsigned char AUTO_GALLERY_2[] = {
	0xBA, // Start offset 0x09D4, base B1h
	0x65, // ascr_strength 01100  ascr_on 1  skin_cb 01
	0x9e, // skin_cb 100111  skin_cr 10
	0xa4, // skin_cr 101001  up_distance 00
	0x5c, // up_distance 010111  down_distance 00                                       
	0xa4, // down_distance 101001  right_distance 00                                    
	0x64, // right_distance 011001  left_distance 00                                    
	0x9c, // left_distance 100111  up_divided_distance 00
	0x16, // up_divided_distance 00010110
	0x42, // up_divided_distance 01000010
	0xc0, // up_divided_distance 11  down_divided_distance 000000
	0xc7, // down_divided_distance 11000111
	0xd0, // down_divided_distance 110100 right_divided_distance 00
	0x14, // right_divided_distance 00010100
	0x7b, // right_divided_distance 01111011
	0x00, // right_divided_distance 00  left_divided_distance 000000
	0xd2, // left_divided_distance 11010010
	0x0f, // left_divided_distance 000011  ascr_skin_Rr 11
	0x91, // ascr_skin_Rr 100100  ascr_skin_Rg 01
	0x01, // ascr_skin_Rg 000000  ascr_skin_Rb 01
	0x02, // ascr_skin_Rb 000000  ascr_skin_Yr 10
	0xff, // ascr_skin_Yr 111111  ascr_skin_Yg 11
	0xfc, // ascr_skin_Yg 111111  ascr_skin_Yb 00
	0x02, // ascr_skin_Yb 000000  ascr_skin_Mr 10
	0xfc, // ascr_skin_Mr 111111  ascr_skin_Mg 00
	0x03, // ascr_skin_Mg 000000  ascr_skin_Mb 11
	0xff, // ascr_skin_Mb 111111  ascr_skin_Wr 11
	0xff, // ascr_skin_Wr 111111  ascr_skin_Wg 11
	0xff, // ascr_skin_Wg 111111  ascr_skin_Wb 11
	0xfc, // ascr_skin_Wb 111111  ascr_Cr 00
	0x03, // ascr_Cr 000000  ascr_Rr 11
	0xff, // ascr_Rr 111111  ascr_Cg 11
	0xfc, // ascr_Cg 111111  ascr_Rg 00
	0x03, // ascr_Rg 000000  ascr_Cb 11
	0xfc, // ascr_Cb 111111  ascr_Rb 00
	0x03, // ascr_Rb 000000  ascr_Mr 11
	0xfc, // ascr_Mr 111111  ascr_Gr 00
	0x00, // ascr_Gr 000000  ascr_Mg 00
	0x03, // ascr_Mg 000000  ascr_Gg 11
	0xff, // ascr_Gg 111111  ascr_Mb 11
	0xfc, // ascr_Mb 111111  ascr_Gb 00
	0x03, // ascr_Gb 000000  ascr_Yr 11
	0xfc, // ascr_Yr 111111  ascr_Br 00
	0x03, // ascr_Br 000000  ascr_Yg 11
	0xfc, // ascr_Yg 111111  ascr_Bg 00
	0x00, // ascr_Bg 000000  ascr_Yb 00
	0x03, // ascr_Yb 000000  ascr_Bb 11
	0xff, // ascr_Bb 111111  ascr_Wr 11
	0xfc, // ascr_Wr 111111  ascr_Kr 00
	0x03, // ascr_Kr 000000  ascr_Wg 11
	0xfc, // ascr_Wg 111111  ascr_Kg 00
	0x03, // ascr_Kg 000000  ascr_Wb 11
	0xfc, // ascr_Wb 111111  ascr_Kb 00
	0x00, // ascr_Kb 000000  reserved 00
	0x0f, // reserved 0000  vsync_mask 1111
	/* end */
};

////////////////// VIDEO /////////////////////
static unsigned char STANDARD_VIDEO_1[] = {
	/* start */
	0xBA, // Start offset 0x0982, base B1h
	0x03, // data_width 00  lce_module 0  lcd_bypass 0  lcd_roi 00  algo_module 1  algo_bypass 1
	0x30, // algo_roi 00  ascr_module 1  ascr_bypass 1  ascr_roi 00  roi_type 00
	0x00, // roi0_x_start 00000000
	0x00, // roi0_x_start 0000  roi0_x_end 0000
	0x00, // roi0_x_end 00000000
	0x00, // roi0_y_start 00000000
	0x00, // roi0_y_start 0000  roi0_y_end 0000
	0x00, // roi0_y_end 00000000
	0x00, // roi1_x_start 00000000
	0x00, // roi1_x_start 0000  roi1_x_end 0000
	0x00, // roi1_x_end 00000000
	0x00, // roi1_y_start 00000000
	0x00, // roi1_y_start 0000  roi1_y_end 0000
	0x00, // roi1_y_end 00000000
	0xa0, // hsize 10100000
	0x06, // hsize 0000  vsize 0110
	0x40, // vsize 01000000
	0x62, // gain 110000 cgain 10
	0x48, // cgain 0100  scene_on 1  scene_tran 000
	0x14, // scene_tran 0  min_diff 0010100
	0xb3, // illum_gain 10110011
	0x87, // ref_offset 10000111
	0x40, // ref_offset 0  ref_gain 1000000
	0x36, // ref_gain 00  hbsize 110  vbsize 110
	0xfa, // bth 11111010
	0x5a, // bin_size_ratio 0101101  dth 0
	0xe5, // dth 11  min_ref_offset 100101
	0x8b, // min_ref_offset 10  gamma 0  cs 0 sharpen 1 nr 0 mask_th 11
	0xfc, // mask_th 111111  sharpen_weight 00
	0x15, // sharpen_weight 00010101
	0x08, // max_plus 00001000
	0x01, // max_plus 000  max_minus 00001
	0x01, // max_minus 000000  cs_gain 01
	0x00, // cs_gain 00000000
	0x00, // curve_1_b
	0x20, // curve_1_a
	0x00, // curve_2_b
	0x20, // curve_2_a
	0x00, // curve_3_b
	0x20, // curve_3_a
	0x00, // curve_4_b
	0x20, // curve_4_a
	0x00, // curve_5_b
	0x20, // curve_5_a
	0x00, // curve_6_b
	0x20, // curve_6_a
	0x00, // curve_7_b
	0x20, // curve_7_a
	0x00, // curve_8_b
	0x20, // curve_8_a
	0x00, // curve_9_b
	0x20, // curve_9_a
	0x00, // curve_10_b
	0x20, // curve_10_a
	0x00, // curve_11_b
	0x20, // curve_11_a
	0x00, // curve_12_b
	0x20, // curve_12_a
	0x00, // curve_13_b
	0x20, // curve_13_a
	0x00, // curve_14_b
	0x20, // curve_14_a
	0x00, // curve_15_b
	0x20, // curve_15_a
	0x00, // curve_16_b
	0x20, // curve_16_a
	0x00, // curve_17_b
	0x20, // curve_17_a
	0x00, // curve_18_b
	0x20, // curve_18_a
	0x00, // curve_19_b
	0x20, // curve_19_a
	0x00, // curve_20_b
	0x20, // curve_20_a
	0x00, // curve_21_b
	0x20, // curve_21_a
	0x00, // curve_22_b
	0x20, // curve_22_a
	0x00, // curve_23_b
	0x20, // curve_23_a
	0x00, // curve_24_b
	0xff, // curve_24_a
};

static unsigned char STANDARD_VIDEO_2[] = {
	0xBA, // Start offset 0x09D4, base B1h
	0x05, // ascr_strength 00000  ascr_on 1  skin_cb 01
	0x9e, // skin_cb 100111  skin_cr 10
	0xa4, // skin_cr 101001  up_distance 00
	0x5c, // up_distance 010111  down_distance 00                                       
	0xa4, // down_distance 101001  right_distance 00                                    
	0x64, // right_distance 011001  left_distance 00                                    
	0x9c, // left_distance 100111  up_divided_distance 00
	0x16, // up_divided_distance 00010110
	0x42, // up_divided_distance 01000010
	0xc0, // up_divided_distance 11  down_divided_distance 000000
	0xc7, // down_divided_distance 11000111
	0xd0, // down_divided_distance 110100 right_divided_distance 00
	0x14, // right_divided_distance 00010100
	0x7b, // right_divided_distance 01111011
	0x00, // right_divided_distance 00  left_divided_distance 000000
	0xd2, // left_divided_distance 11010010
	0x0f, // left_divided_distance 000011  ascr_skin_Rr 11
	0xfc, // ascr_skin_Rr 111111  ascr_skin_Rg 00
	0x00, // ascr_skin_Rg 000000  ascr_skin_Rb 00
	0x03, // ascr_skin_Rb 000000  ascr_skin_Yr 11
	0xff, // ascr_skin_Yr 111111  ascr_skin_Yg 11
	0xfc, // ascr_skin_Yg 111111  ascr_skin_Yb 00
	0x03, // ascr_skin_Yb 000000  ascr_skin_Mr 11
	0xfc, // ascr_skin_Mr 111111  ascr_skin_Mg 00
	0x03, // ascr_skin_Mg 000000  ascr_skin_Mb 11
	0xff, // ascr_skin_Mb 111111  ascr_skin_Wr 11
	0xff, // ascr_skin_Wr 111111  ascr_skin_Wg 11
	0xff, // ascr_skin_Wg 111111  ascr_skin_Wb 11
	0xfc, // ascr_skin_Wb 111111  ascr_Cr 00
	0x03, // ascr_Cr 000000  ascr_Rr 11
	0xff, // ascr_Rr 111111  ascr_Cg 11
	0xfc, // ascr_Cg 111111  ascr_Rg 00
	0x03, // ascr_Rg 000000  ascr_Cb 11
	0xfc, // ascr_Cb 111111  ascr_Rb 00
	0x03, // ascr_Rb 000000  ascr_Mr 11
	0xfc, // ascr_Mr 111111  ascr_Gr 00
	0x00, // ascr_Gr 000000  ascr_Mg 00
	0x03, // ascr_Mg 000000  ascr_Gg 11
	0xff, // ascr_Gg 111111  ascr_Mb 11
	0xfc, // ascr_Mb 111111  ascr_Gb 00
	0x03, // ascr_Gb 000000  ascr_Yr 11
	0xfc, // ascr_Yr 111111  ascr_Br 00
	0x03, // ascr_Br 000000  ascr_Yg 11
	0xfc, // ascr_Yg 111111  ascr_Bg 00
	0x00, // ascr_Bg 000000  ascr_Yb 00
	0x03, // ascr_Yb 000000  ascr_Bb 11
	0xff, // ascr_Bb 111111  ascr_Wr 11
	0xfc, // ascr_Wr 111111  ascr_Kr 00
	0x03, // ascr_Kr 000000  ascr_Wg 11
	0xfc, // ascr_Wg 111111  ascr_Kg 00
	0x03, // ascr_Kg 000000  ascr_Wb 11
	0xfc, // ascr_Wb 111111  ascr_Kb 00
	0x00, // ascr_Kb 000000  reserved 00
	0x0f, // reserved 0000  vsync_mask 1111
	/* end */
};

static unsigned char NATURAL_VIDEO_1[] = {
	/* start */
	0xBA, // Start offset 0x0982, base B1h
	0x03, // data_width 00  lce_module 0  lcd_bypass 0  lcd_roi 00  algo_module 1  algo_bypass 1
	0x30, // algo_roi 00  ascr_module 1  ascr_bypass 1  ascr_roi 00  roi_type 00
	0x00, // roi0_x_start 00000000
	0x00, // roi0_x_start 0000  roi0_x_end 0000
	0x00, // roi0_x_end 00000000
	0x00, // roi0_y_start 00000000
	0x00, // roi0_y_start 0000  roi0_y_end 0000
	0x00, // roi0_y_end 00000000
	0x00, // roi1_x_start 00000000
	0x00, // roi1_x_start 0000  roi1_x_end 0000
	0x00, // roi1_x_end 00000000
	0x00, // roi1_y_start 00000000
	0x00, // roi1_y_start 0000  roi1_y_end 0000
	0x00, // roi1_y_end 00000000
	0xa0, // hsize 10100000
	0x06, // hsize 0000  vsize 0110
	0x40, // vsize 01000000
	0x62, // gain 110000 cgain 10
	0x48, // cgain 0100  scene_on 1  scene_tran 000
	0x14, // scene_tran 0  min_diff 0010100
	0xb3, // illum_gain 10110011
	0x87, // ref_offset 10000111
	0x40, // ref_offset 0  ref_gain 1000000
	0x36, // ref_gain 00  hbsize 110  vbsize 110
	0xfa, // bth 11111010
	0x5a, // bin_size_ratio 0101101  dth 0
	0xe5, // dth 11  min_ref_offset 100101
	0x8b, // min_ref_offset 10  gamma 0  cs 0 sharpen 1 nr 0 mask_th 11
	0xfc, // mask_th 111111  sharpen_weight 00
	0x10, // sharpen_weight 00010000
	0x08, // max_plus 00001000
	0x01, // max_plus 000  max_minus 00001
	0x01, // max_minus 000000  cs_gain 01
	0x00, // cs_gain 00000000
	0x00, // curve_1_b
	0x20, // curve_1_a
	0x00, // curve_2_b
	0x20, // curve_2_a
	0x00, // curve_3_b
	0x20, // curve_3_a
	0x00, // curve_4_b
	0x20, // curve_4_a
	0x00, // curve_5_b
	0x20, // curve_5_a
	0x00, // curve_6_b
	0x20, // curve_6_a
	0x00, // curve_7_b
	0x20, // curve_7_a
	0x00, // curve_8_b
	0x20, // curve_8_a
	0x00, // curve_9_b
	0x20, // curve_9_a
	0x00, // curve_10_b
	0x20, // curve_10_a
	0x00, // curve_11_b
	0x20, // curve_11_a
	0x00, // curve_12_b
	0x20, // curve_12_a
	0x00, // curve_13_b
	0x20, // curve_13_a
	0x00, // curve_14_b
	0x20, // curve_14_a
	0x00, // curve_15_b
	0x20, // curve_15_a
	0x00, // curve_16_b
	0x20, // curve_16_a
	0x00, // curve_17_b
	0x20, // curve_17_a
	0x00, // curve_18_b
	0x20, // curve_18_a
	0x00, // curve_19_b
	0x20, // curve_19_a
	0x00, // curve_20_b
	0x20, // curve_20_a
	0x00, // curve_21_b
	0x20, // curve_21_a
	0x00, // curve_22_b
	0x20, // curve_22_a
	0x00, // curve_23_b
	0x20, // curve_23_a
	0x00, // curve_24_b
	0xff, // curve_24_a
};

static unsigned char NATURAL_VIDEO_2[] = {
	0xBA, // Start offset 0x09D4, base B1h
	0x05, // ascr_strength 00000  ascr_on 1  skin_cb 01
	0x9e, // skin_cb 100111  skin_cr 10
	0xa4, // skin_cr 101001  up_distance 00
	0x5c, // up_distance 010111  down_distance 00                                       
	0xa4, // down_distance 101001  right_distance 00                                    
	0x64, // right_distance 011001  left_distance 00                                    
	0x9c, // left_distance 100111  up_divided_distance 00
	0x16, // up_divided_distance 00010110
	0x42, // up_divided_distance 01000010
	0xc0, // up_divided_distance 11  down_divided_distance 000000
	0xc7, // down_divided_distance 11000111
	0xd0, // down_divided_distance 110100 right_divided_distance 00
	0x14, // right_divided_distance 00010100
	0x7b, // right_divided_distance 01111011
	0x00, // right_divided_distance 00  left_divided_distance 000000
	0xd2, // left_divided_distance 11010010
	0x0f, // left_divided_distance 000011  ascr_skin_Rr 11
	0xfc, // ascr_skin_Rr 111111  ascr_skin_Rg 00
	0x00, // ascr_skin_Rg 000000  ascr_skin_Rb 00
	0x03, // ascr_skin_Rb 000000  ascr_skin_Yr 11
	0xff, // ascr_skin_Yr 111111  ascr_skin_Yg 11
	0xfc, // ascr_skin_Yg 111111  ascr_skin_Yb 00
	0x03, // ascr_skin_Yb 000000  ascr_skin_Mr 11
	0xfc, // ascr_skin_Mr 111111  ascr_skin_Mg 00
	0x03, // ascr_skin_Mg 000000  ascr_skin_Mb 11
	0xff, // ascr_skin_Mb 111111  ascr_skin_Wr 11
	0xff, // ascr_skin_Wr 111111  ascr_skin_Wg 11
	0xff, // ascr_skin_Wg 111111  ascr_skin_Wb 11
	0xfc, // ascr_skin_Wb 111111  ascr_Cr 00
	0x03, // ascr_Cr 000000  ascr_Rr 11
	0xff, // ascr_Rr 111111  ascr_Cg 11
	0xfc, // ascr_Cg 111111  ascr_Rg 00
	0x5b, // ascr_Rg 010110  ascr_Cb 11
	0x94, // ascr_Cb 100101  ascr_Rb 00
	0x3b, // ascr_Rb 001110  ascr_Mr 11
	0xfc, // ascr_Mr 111111  ascr_Gr 00
	0x00, // ascr_Gr 000000  ascr_Mg 00
	0x53, // ascr_Mg 010100  ascr_Gg 11
	0xff, // ascr_Gg 111111  ascr_Mb 11
	0x64, // ascr_Mb 011001  ascr_Gb 00
	0x13, // ascr_Gb 000100  ascr_Yr 11
	0xf0, // ascr_Yr 111100  ascr_Br 00
	0x9b, // ascr_Br 100110  ascr_Yg 11
	0xfc, // ascr_Yg 111111  ascr_Bg 00
	0x6c, // ascr_Bg 011011  ascr_Yb 00
	0x8f, // ascr_Yb 100011  ascr_Bb 11
	0xff, // ascr_Bb 111111  ascr_Wr 11
	0xfc, // ascr_Wr 111111  ascr_Kr 00
	0x03, // ascr_Kr 000000  ascr_Wg 11
	0xe0, // ascr_Wg 111000  ascr_Kg 00
	0x03, // ascr_Kg 000000  ascr_Wb 11
	0xbc, // ascr_Wb 101111  ascr_Kb 00
	0x00, // ascr_Kb 000000  reserved 00
	0x0f, // reserved 0000  vsync_mask 1111
	/* end */
};

static unsigned char DYNAMIC_VIDEO_1[] = {
	/* start */
	0xBA, // Start offset 0x0982, base B1h
	0x03, // data_width 00  lce_module 0  lcd_bypass 0  lcd_roi 00  algo_module 1  algo_bypass 1
	0x30, // algo_roi 00  ascr_module 1  ascr_bypass 1  ascr_roi 00  roi_type 00
	0x00, // roi0_x_start 00000000
	0x00, // roi0_x_start 0000  roi0_x_end 0000
	0x00, // roi0_x_end 00000000
	0x00, // roi0_y_start 00000000
	0x00, // roi0_y_start 0000  roi0_y_end 0000
	0x00, // roi0_y_end 00000000
	0x00, // roi1_x_start 00000000
	0x00, // roi1_x_start 0000  roi1_x_end 0000
	0x00, // roi1_x_end 00000000
	0x00, // roi1_y_start 00000000
	0x00, // roi1_y_start 0000  roi1_y_end 0000
	0x00, // roi1_y_end 00000000
	0xa0, // hsize 10100000
	0x06, // hsize 0000  vsize 0110
	0x40, // vsize 01000000
	0x62, // gain 110000 cgain 10
	0x48, // cgain 0100  scene_on 1  scene_tran 000
	0x14, // scene_tran 0  min_diff 0010100
	0xb3, // illum_gain 10110011
	0x87, // ref_offset 10000111
	0x40, // ref_offset 0  ref_gain 1000000
	0x36, // ref_gain 00  hbsize 110  vbsize 110
	0xfa, // bth 11111010
	0x5a, // bin_size_ratio 0101101  dth 0
	0xe5, // dth 11  min_ref_offset 100101
	0xbb, // min_ref_offset 10  gamma 1  cs 1 sharpen 1 nr 0 mask_th 11
	0xfc, // mask_th 111111  sharpen_weight 00
	0x20, // sharpen_weight 00100000
	0x08, // max_plus 00001000
	0x01, // max_plus 000  max_minus 00001
	0x01, // max_minus 000000  cs_gain 01
	0x20, // cs_gain 00100000
	0x00, // curve_1_b 
	0x0f, // curve_1_a 
	0x00, // curve_2_b 
	0x0f, // curve_2_a 
	0x00, // curve_3_b 
	0x0f, // curve_3_a 
	0x00, // curve_4_b 
	0x0f, // curve_4_a 
	0x09, // curve_5_b 
	0xa2, // curve_5_a 
	0x09, // curve_6_b 
	0xa2, // curve_6_a 
	0x09, // curve_7_b 
	0xa2, // curve_7_a 
	0x09, // curve_8_b 
	0xa2, // curve_8_a 
	0x09, // curve_9_b 
	0xa2, // curve_9_a 
	0x09, // curve_10_b 
	0xa2, // curve_10_a 
	0x0a, // curve_11_b 
	0xa2, // curve_11_a 
	0x0a, // curve_12_b 
	0xa2, // curve_12_a 
	0x0a, // curve_13_b 
	0xa2, // curve_13_a 
	0x0a, // curve_14_b 
	0xa2, // curve_14_a 
	0x0a, // curve_15_b 
	0xa2, // curve_15_a 
	0x0a, // curve_16_b 
	0xa2, // curve_16_a 
	0x0a, // curve_17_b 
	0xa2, // curve_17_a 
	0x0a, // curve_18_b 
	0xa2, // curve_18_a 
	0x0f, // curve_19_b 
	0xa4, // curve_19_a 
	0x0f, // curve_20_b 
	0xa4, // curve_20_a 
	0x0f, // curve_21_b 
	0xa4, // curve_21_a 
	0x23, // curve_22_b 
	0x1c, // curve_22_a 
	0x48, // curve_23_b 
	0x17, // curve_23_a 
	0x00, // curve_24_b 
	0xff, // curve_24_a 
};

static unsigned char DYNAMIC_VIDEO_2[] = {
	0xBA, // Start offset 0x09D4, base B1h
	0x05, // ascr_strength 00000  ascr_on 1  skin_cb 01
	0x9e, // skin_cb 100111  skin_cr 10
	0xa4, // skin_cr 101001  up_distance 00
	0x5c, // up_distance 010111  down_distance 00                                       
	0xa4, // down_distance 101001  right_distance 00                                    
	0x64, // right_distance 011001  left_distance 00                                    
	0x9c, // left_distance 100111  up_divided_distance 00
	0x16, // up_divided_distance 00010110
	0x42, // up_divided_distance 01000010
	0xc0, // up_divided_distance 11  down_divided_distance 000000
	0xc7, // down_divided_distance 11000111
	0xd0, // down_divided_distance 110100 right_divided_distance 00
	0x14, // right_divided_distance 00010100
	0x7b, // right_divided_distance 01111011
	0x00, // right_divided_distance 00  left_divided_distance 000000
	0xd2, // left_divided_distance 11010010
	0x0f, // left_divided_distance 000011  ascr_skin_Rr 11
	0xfc, // ascr_skin_Rr 111111  ascr_skin_Rg 00
	0x00, // ascr_skin_Rg 000000  ascr_skin_Rb 00
	0x03, // ascr_skin_Rb 000000  ascr_skin_Yr 11
	0xff, // ascr_skin_Yr 111111  ascr_skin_Yg 11
	0xfc, // ascr_skin_Yg 111111  ascr_skin_Yb 00
	0x03, // ascr_skin_Yb 000000  ascr_skin_Mr 11
	0xfc, // ascr_skin_Mr 111111  ascr_skin_Mg 00
	0x03, // ascr_skin_Mg 000000  ascr_skin_Mb 11
	0xff, // ascr_skin_Mb 111111  ascr_skin_Wr 11
	0xff, // ascr_skin_Wr 111111  ascr_skin_Wg 11
	0xff, // ascr_skin_Wg 111111  ascr_skin_Wb 11
	0xfc, // ascr_skin_Wb 111111  ascr_Cr 00
	0x03, // ascr_Cr 000000  ascr_Rr 11
	0xff, // ascr_Rr 111111  ascr_Cg 11
	0xfc, // ascr_Cg 111111  ascr_Rg 00
	0x03, // ascr_Rg 000000  ascr_Cb 11
	0xfc, // ascr_Cb 111111  ascr_Rb 00
	0x03, // ascr_Rb 000000  ascr_Mr 11
	0xfc, // ascr_Mr 111111  ascr_Gr 00
	0x00, // ascr_Gr 000000  ascr_Mg 00
	0x03, // ascr_Mg 000000  ascr_Gg 11
	0xff, // ascr_Gg 111111  ascr_Mb 11
	0xfc, // ascr_Mb 111111  ascr_Gb 00
	0x03, // ascr_Gb 000000  ascr_Yr 11
	0xfc, // ascr_Yr 111111  ascr_Br 00
	0x03, // ascr_Br 000000  ascr_Yg 11
	0xfc, // ascr_Yg 111111  ascr_Bg 00
	0x00, // ascr_Bg 000000  ascr_Yb 00
	0x03, // ascr_Yb 000000  ascr_Bb 11
	0xff, // ascr_Bb 111111  ascr_Wr 11
	0xfc, // ascr_Wr 111111  ascr_Kr 00
	0x03, // ascr_Kr 000000  ascr_Wg 11
	0xfc, // ascr_Wg 111111  ascr_Kg 00
	0x03, // ascr_Kg 000000  ascr_Wb 11
	0xfc, // ascr_Wb 111111  ascr_Kb 00
	0x00, // ascr_Kb 000000  reserved 00
	0x0f, // reserved 0000  vsync_mask 1111
	/* end */
};

static unsigned char MOVIE_VIDEO_1[] = {
	/* start */
	0xBA, // Start offset 0x0982, base B1h
	0x03, // data_width 00  lce_module 0  lcd_bypass 0  lcd_roi 00  algo_module 1  algo_bypass 1
	0x30, // algo_roi 00  ascr_module 1  ascr_bypass 1  ascr_roi 00  roi_type 00
	0x00, // roi0_x_start 00000000
	0x00, // roi0_x_start 0000  roi0_x_end 0000
	0x00, // roi0_x_end 00000000
	0x00, // roi0_y_start 00000000
	0x00, // roi0_y_start 0000  roi0_y_end 0000
	0x00, // roi0_y_end 00000000
	0x00, // roi1_x_start 00000000
	0x00, // roi1_x_start 0000  roi1_x_end 0000
	0x00, // roi1_x_end 00000000
	0x00, // roi1_y_start 00000000
	0x00, // roi1_y_start 0000  roi1_y_end 0000
	0x00, // roi1_y_end 00000000
	0xa0, // hsize 10100000
	0x06, // hsize 0000  vsize 0110
	0x40, // vsize 01000000
	0x62, // gain 110000 cgain 10
	0x48, // cgain 0100  scene_on 1  scene_tran 000
	0x14, // scene_tran 0  min_diff 0010100
	0xb3, // illum_gain 10110011
	0x87, // ref_offset 10000111
	0x40, // ref_offset 0  ref_gain 1000000
	0x36, // ref_gain 00  hbsize 110  vbsize 110
	0xfa, // bth 11111010
	0x5a, // bin_size_ratio 0101101  dth 0
	0xe5, // dth 11  min_ref_offset 100101
	0x9b, // min_ref_offset 10  gamma 0  cs 1 sharpen 1 nr 0 mask_th 11
	0xfc, // mask_th 111111  sharpen_weight 00
	0x10, // sharpen_weight 00010000
	0x14, // max_plus 00010100
	0x02, // max_plus 000  max_minus 00010
	0x81, // max_minus 100000  cs_gain 01
	0x20, // cs_gain 00100000
	0x00, // curve_1_b
	0x20, // curve_1_a
	0x00, // curve_2_b
	0x20, // curve_2_a
	0x00, // curve_3_b
	0x20, // curve_3_a
	0x00, // curve_4_b
	0x20, // curve_4_a
	0x00, // curve_5_b
	0x20, // curve_5_a
	0x00, // curve_6_b
	0x20, // curve_6_a
	0x00, // curve_7_b
	0x20, // curve_7_a
	0x00, // curve_8_b
	0x20, // curve_8_a
	0x00, // curve_9_b
	0x20, // curve_9_a
	0x00, // curve_10_b
	0x20, // curve_10_a
	0x00, // curve_11_b
	0x20, // curve_11_a
	0x00, // curve_12_b
	0x20, // curve_12_a
	0x00, // curve_13_b
	0x20, // curve_13_a
	0x00, // curve_14_b
	0x20, // curve_14_a
	0x00, // curve_15_b
	0x20, // curve_15_a
	0x00, // curve_16_b
	0x20, // curve_16_a
	0x00, // curve_17_b
	0x20, // curve_17_a
	0x00, // curve_18_b
	0x20, // curve_18_a
	0x00, // curve_19_b
	0x20, // curve_19_a
	0x00, // curve_20_b
	0x20, // curve_20_a
	0x00, // curve_21_b
	0x20, // curve_21_a
	0x00, // curve_22_b
	0x20, // curve_22_a
	0x00, // curve_23_b
	0x20, // curve_23_a
	0x00, // curve_24_b
	0xff, // curve_24_a
};

static unsigned char MOVIE_VIDEO_2[] = {
	0xBA, // Start offset 0x09D4, base B1h
	0x05, // ascr_strength 00000  ascr_on 1  skin_cb 01
	0x9e, // skin_cb 100111  skin_cr 10
	0xa4, // skin_cr 101001  up_distance 00
	0x5c, // up_distance 010111  down_distance 00                                       
	0xa4, // down_distance 101001  right_distance 00                                    
	0x64, // right_distance 011001  left_distance 00                                    
	0x9c, // left_distance 100111  up_divided_distance 00
	0x16, // up_divided_distance 00010110
	0x42, // up_divided_distance 01000010
	0xc0, // up_divided_distance 11  down_divided_distance 000000
	0xc7, // down_divided_distance 11000111
	0xd0, // down_divided_distance 110100 right_divided_distance 00
	0x14, // right_divided_distance 00010100
	0x7b, // right_divided_distance 01111011
	0x00, // right_divided_distance 00  left_divided_distance 000000
	0xd2, // left_divided_distance 11010010
	0x0f, // left_divided_distance 000011  ascr_skin_Rr 11
	0xfc, // ascr_skin_Rr 111111  ascr_skin_Rg 00
	0x00, // ascr_skin_Rg 000000  ascr_skin_Rb 00
	0x03, // ascr_skin_Rb 000000  ascr_skin_Yr 11
	0xff, // ascr_skin_Yr 111111  ascr_skin_Yg 11
	0xfc, // ascr_skin_Yg 111111  ascr_skin_Yb 00
	0x03, // ascr_skin_Yb 000000  ascr_skin_Mr 11
	0xfc, // ascr_skin_Mr 111111  ascr_skin_Mg 00
	0x03, // ascr_skin_Mg 000000  ascr_skin_Mb 11
	0xff, // ascr_skin_Mb 111111  ascr_skin_Wr 11
	0xff, // ascr_skin_Wr 111111  ascr_skin_Wg 11
	0xff, // ascr_skin_Wg 111111  ascr_skin_Wb 11
	0xfd, // ascr_skin_Wb 111111  ascr_Cr 01
	0xeb, // ascr_Cr 111010  ascr_Rr 11
	0xff, // ascr_Rr 111111  ascr_Cg 11
	0xfc, // ascr_Cg 111111  ascr_Rg 00
	0xaf, // ascr_Rg 101011  ascr_Cb 11
	0xc4, // ascr_Cb 110001  ascr_Rb 00
	0xb7, // ascr_Rb 101101  ascr_Mr 11
	0xfd, // ascr_Mr 111111  ascr_Gr 01
	0xa0, // ascr_Gr 101000  ascr_Mg 00
	0x57, // ascr_Mg 010101  ascr_Gg 11
	0xff, // ascr_Gg 111111  ascr_Mb 11
	0xe8, // ascr_Mb 111010  ascr_Gb 00
	0xc7, // ascr_Gb 110001  ascr_Yr 11
	0xe0, // ascr_Yr 111000  ascr_Br 00
	0xd3, // ascr_Br 110100  ascr_Yg 11
	0xfc, // ascr_Yg 111111  ascr_Bg 00
	0x81, // ascr_Bg 100000  ascr_Yb 01
	0x2f, // ascr_Yb 001011  ascr_Bb 11
	0xff, // ascr_Bb 111111  ascr_Wr 11
	0xfc, // ascr_Wr 111111  ascr_Kr 00
	0x03, // ascr_Kr 000000  ascr_Wg 11
	0xe0, // ascr_Wg 111000  ascr_Kg 00
	0x03, // ascr_Kg 000000  ascr_Wb 11
	0xbc, // ascr_Wb 101111  ascr_Kb 00
	0x00, // ascr_Kb 000000  reserved 00
	0x0f, // reserved 0000  vsync_mask 1111
	/* end */
};

static unsigned char AUTO_VIDEO_1[] = {
	/* start */
	0xBA, // Start offset 0x0982, base B1h
	0x03, // data_width 00  lce_module 0  lcd_bypass 0  lcd_roi 00  algo_module 1  algo_bypass 1
	0x30, // algo_roi 00  ascr_module 1  ascr_bypass 1  ascr_roi 00  roi_type 00
	0x00, // roi0_x_start 00000000
	0x00, // roi0_x_start 0000  roi0_x_end 0000
	0x00, // roi0_x_end 00000000
	0x00, // roi0_y_start 00000000
	0x00, // roi0_y_start 0000  roi0_y_end 0000
	0x00, // roi0_y_end 00000000
	0x00, // roi1_x_start 00000000
	0x00, // roi1_x_start 0000  roi1_x_end 0000
	0x00, // roi1_x_end 00000000
	0x00, // roi1_y_start 00000000
	0x00, // roi1_y_start 0000  roi1_y_end 0000
	0x00, // roi1_y_end 00000000
	0xa0, // hsize 10100000
	0x06, // hsize 0000  vsize 0110
	0x40, // vsize 01000000
	0x62, // gain 110000 cgain 10
	0x48, // cgain 0100  scene_on 1  scene_tran 000
	0x14, // scene_tran 0  min_diff 0010100
	0xb3, // illum_gain 10110011
	0x87, // ref_offset 10000111
	0x40, // ref_offset 0  ref_gain 1000000
	0x36, // ref_gain 00  hbsize 110  vbsize 110
	0xfa, // bth 11111010
	0x5a, // bin_size_ratio 0101101  dth 0
	0xe5, // dth 11  min_ref_offset 100101
	0x8b, // min_ref_offset 10  gamma 0  cs 0 sharpen 1 nr 0 mask_th 11
	0xfc, // mask_th 111111  sharpen_weight 00
	0x15, // sharpen_weight 00010101
	0x08, // max_plus 00001000
	0x01, // max_plus 000  max_minus 00001
	0x01, // max_minus 000000  cs_gain 01
	0x00, // cs_gain 00000000
	0x00, // curve_1_b
	0x20, // curve_1_a
	0x00, // curve_2_b
	0x20, // curve_2_a
	0x00, // curve_3_b
	0x20, // curve_3_a
	0x00, // curve_4_b
	0x20, // curve_4_a
	0x00, // curve_5_b
	0x20, // curve_5_a
	0x00, // curve_6_b
	0x20, // curve_6_a
	0x00, // curve_7_b
	0x20, // curve_7_a
	0x00, // curve_8_b
	0x20, // curve_8_a
	0x00, // curve_9_b
	0x20, // curve_9_a
	0x00, // curve_10_b
	0x20, // curve_10_a
	0x00, // curve_11_b
	0x20, // curve_11_a
	0x00, // curve_12_b
	0x20, // curve_12_a
	0x00, // curve_13_b
	0x20, // curve_13_a
	0x00, // curve_14_b
	0x20, // curve_14_a
	0x00, // curve_15_b
	0x20, // curve_15_a
	0x00, // curve_16_b
	0x20, // curve_16_a
	0x00, // curve_17_b
	0x20, // curve_17_a
	0x00, // curve_18_b
	0x20, // curve_18_a
	0x00, // curve_19_b
	0x20, // curve_19_a
	0x00, // curve_20_b
	0x20, // curve_20_a
	0x00, // curve_21_b
	0x20, // curve_21_a
	0x00, // curve_22_b
	0x20, // curve_22_a
	0x00, // curve_23_b
	0x20, // curve_23_a
	0x00, // curve_24_b
	0xff, // curve_24_a
};

static unsigned char AUTO_VIDEO_2[] = {
	0xBA, // Start offset 0x09D4, base B1h
	0x65, // ascr_strength 01100  ascr_on 1  skin_cb 01
	0x9e, // skin_cb 100111  skin_cr 10
	0xa4, // skin_cr 101001  up_distance 00
	0x5c, // up_distance 010111  down_distance 00                                       
	0xa4, // down_distance 101001  right_distance 00                                    
	0x64, // right_distance 011001  left_distance 00                                    
	0x9c, // left_distance 100111  up_divided_distance 00
	0x16, // up_divided_distance 00010110
	0x42, // up_divided_distance 01000010
	0xc0, // up_divided_distance 11  down_divided_distance 000000
	0xc7, // down_divided_distance 11000111
	0xd0, // down_divided_distance 110100 right_divided_distance 00
	0x14, // right_divided_distance 00010100
	0x7b, // right_divided_distance 01111011
	0x00, // right_divided_distance 00  left_divided_distance 000000
	0xd2, // left_divided_distance 11010010
	0x0f, // left_divided_distance 000011  ascr_skin_Rr 11
	0x91, // ascr_skin_Rr 100100  ascr_skin_Rg 01
	0x01, // ascr_skin_Rg 000000  ascr_skin_Rb 01
	0x02, // ascr_skin_Rb 000000  ascr_skin_Yr 10
	0xff, // ascr_skin_Yr 111111  ascr_skin_Yg 11
	0xfc, // ascr_skin_Yg 111111  ascr_skin_Yb 00
	0x02, // ascr_skin_Yb 000000  ascr_skin_Mr 10
	0xfc, // ascr_skin_Mr 111111  ascr_skin_Mg 00
	0x03, // ascr_skin_Mg 000000  ascr_skin_Mb 11
	0xff, // ascr_skin_Mb 111111  ascr_skin_Wr 11
	0xff, // ascr_skin_Wr 111111  ascr_skin_Wg 11
	0xff, // ascr_skin_Wg 111111  ascr_skin_Wb 11
	0xfc, // ascr_skin_Wb 111111  ascr_Cr 00
	0x03, // ascr_Cr 000000  ascr_Rr 11
	0xff, // ascr_Rr 111111  ascr_Cg 11
	0xfc, // ascr_Cg 111111  ascr_Rg 00
	0x03, // ascr_Rg 000000  ascr_Cb 11
	0xfc, // ascr_Cb 111111  ascr_Rb 00
	0x03, // ascr_Rb 000000  ascr_Mr 11
	0xfc, // ascr_Mr 111111  ascr_Gr 00
	0x00, // ascr_Gr 000000  ascr_Mg 00
	0x03, // ascr_Mg 000000  ascr_Gg 11
	0xff, // ascr_Gg 111111  ascr_Mb 11
	0xfc, // ascr_Mb 111111  ascr_Gb 00
	0x03, // ascr_Gb 000000  ascr_Yr 11
	0xfc, // ascr_Yr 111111  ascr_Br 00
	0x03, // ascr_Br 000000  ascr_Yg 11
	0xfc, // ascr_Yg 111111  ascr_Bg 00
	0x00, // ascr_Bg 000000  ascr_Yb 00
	0x03, // ascr_Yb 000000  ascr_Bb 11
	0xff, // ascr_Bb 111111  ascr_Wr 11
	0xfc, // ascr_Wr 111111  ascr_Kr 00
	0x03, // ascr_Kr 000000  ascr_Wg 11
	0xfc, // ascr_Wg 111111  ascr_Kg 00
	0x03, // ascr_Kg 000000  ascr_Wb 11
	0xfc, // ascr_Wb 111111  ascr_Kb 00
	0x00, // ascr_Kb 000000  reserved 00
	0x0f, // reserved 0000  vsync_mask 1111
	/* end */
};

////////////////// VT /////////////////////
static unsigned char STANDARD_VT_1[] = {
	/* start */
	0xBA, // Start offset 0x0982, base B1h
	0x03, // data_width 00  lce_module 0  lcd_bypass 0  lcd_roi 00  algo_module 1  algo_bypass 1
	0x30, // algo_roi 00  ascr_module 1  ascr_bypass 1  ascr_roi 00  roi_type 00
	0x00, // roi0_x_start 00000000
	0x00, // roi0_x_start 0000  roi0_x_end 0000
	0x00, // roi0_x_end 00000000
	0x00, // roi0_y_start 00000000
	0x00, // roi0_y_start 0000  roi0_y_end 0000
	0x00, // roi0_y_end 00000000
	0x00, // roi1_x_start 00000000
	0x00, // roi1_x_start 0000  roi1_x_end 0000
	0x00, // roi1_x_end 00000000
	0x00, // roi1_y_start 00000000
	0x00, // roi1_y_start 0000  roi1_y_end 0000
	0x00, // roi1_y_end 00000000
	0xa0, // hsize 10100000
	0x06, // hsize 0000  vsize 0110
	0x40, // vsize 01000000
	0x62, // gain 110000 cgain 10
	0x48, // cgain 0100  scene_on 1  scene_tran 000
	0x14, // scene_tran 0  min_diff 0010100
	0xb3, // illum_gain 10110011
	0x87, // ref_offset 10000111
	0x40, // ref_offset 0  ref_gain 1000000
	0x36, // ref_gain 00  hbsize 110  vbsize 110
	0xfa, // bth 11111010
	0x5a, // bin_size_ratio 0101101  dth 0
	0xe5, // dth 11  min_ref_offset 100101
	0x8b, // min_ref_offset 10  gamma 0  cs 0 sharpen 1 nr 0 mask_th 11
	0xfc, // mask_th 111111  sharpen_weight 00
	0x15, // sharpen_weight 00010101
	0x14, // max_plus 00010100
	0x02, // max_plus 000  max_minus 00010
	0x81, // max_minus 100000  cs_gain 01
	0x00, // cs_gain 00000000
	0x00, // curve_1_b
	0x20, // curve_1_a
	0x00, // curve_2_b
	0x20, // curve_2_a
	0x00, // curve_3_b
	0x20, // curve_3_a
	0x00, // curve_4_b
	0x20, // curve_4_a
	0x00, // curve_5_b
	0x20, // curve_5_a
	0x00, // curve_6_b
	0x20, // curve_6_a
	0x00, // curve_7_b
	0x20, // curve_7_a
	0x00, // curve_8_b
	0x20, // curve_8_a
	0x00, // curve_9_b
	0x20, // curve_9_a
	0x00, // curve_10_b
	0x20, // curve_10_a
	0x00, // curve_11_b
	0x20, // curve_11_a
	0x00, // curve_12_b
	0x20, // curve_12_a
	0x00, // curve_13_b
	0x20, // curve_13_a
	0x00, // curve_14_b
	0x20, // curve_14_a
	0x00, // curve_15_b
	0x20, // curve_15_a
	0x00, // curve_16_b
	0x20, // curve_16_a
	0x00, // curve_17_b
	0x20, // curve_17_a
	0x00, // curve_18_b
	0x20, // curve_18_a
	0x00, // curve_19_b
	0x20, // curve_19_a
	0x00, // curve_20_b
	0x20, // curve_20_a
	0x00, // curve_21_b
	0x20, // curve_21_a
	0x00, // curve_22_b
	0x20, // curve_22_a
	0x00, // curve_23_b
	0x20, // curve_23_a
	0x00, // curve_24_b
	0xff, // curve_24_a
};

static unsigned char STANDARD_VT_2[] = {
	0xBA, // Start offset 0x09D4, base B1h
	0x05, // ascr_strength 00000  ascr_on 1  skin_cb 01
	0x9e, // skin_cb 100111  skin_cr 10
	0xa4, // skin_cr 101001  up_distance 00
	0x5c, // up_distance 010111  down_distance 00                                       
	0xa4, // down_distance 101001  right_distance 00                                    
	0x64, // right_distance 011001  left_distance 00                                    
	0x9c, // left_distance 100111  up_divided_distance 00
	0x16, // up_divided_distance 00010110
	0x42, // up_divided_distance 01000010
	0xc0, // up_divided_distance 11  down_divided_distance 000000
	0xc7, // down_divided_distance 11000111
	0xd0, // down_divided_distance 110100 right_divided_distance 00
	0x14, // right_divided_distance 00010100
	0x7b, // right_divided_distance 01111011
	0x00, // right_divided_distance 00  left_divided_distance 000000
	0xd2, // left_divided_distance 11010010
	0x0f, // left_divided_distance 000011  ascr_skin_Rr 11
	0xfc, // ascr_skin_Rr 111111  ascr_skin_Rg 00
	0x00, // ascr_skin_Rg 000000  ascr_skin_Rb 00
	0x03, // ascr_skin_Rb 000000  ascr_skin_Yr 11
	0xff, // ascr_skin_Yr 111111  ascr_skin_Yg 11
	0xfc, // ascr_skin_Yg 111111  ascr_skin_Yb 00
	0x03, // ascr_skin_Yb 000000  ascr_skin_Mr 11
	0xfc, // ascr_skin_Mr 111111  ascr_skin_Mg 00
	0x03, // ascr_skin_Mg 000000  ascr_skin_Mb 11
	0xff, // ascr_skin_Mb 111111  ascr_skin_Wr 11
	0xff, // ascr_skin_Wr 111111  ascr_skin_Wg 11
	0xff, // ascr_skin_Wg 111111  ascr_skin_Wb 11
	0xfc, // ascr_skin_Wb 111111  ascr_Cr 00
	0x03, // ascr_Cr 000000  ascr_Rr 11
	0xff, // ascr_Rr 111111  ascr_Cg 11
	0xfc, // ascr_Cg 111111  ascr_Rg 00
	0x03, // ascr_Rg 000000  ascr_Cb 11
	0xfc, // ascr_Cb 111111  ascr_Rb 00
	0x03, // ascr_Rb 000000  ascr_Mr 11
	0xfc, // ascr_Mr 111111  ascr_Gr 00
	0x00, // ascr_Gr 000000  ascr_Mg 00
	0x03, // ascr_Mg 000000  ascr_Gg 11
	0xff, // ascr_Gg 111111  ascr_Mb 11
	0xfc, // ascr_Mb 111111  ascr_Gb 00
	0x03, // ascr_Gb 000000  ascr_Yr 11
	0xfc, // ascr_Yr 111111  ascr_Br 00
	0x03, // ascr_Br 000000  ascr_Yg 11
	0xfc, // ascr_Yg 111111  ascr_Bg 00
	0x00, // ascr_Bg 000000  ascr_Yb 00
	0x03, // ascr_Yb 000000  ascr_Bb 11
	0xff, // ascr_Bb 111111  ascr_Wr 11
	0xfc, // ascr_Wr 111111  ascr_Kr 00
	0x03, // ascr_Kr 000000  ascr_Wg 11
	0xfc, // ascr_Wg 111111  ascr_Kg 00
	0x03, // ascr_Kg 000000  ascr_Wb 11
	0xfc, // ascr_Wb 111111  ascr_Kb 00
	0x00, // ascr_Kb 000000  reserved 00
	0x0f, // reserved 0000  vsync_mask 1111
	/* end */
};

static unsigned char NATURAL_VT_1[] = {
	/* start */
	0xBA, // Start offset 0x0982, base B1h
	0x03, // data_width 00  lce_module 0  lcd_bypass 0  lcd_roi 00  algo_module 1  algo_bypass 1
	0x30, // algo_roi 00  ascr_module 1  ascr_bypass 1  ascr_roi 00  roi_type 00
	0x00, // roi0_x_start 00000000
	0x00, // roi0_x_start 0000  roi0_x_end 0000
	0x00, // roi0_x_end 00000000
	0x00, // roi0_y_start 00000000
	0x00, // roi0_y_start 0000  roi0_y_end 0000
	0x00, // roi0_y_end 00000000
	0x00, // roi1_x_start 00000000
	0x00, // roi1_x_start 0000  roi1_x_end 0000
	0x00, // roi1_x_end 00000000
	0x00, // roi1_y_start 00000000
	0x00, // roi1_y_start 0000  roi1_y_end 0000
	0x00, // roi1_y_end 00000000
	0xa0, // hsize 10100000
	0x06, // hsize 0000  vsize 0110
	0x40, // vsize 01000000
	0x62, // gain 110000 cgain 10
	0x48, // cgain 0100  scene_on 1  scene_tran 000
	0x14, // scene_tran 0  min_diff 0010100
	0xb3, // illum_gain 10110011
	0x87, // ref_offset 10000111
	0x40, // ref_offset 0  ref_gain 1000000
	0x36, // ref_gain 00  hbsize 110  vbsize 110
	0xfa, // bth 11111010
	0x5a, // bin_size_ratio 0101101  dth 0
	0xe5, // dth 11  min_ref_offset 100101
	0x8b, // min_ref_offset 10  gamma 0  cs 0 sharpen 1 nr 0 mask_th 11
	0xfc, // mask_th 111111  sharpen_weight 00
	0x10, // sharpen_weight 00010000
	0x14, // max_plus 00010100
	0x02, // max_plus 000  max_minus 00010
	0x81, // max_minus 100000  cs_gain 01
	0x00, // cs_gain 00000000
	0x00, // curve_1_b
	0x20, // curve_1_a
	0x00, // curve_2_b
	0x20, // curve_2_a
	0x00, // curve_3_b
	0x20, // curve_3_a
	0x00, // curve_4_b
	0x20, // curve_4_a
	0x00, // curve_5_b
	0x20, // curve_5_a
	0x00, // curve_6_b
	0x20, // curve_6_a
	0x00, // curve_7_b
	0x20, // curve_7_a
	0x00, // curve_8_b
	0x20, // curve_8_a
	0x00, // curve_9_b
	0x20, // curve_9_a
	0x00, // curve_10_b
	0x20, // curve_10_a
	0x00, // curve_11_b
	0x20, // curve_11_a
	0x00, // curve_12_b
	0x20, // curve_12_a
	0x00, // curve_13_b
	0x20, // curve_13_a
	0x00, // curve_14_b
	0x20, // curve_14_a
	0x00, // curve_15_b
	0x20, // curve_15_a
	0x00, // curve_16_b
	0x20, // curve_16_a
	0x00, // curve_17_b
	0x20, // curve_17_a
	0x00, // curve_18_b
	0x20, // curve_18_a
	0x00, // curve_19_b
	0x20, // curve_19_a
	0x00, // curve_20_b
	0x20, // curve_20_a
	0x00, // curve_21_b
	0x20, // curve_21_a
	0x00, // curve_22_b
	0x20, // curve_22_a
	0x00, // curve_23_b
	0x20, // curve_23_a
	0x00, // curve_24_b
	0xff, // curve_24_a
};

static unsigned char NATURAL_VT_2[] = {
	0xBA, // Start offset 0x09D4, base B1h
	0x05, // ascr_strength 00000  ascr_on 1  skin_cb 01
	0x9e, // skin_cb 100111  skin_cr 10
	0xa4, // skin_cr 101001  up_distance 00
	0x5c, // up_distance 010111  down_distance 00                                       
	0xa4, // down_distance 101001  right_distance 00                                    
	0x64, // right_distance 011001  left_distance 00                                    
	0x9c, // left_distance 100111  up_divided_distance 00
	0x16, // up_divided_distance 00010110
	0x42, // up_divided_distance 01000010
	0xc0, // up_divided_distance 11  down_divided_distance 000000
	0xc7, // down_divided_distance 11000111
	0xd0, // down_divided_distance 110100 right_divided_distance 00
	0x14, // right_divided_distance 00010100
	0x7b, // right_divided_distance 01111011
	0x00, // right_divided_distance 00  left_divided_distance 000000
	0xd2, // left_divided_distance 11010010
	0x0f, // left_divided_distance 000011  ascr_skin_Rr 11
	0xfc, // ascr_skin_Rr 111111  ascr_skin_Rg 00
	0x00, // ascr_skin_Rg 000000  ascr_skin_Rb 00
	0x03, // ascr_skin_Rb 000000  ascr_skin_Yr 11
	0xff, // ascr_skin_Yr 111111  ascr_skin_Yg 11
	0xfc, // ascr_skin_Yg 111111  ascr_skin_Yb 00
	0x03, // ascr_skin_Yb 000000  ascr_skin_Mr 11
	0xfc, // ascr_skin_Mr 111111  ascr_skin_Mg 00
	0x03, // ascr_skin_Mg 000000  ascr_skin_Mb 11
	0xff, // ascr_skin_Mb 111111  ascr_skin_Wr 11
	0xff, // ascr_skin_Wr 111111  ascr_skin_Wg 11
	0xff, // ascr_skin_Wg 111111  ascr_skin_Wb 11
	0xfc, // ascr_skin_Wb 111111  ascr_Cr 00
	0x03, // ascr_Cr 000000  ascr_Rr 11
	0xff, // ascr_Rr 111111  ascr_Cg 11
	0xfc, // ascr_Cg 111111  ascr_Rg 00
	0x5b, // ascr_Rg 010110  ascr_Cb 11
	0x94, // ascr_Cb 100101  ascr_Rb 00
	0x3b, // ascr_Rb 001110  ascr_Mr 11
	0xfc, // ascr_Mr 111111  ascr_Gr 00
	0x00, // ascr_Gr 000000  ascr_Mg 00
	0x53, // ascr_Mg 010100  ascr_Gg 11
	0xff, // ascr_Gg 111111  ascr_Mb 11
	0x64, // ascr_Mb 011001  ascr_Gb 00
	0x13, // ascr_Gb 000100  ascr_Yr 11
	0xf0, // ascr_Yr 111100  ascr_Br 00
	0x9b, // ascr_Br 100110  ascr_Yg 11
	0xfc, // ascr_Yg 111111  ascr_Bg 00
	0x6c, // ascr_Bg 011011  ascr_Yb 00
	0x8f, // ascr_Yb 100011  ascr_Bb 11
	0xff, // ascr_Bb 111111  ascr_Wr 11
	0xfc, // ascr_Wr 111111  ascr_Kr 00
	0x03, // ascr_Kr 000000  ascr_Wg 11
	0xe0, // ascr_Wg 111000  ascr_Kg 00
	0x03, // ascr_Kg 000000  ascr_Wb 11
	0xbc, // ascr_Wb 101111  ascr_Kb 00
	0x00, // ascr_Kb 000000  reserved 00
	0x0f, // reserved 0000  vsync_mask 1111
	/* end */
};

static unsigned char DYNAMIC_VT_1[] = {
	/* start */
	0xBA, // Start offset 0x0982, base B1h
	0x03, // data_width 00  lce_module 0  lcd_bypass 0  lcd_roi 00  algo_module 1  algo_bypass 1
	0x30, // algo_roi 00  ascr_module 1  ascr_bypass 1  ascr_roi 00  roi_type 00
	0x00, // roi0_x_start 00000000
	0x00, // roi0_x_start 0000  roi0_x_end 0000
	0x00, // roi0_x_end 00000000
	0x00, // roi0_y_start 00000000
	0x00, // roi0_y_start 0000  roi0_y_end 0000
	0x00, // roi0_y_end 00000000
	0x00, // roi1_x_start 00000000
	0x00, // roi1_x_start 0000  roi1_x_end 0000
	0x00, // roi1_x_end 00000000
	0x00, // roi1_y_start 00000000
	0x00, // roi1_y_start 0000  roi1_y_end 0000
	0x00, // roi1_y_end 00000000
	0xa0, // hsize 10100000
	0x06, // hsize 0000  vsize 0110
	0x40, // vsize 01000000
	0x62, // gain 110000 cgain 10
	0x48, // cgain 0100  scene_on 1  scene_tran 000
	0x14, // scene_tran 0  min_diff 0010100
	0xb3, // illum_gain 10110011
	0x87, // ref_offset 10000111
	0x40, // ref_offset 0  ref_gain 1000000
	0x36, // ref_gain 00  hbsize 110  vbsize 110
	0xfa, // bth 11111010
	0x5a, // bin_size_ratio 0101101  dth 0
	0xe5, // dth 11  min_ref_offset 100101
	0xbb, // min_ref_offset 10  gamma 1  cs 1 sharpen 1 nr 0 mask_th 11
	0xfc, // mask_th 111111  sharpen_weight 00
	0x20, // sharpen_weight 00100000
	0x14, // max_plus 00010100
	0x02, // max_plus 000  max_minus 00010
	0x81, // max_minus 100000  cs_gain 01
	0x20, // cs_gain 00100000
	0x00, // curve_1_b 
	0x0f, // curve_1_a 
	0x00, // curve_2_b 
	0x0f, // curve_2_a 
	0x00, // curve_3_b 
	0x0f, // curve_3_a 
	0x00, // curve_4_b 
	0x0f, // curve_4_a 
	0x09, // curve_5_b 
	0xa2, // curve_5_a 
	0x09, // curve_6_b 
	0xa2, // curve_6_a 
	0x09, // curve_7_b 
	0xa2, // curve_7_a 
	0x09, // curve_8_b 
	0xa2, // curve_8_a 
	0x09, // curve_9_b 
	0xa2, // curve_9_a 
	0x09, // curve_10_b 
	0xa2, // curve_10_a 
	0x0a, // curve_11_b 
	0xa2, // curve_11_a 
	0x0a, // curve_12_b 
	0xa2, // curve_12_a 
	0x0a, // curve_13_b 
	0xa2, // curve_13_a 
	0x0a, // curve_14_b 
	0xa2, // curve_14_a 
	0x0a, // curve_15_b 
	0xa2, // curve_15_a 
	0x0a, // curve_16_b 
	0xa2, // curve_16_a 
	0x0a, // curve_17_b 
	0xa2, // curve_17_a 
	0x0a, // curve_18_b 
	0xa2, // curve_18_a 
	0x0f, // curve_19_b 
	0xa4, // curve_19_a 
	0x0f, // curve_20_b 
	0xa4, // curve_20_a 
	0x0f, // curve_21_b 
	0xa4, // curve_21_a 
	0x23, // curve_22_b 
	0x1c, // curve_22_a 
	0x48, // curve_23_b 
	0x17, // curve_23_a 
	0x00, // curve_24_b 
	0xff, // curve_24_a 
};

static unsigned char DYNAMIC_VT_2[] = {
	0xBA, // Start offset 0x09D4, base B1h
	0x05, // ascr_strength 00000  ascr_on 1  skin_cb 01
	0x9e, // skin_cb 100111  skin_cr 10
	0xa4, // skin_cr 101001  up_distance 00
	0x5c, // up_distance 010111  down_distance 00                                       
	0xa4, // down_distance 101001  right_distance 00                                    
	0x64, // right_distance 011001  left_distance 00                                    
	0x9c, // left_distance 100111  up_divided_distance 00
	0x16, // up_divided_distance 00010110
	0x42, // up_divided_distance 01000010
	0xc0, // up_divided_distance 11  down_divided_distance 000000
	0xc7, // down_divided_distance 11000111
	0xd0, // down_divided_distance 110100 right_divided_distance 00
	0x14, // right_divided_distance 00010100
	0x7b, // right_divided_distance 01111011
	0x00, // right_divided_distance 00  left_divided_distance 000000
	0xd2, // left_divided_distance 11010010
	0x0f, // left_divided_distance 000011  ascr_skin_Rr 11
	0xfc, // ascr_skin_Rr 111111  ascr_skin_Rg 00
	0x00, // ascr_skin_Rg 000000  ascr_skin_Rb 00
	0x03, // ascr_skin_Rb 000000  ascr_skin_Yr 11
	0xff, // ascr_skin_Yr 111111  ascr_skin_Yg 11
	0xfc, // ascr_skin_Yg 111111  ascr_skin_Yb 00
	0x03, // ascr_skin_Yb 000000  ascr_skin_Mr 11
	0xfc, // ascr_skin_Mr 111111  ascr_skin_Mg 00
	0x03, // ascr_skin_Mg 000000  ascr_skin_Mb 11
	0xff, // ascr_skin_Mb 111111  ascr_skin_Wr 11
	0xff, // ascr_skin_Wr 111111  ascr_skin_Wg 11
	0xff, // ascr_skin_Wg 111111  ascr_skin_Wb 11
	0xfc, // ascr_skin_Wb 111111  ascr_Cr 00
	0x03, // ascr_Cr 000000  ascr_Rr 11
	0xff, // ascr_Rr 111111  ascr_Cg 11
	0xfc, // ascr_Cg 111111  ascr_Rg 00
	0x03, // ascr_Rg 000000  ascr_Cb 11
	0xfc, // ascr_Cb 111111  ascr_Rb 00
	0x03, // ascr_Rb 000000  ascr_Mr 11
	0xfc, // ascr_Mr 111111  ascr_Gr 00
	0x00, // ascr_Gr 000000  ascr_Mg 00
	0x03, // ascr_Mg 000000  ascr_Gg 11
	0xff, // ascr_Gg 111111  ascr_Mb 11
	0xfc, // ascr_Mb 111111  ascr_Gb 00
	0x03, // ascr_Gb 000000  ascr_Yr 11
	0xfc, // ascr_Yr 111111  ascr_Br 00
	0x03, // ascr_Br 000000  ascr_Yg 11
	0xfc, // ascr_Yg 111111  ascr_Bg 00
	0x00, // ascr_Bg 000000  ascr_Yb 00
	0x03, // ascr_Yb 000000  ascr_Bb 11
	0xff, // ascr_Bb 111111  ascr_Wr 11
	0xfc, // ascr_Wr 111111  ascr_Kr 00
	0x03, // ascr_Kr 000000  ascr_Wg 11
	0xfc, // ascr_Wg 111111  ascr_Kg 00
	0x03, // ascr_Kg 000000  ascr_Wb 11
	0xfc, // ascr_Wb 111111  ascr_Kb 00
	0x00, // ascr_Kb 000000  reserved 00
	0x0f, // reserved 0000  vsync_mask 1111
	/* end */
};

static unsigned char MOVIE_VT_1[] = {
	/* start */
	0xBA, // Start offset 0x0982, base B1h
	0x03, // data_width 00  lce_module 0  lcd_bypass 0  lcd_roi 00  algo_module 1  algo_bypass 1
	0x30, // algo_roi 00  ascr_module 1  ascr_bypass 1  ascr_roi 00  roi_type 00
	0x00, // roi0_x_start 00000000
	0x00, // roi0_x_start 0000  roi0_x_end 0000
	0x00, // roi0_x_end 00000000
	0x00, // roi0_y_start 00000000
	0x00, // roi0_y_start 0000  roi0_y_end 0000
	0x00, // roi0_y_end 00000000
	0x00, // roi1_x_start 00000000
	0x00, // roi1_x_start 0000  roi1_x_end 0000
	0x00, // roi1_x_end 00000000
	0x00, // roi1_y_start 00000000
	0x00, // roi1_y_start 0000  roi1_y_end 0000
	0x00, // roi1_y_end 00000000
	0xa0, // hsize 10100000
	0x06, // hsize 0000  vsize 0110
	0x40, // vsize 01000000
	0x62, // gain 110000 cgain 10
	0x48, // cgain 0100  scene_on 1  scene_tran 000
	0x14, // scene_tran 0  min_diff 0010100
	0xb3, // illum_gain 10110011
	0x87, // ref_offset 10000111
	0x40, // ref_offset 0  ref_gain 1000000
	0x36, // ref_gain 00  hbsize 110  vbsize 110
	0xfa, // bth 11111010
	0x5a, // bin_size_ratio 0101101  dth 0
	0xe5, // dth 11  min_ref_offset 100101
	0x9b, // min_ref_offset 10  gamma 0  cs 1 sharpen 1 nr 0 mask_th 11
	0xfc, // mask_th 111111  sharpen_weight 00
	0x10, // sharpen_weight 00010000
	0x14, // max_plus 00010100
	0x02, // max_plus 000  max_minus 00010
	0x81, // max_minus 100000  cs_gain 01
	0x20, // cs_gain 00100000
	0x00, // curve_1_b
	0x20, // curve_1_a
	0x00, // curve_2_b
	0x20, // curve_2_a
	0x00, // curve_3_b
	0x20, // curve_3_a
	0x00, // curve_4_b
	0x20, // curve_4_a
	0x00, // curve_5_b
	0x20, // curve_5_a
	0x00, // curve_6_b
	0x20, // curve_6_a
	0x00, // curve_7_b
	0x20, // curve_7_a
	0x00, // curve_8_b
	0x20, // curve_8_a
	0x00, // curve_9_b
	0x20, // curve_9_a
	0x00, // curve_10_b
	0x20, // curve_10_a
	0x00, // curve_11_b
	0x20, // curve_11_a
	0x00, // curve_12_b
	0x20, // curve_12_a
	0x00, // curve_13_b
	0x20, // curve_13_a
	0x00, // curve_14_b
	0x20, // curve_14_a
	0x00, // curve_15_b
	0x20, // curve_15_a
	0x00, // curve_16_b
	0x20, // curve_16_a
	0x00, // curve_17_b
	0x20, // curve_17_a
	0x00, // curve_18_b
	0x20, // curve_18_a
	0x00, // curve_19_b
	0x20, // curve_19_a
	0x00, // curve_20_b
	0x20, // curve_20_a
	0x00, // curve_21_b
	0x20, // curve_21_a
	0x00, // curve_22_b
	0x20, // curve_22_a
	0x00, // curve_23_b
	0x20, // curve_23_a
	0x00, // curve_24_b
	0xff, // curve_24_a
};

static unsigned char MOVIE_VT_2[] = {
	0xBA, // Start offset 0x09D4, base B1h
	0x05, // ascr_strength 00000  ascr_on 1  skin_cb 01
	0x9e, // skin_cb 100111  skin_cr 10
	0xa4, // skin_cr 101001  up_distance 00
	0x5c, // up_distance 010111  down_distance 00                                       
	0xa4, // down_distance 101001  right_distance 00                                    
	0x64, // right_distance 011001  left_distance 00                                    
	0x9c, // left_distance 100111  up_divided_distance 00
	0x16, // up_divided_distance 00010110
	0x42, // up_divided_distance 01000010
	0xc0, // up_divided_distance 11  down_divided_distance 000000
	0xc7, // down_divided_distance 11000111
	0xd0, // down_divided_distance 110100 right_divided_distance 00
	0x14, // right_divided_distance 00010100
	0x7b, // right_divided_distance 01111011
	0x00, // right_divided_distance 00  left_divided_distance 000000
	0xd2, // left_divided_distance 11010010
	0x0f, // left_divided_distance 000011  ascr_skin_Rr 11
	0xfc, // ascr_skin_Rr 111111  ascr_skin_Rg 00
	0x00, // ascr_skin_Rg 000000  ascr_skin_Rb 00
	0x03, // ascr_skin_Rb 000000  ascr_skin_Yr 11
	0xff, // ascr_skin_Yr 111111  ascr_skin_Yg 11
	0xfc, // ascr_skin_Yg 111111  ascr_skin_Yb 00
	0x03, // ascr_skin_Yb 000000  ascr_skin_Mr 11
	0xfc, // ascr_skin_Mr 111111  ascr_skin_Mg 00
	0x03, // ascr_skin_Mg 000000  ascr_skin_Mb 11
	0xff, // ascr_skin_Mb 111111  ascr_skin_Wr 11
	0xff, // ascr_skin_Wr 111111  ascr_skin_Wg 11
	0xff, // ascr_skin_Wg 111111  ascr_skin_Wb 11
	0xfd, // ascr_skin_Wb 111111  ascr_Cr 01
	0xeb, // ascr_Cr 111010  ascr_Rr 11
	0xff, // ascr_Rr 111111  ascr_Cg 11
	0xfc, // ascr_Cg 111111  ascr_Rg 00
	0xaf, // ascr_Rg 101011  ascr_Cb 11
	0xc4, // ascr_Cb 110001  ascr_Rb 00
	0xb7, // ascr_Rb 101101  ascr_Mr 11
	0xfd, // ascr_Mr 111111  ascr_Gr 01
	0xa0, // ascr_Gr 101000  ascr_Mg 00
	0x57, // ascr_Mg 010101  ascr_Gg 11
	0xff, // ascr_Gg 111111  ascr_Mb 11
	0xe8, // ascr_Mb 111010  ascr_Gb 00
	0xc7, // ascr_Gb 110001  ascr_Yr 11
	0xe0, // ascr_Yr 111000  ascr_Br 00
	0xd3, // ascr_Br 110100  ascr_Yg 11
	0xfc, // ascr_Yg 111111  ascr_Bg 00
	0x81, // ascr_Bg 100000  ascr_Yb 01
	0x2f, // ascr_Yb 001011  ascr_Bb 11
	0xff, // ascr_Bb 111111  ascr_Wr 11
	0xfc, // ascr_Wr 111111  ascr_Kr 00
	0x03, // ascr_Kr 000000  ascr_Wg 11
	0xe0, // ascr_Wg 111000  ascr_Kg 00
	0x03, // ascr_Kg 000000  ascr_Wb 11
	0xbc, // ascr_Wb 101111  ascr_Kb 00
	0x00, // ascr_Kb 000000  reserved 00
	0x0f, // reserved 0000  vsync_mask 1111
	/* end */
};

static unsigned char BYPASS_1[] = {
	/* start */
	0xBA, // Start offset 0x0982, base B1h
	0x00, // data_width 00  lce_module 0  lcd_bypass 0  lcd_roi 00  algo_module 0  algo_bypass 0
	0x00, // algo_roi 00  ascr_module 0  ascr_bypass 0  ascr_roi 00  roi_type 00
	0x00, // roi0_x_start 00000000
	0x00, // roi0_x_start 0000  roi0_x_end 0000
	0x00, // roi0_x_end 00000000
	0x00, // roi0_y_start 00000000
	0x00, // roi0_y_start 0000  roi0_y_end 0000
	0x00, // roi0_y_end 00000000
	0x00, // roi1_x_start 00000000
	0x00, // roi1_x_start 0000  roi1_x_end 0000
	0x00, // roi1_x_end 00000000
	0x00, // roi1_y_start 00000000
	0x00, // roi1_y_start 0000  roi1_y_end 0000
	0x00, // roi1_y_end 00000000
	0xa0, // hsize 10100000
	0x06, // hsize 0000  vsize 0110
	0x40, // vsize 01000000
	0x62, // gain 110000 cgain 10
	0x48, // cgain 0100  scene_on 1  scene_tran 000
	0x14, // scene_tran 0  min_diff 0010100
	0xb3, // illum_gain 10110011
	0x87, // ref_offset 10000111
	0x40, // ref_offset 0  ref_gain 1000000
	0x36, // ref_gain 00  hbsize 110  vbsize 110
	0xfa, // bth 11111010
	0x5a, // bin_size_ratio 0101101  dth 0
	0xe5, // dth 11  min_ref_offset 100101
	0x83, // min_ref_offset 10  gamma 0  cs 0 sharpen 0 nr 0 mask_th 11
	0xfc, // mask_th 111111  sharpen_weight 00
	0x00, // sharpen_weight 00000000
	0xff, // max_plus 11111111
	0xff, // max_plus 111  max_minus 11111
	0xfd, // max_minus 111111  cs_gain 01
	0x00, // cs_gain 00000000
	0x00, // curve_1_b
	0x20, // curve_1_a
	0x00, // curve_2_b
	0x20, // curve_2_a
	0x00, // curve_3_b
	0x20, // curve_3_a
	0x00, // curve_4_b
	0x20, // curve_4_a
	0x00, // curve_5_b
	0x20, // curve_5_a
	0x00, // curve_6_b
	0x20, // curve_6_a
	0x00, // curve_7_b
	0x20, // curve_7_a
	0x00, // curve_8_b
	0x20, // curve_8_a
	0x00, // curve_9_b
	0x20, // curve_9_a
	0x00, // curve_10_b
	0x20, // curve_10_a
	0x00, // curve_11_b
	0x20, // curve_11_a
	0x00, // curve_12_b
	0x20, // curve_12_a
	0x00, // curve_13_b
	0x20, // curve_13_a
	0x00, // curve_14_b
	0x20, // curve_14_a
	0x00, // curve_15_b
	0x20, // curve_15_a
	0x00, // curve_16_b
	0x20, // curve_16_a
	0x00, // curve_17_b
	0x20, // curve_17_a
	0x00, // curve_18_b
	0x20, // curve_18_a
	0x00, // curve_19_b
	0x20, // curve_19_a
	0x00, // curve_20_b
	0x20, // curve_20_a
	0x00, // curve_21_b
	0x20, // curve_21_a
	0x00, // curve_22_b
	0x20, // curve_22_a
	0x00, // curve_23_b
	0x20, // curve_23_a
	0x00, // curve_24_b
	0xff, // curve_24_a
};

static unsigned char BYPASS_2[] = {
	0xBA, // Start offset 0x09D4, base B1h
	0x05, // ascr_strength 00000  ascr_on 1  skin_cb 01
	0x9e, // skin_cb 100111  skin_cr 10
	0xa4, // skin_cr 101001  up_distance 00
	0x30, // up_distance 001100  down_distance 00
	0x30, // down_distance 001100  right_distance 00
	0x30, // right_distance 001100  left_distance 00
	0x30, // left_distance 001100  up_divided_distance 00
	0x2a, // up_divided_distance 00101010
	0xaa, // up_divided_distance 10101010
	0xc2, // up_divided_distance 11  down_divided_distance 000010
	0xaa, // down_divided_distance 10101010
	0xac, // down_divided_distance 101011 right_divided_distance 00
	0x2a, // right_divided_distance 00101010
	0xaa, // right_divided_distance 10101010
	0xc2, // right_divided_distance 11  left_divided_distance 000010
	0xaa, // left_divided_distance 10101010
	0xaf, // left_divided_distance 101011  ascr_skin_Rr 11
	0xfc, // ascr_skin_Rr 111111  ascr_skin_Rg 00
	0x00, // ascr_skin_Rg 000000  ascr_skin_Rb 00
	0x03, // ascr_skin_Rb 000000  ascr_skin_Yr 11
	0xff, // ascr_skin_Yr 111111  ascr_skin_Yg 11
	0xfc, // ascr_skin_Yg 111111  ascr_skin_Yb 00
	0x03, // ascr_skin_Yb 000000  ascr_skin_Mr 11
	0xfc, // ascr_skin_Mr 111111  ascr_skin_Mg 00
	0x03, // ascr_skin_Mg 000000  ascr_skin_Mb 11
	0xff, // ascr_skin_Mb 111111  ascr_skin_Wr 11
	0xff, // ascr_skin_Wr 111111  ascr_skin_Wg 11
	0xff, // ascr_skin_Wg 111111  ascr_skin_Wb 11
	0xfc, // ascr_skin_Wb 111111  ascr_Cr 00
	0x03, // ascr_Cr 000000  ascr_Rr 11
	0xff, // ascr_Rr 111111  ascr_Cg 11
	0xfc, // ascr_Cg 111111  ascr_Rg 00
	0x03, // ascr_Rg 000000  ascr_Cb 11
	0xfc, // ascr_Cb 111111  ascr_Rb 00
	0x03, // ascr_Rb 000000  ascr_Mr 11
	0xfc, // ascr_Mr 111111  ascr_Gr 00
	0x00, // ascr_Gr 000000  ascr_Mg 00
	0x03, // ascr_Mg 000000  ascr_Gg 11
	0xff, // ascr_Gg 111111  ascr_Mb 11
	0xfc, // ascr_Mb 111111  ascr_Gb 00
	0x03, // ascr_Gb 000000  ascr_Yr 11
	0xfc, // ascr_Yr 111111  ascr_Br 00
	0x03, // ascr_Br 000000  ascr_Yg 11
	0xfc, // ascr_Yg 111111  ascr_Bg 00
	0x00, // ascr_Bg 000000  ascr_Yb 00
	0x03, // ascr_Yb 000000  ascr_Bb 11
	0xff, // ascr_Bb 111111  ascr_Wr 11
	0xfc, // ascr_Wr 111111  ascr_Kr 00
	0x03, // ascr_Kr 000000  ascr_Wg 11
	0xfc, // ascr_Wg 111111  ascr_Kg 00
	0x03, // ascr_Kg 000000  ascr_Wb 11
	0xfc, // ascr_Wb 111111  ascr_Kb 00
	0x00, // ascr_Kb 000000  reserved 00
	0x0f, // reserved 0000  vsync_mask 1111
	/* end */
};

static unsigned char AUTO_VT_1[] = {
	/* start */
	0xBA, // Start offset 0x0982, base B1h
	0x03, // data_width 00  lce_module 0  lcd_bypass 0  lcd_roi 00  algo_module 1  algo_bypass 1
	0x30, // algo_roi 00  ascr_module 1  ascr_bypass 1  ascr_roi 00  roi_type 00
	0x00, // roi0_x_start 00000000
	0x00, // roi0_x_start 0000  roi0_x_end 0000
	0x00, // roi0_x_end 00000000
	0x00, // roi0_y_start 00000000
	0x00, // roi0_y_start 0000  roi0_y_end 0000
	0x00, // roi0_y_end 00000000
	0x00, // roi1_x_start 00000000
	0x00, // roi1_x_start 0000  roi1_x_end 0000
	0x00, // roi1_x_end 00000000
	0x00, // roi1_y_start 00000000
	0x00, // roi1_y_start 0000  roi1_y_end 0000
	0x00, // roi1_y_end 00000000
	0xa0, // hsize 10100000
	0x06, // hsize 0000  vsize 0110
	0x40, // vsize 01000000
	0x62, // gain 110000 cgain 10
	0x48, // cgain 0100  scene_on 1  scene_tran 000
	0x14, // scene_tran 0  min_diff 0010100
	0xb3, // illum_gain 10110011
	0x87, // ref_offset 10000111
	0x40, // ref_offset 0  ref_gain 1000000
	0x36, // ref_gain 00  hbsize 110  vbsize 110
	0xfa, // bth 11111010
	0x5a, // bin_size_ratio 0101101  dth 0
	0xe5, // dth 11  min_ref_offset 100101
	0x8b, // min_ref_offset 10  gamma 0  cs 0 sharpen 1 nr 0 mask_th 11
	0xfc, // mask_th 111111  sharpen_weight 00
	0x15, // sharpen_weight 00010101
	0x14, // max_plus 00010100
	0x02, // max_plus 000  max_minus 00010
	0x81, // max_minus 100000  cs_gain 01
	0x00, // cs_gain 00000000
	0x00, // curve_1_b
	0x20, // curve_1_a
	0x00, // curve_2_b
	0x20, // curve_2_a
	0x00, // curve_3_b
	0x20, // curve_3_a
	0x00, // curve_4_b
	0x20, // curve_4_a
	0x00, // curve_5_b
	0x20, // curve_5_a
	0x00, // curve_6_b
	0x20, // curve_6_a
	0x00, // curve_7_b
	0x20, // curve_7_a
	0x00, // curve_8_b
	0x20, // curve_8_a
	0x00, // curve_9_b
	0x20, // curve_9_a
	0x00, // curve_10_b
	0x20, // curve_10_a
	0x00, // curve_11_b
	0x20, // curve_11_a
	0x00, // curve_12_b
	0x20, // curve_12_a
	0x00, // curve_13_b
	0x20, // curve_13_a
	0x00, // curve_14_b
	0x20, // curve_14_a
	0x00, // curve_15_b
	0x20, // curve_15_a
	0x00, // curve_16_b
	0x20, // curve_16_a
	0x00, // curve_17_b
	0x20, // curve_17_a
	0x00, // curve_18_b
	0x20, // curve_18_a
	0x00, // curve_19_b
	0x20, // curve_19_a
	0x00, // curve_20_b
	0x20, // curve_20_a
	0x00, // curve_21_b
	0x20, // curve_21_a
	0x00, // curve_22_b
	0x20, // curve_22_a
	0x00, // curve_23_b
	0x20, // curve_23_a
	0x00, // curve_24_b
	0xff, // curve_24_a
};

static unsigned char AUTO_VT_2[] = {
	0xBA, // Start offset 0x09D4, base B1h
	0x05, // ascr_strength 00000  ascr_on 1  skin_cb 01
	0x9e, // skin_cb 100111  skin_cr 10
	0xa4, // skin_cr 101001  up_distance 00
	0x5c, // up_distance 010111  down_distance 00                                       
	0xa4, // down_distance 101001  right_distance 00                                    
	0x64, // right_distance 011001  left_distance 00                                    
	0x9c, // left_distance 100111  up_divided_distance 00
	0x16, // up_divided_distance 00010110
	0x42, // up_divided_distance 01000010
	0xc0, // up_divided_distance 11  down_divided_distance 000000
	0xc7, // down_divided_distance 11000111
	0xd0, // down_divided_distance 110100 right_divided_distance 00
	0x14, // right_divided_distance 00010100
	0x7b, // right_divided_distance 01111011
	0x00, // right_divided_distance 00  left_divided_distance 000000
	0xd2, // left_divided_distance 11010010
	0x0f, // left_divided_distance 000011  ascr_skin_Rr 11
	0xfc, // ascr_skin_Rr 111111  ascr_skin_Rg 00
	0x00, // ascr_skin_Rg 000000  ascr_skin_Rb 00
	0x03, // ascr_skin_Rb 000000  ascr_skin_Yr 11
	0xff, // ascr_skin_Yr 111111  ascr_skin_Yg 11
	0xfc, // ascr_skin_Yg 111111  ascr_skin_Yb 00
	0x03, // ascr_skin_Yb 000000  ascr_skin_Mr 11
	0xfc, // ascr_skin_Mr 111111  ascr_skin_Mg 00
	0x03, // ascr_skin_Mg 000000  ascr_skin_Mb 11
	0xff, // ascr_skin_Mb 111111  ascr_skin_Wr 11
	0xff, // ascr_skin_Wr 111111  ascr_skin_Wg 11
	0xff, // ascr_skin_Wg 111111  ascr_skin_Wb 11
	0xfc, // ascr_skin_Wb 111111  ascr_Cr 00
	0x03, // ascr_Cr 000000  ascr_Rr 11
	0xff, // ascr_Rr 111111  ascr_Cg 11
	0xfc, // ascr_Cg 111111  ascr_Rg 00
	0x03, // ascr_Rg 000000  ascr_Cb 11
	0xfc, // ascr_Cb 111111  ascr_Rb 00
	0x03, // ascr_Rb 000000  ascr_Mr 11
	0xfc, // ascr_Mr 111111  ascr_Gr 00
	0x00, // ascr_Gr 000000  ascr_Mg 00
	0x03, // ascr_Mg 000000  ascr_Gg 11
	0xff, // ascr_Gg 111111  ascr_Mb 11
	0xfc, // ascr_Mb 111111  ascr_Gb 00
	0x03, // ascr_Gb 000000  ascr_Yr 11
	0xfc, // ascr_Yr 111111  ascr_Br 00
	0x03, // ascr_Br 000000  ascr_Yg 11
	0xfc, // ascr_Yg 111111  ascr_Bg 00
	0x00, // ascr_Bg 000000  ascr_Yb 00
	0x03, // ascr_Yb 000000  ascr_Bb 11
	0xff, // ascr_Bb 111111  ascr_Wr 11
	0xfc, // ascr_Wr 111111  ascr_Kr 00
	0x03, // ascr_Kr 000000  ascr_Wg 11
	0xfc, // ascr_Wg 111111  ascr_Kg 00
	0x03, // ascr_Kg 000000  ascr_Wb 11
	0xfc, // ascr_Wb 111111  ascr_Kb 00
	0x00, // ascr_Kb 000000  reserved 00
	0x0f, // reserved 0000  vsync_mask 1111
	/* end */
};

////////////////// CAMERA /////////////////////
static unsigned char CAMERA_1[] = {
	/* start */
	0xBA, // Start offset 0x0982, base B1h
	0x00, // data_width 00  lce_module 0  lcd_bypass 0  lcd_roi 00  algo_module 0  algo_bypass 0
	0x30, // algo_roi 00  ascr_module 1  ascr_bypass 1  ascr_roi 00  roi_type 00
	0x00, // roi0_x_start 00000000
	0x00, // roi0_x_start 0000  roi0_x_end 0000
	0x00, // roi0_x_end 00000000
	0x00, // roi0_y_start 00000000
	0x00, // roi0_y_start 0000  roi0_y_end 0000
	0x00, // roi0_y_end 00000000
	0x00, // roi1_x_start 00000000
	0x00, // roi1_x_start 0000  roi1_x_end 0000
	0x00, // roi1_x_end 00000000
	0x00, // roi1_y_start 00000000
	0x00, // roi1_y_start 0000  roi1_y_end 0000
	0x00, // roi1_y_end 00000000
	0xa0, // hsize 10100000
	0x06, // hsize 0000  vsize 0110
	0x40, // vsize 01000000
	0x62, // gain 110000 cgain 10
	0x48, // cgain 0100  scene_on 1  scene_tran 000
	0x14, // scene_tran 0  min_diff 0010100
	0xb3, // illum_gain 10110011
	0x87, // ref_offset 10000111
	0x40, // ref_offset 0  ref_gain 1000000
	0x36, // ref_gain 00  hbsize 110  vbsize 110
	0xfa, // bth 11111010
	0x5a, // bin_size_ratio 0101101  dth 0
	0xe5, // dth 11  min_ref_offset 100101
	0x83, // min_ref_offset 10  gamma 0  cs 0 sharpen 0 nr 0 mask_th 11
	0xfc, // mask_th 111111  sharpen_weight 00
	0x40, // sharpen_weight 01000000
	0x14, // max_plus 00010100
	0x02, // max_plus 000  max_minus 00010
	0x81, // max_minus 100000  cs_gain 01
	0x00, // cs_gain 00000000
	0x00, // curve_1_b
	0x20, // curve_1_a
	0x00, // curve_2_b
	0x20, // curve_2_a
	0x00, // curve_3_b
	0x20, // curve_3_a
	0x00, // curve_4_b
	0x20, // curve_4_a
	0x00, // curve_5_b
	0x20, // curve_5_a
	0x00, // curve_6_b
	0x20, // curve_6_a
	0x00, // curve_7_b
	0x20, // curve_7_a
	0x00, // curve_8_b
	0x20, // curve_8_a
	0x00, // curve_9_b
	0x20, // curve_9_a
	0x00, // curve_10_b
	0x20, // curve_10_a
	0x00, // curve_11_b
	0x20, // curve_11_a
	0x00, // curve_12_b
	0x20, // curve_12_a
	0x00, // curve_13_b
	0x20, // curve_13_a
	0x00, // curve_14_b
	0x20, // curve_14_a
	0x00, // curve_15_b
	0x20, // curve_15_a
	0x00, // curve_16_b
	0x20, // curve_16_a
	0x00, // curve_17_b
	0x20, // curve_17_a
	0x00, // curve_18_b
	0x20, // curve_18_a
	0x00, // curve_19_b
	0x20, // curve_19_a
	0x00, // curve_20_b
	0x20, // curve_20_a
	0x00, // curve_21_b
	0x20, // curve_21_a
	0x00, // curve_22_b
	0x20, // curve_22_a
	0x00, // curve_23_b
	0x20, // curve_23_a
	0x00, // curve_24_b
	0xff, // curve_24_a
};

static unsigned char CAMERA_2[] = {
	0xBA, // Start offset 0x09D4, base B1h
	0x05, // ascr_strength 00000  ascr_on 1  skin_cb 01
	0x9e, // skin_cb 100111  skin_cr 10
	0xa4, // skin_cr 101001  up_distance 00
	0x5c, // up_distance 010111  down_distance 00                                       
	0xa4, // down_distance 101001  right_distance 00                                    
	0x64, // right_distance 011001  left_distance 00                                    
	0x9c, // left_distance 100111  up_divided_distance 00
	0x16, // up_divided_distance 00010110
	0x42, // up_divided_distance 01000010
	0xc0, // up_divided_distance 11  down_divided_distance 000000
	0xc7, // down_divided_distance 11000111
	0xd0, // down_divided_distance 110100 right_divided_distance 00
	0x14, // right_divided_distance 00010100
	0x7b, // right_divided_distance 01111011
	0x00, // right_divided_distance 00  left_divided_distance 000000
	0xd2, // left_divided_distance 11010010
	0x0f, // left_divided_distance 000011  ascr_skin_Rr 11
	0xfc, // ascr_skin_Rr 111111  ascr_skin_Rg 00
	0x00, // ascr_skin_Rg 000000  ascr_skin_Rb 00
	0x03, // ascr_skin_Rb 000000  ascr_skin_Yr 11
	0xff, // ascr_skin_Yr 111111  ascr_skin_Yg 11
	0xfc, // ascr_skin_Yg 111111  ascr_skin_Yb 00
	0x03, // ascr_skin_Yb 000000  ascr_skin_Mr 11
	0xfc, // ascr_skin_Mr 111111  ascr_skin_Mg 00
	0x03, // ascr_skin_Mg 000000  ascr_skin_Mb 11
	0xff, // ascr_skin_Mb 111111  ascr_skin_Wr 11
	0xff, // ascr_skin_Wr 111111  ascr_skin_Wg 11
	0xff, // ascr_skin_Wg 111111  ascr_skin_Wb 11
	0xfc, // ascr_skin_Wb 111111  ascr_Cr 00
	0x03, // ascr_Cr 000000  ascr_Rr 11
	0xff, // ascr_Rr 111111  ascr_Cg 11
	0xfc, // ascr_Cg 111111  ascr_Rg 00
	0x03, // ascr_Rg 000000  ascr_Cb 11
	0xfc, // ascr_Cb 111111  ascr_Rb 00
	0x03, // ascr_Rb 000000  ascr_Mr 11
	0xfc, // ascr_Mr 111111  ascr_Gr 00
	0x00, // ascr_Gr 000000  ascr_Mg 00
	0x03, // ascr_Mg 000000  ascr_Gg 11
	0xff, // ascr_Gg 111111  ascr_Mb 11
	0xfc, // ascr_Mb 111111  ascr_Gb 00
	0x03, // ascr_Gb 000000  ascr_Yr 11
	0xfc, // ascr_Yr 111111  ascr_Br 00
	0x03, // ascr_Br 000000  ascr_Yg 11
	0xfc, // ascr_Yg 111111  ascr_Bg 00
	0x00, // ascr_Bg 000000  ascr_Yb 00
	0x03, // ascr_Yb 000000  ascr_Bb 11
	0xff, // ascr_Bb 111111  ascr_Wr 11
	0xfc, // ascr_Wr 111111  ascr_Kr 00
	0x03, // ascr_Kr 000000  ascr_Wg 11
	0xfc, // ascr_Wg 111111  ascr_Kg 00
	0x03, // ascr_Kg 000000  ascr_Wb 11
	0xfc, // ascr_Wb 111111  ascr_Kb 00
	0x00, // ascr_Kb 000000  reserved 00
	0x0f, // reserved 0000  vsync_mask 1111
	/* end */
};

static unsigned char AUTO_CAMERA_1[] = {
	/* start */
	0xBA, // Start offset 0x0982, base B1h
	0x00, // data_width 00  lce_module 0  lcd_bypass 0  lcd_roi 00  algo_module 0  algo_bypass 0
	0x30, // algo_roi 00  ascr_module 1  ascr_bypass 1  ascr_roi 00  roi_type 00
	0x00, // roi0_x_start 00000000
	0x00, // roi0_x_start 0000  roi0_x_end 0000
	0x00, // roi0_x_end 00000000
	0x00, // roi0_y_start 00000000
	0x00, // roi0_y_start 0000  roi0_y_end 0000
	0x00, // roi0_y_end 00000000
	0x00, // roi1_x_start 00000000
	0x00, // roi1_x_start 0000  roi1_x_end 0000
	0x00, // roi1_x_end 00000000
	0x00, // roi1_y_start 00000000
	0x00, // roi1_y_start 0000  roi1_y_end 0000
	0x00, // roi1_y_end 00000000
	0xa0, // hsize 10100000
	0x06, // hsize 0000  vsize 0110
	0x40, // vsize 01000000
	0x62, // gain 110000 cgain 10
	0x48, // cgain 0100  scene_on 1  scene_tran 000
	0x14, // scene_tran 0  min_diff 0010100
	0xb3, // illum_gain 10110011
	0x87, // ref_offset 10000111
	0x40, // ref_offset 0  ref_gain 1000000
	0x36, // ref_gain 00  hbsize 110  vbsize 110
	0xfa, // bth 11111010
	0x5a, // bin_size_ratio 0101101  dth 0
	0xe5, // dth 11  min_ref_offset 100101
	0x83, // min_ref_offset 10  gamma 0  cs 0 sharpen 0 nr 0 mask_th 11
	0xfc, // mask_th 111111  sharpen_weight 00
	0x40, // sharpen_weight 01000000
	0x14, // max_plus 00010100
	0x02, // max_plus 000  max_minus 00010
	0x81, // max_minus 100000  cs_gain 01
	0x00, // cs_gain 00000000
	0x00, // curve_1_b
	0x20, // curve_1_a
	0x00, // curve_2_b
	0x20, // curve_2_a
	0x00, // curve_3_b
	0x20, // curve_3_a
	0x00, // curve_4_b
	0x20, // curve_4_a
	0x00, // curve_5_b
	0x20, // curve_5_a
	0x00, // curve_6_b
	0x20, // curve_6_a
	0x00, // curve_7_b
	0x20, // curve_7_a
	0x00, // curve_8_b
	0x20, // curve_8_a
	0x00, // curve_9_b
	0x20, // curve_9_a
	0x00, // curve_10_b
	0x20, // curve_10_a
	0x00, // curve_11_b
	0x20, // curve_11_a
	0x00, // curve_12_b
	0x20, // curve_12_a
	0x00, // curve_13_b
	0x20, // curve_13_a
	0x00, // curve_14_b
	0x20, // curve_14_a
	0x00, // curve_15_b
	0x20, // curve_15_a
	0x00, // curve_16_b
	0x20, // curve_16_a
	0x00, // curve_17_b
	0x20, // curve_17_a
	0x00, // curve_18_b
	0x20, // curve_18_a
	0x00, // curve_19_b
	0x20, // curve_19_a
	0x00, // curve_20_b
	0x20, // curve_20_a
	0x00, // curve_21_b
	0x20, // curve_21_a
	0x00, // curve_22_b
	0x20, // curve_22_a
	0x00, // curve_23_b
	0x20, // curve_23_a
	0x00, // curve_24_b
	0xff, // curve_24_a
};

static unsigned char AUTO_CAMERA_2[] = {
	0xBA, // Start offset 0x09D4, base B1h
	0x65, // ascr_strength 01100  ascr_on 1  skin_cb 01
	0x9e, // skin_cb 100111  skin_cr 10
	0xa4, // skin_cr 101001  up_distance 00
	0x5c, // up_distance 010111  down_distance 00                                       
	0xa4, // down_distance 101001  right_distance 00                                    
	0x64, // right_distance 011001  left_distance 00                                    
	0x9c, // left_distance 100111  up_divided_distance 00
	0x16, // up_divided_distance 00010110
	0x42, // up_divided_distance 01000010
	0xc0, // up_divided_distance 11  down_divided_distance 000000
	0xc7, // down_divided_distance 11000111
	0xd0, // down_divided_distance 110100 right_divided_distance 00
	0x14, // right_divided_distance 00010100
	0x7b, // right_divided_distance 01111011
	0x00, // right_divided_distance 00  left_divided_distance 000000
	0xd2, // left_divided_distance 11010010
	0x0f, // left_divided_distance 000011  ascr_skin_Rr 11
	0x91, // ascr_skin_Rr 100100  ascr_skin_Rg 01
	0x01, // ascr_skin_Rg 000000  ascr_skin_Rb 01
	0x02, // ascr_skin_Rb 000000  ascr_skin_Yr 10
	0xff, // ascr_skin_Yr 111111  ascr_skin_Yg 11
	0xfc, // ascr_skin_Yg 111111  ascr_skin_Yb 00
	0x02, // ascr_skin_Yb 000000  ascr_skin_Mr 10
	0xfc, // ascr_skin_Mr 111111  ascr_skin_Mg 00
	0x03, // ascr_skin_Mg 000000  ascr_skin_Mb 11
	0xff, // ascr_skin_Mb 111111  ascr_skin_Wr 11
	0xff, // ascr_skin_Wr 111111  ascr_skin_Wg 11
	0xff, // ascr_skin_Wg 111111  ascr_skin_Wb 11
	0xfc, // ascr_skin_Wb 111111  ascr_Cr 00
	0x03, // ascr_Cr 000000  ascr_Rr 11
	0xff, // ascr_Rr 111111  ascr_Cg 11
	0xfc, // ascr_Cg 111111  ascr_Rg 00
	0x03, // ascr_Rg 000000  ascr_Cb 11
	0xfc, // ascr_Cb 111111  ascr_Rb 00
	0x03, // ascr_Rb 000000  ascr_Mr 11
	0xfc, // ascr_Mr 111111  ascr_Gr 00
	0x00, // ascr_Gr 000000  ascr_Mg 00
	0x03, // ascr_Mg 000000  ascr_Gg 11
	0xff, // ascr_Gg 111111  ascr_Mb 11
	0xfc, // ascr_Mb 111111  ascr_Gb 00
	0x03, // ascr_Gb 000000  ascr_Yr 11
	0xfc, // ascr_Yr 111111  ascr_Br 00
	0x03, // ascr_Br 000000  ascr_Yg 11
	0xfc, // ascr_Yg 111111  ascr_Bg 00
	0x00, // ascr_Bg 000000  ascr_Yb 00
	0x03, // ascr_Yb 000000  ascr_Bb 11
	0xff, // ascr_Bb 111111  ascr_Wr 11
	0xfc, // ascr_Wr 111111  ascr_Kr 00
	0x03, // ascr_Kr 000000  ascr_Wg 11
	0xfc, // ascr_Wg 111111  ascr_Kg 00
	0x03, // ascr_Kg 000000  ascr_Wb 11
	0xfc, // ascr_Wb 111111  ascr_Kb 00
	0x00, // ascr_Kb 000000  reserved 00
	0x0f, // reserved 0000  vsync_mask 1111
	/* end */
};

static unsigned char NEGATIVE_1[] = {
	/* start */
	0xBA, // Start offset 0x0982, base B1h
	0x00, // data_width 00  lce_module 0  lcd_bypass 0  lcd_roi 00  algo_module 0  algo_bypass 0
	0x30, // algo_roi 00  ascr_module 1  ascr_bypass 1  ascr_roi 00  roi_type 00
	0x00, // roi0_x_start 00000000
	0x00, // roi0_x_start 0000  roi0_x_end 0000
	0x00, // roi0_x_end 00000000
	0x00, // roi0_y_start 00000000
	0x00, // roi0_y_start 0000  roi0_y_end 0000
	0x00, // roi0_y_end 00000000
	0x00, // roi1_x_start 00000000
	0x00, // roi1_x_start 0000  roi1_x_end 0000
	0x00, // roi1_x_end 00000000
	0x00, // roi1_y_start 00000000
	0x00, // roi1_y_start 0000  roi1_y_end 0000
	0x00, // roi1_y_end 00000000
	0xa0, // hsize 10100000
	0x06, // hsize 0000  vsize 0110
	0x40, // vsize 01000000
	0x62, // gain 110000 cgain 10
	0x48, // cgain 0100  scene_on 1  scene_tran 000
	0x14, // scene_tran 0  min_diff 0010100
	0xb3, // illum_gain 10110011
	0x87, // ref_offset 10000111
	0x40, // ref_offset 0  ref_gain 1000000
	0x36, // ref_gain 00  hbsize 110  vbsize 110
	0xfa, // bth 11111010
	0x5a, // bin_size_ratio 0101101  dth 0
	0xe5, // dth 11  min_ref_offset 100101
	0x83, // min_ref_offset 10  gamma 0  cs 0 sharpen 0 nr 0 mask_th 11
	0xfc, // mask_th 111111  sharpen_weight 00
	0x00, // sharpen_weight 00000000
	0xff, // max_plus 11111111
	0xff, // max_plus 111  max_minus 11111
	0xfd, // max_minus 111111  cs_gain 01
	0x00, // cs_gain 00000000
	0x00, // curve_1_b
	0x20, // curve_1_a
	0x00, // curve_2_b
	0x20, // curve_2_a
	0x00, // curve_3_b
	0x20, // curve_3_a
	0x00, // curve_4_b
	0x20, // curve_4_a
	0x00, // curve_5_b
	0x20, // curve_5_a
	0x00, // curve_6_b
	0x20, // curve_6_a
	0x00, // curve_7_b
	0x20, // curve_7_a
	0x00, // curve_8_b
	0x20, // curve_8_a
	0x00, // curve_9_b
	0x20, // curve_9_a
	0x00, // curve_10_b
	0x20, // curve_10_a
	0x00, // curve_11_b
	0x20, // curve_11_a
	0x00, // curve_12_b
	0x20, // curve_12_a
	0x00, // curve_13_b
	0x20, // curve_13_a
	0x00, // curve_14_b
	0x20, // curve_14_a
	0x00, // curve_15_b
	0x20, // curve_15_a
	0x00, // curve_16_b
	0x20, // curve_16_a
	0x00, // curve_17_b
	0x20, // curve_17_a
	0x00, // curve_18_b
	0x20, // curve_18_a
	0x00, // curve_19_b
	0x20, // curve_19_a
	0x00, // curve_20_b
	0x20, // curve_20_a
	0x00, // curve_21_b
	0x20, // curve_21_a
	0x00, // curve_22_b
	0x20, // curve_22_a
	0x00, // curve_23_b
	0x20, // curve_23_a
	0x00, // curve_24_b
	0xff, // curve_24_a
};

static unsigned char NEGATIVE_2[] = {
	0xBA, // Start offset 0x09D4, base B1h
	0x05, // ascr_strength 00000  ascr_on 1  skin_cb 01
	0x9e, // skin_cb 100111  skin_cr 10
	0xa4, // skin_cr 101001  up_distance 00
	0x30, // up_distance 001100  down_distance 00
	0x30, // down_distance 001100  right_distance 00
	0x30, // right_distance 001100  left_distance 00
	0x30, // left_distance 001100  up_divided_distance 00
	0x2a, // up_divided_distance 00101010
	0xaa, // up_divided_distance 10101010
	0xc2, // up_divided_distance 11  down_divided_distance 000010
	0xaa, // down_divided_distance 10101010
	0xac, // down_divided_distance 101011 right_divided_distance 00
	0x2a, // right_divided_distance 00101010
	0xaa, // right_divided_distance 10101010
	0xc2, // right_divided_distance 11  left_divided_distance 000010
	0xaa, // left_divided_distance 10101010
	0xac, // left_divided_distance 101011  ascr_skin_Rr 00
	0x03, // ascr_skin_Rr 000000  ascr_skin_Rg 11
	0xff, // ascr_skin_Rg 111111  ascr_skin_Rb 11
	0xfc, // ascr_skin_Rb 111111  ascr_skin_Yr 00
	0x00, // ascr_skin_Yr 000000  ascr_skin_Yg 00
	0x03, // ascr_skin_Yg 000000  ascr_skin_Yb 11
	0xfc, // ascr_skin_Yb 111111  ascr_skin_Mr 00
	0x03, // ascr_skin_Mr 000000  ascr_skin_Mg 11
	0xfc, // ascr_skin_Mg 111111  ascr_skin_Mb 00
	0x00, // ascr_skin_Mb 000000  ascr_skin_Wr 00
	0x00, // ascr_skin_Wr 000000  ascr_skin_Wg 00
	0x00, // ascr_skin_Wg 000000  ascr_skin_Wb 00
	0x03, // ascr_skin_Wb 000000  ascr_Cr 11
	0xfc, // ascr_Cr 111111  ascr_Rr 00
	0x00, // ascr_Rr 000000  ascr_Cg 00
	0x03, // ascr_Cg 000000  ascr_Rg 11
	0xfc, // ascr_Rg 111111  ascr_Cb 00
	0x03, // ascr_Cb 000000  ascr_Rb 11
	0xfc, // ascr_Rb 111111  ascr_Mr 00
	0x03, // ascr_Mr 000000  ascr_Gr 11
	0xff, // ascr_Gr 111111  ascr_Mg 11
	0xfc, // ascr_Mg 111111  ascr_Gg 00
	0x00, // ascr_Gg 000000  ascr_Mb 00
	0x03, // ascr_Mb 000000  ascr_Gb 11
	0xfc, // ascr_Gb 111111  ascr_Yr 00
	0x03, // ascr_Yr 000000  ascr_Br 11
	0xfc, // ascr_Br 111111  ascr_Yg 00
	0x03, // ascr_Yg 000000  ascr_Bg 11
	0xff, // ascr_Bg 111111  ascr_Yb 11
	0xfc, // ascr_Yb 111111  ascr_Bb 00
	0x00, // ascr_Bb 000000  ascr_Wr 00
	0x03, // ascr_Wr 000000  ascr_Kr 11
	0xfc, // ascr_Kr 111111  ascr_Wg 00
	0x03, // ascr_Wg 000000  ascr_Kg 11
	0xfc, // ascr_Kg 111111  ascr_Wb 00
	0x03, // ascr_Wb 000000  ascr_Kb 11
	0xfc, // ascr_Kb 111111  reserved 00
	0x0f, // reserved 0000  vsync_mask 1111
	/* end */
};

static unsigned char COLOR_BLIND_1[] = {
	/* start */
	0xBA, // Start offset 0x0982, base B1h
	0x00, // data_width 00  lce_module 0  lcd_bypass 0  lcd_roi 00  algo_module 0  algo_bypass 0
	0x30, // algo_roi 00  ascr_module 1  ascr_bypass 1  ascr_roi 00  roi_type 00
	0x00, // roi0_x_start 00000000
	0x00, // roi0_x_start 0000  roi0_x_end 0000
	0x00, // roi0_x_end 00000000
	0x00, // roi0_y_start 00000000
	0x00, // roi0_y_start 0000  roi0_y_end 0000
	0x00, // roi0_y_end 00000000
	0x00, // roi1_x_start 00000000
	0x00, // roi1_x_start 0000  roi1_x_end 0000
	0x00, // roi1_x_end 00000000
	0x00, // roi1_y_start 00000000
	0x00, // roi1_y_start 0000  roi1_y_end 0000
	0x00, // roi1_y_end 00000000
	0xa0, // hsize 10100000
	0x06, // hsize 0000  vsize 0110
	0x40, // vsize 01000000
	0x62, // gain 110000 cgain 10
	0x48, // cgain 0100  scene_on 1  scene_tran 000
	0x14, // scene_tran 0  min_diff 0010100
	0xb3, // illum_gain 10110011
	0x87, // ref_offset 10000111
	0x40, // ref_offset 0  ref_gain 1000000
	0x36, // ref_gain 00  hbsize 110  vbsize 110
	0xfa, // bth 11111010
	0x5a, // bin_size_ratio 0101101  dth 0
	0xe5, // dth 11  min_ref_offset 100101
	0x83, // min_ref_offset 10  gamma 0  cs 0 sharpen 0 nr 0 mask_th 11
	0xfc, // mask_th 111111  sharpen_weight 00
	0x00, // sharpen_weight 00000000
	0xff, // max_plus 11111111
	0xff, // max_plus 111  max_minus 11111
	0xfd, // max_minus 111111  cs_gain 01
	0x00, // cs_gain 00000000
	0x00, // curve_1_b
	0x20, // curve_1_a
	0x00, // curve_2_b
	0x20, // curve_2_a
	0x00, // curve_3_b
	0x20, // curve_3_a
	0x00, // curve_4_b
	0x20, // curve_4_a
	0x00, // curve_5_b
	0x20, // curve_5_a
	0x00, // curve_6_b
	0x20, // curve_6_a
	0x00, // curve_7_b
	0x20, // curve_7_a
	0x00, // curve_8_b
	0x20, // curve_8_a
	0x00, // curve_9_b
	0x20, // curve_9_a
	0x00, // curve_10_b
	0x20, // curve_10_a
	0x00, // curve_11_b
	0x20, // curve_11_a
	0x00, // curve_12_b
	0x20, // curve_12_a
	0x00, // curve_13_b
	0x20, // curve_13_a
	0x00, // curve_14_b
	0x20, // curve_14_a
	0x00, // curve_15_b
	0x20, // curve_15_a
	0x00, // curve_16_b
	0x20, // curve_16_a
	0x00, // curve_17_b
	0x20, // curve_17_a
	0x00, // curve_18_b
	0x20, // curve_18_a
	0x00, // curve_19_b
	0x20, // curve_19_a
	0x00, // curve_20_b
	0x20, // curve_20_a
	0x00, // curve_21_b
	0x20, // curve_21_a
	0x00, // curve_22_b
	0x20, // curve_22_a
	0x00, // curve_23_b
	0x20, // curve_23_a
	0x00, // curve_24_b
	0xff, // curve_24_a
};

static unsigned char COLOR_BLIND_2[] = {
	0xBA, // Start offset 0x09D4, base B1h
	0x05, // ascr_strength 00000  ascr_on 1  skin_cb 01
	0x9e, // skin_cb 100111  skin_cr 10
	0xa4, // skin_cr 101001  up_distance 00
	0x30, // up_distance 001100  down_distance 00
	0x30, // down_distance 001100  right_distance 00
	0x30, // right_distance 001100  left_distance 00
	0x30, // left_distance 001100  up_divided_distance 00
	0x2a, // up_divided_distance 00101010
	0xaa, // up_divided_distance 10101010
	0xc2, // up_divided_distance 11  down_divided_distance 000010
	0xaa, // down_divided_distance 10101010
	0xac, // down_divided_distance 101011 right_divided_distance 00
	0x2a, // right_divided_distance 00101010
	0xaa, // right_divided_distance 10101010
	0xc2, // right_divided_distance 11  left_divided_distance 000010
	0xaa, // left_divided_distance 10101010
	0xaf, // left_divided_distance 101011  ascr_skin_Rr 11
	0xfc, // ascr_skin_Rr 111111  ascr_skin_Rg 00
	0x00, // ascr_skin_Rg 000000  ascr_skin_Rb 00
	0x03, // ascr_skin_Rb 000000  ascr_skin_Yr 11
	0xff, // ascr_skin_Yr 111111  ascr_skin_Yg 11
	0xfc, // ascr_skin_Yg 111111  ascr_skin_Yb 00
	0x03, // ascr_skin_Yb 000000  ascr_skin_Mr 11
	0xfc, // ascr_skin_Mr 111111  ascr_skin_Mg 00
	0x03, // ascr_skin_Mg 000000  ascr_skin_Mb 11
	0xff, // ascr_skin_Mb 111111  ascr_skin_Wr 11
	0xff, // ascr_skin_Wr 111111  ascr_skin_Wg 11
	0xff, // ascr_skin_Wg 111111  ascr_skin_Wb 11
	0xfc, // ascr_skin_Wb 111111  ascr_Cr 00
	0x03, // ascr_Cr 000000  ascr_Rr 11
	0xff, // ascr_Rr 111111  ascr_Cg 11
	0xfc, // ascr_Cg 111111  ascr_Rg 00
	0x03, // ascr_Rg 000000  ascr_Cb 11
	0xfc, // ascr_Cb 111111  ascr_Rb 00
	0x03, // ascr_Rb 000000  ascr_Mr 11
	0xfc, // ascr_Mr 111111  ascr_Gr 00
	0x00, // ascr_Gr 000000  ascr_Mg 00
	0x03, // ascr_Mg 000000  ascr_Gg 11
	0xff, // ascr_Gg 111111  ascr_Mb 11
	0xfc, // ascr_Mb 111111  ascr_Gb 00
	0x03, // ascr_Gb 000000  ascr_Yr 11
	0xfc, // ascr_Yr 111111  ascr_Br 00
	0x03, // ascr_Br 000000  ascr_Yg 11
	0xfc, // ascr_Yg 111111  ascr_Bg 00
	0x00, // ascr_Bg 000000  ascr_Yb 00
	0x03, // ascr_Yb 000000  ascr_Bb 11
	0xff, // ascr_Bb 111111  ascr_Wr 11
	0xfc, // ascr_Wr 111111  ascr_Kr 00
	0x03, // ascr_Kr 000000  ascr_Wg 11
	0xfc, // ascr_Wg 111111  ascr_Kg 00
	0x03, // ascr_Kg 000000  ascr_Wb 11
	0xfc, // ascr_Wb 111111  ascr_Kb 00
	0x00, // ascr_Kb 000000  reserved 00
	0x0f, // reserved 0000  vsync_mask 1111
	/* end */
};

////////////////// BROWSER /////////////////////
static unsigned char STANDARD_BROWSER_1[] = {
	/* start */
	0xBA, // Start offset 0x0982, base B1h
	0x00, // data_width 00  lce_module 0  lcd_bypass 0  lcd_roi 00  algo_module 0  algo_bypass 0
	0x30, // algo_roi 00  ascr_module 1  ascr_bypass 1  ascr_roi 00  roi_type 00
	0x00, // roi0_x_start 00000000
	0x00, // roi0_x_start 0000  roi0_x_end 0000
	0x00, // roi0_x_end 00000000
	0x00, // roi0_y_start 00000000
	0x00, // roi0_y_start 0000  roi0_y_end 0000
	0x00, // roi0_y_end 00000000
	0x00, // roi1_x_start 00000000
	0x00, // roi1_x_start 0000  roi1_x_end 0000
	0x00, // roi1_x_end 00000000
	0x00, // roi1_y_start 00000000
	0x00, // roi1_y_start 0000  roi1_y_end 0000
	0x00, // roi1_y_end 00000000
	0xa0, // hsize 10100000
	0x06, // hsize 0000  vsize 0110
	0x40, // vsize 01000000
	0x62, // gain 110000 cgain 10
	0x48, // cgain 0100  scene_on 1  scene_tran 000
	0x14, // scene_tran 0  min_diff 0010100
	0xb3, // illum_gain 10110011
	0x87, // ref_offset 10000111
	0x40, // ref_offset 0  ref_gain 1000000
	0x36, // ref_gain 00  hbsize 110  vbsize 110
	0xfa, // bth 11111010
	0x5a, // bin_size_ratio 0101101  dth 0
	0xe5, // dth 11  min_ref_offset 100101
	0x83, // min_ref_offset 10  gamma 0  cs 0 sharpen 0 nr 0 mask_th 11
	0xfc, // mask_th 111111  sharpen_weight 00
	0x40, // sharpen_weight 01000000
	0x14, // max_plus 00010100
	0x02, // max_plus 000  max_minus 00010
	0x81, // max_minus 100000  cs_gain 01
	0x00, // cs_gain 00000000
	0x00, // curve_1_b
	0x20, // curve_1_a
	0x00, // curve_2_b
	0x20, // curve_2_a
	0x00, // curve_3_b
	0x20, // curve_3_a
	0x00, // curve_4_b
	0x20, // curve_4_a
	0x00, // curve_5_b
	0x20, // curve_5_a
	0x00, // curve_6_b
	0x20, // curve_6_a
	0x00, // curve_7_b
	0x20, // curve_7_a
	0x00, // curve_8_b
	0x20, // curve_8_a
	0x00, // curve_9_b
	0x20, // curve_9_a
	0x00, // curve_10_b
	0x20, // curve_10_a
	0x00, // curve_11_b
	0x20, // curve_11_a
	0x00, // curve_12_b
	0x20, // curve_12_a
	0x00, // curve_13_b
	0x20, // curve_13_a
	0x00, // curve_14_b
	0x20, // curve_14_a
	0x00, // curve_15_b
	0x20, // curve_15_a
	0x00, // curve_16_b
	0x20, // curve_16_a
	0x00, // curve_17_b
	0x20, // curve_17_a
	0x00, // curve_18_b
	0x20, // curve_18_a
	0x00, // curve_19_b
	0x20, // curve_19_a
	0x00, // curve_20_b
	0x20, // curve_20_a
	0x00, // curve_21_b
	0x20, // curve_21_a
	0x00, // curve_22_b
	0x20, // curve_22_a
	0x00, // curve_23_b
	0x20, // curve_23_a
	0x00, // curve_24_b
	0xff, // curve_24_a
};

static unsigned char STANDARD_BROWSER_2[] = {
	0xBA, // Start offset 0x09D4, base B1h
	0x05, // ascr_strength 00000  ascr_on 1  skin_cb 01
	0x9e, // skin_cb 100111  skin_cr 10
	0xa4, // skin_cr 101001  up_distance 00
	0x5c, // up_distance 010111  down_distance 00                                       
	0xa4, // down_distance 101001  right_distance 00                                    
	0x64, // right_distance 011001  left_distance 00                                    
	0x9c, // left_distance 100111  up_divided_distance 00
	0x16, // up_divided_distance 00010110
	0x42, // up_divided_distance 01000010
	0xc0, // up_divided_distance 11  down_divided_distance 000000
	0xc7, // down_divided_distance 11000111
	0xd0, // down_divided_distance 110100 right_divided_distance 00
	0x14, // right_divided_distance 00010100
	0x7b, // right_divided_distance 01111011
	0x00, // right_divided_distance 00  left_divided_distance 000000
	0xd2, // left_divided_distance 11010010
	0x0f, // left_divided_distance 000011  ascr_skin_Rr 11
	0xfc, // ascr_skin_Rr 111111  ascr_skin_Rg 00
	0x00, // ascr_skin_Rg 000000  ascr_skin_Rb 00
	0x03, // ascr_skin_Rb 000000  ascr_skin_Yr 11
	0xff, // ascr_skin_Yr 111111  ascr_skin_Yg 11
	0xfc, // ascr_skin_Yg 111111  ascr_skin_Yb 00
	0x03, // ascr_skin_Yb 000000  ascr_skin_Mr 11
	0xfc, // ascr_skin_Mr 111111  ascr_skin_Mg 00
	0x03, // ascr_skin_Mg 000000  ascr_skin_Mb 11
	0xff, // ascr_skin_Mb 111111  ascr_skin_Wr 11
	0xff, // ascr_skin_Wr 111111  ascr_skin_Wg 11
	0xff, // ascr_skin_Wg 111111  ascr_skin_Wb 11
	0xfc, // ascr_skin_Wb 111111  ascr_Cr 00
	0x03, // ascr_Cr 000000  ascr_Rr 11
	0xff, // ascr_Rr 111111  ascr_Cg 11
	0xfc, // ascr_Cg 111111  ascr_Rg 00
	0x03, // ascr_Rg 000000  ascr_Cb 11
	0xfc, // ascr_Cb 111111  ascr_Rb 00
	0x03, // ascr_Rb 000000  ascr_Mr 11
	0xfc, // ascr_Mr 111111  ascr_Gr 00
	0x00, // ascr_Gr 000000  ascr_Mg 00
	0x03, // ascr_Mg 000000  ascr_Gg 11
	0xff, // ascr_Gg 111111  ascr_Mb 11
	0xfc, // ascr_Mb 111111  ascr_Gb 00
	0x03, // ascr_Gb 000000  ascr_Yr 11
	0xfc, // ascr_Yr 111111  ascr_Br 00
	0x03, // ascr_Br 000000  ascr_Yg 11
	0xfc, // ascr_Yg 111111  ascr_Bg 00
	0x00, // ascr_Bg 000000  ascr_Yb 00
	0x03, // ascr_Yb 000000  ascr_Bb 11
	0xff, // ascr_Bb 111111  ascr_Wr 11
	0xfc, // ascr_Wr 111111  ascr_Kr 00
	0x03, // ascr_Kr 000000  ascr_Wg 11
	0xfc, // ascr_Wg 111111  ascr_Kg 00
	0x03, // ascr_Kg 000000  ascr_Wb 11
	0xfc, // ascr_Wb 111111  ascr_Kb 00
	0x00, // ascr_Kb 000000  reserved 00
	0x0f, // reserved 0000  vsync_mask 1111
	/* end */
};

static unsigned char NATURAL_BROWSER_1[] = {
	/* start */
	0xBA, // Start offset 0x0982, base B1h
	0x00, // data_width 00  lce_module 0  lcd_bypass 0  lcd_roi 00  algo_module 0  algo_bypass 0
	0x30, // algo_roi 00  ascr_module 1  ascr_bypass 1  ascr_roi 00  roi_type 00
	0x00, // roi0_x_start 00000000
	0x00, // roi0_x_start 0000  roi0_x_end 0000
	0x00, // roi0_x_end 00000000
	0x00, // roi0_y_start 00000000
	0x00, // roi0_y_start 0000  roi0_y_end 0000
	0x00, // roi0_y_end 00000000
	0x00, // roi1_x_start 00000000
	0x00, // roi1_x_start 0000  roi1_x_end 0000
	0x00, // roi1_x_end 00000000
	0x00, // roi1_y_start 00000000
	0x00, // roi1_y_start 0000  roi1_y_end 0000
	0x00, // roi1_y_end 00000000
	0xa0, // hsize 10100000
	0x06, // hsize 0000  vsize 0110
	0x40, // vsize 01000000
	0x62, // gain 110000 cgain 10
	0x48, // cgain 0100  scene_on 1  scene_tran 000
	0x14, // scene_tran 0  min_diff 0010100
	0xb3, // illum_gain 10110011
	0x87, // ref_offset 10000111
	0x40, // ref_offset 0  ref_gain 1000000
	0x36, // ref_gain 00  hbsize 110  vbsize 110
	0xfa, // bth 11111010
	0x5a, // bin_size_ratio 0101101  dth 0
	0xe5, // dth 11  min_ref_offset 100101
	0x83, // min_ref_offset 10  gamma 0  cs 0 sharpen 0 nr 0 mask_th 11
	0xfc, // mask_th 111111  sharpen_weight 00
	0x10, // sharpen_weight 00010000
	0x14, // max_plus 00010100
	0x02, // max_plus 000  max_minus 00010
	0x81, // max_minus 100000  cs_gain 01
	0x00, // cs_gain 00000000
	0x00, // curve_1_b
	0x20, // curve_1_a
	0x00, // curve_2_b
	0x20, // curve_2_a
	0x00, // curve_3_b
	0x20, // curve_3_a
	0x00, // curve_4_b
	0x20, // curve_4_a
	0x00, // curve_5_b
	0x20, // curve_5_a
	0x00, // curve_6_b
	0x20, // curve_6_a
	0x00, // curve_7_b
	0x20, // curve_7_a
	0x00, // curve_8_b
	0x20, // curve_8_a
	0x00, // curve_9_b
	0x20, // curve_9_a
	0x00, // curve_10_b
	0x20, // curve_10_a
	0x00, // curve_11_b
	0x20, // curve_11_a
	0x00, // curve_12_b
	0x20, // curve_12_a
	0x00, // curve_13_b
	0x20, // curve_13_a
	0x00, // curve_14_b
	0x20, // curve_14_a
	0x00, // curve_15_b
	0x20, // curve_15_a
	0x00, // curve_16_b
	0x20, // curve_16_a
	0x00, // curve_17_b
	0x20, // curve_17_a
	0x00, // curve_18_b
	0x20, // curve_18_a
	0x00, // curve_19_b
	0x20, // curve_19_a
	0x00, // curve_20_b
	0x20, // curve_20_a
	0x00, // curve_21_b
	0x20, // curve_21_a
	0x00, // curve_22_b
	0x20, // curve_22_a
	0x00, // curve_23_b
	0x20, // curve_23_a
	0x00, // curve_24_b
	0xff, // curve_24_a
};

static unsigned char NATURAL_BROWSER_2[] = {
	0xBA, // Start offset 0x09D4, base B1h
	0x05, // ascr_strength 00000  ascr_on 1  skin_cb 01
	0x9e, // skin_cb 100111  skin_cr 10
	0xa4, // skin_cr 101001  up_distance 00
	0x5c, // up_distance 010111  down_distance 00                                       
	0xa4, // down_distance 101001  right_distance 00                                    
	0x64, // right_distance 011001  left_distance 00                                    
	0x9c, // left_distance 100111  up_divided_distance 00
	0x16, // up_divided_distance 00010110
	0x42, // up_divided_distance 01000010
	0xc0, // up_divided_distance 11  down_divided_distance 000000
	0xc7, // down_divided_distance 11000111
	0xd0, // down_divided_distance 110100 right_divided_distance 00
	0x14, // right_divided_distance 00010100
	0x7b, // right_divided_distance 01111011
	0x00, // right_divided_distance 00  left_divided_distance 000000
	0xd2, // left_divided_distance 11010010
	0x0f, // left_divided_distance 000011  ascr_skin_Rr 11
	0xfc, // ascr_skin_Rr 111111  ascr_skin_Rg 00
	0x00, // ascr_skin_Rg 000000  ascr_skin_Rb 00
	0x03, // ascr_skin_Rb 000000  ascr_skin_Yr 11
	0xff, // ascr_skin_Yr 111111  ascr_skin_Yg 11
	0xfc, // ascr_skin_Yg 111111  ascr_skin_Yb 00
	0x03, // ascr_skin_Yb 000000  ascr_skin_Mr 11
	0xfc, // ascr_skin_Mr 111111  ascr_skin_Mg 00
	0x03, // ascr_skin_Mg 000000  ascr_skin_Mb 11
	0xff, // ascr_skin_Mb 111111  ascr_skin_Wr 11
	0xff, // ascr_skin_Wr 111111  ascr_skin_Wg 11
	0xff, // ascr_skin_Wg 111111  ascr_skin_Wb 11
	0xfc, // ascr_skin_Wb 111111  ascr_Cr 00
	0x03, // ascr_Cr 000000  ascr_Rr 11
	0xff, // ascr_Rr 111111  ascr_Cg 11
	0xfc, // ascr_Cg 111111  ascr_Rg 00
	0x5b, // ascr_Rg 010110  ascr_Cb 11
	0x94, // ascr_Cb 100101  ascr_Rb 00
	0x3b, // ascr_Rb 001110  ascr_Mr 11
	0xfc, // ascr_Mr 111111  ascr_Gr 00
	0x00, // ascr_Gr 000000  ascr_Mg 00
	0x53, // ascr_Mg 010100  ascr_Gg 11
	0xff, // ascr_Gg 111111  ascr_Mb 11
	0x64, // ascr_Mb 011001  ascr_Gb 00
	0x13, // ascr_Gb 000100  ascr_Yr 11
	0xf0, // ascr_Yr 111100  ascr_Br 00
	0x9b, // ascr_Br 100110  ascr_Yg 11
	0xfc, // ascr_Yg 111111  ascr_Bg 00
	0x6c, // ascr_Bg 011011  ascr_Yb 00
	0x8f, // ascr_Yb 100011  ascr_Bb 11
	0xff, // ascr_Bb 111111  ascr_Wr 11
	0xfc, // ascr_Wr 111111  ascr_Kr 00
	0x03, // ascr_Kr 000000  ascr_Wg 11
	0xe0, // ascr_Wg 111000  ascr_Kg 00
	0x03, // ascr_Kg 000000  ascr_Wb 11
	0xbc, // ascr_Wb 101111  ascr_Kb 00
	0x00, // ascr_Kb 000000  reserved 00
	0x0f, // reserved 0000  vsync_mask 1111
	/* end */
};

static unsigned char DYNAMIC_BROWSER_1[] = {
	/* start */
	0xBA, // Start offset 0x0982, base B1h
	0x03, // data_width 00  lce_module 0  lcd_bypass 0  lcd_roi 00  algo_module 1  algo_bypass 1
	0x30, // algo_roi 00  ascr_module 1  ascr_bypass 1  ascr_roi 00  roi_type 00
	0x00, // roi0_x_start 00000000
	0x00, // roi0_x_start 0000  roi0_x_end 0000
	0x00, // roi0_x_end 00000000
	0x00, // roi0_y_start 00000000
	0x00, // roi0_y_start 0000  roi0_y_end 0000
	0x00, // roi0_y_end 00000000
	0x00, // roi1_x_start 00000000
	0x00, // roi1_x_start 0000  roi1_x_end 0000
	0x00, // roi1_x_end 00000000
	0x00, // roi1_y_start 00000000
	0x00, // roi1_y_start 0000  roi1_y_end 0000
	0x00, // roi1_y_end 00000000
	0xa0, // hsize 10100000
	0x06, // hsize 0000  vsize 0110
	0x40, // vsize 01000000
	0x62, // gain 110000 cgain 10
	0x48, // cgain 0100  scene_on 1  scene_tran 000
	0x14, // scene_tran 0  min_diff 0010100
	0xb3, // illum_gain 10110011
	0x87, // ref_offset 10000111
	0x40, // ref_offset 0  ref_gain 1000000
	0x36, // ref_gain 00  hbsize 110  vbsize 110
	0xfa, // bth 11111010
	0x5a, // bin_size_ratio 0101101  dth 0
	0xe5, // dth 11  min_ref_offset 100101
	0xb3, // min_ref_offset 10  gamma 1  cs 1 sharpen 0 nr 0 mask_th 11
	0xfc, // mask_th 111111  sharpen_weight 00
	0x50, // sharpen_weight 01010000
	0x14, // max_plus 00010100
	0x02, // max_plus 000  max_minus 00010
	0x81, // max_minus 100000  cs_gain 01
	0x20, // cs_gain 00100000
	0x00, // curve_1_b 
	0x0f, // curve_1_a 
	0x00, // curve_2_b 
	0x0f, // curve_2_a 
	0x00, // curve_3_b 
	0x0f, // curve_3_a 
	0x00, // curve_4_b 
	0x0f, // curve_4_a 
	0x09, // curve_5_b 
	0xa2, // curve_5_a 
	0x09, // curve_6_b 
	0xa2, // curve_6_a 
	0x09, // curve_7_b 
	0xa2, // curve_7_a 
	0x09, // curve_8_b 
	0xa2, // curve_8_a 
	0x09, // curve_9_b 
	0xa2, // curve_9_a 
	0x09, // curve_10_b 
	0xa2, // curve_10_a 
	0x0a, // curve_11_b 
	0xa2, // curve_11_a 
	0x0a, // curve_12_b 
	0xa2, // curve_12_a 
	0x0a, // curve_13_b 
	0xa2, // curve_13_a 
	0x0a, // curve_14_b 
	0xa2, // curve_14_a 
	0x0a, // curve_15_b 
	0xa2, // curve_15_a 
	0x0a, // curve_16_b 
	0xa2, // curve_16_a 
	0x0a, // curve_17_b 
	0xa2, // curve_17_a 
	0x0a, // curve_18_b 
	0xa2, // curve_18_a 
	0x0f, // curve_19_b 
	0xa4, // curve_19_a 
	0x0f, // curve_20_b 
	0xa4, // curve_20_a 
	0x0f, // curve_21_b 
	0xa4, // curve_21_a 
	0x23, // curve_22_b 
	0x1c, // curve_22_a 
	0x48, // curve_23_b 
	0x17, // curve_23_a 
	0x00, // curve_24_b 
	0xff, // curve_24_a 
};

static unsigned char DYNAMIC_BROWSER_2[] = {
	0xBA, // Start offset 0x09D4, base B1h
	0x05, // ascr_strength 00000  ascr_on 1  skin_cb 01
	0x9e, // skin_cb 100111  skin_cr 10
	0xa4, // skin_cr 101001  up_distance 00
	0x5c, // up_distance 010111  down_distance 00                                       
	0xa4, // down_distance 101001  right_distance 00                                    
	0x64, // right_distance 011001  left_distance 00                                    
	0x9c, // left_distance 100111  up_divided_distance 00
	0x16, // up_divided_distance 00010110
	0x42, // up_divided_distance 01000010
	0xc0, // up_divided_distance 11  down_divided_distance 000000
	0xc7, // down_divided_distance 11000111
	0xd0, // down_divided_distance 110100 right_divided_distance 00
	0x14, // right_divided_distance 00010100
	0x7b, // right_divided_distance 01111011
	0x00, // right_divided_distance 00  left_divided_distance 000000
	0xd2, // left_divided_distance 11010010
	0x0f, // left_divided_distance 000011  ascr_skin_Rr 11
	0xfc, // ascr_skin_Rr 111111  ascr_skin_Rg 00
	0x00, // ascr_skin_Rg 000000  ascr_skin_Rb 00
	0x03, // ascr_skin_Rb 000000  ascr_skin_Yr 11
	0xff, // ascr_skin_Yr 111111  ascr_skin_Yg 11
	0xfc, // ascr_skin_Yg 111111  ascr_skin_Yb 00
	0x03, // ascr_skin_Yb 000000  ascr_skin_Mr 11
	0xfc, // ascr_skin_Mr 111111  ascr_skin_Mg 00
	0x03, // ascr_skin_Mg 000000  ascr_skin_Mb 11
	0xff, // ascr_skin_Mb 111111  ascr_skin_Wr 11
	0xff, // ascr_skin_Wr 111111  ascr_skin_Wg 11
	0xff, // ascr_skin_Wg 111111  ascr_skin_Wb 11
	0xfc, // ascr_skin_Wb 111111  ascr_Cr 00
	0x03, // ascr_Cr 000000  ascr_Rr 11
	0xff, // ascr_Rr 111111  ascr_Cg 11
	0xfc, // ascr_Cg 111111  ascr_Rg 00
	0x03, // ascr_Rg 000000  ascr_Cb 11
	0xfc, // ascr_Cb 111111  ascr_Rb 00
	0x03, // ascr_Rb 000000  ascr_Mr 11
	0xfc, // ascr_Mr 111111  ascr_Gr 00
	0x00, // ascr_Gr 000000  ascr_Mg 00
	0x03, // ascr_Mg 000000  ascr_Gg 11
	0xff, // ascr_Gg 111111  ascr_Mb 11
	0xfc, // ascr_Mb 111111  ascr_Gb 00
	0x03, // ascr_Gb 000000  ascr_Yr 11
	0xfc, // ascr_Yr 111111  ascr_Br 00
	0x03, // ascr_Br 000000  ascr_Yg 11
	0xfc, // ascr_Yg 111111  ascr_Bg 00
	0x00, // ascr_Bg 000000  ascr_Yb 00
	0x03, // ascr_Yb 000000  ascr_Bb 11
	0xff, // ascr_Bb 111111  ascr_Wr 11
	0xfc, // ascr_Wr 111111  ascr_Kr 00
	0x03, // ascr_Kr 000000  ascr_Wg 11
	0xfc, // ascr_Wg 111111  ascr_Kg 00
	0x03, // ascr_Kg 000000  ascr_Wb 11
	0xfc, // ascr_Wb 111111  ascr_Kb 00
	0x00, // ascr_Kb 000000  reserved 00
	0x0f, // reserved 0000  vsync_mask 1111
	/* end */
};

static unsigned char MOVIE_BROWSER_1[] = {
	/* start */
	0xBA, // Start offset 0x0982, base B1h
	0x03, // data_width 00  lce_module 0  lcd_bypass 0  lcd_roi 00  algo_module 1  algo_bypass 1
	0x30, // algo_roi 00  ascr_module 1  ascr_bypass 1  ascr_roi 00  roi_type 00
	0x00, // roi0_x_start 00000000
	0x00, // roi0_x_start 0000  roi0_x_end 0000
	0x00, // roi0_x_end 00000000
	0x00, // roi0_y_start 00000000
	0x00, // roi0_y_start 0000  roi0_y_end 0000
	0x00, // roi0_y_end 00000000
	0x00, // roi1_x_start 00000000
	0x00, // roi1_x_start 0000  roi1_x_end 0000
	0x00, // roi1_x_end 00000000
	0x00, // roi1_y_start 00000000
	0x00, // roi1_y_start 0000  roi1_y_end 0000
	0x00, // roi1_y_end 00000000
	0xa0, // hsize 10100000
	0x06, // hsize 0000  vsize 0110
	0x40, // vsize 01000000
	0x62, // gain 110000 cgain 10
	0x48, // cgain 0100  scene_on 1  scene_tran 000
	0x14, // scene_tran 0  min_diff 0010100
	0xb3, // illum_gain 10110011
	0x87, // ref_offset 10000111
	0x40, // ref_offset 0  ref_gain 1000000
	0x36, // ref_gain 00  hbsize 110  vbsize 110
	0xfa, // bth 11111010
	0x5a, // bin_size_ratio 0101101  dth 0
	0xe5, // dth 11  min_ref_offset 100101
	0x93, // min_ref_offset 10  gamma 0  cs 1 sharpen 0 nr 0 mask_th 11
	0xfc, // mask_th 111111  sharpen_weight 00
	0x10, // sharpen_weight 00010000
	0x14, // max_plus 00010100
	0x02, // max_plus 000  max_minus 00010
	0x81, // max_minus 100000  cs_gain 01
	0x20, // cs_gain 00100000
	0x00, // curve_1_b
	0x20, // curve_1_a
	0x00, // curve_2_b
	0x20, // curve_2_a
	0x00, // curve_3_b
	0x20, // curve_3_a
	0x00, // curve_4_b
	0x20, // curve_4_a
	0x00, // curve_5_b
	0x20, // curve_5_a
	0x00, // curve_6_b
	0x20, // curve_6_a
	0x00, // curve_7_b
	0x20, // curve_7_a
	0x00, // curve_8_b
	0x20, // curve_8_a
	0x00, // curve_9_b
	0x20, // curve_9_a
	0x00, // curve_10_b
	0x20, // curve_10_a
	0x00, // curve_11_b
	0x20, // curve_11_a
	0x00, // curve_12_b
	0x20, // curve_12_a
	0x00, // curve_13_b
	0x20, // curve_13_a
	0x00, // curve_14_b
	0x20, // curve_14_a
	0x00, // curve_15_b
	0x20, // curve_15_a
	0x00, // curve_16_b
	0x20, // curve_16_a
	0x00, // curve_17_b
	0x20, // curve_17_a
	0x00, // curve_18_b
	0x20, // curve_18_a
	0x00, // curve_19_b
	0x20, // curve_19_a
	0x00, // curve_20_b
	0x20, // curve_20_a
	0x00, // curve_21_b
	0x20, // curve_21_a
	0x00, // curve_22_b
	0x20, // curve_22_a
	0x00, // curve_23_b
	0x20, // curve_23_a
	0x00, // curve_24_b
	0xff, // curve_24_a
};

static unsigned char MOVIE_BROWSER_2[] = {
	0xBA, // Start offset 0x09D4, base B1h
	0x05, // ascr_strength 00000  ascr_on 1  skin_cb 01
	0x9e, // skin_cb 100111  skin_cr 10
	0xa4, // skin_cr 101001  up_distance 00
	0x5c, // up_distance 010111  down_distance 00                                       
	0xa4, // down_distance 101001  right_distance 00                                    
	0x64, // right_distance 011001  left_distance 00                                    
	0x9c, // left_distance 100111  up_divided_distance 00
	0x16, // up_divided_distance 00010110
	0x42, // up_divided_distance 01000010
	0xc0, // up_divided_distance 11  down_divided_distance 000000
	0xc7, // down_divided_distance 11000111
	0xd0, // down_divided_distance 110100 right_divided_distance 00
	0x14, // right_divided_distance 00010100
	0x7b, // right_divided_distance 01111011
	0x00, // right_divided_distance 00  left_divided_distance 000000
	0xd2, // left_divided_distance 11010010
	0x0f, // left_divided_distance 000011  ascr_skin_Rr 11
	0xfc, // ascr_skin_Rr 111111  ascr_skin_Rg 00
	0x00, // ascr_skin_Rg 000000  ascr_skin_Rb 00
	0x03, // ascr_skin_Rb 000000  ascr_skin_Yr 11
	0xff, // ascr_skin_Yr 111111  ascr_skin_Yg 11
	0xfc, // ascr_skin_Yg 111111  ascr_skin_Yb 00
	0x03, // ascr_skin_Yb 000000  ascr_skin_Mr 11
	0xfc, // ascr_skin_Mr 111111  ascr_skin_Mg 00
	0x03, // ascr_skin_Mg 000000  ascr_skin_Mb 11
	0xff, // ascr_skin_Mb 111111  ascr_skin_Wr 11
	0xff, // ascr_skin_Wr 111111  ascr_skin_Wg 11
	0xff, // ascr_skin_Wg 111111  ascr_skin_Wb 11
	0xfd, // ascr_skin_Wb 111111  ascr_Cr 01
	0xeb, // ascr_Cr 111010  ascr_Rr 11
	0xff, // ascr_Rr 111111  ascr_Cg 11
	0xfc, // ascr_Cg 111111  ascr_Rg 00
	0xaf, // ascr_Rg 101011  ascr_Cb 11
	0xc4, // ascr_Cb 110001  ascr_Rb 00
	0xb7, // ascr_Rb 101101  ascr_Mr 11
	0xfd, // ascr_Mr 111111  ascr_Gr 01
	0xa0, // ascr_Gr 101000  ascr_Mg 00
	0x57, // ascr_Mg 010101  ascr_Gg 11
	0xff, // ascr_Gg 111111  ascr_Mb 11
	0xe8, // ascr_Mb 111010  ascr_Gb 00
	0xc7, // ascr_Gb 110001  ascr_Yr 11
	0xe0, // ascr_Yr 111000  ascr_Br 00
	0xd3, // ascr_Br 110100  ascr_Yg 11
	0xfc, // ascr_Yg 111111  ascr_Bg 00
	0x81, // ascr_Bg 100000  ascr_Yb 01
	0x2f, // ascr_Yb 001011  ascr_Bb 11
	0xff, // ascr_Bb 111111  ascr_Wr 11
	0xfc, // ascr_Wr 111111  ascr_Kr 00
	0x03, // ascr_Kr 000000  ascr_Wg 11
	0xe0, // ascr_Wg 111000  ascr_Kg 00
	0x03, // ascr_Kg 000000  ascr_Wb 11
	0xbc, // ascr_Wb 101111  ascr_Kb 00
	0x00, // ascr_Kb 000000  reserved 00
	0x0f, // reserved 0000  vsync_mask 1111
	/* end */
};

static unsigned char AUTO_BROWSER_1[] = {
	/* start */
	0xBA, // Start offset 0x0982, base B1h
	0x00, // data_width 00  lce_module 0  lcd_bypass 0  lcd_roi 00  algo_module 0  algo_bypass 0
	0x30, // algo_roi 00  ascr_module 1  ascr_bypass 1  ascr_roi 00  roi_type 00
	0x00, // roi0_x_start 00000000
	0x00, // roi0_x_start 0000  roi0_x_end 0000
	0x00, // roi0_x_end 00000000
	0x00, // roi0_y_start 00000000
	0x00, // roi0_y_start 0000  roi0_y_end 0000
	0x00, // roi0_y_end 00000000
	0x00, // roi1_x_start 00000000
	0x00, // roi1_x_start 0000  roi1_x_end 0000
	0x00, // roi1_x_end 00000000
	0x00, // roi1_y_start 00000000
	0x00, // roi1_y_start 0000  roi1_y_end 0000
	0x00, // roi1_y_end 00000000
	0xa0, // hsize 10100000
	0x06, // hsize 0000  vsize 0110
	0x40, // vsize 01000000
	0x62, // gain 110000 cgain 10
	0x48, // cgain 0100  scene_on 1  scene_tran 000
	0x14, // scene_tran 0  min_diff 0010100
	0xb3, // illum_gain 10110011
	0x87, // ref_offset 10000111
	0x40, // ref_offset 0  ref_gain 1000000
	0x36, // ref_gain 00  hbsize 110  vbsize 110
	0xfa, // bth 11111010
	0x5a, // bin_size_ratio 0101101  dth 0
	0xe5, // dth 11  min_ref_offset 100101
	0x83, // min_ref_offset 10  gamma 0  cs 0 sharpen 0 nr 0 mask_th 11
	0xfc, // mask_th 111111  sharpen_weight 00
	0x40, // sharpen_weight 01000000
	0x14, // max_plus 00010100
	0x02, // max_plus 000  max_minus 00010
	0x81, // max_minus 100000  cs_gain 01
	0x00, // cs_gain 00000000
	0x00, // curve_1_b
	0x20, // curve_1_a
	0x00, // curve_2_b
	0x20, // curve_2_a
	0x00, // curve_3_b
	0x20, // curve_3_a
	0x00, // curve_4_b
	0x20, // curve_4_a
	0x00, // curve_5_b
	0x20, // curve_5_a
	0x00, // curve_6_b
	0x20, // curve_6_a
	0x00, // curve_7_b
	0x20, // curve_7_a
	0x00, // curve_8_b
	0x20, // curve_8_a
	0x00, // curve_9_b
	0x20, // curve_9_a
	0x00, // curve_10_b
	0x20, // curve_10_a
	0x00, // curve_11_b
	0x20, // curve_11_a
	0x00, // curve_12_b
	0x20, // curve_12_a
	0x00, // curve_13_b
	0x20, // curve_13_a
	0x00, // curve_14_b
	0x20, // curve_14_a
	0x00, // curve_15_b
	0x20, // curve_15_a
	0x00, // curve_16_b
	0x20, // curve_16_a
	0x00, // curve_17_b
	0x20, // curve_17_a
	0x00, // curve_18_b
	0x20, // curve_18_a
	0x00, // curve_19_b
	0x20, // curve_19_a
	0x00, // curve_20_b
	0x20, // curve_20_a
	0x00, // curve_21_b
	0x20, // curve_21_a
	0x00, // curve_22_b
	0x20, // curve_22_a
	0x00, // curve_23_b
	0x20, // curve_23_a
	0x00, // curve_24_b
	0xff, // curve_24_a
};

static unsigned char AUTO_BROWSER_2[] = {
	0xBA, // Start offset 0x09D4, base B1h
	0x65, // ascr_strength 01100  ascr_on 1  skin_cb 01
	0x9e, // skin_cb 100111  skin_cr 10
	0xa4, // skin_cr 101001  up_distance 00
	0x5c, // up_distance 010111  down_distance 00                                       
	0xa4, // down_distance 101001  right_distance 00                                    
	0x64, // right_distance 011001  left_distance 00                                    
	0x9c, // left_distance 100111  up_divided_distance 00
	0x16, // up_divided_distance 00010110
	0x42, // up_divided_distance 01000010
	0xc0, // up_divided_distance 11  down_divided_distance 000000
	0xc7, // down_divided_distance 11000111
	0xd0, // down_divided_distance 110100 right_divided_distance 00
	0x14, // right_divided_distance 00010100
	0x7b, // right_divided_distance 01111011
	0x00, // right_divided_distance 00  left_divided_distance 000000
	0xd2, // left_divided_distance 11010010
	0x0f, // left_divided_distance 000011  ascr_skin_Rr 11
	0x91, // ascr_skin_Rr 100100  ascr_skin_Rg 01
	0x01, // ascr_skin_Rg 000000  ascr_skin_Rb 01
	0x02, // ascr_skin_Rb 000000  ascr_skin_Yr 10
	0xff, // ascr_skin_Yr 111111  ascr_skin_Yg 11
	0xfc, // ascr_skin_Yg 111111  ascr_skin_Yb 00
	0x02, // ascr_skin_Yb 000000  ascr_skin_Mr 10
	0xfc, // ascr_skin_Mr 111111  ascr_skin_Mg 00
	0x03, // ascr_skin_Mg 000000  ascr_skin_Mb 11
	0xff, // ascr_skin_Mb 111111  ascr_skin_Wr 11
	0xff, // ascr_skin_Wr 111111  ascr_skin_Wg 11
	0xff, // ascr_skin_Wg 111111  ascr_skin_Wb 11
	0xfc, // ascr_skin_Wb 111111  ascr_Cr 00
	0x03, // ascr_Cr 000000  ascr_Rr 11
	0xff, // ascr_Rr 111111  ascr_Cg 11
	0xfc, // ascr_Cg 111111  ascr_Rg 00
	0x03, // ascr_Rg 000000  ascr_Cb 11
	0xfc, // ascr_Cb 111111  ascr_Rb 00
	0x03, // ascr_Rb 000000  ascr_Mr 11
	0xfc, // ascr_Mr 111111  ascr_Gr 00
	0x00, // ascr_Gr 000000  ascr_Mg 00
	0x03, // ascr_Mg 000000  ascr_Gg 11
	0xff, // ascr_Gg 111111  ascr_Mb 11
	0xfc, // ascr_Mb 111111  ascr_Gb 00
	0x03, // ascr_Gb 000000  ascr_Yr 11
	0xfc, // ascr_Yr 111111  ascr_Br 00
	0x03, // ascr_Br 000000  ascr_Yg 11
	0xfc, // ascr_Yg 111111  ascr_Bg 00
	0x00, // ascr_Bg 000000  ascr_Yb 00
	0x03, // ascr_Yb 000000  ascr_Bb 11
	0xff, // ascr_Bb 111111  ascr_Wr 11
	0xfc, // ascr_Wr 111111  ascr_Kr 00
	0x03, // ascr_Kr 000000  ascr_Wg 11
	0xfc, // ascr_Wg 111111  ascr_Kg 00
	0x03, // ascr_Kg 000000  ascr_Wb 11
	0xfc, // ascr_Wb 111111  ascr_Kb 00
	0x00, // ascr_Kb 000000  reserved 00
	0x0f, // reserved 0000  vsync_mask 1111
	/* end; */
};

////////////////// eBOOK /////////////////////
static unsigned char AUTO_EBOOK_1[] = {
	/* start */
	0xBA, // Start offset 0x0982, base B1h
	0x00, // data_width 00  lce_module 0  lcd_bypass 0  lcd_roi 00  algo_module 0  algo_bypass 0
	0x30, // algo_roi 00  ascr_module 1  ascr_bypass 1  ascr_roi 00  roi_type 00
	0x00, // roi0_x_start 00000000
	0x00, // roi0_x_start 0000  roi0_x_end 0000
	0x00, // roi0_x_end 00000000
	0x00, // roi0_y_start 00000000
	0x00, // roi0_y_start 0000  roi0_y_end 0000
	0x00, // roi0_y_end 00000000
	0x00, // roi1_x_start 00000000
	0x00, // roi1_x_start 0000  roi1_x_end 0000
	0x00, // roi1_x_end 00000000
	0x00, // roi1_y_start 00000000
	0x00, // roi1_y_start 0000  roi1_y_end 0000
	0x00, // roi1_y_end 00000000
	0xa0, // hsize 10100000
	0x06, // hsize 0000  vsize 0110
	0x40, // vsize 01000000
	0x62, // gain 110000 cgain 10
	0x48, // cgain 0100  scene_on 1  scene_tran 000
	0x14, // scene_tran 0  min_diff 0010100
	0xb3, // illum_gain 10110011
	0x87, // ref_offset 10000111
	0x40, // ref_offset 0  ref_gain 1000000
	0x36, // ref_gain 00  hbsize 110  vbsize 110
	0xfa, // bth 11111010
	0x5a, // bin_size_ratio 0101101  dth 0
	0xe5, // dth 11  min_ref_offset 100101
	0x83, // min_ref_offset 10  gamma 0  cs 0 sharpen 0 nr 0 mask_th 11
	0xfc, // mask_th 111111  sharpen_weight 00
	0x40, // sharpen_weight 01000000
	0x14, // max_plus 00010100
	0x02, // max_plus 000  max_minus 00010
	0x81, // max_minus 100000  cs_gain 01
	0x00, // cs_gain 00000000
	0x00, // curve_1_b
	0x20, // curve_1_a
	0x00, // curve_2_b
	0x20, // curve_2_a
	0x00, // curve_3_b
	0x20, // curve_3_a
	0x00, // curve_4_b
	0x20, // curve_4_a
	0x00, // curve_5_b
	0x20, // curve_5_a
	0x00, // curve_6_b
	0x20, // curve_6_a
	0x00, // curve_7_b
	0x20, // curve_7_a
	0x00, // curve_8_b
	0x20, // curve_8_a
	0x00, // curve_9_b
	0x20, // curve_9_a
	0x00, // curve_10_b
	0x20, // curve_10_a
	0x00, // curve_11_b
	0x20, // curve_11_a
	0x00, // curve_12_b
	0x20, // curve_12_a
	0x00, // curve_13_b
	0x20, // curve_13_a
	0x00, // curve_14_b
	0x20, // curve_14_a
	0x00, // curve_15_b
	0x20, // curve_15_a
	0x00, // curve_16_b
	0x20, // curve_16_a
	0x00, // curve_17_b
	0x20, // curve_17_a
	0x00, // curve_18_b
	0x20, // curve_18_a
	0x00, // curve_19_b
	0x20, // curve_19_a
	0x00, // curve_20_b
	0x20, // curve_20_a
	0x00, // curve_21_b
	0x20, // curve_21_a
	0x00, // curve_22_b
	0x20, // curve_22_a
	0x00, // curve_23_b
	0x20, // curve_23_a
	0x00, // curve_24_b
	0xff, // curve_24_a
};

static unsigned char AUTO_EBOOK_2[] = {
	0xBA, // Start offset 0x09D4, base B1h
	0x05, // ascr_strength 00000  ascr_on 1  skin_cb 01
	0x9e, // skin_cb 100111  skin_cr 10
	0xa4, // skin_cr 101001  up_distance 00
	0x5c, // up_distance 010111  down_distance 00                                       
	0xa4, // down_distance 101001  right_distance 00                                    
	0x64, // right_distance 011001  left_distance 00                                    
	0x9c, // left_distance 100111  up_divided_distance 00
	0x16, // up_divided_distance 00010110
	0x42, // up_divided_distance 01000010
	0xc0, // up_divided_distance 11  down_divided_distance 000000
	0xc7, // down_divided_distance 11000111
	0xd0, // down_divided_distance 110100 right_divided_distance 00
	0x14, // right_divided_distance 00010100
	0x7b, // right_divided_distance 01111011
	0x00, // right_divided_distance 00  left_divided_distance 000000
	0xd2, // left_divided_distance 11010010
	0x0f, // left_divided_distance 000011  ascr_skin_Rr 11
	0xfc, // ascr_skin_Rr 111111  ascr_skin_Rg 00
	0x00, // ascr_skin_Rg 000000  ascr_skin_Rb 00
	0x03, // ascr_skin_Rb 000000  ascr_skin_Yr 11
	0xff, // ascr_skin_Yr 111111  ascr_skin_Yg 11
	0xfc, // ascr_skin_Yg 111111  ascr_skin_Yb 00
	0x03, // ascr_skin_Yb 000000  ascr_skin_Mr 11
	0xfc, // ascr_skin_Mr 111111  ascr_skin_Mg 00
	0x03, // ascr_skin_Mg 000000  ascr_skin_Mb 11
	0xff, // ascr_skin_Mb 111111  ascr_skin_Wr 11
	0xff, // ascr_skin_Wr 111111  ascr_skin_Wg 11
	0xff, // ascr_skin_Wg 111111  ascr_skin_Wb 11
	0xfc, // ascr_skin_Wb 111111  ascr_Cr 00
	0x03, // ascr_Cr 000000  ascr_Rr 11
	0xff, // ascr_Rr 111111  ascr_Cg 11
	0xfc, // ascr_Cg 111111  ascr_Rg 00
	0x03, // ascr_Rg 000000  ascr_Cb 11
	0xfc, // ascr_Cb 111111  ascr_Rb 00
	0x03, // ascr_Rb 000000  ascr_Mr 11
	0xfc, // ascr_Mr 111111  ascr_Gr 00
	0x00, // ascr_Gr 000000  ascr_Mg 00
	0x03, // ascr_Mg 000000  ascr_Gg 11
	0xff, // ascr_Gg 111111  ascr_Mb 11
	0xfc, // ascr_Mb 111111  ascr_Gb 00
	0x03, // ascr_Gb 000000  ascr_Yr 11
	0xfc, // ascr_Yr 111111  ascr_Br 00
	0x03, // ascr_Br 000000  ascr_Yg 11
	0xfc, // ascr_Yg 111111  ascr_Bg 00
	0x00, // ascr_Bg 000000  ascr_Yb 00
	0x03, // ascr_Yb 000000  ascr_Bb 11
	0xff, // ascr_Bb 111111  ascr_Wr 11
	0xfc, // ascr_Wr 111111  ascr_Kr 00
	0x03, // ascr_Kr 000000  ascr_Wg 11
	0xdc, // ascr_Wg 110111  ascr_Kg 00
	0x03, // ascr_Kg 000000  ascr_Wb 11
	0x98, // ascr_Wb 100110  ascr_Kb 00
	0x00, // ascr_Kb 000000  reserved 00
	0x0f, // reserved 0000  vsync_mask 1111
	/* end */
};

static unsigned char DYNAMIC_EBOOK_1[] = {
	/* start */
	0xBA, // Start offset 0x0982, base B1h
	0x00, // data_width 00  lce_module 0  lcd_bypass 0  lcd_roi 00  algo_module 0  algo_bypass 0
	0x30, // algo_roi 00  ascr_module 1  ascr_bypass 1  ascr_roi 00  roi_type 00
	0x00, // roi0_x_start 00000000
	0x00, // roi0_x_start 0000  roi0_x_end 0000
	0x00, // roi0_x_end 00000000
	0x00, // roi0_y_start 00000000
	0x00, // roi0_y_start 0000  roi0_y_end 0000
	0x00, // roi0_y_end 00000000
	0x00, // roi1_x_start 00000000
	0x00, // roi1_x_start 0000  roi1_x_end 0000
	0x00, // roi1_x_end 00000000
	0x00, // roi1_y_start 00000000
	0x00, // roi1_y_start 0000  roi1_y_end 0000
	0x00, // roi1_y_end 00000000
	0xa0, // hsize 10100000
	0x06, // hsize 0000  vsize 0110
	0x40, // vsize 01000000
	0x62, // gain 110000 cgain 10
	0x48, // cgain 0100  scene_on 1  scene_tran 000
	0x14, // scene_tran 0  min_diff 0010100
	0xb3, // illum_gain 10110011
	0x87, // ref_offset 10000111
	0x40, // ref_offset 0  ref_gain 1000000
	0x36, // ref_gain 00  hbsize 110  vbsize 110
	0xfa, // bth 11111010
	0x5a, // bin_size_ratio 0101101  dth 0
	0xe5, // dth 11  min_ref_offset 100101
	0x83, // min_ref_offset 10  gamma 0  cs 0 sharpen 0 nr 0 mask_th 11
	0xfc, // mask_th 111111  sharpen_weight 00
	0x40, // sharpen_weight 01000000
	0x14, // max_plus 00010100
	0x02, // max_plus 000  max_minus 00010
	0x81, // max_minus 100000  cs_gain 01
	0x00, // cs_gain 00000000
	0x00, // curve_1_b
	0x20, // curve_1_a
	0x00, // curve_2_b
	0x20, // curve_2_a
	0x00, // curve_3_b
	0x20, // curve_3_a
	0x00, // curve_4_b
	0x20, // curve_4_a
	0x00, // curve_5_b
	0x20, // curve_5_a
	0x00, // curve_6_b
	0x20, // curve_6_a
	0x00, // curve_7_b
	0x20, // curve_7_a
	0x00, // curve_8_b
	0x20, // curve_8_a
	0x00, // curve_9_b
	0x20, // curve_9_a
	0x00, // curve_10_b
	0x20, // curve_10_a
	0x00, // curve_11_b
	0x20, // curve_11_a
	0x00, // curve_12_b
	0x20, // curve_12_a
	0x00, // curve_13_b
	0x20, // curve_13_a
	0x00, // curve_14_b
	0x20, // curve_14_a
	0x00, // curve_15_b
	0x20, // curve_15_a
	0x00, // curve_16_b
	0x20, // curve_16_a
	0x00, // curve_17_b
	0x20, // curve_17_a
	0x00, // curve_18_b
	0x20, // curve_18_a
	0x00, // curve_19_b
	0x20, // curve_19_a
	0x00, // curve_20_b
	0x20, // curve_20_a
	0x00, // curve_21_b
	0x20, // curve_21_a
	0x00, // curve_22_b
	0x20, // curve_22_a
	0x00, // curve_23_b
	0x20, // curve_23_a
	0x00, // curve_24_b
	0xff, // curve_24_a
};

static unsigned char DYNAMIC_EBOOK_2[] = {
	0xBA, // Start offset 0x09D4, base B1h
	0x05, // ascr_strength 00000  ascr_on 1  skin_cb 01
	0x9e, // skin_cb 100111  skin_cr 10
	0xa4, // skin_cr 101001  up_distance 00
	0x5c, // up_distance 010111  down_distance 00                                       
	0xa4, // down_distance 101001  right_distance 00                                    
	0x64, // right_distance 011001  left_distance 00                                    
	0x9c, // left_distance 100111  up_divided_distance 00
	0x16, // up_divided_distance 00010110
	0x42, // up_divided_distance 01000010
	0xc0, // up_divided_distance 11  down_divided_distance 000000
	0xc7, // down_divided_distance 11000111
	0xd0, // down_divided_distance 110100 right_divided_distance 00
	0x14, // right_divided_distance 00010100
	0x7b, // right_divided_distance 01111011
	0x00, // right_divided_distance 00  left_divided_distance 000000
	0xd2, // left_divided_distance 11010010
	0x0f, // left_divided_distance 000011  ascr_skin_Rr 11
	0xfc, // ascr_skin_Rr 111111  ascr_skin_Rg 00
	0x00, // ascr_skin_Rg 000000  ascr_skin_Rb 00
	0x03, // ascr_skin_Rb 000000  ascr_skin_Yr 11
	0xff, // ascr_skin_Yr 111111  ascr_skin_Yg 11
	0xfc, // ascr_skin_Yg 111111  ascr_skin_Yb 00
	0x03, // ascr_skin_Yb 000000  ascr_skin_Mr 11
	0xfc, // ascr_skin_Mr 111111  ascr_skin_Mg 00
	0x03, // ascr_skin_Mg 000000  ascr_skin_Mb 11
	0xff, // ascr_skin_Mb 111111  ascr_skin_Wr 11
	0xff, // ascr_skin_Wr 111111  ascr_skin_Wg 11
	0xff, // ascr_skin_Wg 111111  ascr_skin_Wb 11
	0xfc, // ascr_skin_Wb 111111  ascr_Cr 00
	0x03, // ascr_Cr 000000  ascr_Rr 11
	0xff, // ascr_Rr 111111  ascr_Cg 11
	0xfc, // ascr_Cg 111111  ascr_Rg 00
	0x03, // ascr_Rg 000000  ascr_Cb 11
	0xfc, // ascr_Cb 111111  ascr_Rb 00
	0x03, // ascr_Rb 000000  ascr_Mr 11
	0xfc, // ascr_Mr 111111  ascr_Gr 00
	0x00, // ascr_Gr 000000  ascr_Mg 00
	0x03, // ascr_Mg 000000  ascr_Gg 11
	0xff, // ascr_Gg 111111  ascr_Mb 11
	0xfc, // ascr_Mb 111111  ascr_Gb 00
	0x03, // ascr_Gb 000000  ascr_Yr 11
	0xfc, // ascr_Yr 111111  ascr_Br 00
	0x03, // ascr_Br 000000  ascr_Yg 11
	0xfc, // ascr_Yg 111111  ascr_Bg 00
	0x00, // ascr_Bg 000000  ascr_Yb 00
	0x03, // ascr_Yb 000000  ascr_Bb 11
	0xff, // ascr_Bb 111111  ascr_Wr 11
	0xfc, // ascr_Wr 111111  ascr_Kr 00
	0x03, // ascr_Kr 000000  ascr_Wg 11
	0xdc, // ascr_Wg 110111  ascr_Kg 00
	0x03, // ascr_Kg 000000  ascr_Wb 11
	0x98, // ascr_Wb 100110  ascr_Kb 00
	0x00, // ascr_Kb 000000  reserved 00
	0x0f, // reserved 0000  vsync_mask 1111
	/* end */
};

static unsigned char STANDARD_EBOOK_1[] = {
	/* start */
	0xBA, // Start offset 0x0982, base B1h
	0x00, // data_width 00  lce_module 0  lcd_bypass 0  lcd_roi 00  algo_module 0  algo_bypass 0
	0x30, // algo_roi 00  ascr_module 1  ascr_bypass 1  ascr_roi 00  roi_type 00
	0x00, // roi0_x_start 00000000
	0x00, // roi0_x_start 0000  roi0_x_end 0000
	0x00, // roi0_x_end 00000000
	0x00, // roi0_y_start 00000000
	0x00, // roi0_y_start 0000  roi0_y_end 0000
	0x00, // roi0_y_end 00000000
	0x00, // roi1_x_start 00000000
	0x00, // roi1_x_start 0000  roi1_x_end 0000
	0x00, // roi1_x_end 00000000
	0x00, // roi1_y_start 00000000
	0x00, // roi1_y_start 0000  roi1_y_end 0000
	0x00, // roi1_y_end 00000000
	0xa0, // hsize 10100000
	0x06, // hsize 0000  vsize 0110
	0x40, // vsize 01000000
	0x62, // gain 110000 cgain 10
	0x48, // cgain 0100  scene_on 1  scene_tran 000
	0x14, // scene_tran 0  min_diff 0010100
	0xb3, // illum_gain 10110011
	0x87, // ref_offset 10000111
	0x40, // ref_offset 0  ref_gain 1000000
	0x36, // ref_gain 00  hbsize 110  vbsize 110
	0xfa, // bth 11111010
	0x5a, // bin_size_ratio 0101101  dth 0
	0xe5, // dth 11  min_ref_offset 100101
	0x83, // min_ref_offset 10  gamma 0  cs 0 sharpen 0 nr 0 mask_th 11
	0xfc, // mask_th 111111  sharpen_weight 00
	0x40, // sharpen_weight 01000000
	0x14, // max_plus 00010100
	0x02, // max_plus 000  max_minus 00010
	0x81, // max_minus 100000  cs_gain 01
	0x00, // cs_gain 00000000
	0x00, // curve_1_b
	0x20, // curve_1_a
	0x00, // curve_2_b
	0x20, // curve_2_a
	0x00, // curve_3_b
	0x20, // curve_3_a
	0x00, // curve_4_b
	0x20, // curve_4_a
	0x00, // curve_5_b
	0x20, // curve_5_a
	0x00, // curve_6_b
	0x20, // curve_6_a
	0x00, // curve_7_b
	0x20, // curve_7_a
	0x00, // curve_8_b
	0x20, // curve_8_a
	0x00, // curve_9_b
	0x20, // curve_9_a
	0x00, // curve_10_b
	0x20, // curve_10_a
	0x00, // curve_11_b
	0x20, // curve_11_a
	0x00, // curve_12_b
	0x20, // curve_12_a
	0x00, // curve_13_b
	0x20, // curve_13_a
	0x00, // curve_14_b
	0x20, // curve_14_a
	0x00, // curve_15_b
	0x20, // curve_15_a
	0x00, // curve_16_b
	0x20, // curve_16_a
	0x00, // curve_17_b
	0x20, // curve_17_a
	0x00, // curve_18_b
	0x20, // curve_18_a
	0x00, // curve_19_b
	0x20, // curve_19_a
	0x00, // curve_20_b
	0x20, // curve_20_a
	0x00, // curve_21_b
	0x20, // curve_21_a
	0x00, // curve_22_b
	0x20, // curve_22_a
	0x00, // curve_23_b
	0x20, // curve_23_a
	0x00, // curve_24_b
	0xff, // curve_24_a
};

static unsigned char STANDARD_EBOOK_2[] = {
	0xBA, // Start offset 0x09D4, base B1h
	0x05, // ascr_strength 00000  ascr_on 1  skin_cb 01
	0x9e, // skin_cb 100111  skin_cr 10
	0xa4, // skin_cr 101001  up_distance 00
	0x5c, // up_distance 010111  down_distance 00                                       
	0xa4, // down_distance 101001  right_distance 00                                    
	0x64, // right_distance 011001  left_distance 00                                    
	0x9c, // left_distance 100111  up_divided_distance 00
	0x16, // up_divided_distance 00010110
	0x42, // up_divided_distance 01000010
	0xc0, // up_divided_distance 11  down_divided_distance 000000
	0xc7, // down_divided_distance 11000111
	0xd0, // down_divided_distance 110100 right_divided_distance 00
	0x14, // right_divided_distance 00010100
	0x7b, // right_divided_distance 01111011
	0x00, // right_divided_distance 00  left_divided_distance 000000
	0xd2, // left_divided_distance 11010010
	0x0f, // left_divided_distance 000011  ascr_skin_Rr 11
	0xfc, // ascr_skin_Rr 111111  ascr_skin_Rg 00
	0x00, // ascr_skin_Rg 000000  ascr_skin_Rb 00
	0x03, // ascr_skin_Rb 000000  ascr_skin_Yr 11
	0xff, // ascr_skin_Yr 111111  ascr_skin_Yg 11
	0xfc, // ascr_skin_Yg 111111  ascr_skin_Yb 00
	0x03, // ascr_skin_Yb 000000  ascr_skin_Mr 11
	0xfc, // ascr_skin_Mr 111111  ascr_skin_Mg 00
	0x03, // ascr_skin_Mg 000000  ascr_skin_Mb 11
	0xff, // ascr_skin_Mb 111111  ascr_skin_Wr 11
	0xff, // ascr_skin_Wr 111111  ascr_skin_Wg 11
	0xff, // ascr_skin_Wg 111111  ascr_skin_Wb 11
	0xfc, // ascr_skin_Wb 111111  ascr_Cr 00
	0x03, // ascr_Cr 000000  ascr_Rr 11
	0xff, // ascr_Rr 111111  ascr_Cg 11
	0xfc, // ascr_Cg 111111  ascr_Rg 00
	0x03, // ascr_Rg 000000  ascr_Cb 11
	0xfc, // ascr_Cb 111111  ascr_Rb 00
	0x03, // ascr_Rb 000000  ascr_Mr 11
	0xfc, // ascr_Mr 111111  ascr_Gr 00
	0x00, // ascr_Gr 000000  ascr_Mg 00
	0x03, // ascr_Mg 000000  ascr_Gg 11
	0xff, // ascr_Gg 111111  ascr_Mb 11
	0xfc, // ascr_Mb 111111  ascr_Gb 00
	0x03, // ascr_Gb 000000  ascr_Yr 11
	0xfc, // ascr_Yr 111111  ascr_Br 00
	0x03, // ascr_Br 000000  ascr_Yg 11
	0xfc, // ascr_Yg 111111  ascr_Bg 00
	0x00, // ascr_Bg 000000  ascr_Yb 00
	0x03, // ascr_Yb 000000  ascr_Bb 11
	0xff, // ascr_Bb 111111  ascr_Wr 11
	0xfc, // ascr_Wr 111111  ascr_Kr 00
	0x03, // ascr_Kr 000000  ascr_Wg 11
	0xdc, // ascr_Wg 110111  ascr_Kg 00
	0x03, // ascr_Kg 000000  ascr_Wb 11
	0x98, // ascr_Wb 100110  ascr_Kb 00
	0x00, // ascr_Kb 000000  reserved 00
	0x0f, // reserved 0000  vsync_mask 1111
	/* end */
};

static unsigned char NATURAL_EBOOK_1[] = {
	/* start */
	0xBA, // Start offset 0x0982, base B1h
	0x00, // data_width 00  lce_module 0  lcd_bypass 0  lcd_roi 00  algo_module 0  algo_bypass 0
	0x30, // algo_roi 00  ascr_module 1  ascr_bypass 1  ascr_roi 00  roi_type 00
	0x00, // roi0_x_start 00000000
	0x00, // roi0_x_start 0000  roi0_x_end 0000
	0x00, // roi0_x_end 00000000
	0x00, // roi0_y_start 00000000
	0x00, // roi0_y_start 0000  roi0_y_end 0000
	0x00, // roi0_y_end 00000000
	0x00, // roi1_x_start 00000000
	0x00, // roi1_x_start 0000  roi1_x_end 0000
	0x00, // roi1_x_end 00000000
	0x00, // roi1_y_start 00000000
	0x00, // roi1_y_start 0000  roi1_y_end 0000
	0x00, // roi1_y_end 00000000
	0xa0, // hsize 10100000
	0x06, // hsize 0000  vsize 0110
	0x40, // vsize 01000000
	0x62, // gain 110000 cgain 10
	0x48, // cgain 0100  scene_on 1  scene_tran 000
	0x14, // scene_tran 0  min_diff 0010100
	0xb3, // illum_gain 10110011
	0x87, // ref_offset 10000111
	0x40, // ref_offset 0  ref_gain 1000000
	0x36, // ref_gain 00  hbsize 110  vbsize 110
	0xfa, // bth 11111010
	0x5a, // bin_size_ratio 0101101  dth 0
	0xe5, // dth 11  min_ref_offset 100101
	0x83, // min_ref_offset 10  gamma 0  cs 0 sharpen 0 nr 0 mask_th 11
	0xfc, // mask_th 111111  sharpen_weight 00
	0x40, // sharpen_weight 01000000
	0x14, // max_plus 00010100
	0x02, // max_plus 000  max_minus 00010
	0x81, // max_minus 100000  cs_gain 01
	0x00, // cs_gain 00000000
	0x00, // curve_1_b
	0x20, // curve_1_a
	0x00, // curve_2_b
	0x20, // curve_2_a
	0x00, // curve_3_b
	0x20, // curve_3_a
	0x00, // curve_4_b
	0x20, // curve_4_a
	0x00, // curve_5_b
	0x20, // curve_5_a
	0x00, // curve_6_b
	0x20, // curve_6_a
	0x00, // curve_7_b
	0x20, // curve_7_a
	0x00, // curve_8_b
	0x20, // curve_8_a
	0x00, // curve_9_b
	0x20, // curve_9_a
	0x00, // curve_10_b
	0x20, // curve_10_a
	0x00, // curve_11_b
	0x20, // curve_11_a
	0x00, // curve_12_b
	0x20, // curve_12_a
	0x00, // curve_13_b
	0x20, // curve_13_a
	0x00, // curve_14_b
	0x20, // curve_14_a
	0x00, // curve_15_b
	0x20, // curve_15_a
	0x00, // curve_16_b
	0x20, // curve_16_a
	0x00, // curve_17_b
	0x20, // curve_17_a
	0x00, // curve_18_b
	0x20, // curve_18_a
	0x00, // curve_19_b
	0x20, // curve_19_a
	0x00, // curve_20_b
	0x20, // curve_20_a
	0x00, // curve_21_b
	0x20, // curve_21_a
	0x00, // curve_22_b
	0x20, // curve_22_a
	0x00, // curve_23_b
	0x20, // curve_23_a
	0x00, // curve_24_b
	0xff, // curve_24_a
};

static unsigned char NATURAL_EBOOK_2[] = {
	0xBA, // Start offset 0x09D4, base B1h
	0x05, // ascr_strength 00000  ascr_on 1  skin_cb 01
	0x9e, // skin_cb 100111  skin_cr 10
	0xa4, // skin_cr 101001  up_distance 00
	0x5c, // up_distance 010111  down_distance 00                                       
	0xa4, // down_distance 101001  right_distance 00                                    
	0x64, // right_distance 011001  left_distance 00                                    
	0x9c, // left_distance 100111  up_divided_distance 00
	0x16, // up_divided_distance 00010110
	0x42, // up_divided_distance 01000010
	0xc0, // up_divided_distance 11  down_divided_distance 000000
	0xc7, // down_divided_distance 11000111
	0xd0, // down_divided_distance 110100 right_divided_distance 00
	0x14, // right_divided_distance 00010100
	0x7b, // right_divided_distance 01111011
	0x00, // right_divided_distance 00  left_divided_distance 000000
	0xd2, // left_divided_distance 11010010
	0x0f, // left_divided_distance 000011  ascr_skin_Rr 11
	0xfc, // ascr_skin_Rr 111111  ascr_skin_Rg 00
	0x00, // ascr_skin_Rg 000000  ascr_skin_Rb 00
	0x03, // ascr_skin_Rb 000000  ascr_skin_Yr 11
	0xff, // ascr_skin_Yr 111111  ascr_skin_Yg 11
	0xfc, // ascr_skin_Yg 111111  ascr_skin_Yb 00
	0x03, // ascr_skin_Yb 000000  ascr_skin_Mr 11
	0xfc, // ascr_skin_Mr 111111  ascr_skin_Mg 00
	0x03, // ascr_skin_Mg 000000  ascr_skin_Mb 11
	0xff, // ascr_skin_Mb 111111  ascr_skin_Wr 11
	0xff, // ascr_skin_Wr 111111  ascr_skin_Wg 11
	0xff, // ascr_skin_Wg 111111  ascr_skin_Wb 11
	0xfc, // ascr_skin_Wb 111111  ascr_Cr 00
	0x03, // ascr_Cr 000000  ascr_Rr 11
	0xff, // ascr_Rr 111111  ascr_Cg 11
	0xfc, // ascr_Cg 111111  ascr_Rg 00
	0x03, // ascr_Rg 000000  ascr_Cb 11
	0xfc, // ascr_Cb 111111  ascr_Rb 00
	0x03, // ascr_Rb 000000  ascr_Mr 11
	0xfc, // ascr_Mr 111111  ascr_Gr 00
	0x00, // ascr_Gr 000000  ascr_Mg 00
	0x03, // ascr_Mg 000000  ascr_Gg 11
	0xff, // ascr_Gg 111111  ascr_Mb 11
	0xfc, // ascr_Mb 111111  ascr_Gb 00
	0x03, // ascr_Gb 000000  ascr_Yr 11
	0xfc, // ascr_Yr 111111  ascr_Br 00
	0x03, // ascr_Br 000000  ascr_Yg 11
	0xfc, // ascr_Yg 111111  ascr_Bg 00
	0x00, // ascr_Bg 000000  ascr_Yb 00
	0x03, // ascr_Yb 000000  ascr_Bb 11
	0xff, // ascr_Bb 111111  ascr_Wr 11
	0xfc, // ascr_Wr 111111  ascr_Kr 00
	0x03, // ascr_Kr 000000  ascr_Wg 11
	0xdc, // ascr_Wg 110111  ascr_Kg 00
	0x03, // ascr_Kg 000000  ascr_Wb 11
	0x98, // ascr_Wb 100110  ascr_Kb 00
	0x00, // ascr_Kb 000000  reserved 00
	0x0f, // reserved 0000  vsync_mask 1111
	/* end */
};

static unsigned char MOVIE_EBOOK_1[] = {
	/* start */
	0xBA, // Start offset 0x0982, base B1h
	0x00, // data_width 00  lce_module 0  lcd_bypass 0  lcd_roi 00  algo_module 0  algo_bypass 0
	0x30, // algo_roi 00  ascr_module 1  ascr_bypass 1  ascr_roi 00  roi_type 00
	0x00, // roi0_x_start 00000000
	0x00, // roi0_x_start 0000  roi0_x_end 0000
	0x00, // roi0_x_end 00000000
	0x00, // roi0_y_start 00000000
	0x00, // roi0_y_start 0000  roi0_y_end 0000
	0x00, // roi0_y_end 00000000
	0x00, // roi1_x_start 00000000
	0x00, // roi1_x_start 0000  roi1_x_end 0000
	0x00, // roi1_x_end 00000000
	0x00, // roi1_y_start 00000000
	0x00, // roi1_y_start 0000  roi1_y_end 0000
	0x00, // roi1_y_end 00000000
	0xa0, // hsize 10100000
	0x06, // hsize 0000  vsize 0110
	0x40, // vsize 01000000
	0x62, // gain 110000 cgain 10
	0x48, // cgain 0100  scene_on 1  scene_tran 000
	0x14, // scene_tran 0  min_diff 0010100
	0xb3, // illum_gain 10110011
	0x87, // ref_offset 10000111
	0x40, // ref_offset 0  ref_gain 1000000
	0x36, // ref_gain 00  hbsize 110  vbsize 110
	0xfa, // bth 11111010
	0x5a, // bin_size_ratio 0101101  dth 0
	0xe5, // dth 11  min_ref_offset 100101
	0x83, // min_ref_offset 10  gamma 0  cs 0 sharpen 0 nr 0 mask_th 11
	0xfc, // mask_th 111111  sharpen_weight 00
	0x40, // sharpen_weight 01000000
	0x14, // max_plus 00010100
	0x02, // max_plus 000  max_minus 00010
	0x81, // max_minus 100000  cs_gain 01
	0x00, // cs_gain 00000000
	0x00, // curve_1_b
	0x20, // curve_1_a
	0x00, // curve_2_b
	0x20, // curve_2_a
	0x00, // curve_3_b
	0x20, // curve_3_a
	0x00, // curve_4_b
	0x20, // curve_4_a
	0x00, // curve_5_b
	0x20, // curve_5_a
	0x00, // curve_6_b
	0x20, // curve_6_a
	0x00, // curve_7_b
	0x20, // curve_7_a
	0x00, // curve_8_b
	0x20, // curve_8_a
	0x00, // curve_9_b
	0x20, // curve_9_a
	0x00, // curve_10_b
	0x20, // curve_10_a
	0x00, // curve_11_b
	0x20, // curve_11_a
	0x00, // curve_12_b
	0x20, // curve_12_a
	0x00, // curve_13_b
	0x20, // curve_13_a
	0x00, // curve_14_b
	0x20, // curve_14_a
	0x00, // curve_15_b
	0x20, // curve_15_a
	0x00, // curve_16_b
	0x20, // curve_16_a
	0x00, // curve_17_b
	0x20, // curve_17_a
	0x00, // curve_18_b
	0x20, // curve_18_a
	0x00, // curve_19_b
	0x20, // curve_19_a
	0x00, // curve_20_b
	0x20, // curve_20_a
	0x00, // curve_21_b
	0x20, // curve_21_a
	0x00, // curve_22_b
	0x20, // curve_22_a
	0x00, // curve_23_b
	0x20, // curve_23_a
	0x00, // curve_24_b
	0xff, // curve_24_a
};

static unsigned char MOVIE_EBOOK_2[] = {
	0xBA, // Start offset 0x09D4, base B1h
	0x05, // ascr_strength 00000  ascr_on 1  skin_cb 01
	0x9e, // skin_cb 100111  skin_cr 10
	0xa4, // skin_cr 101001  up_distance 00
	0x5c, // up_distance 010111  down_distance 00                                       
	0xa4, // down_distance 101001  right_distance 00                                    
	0x64, // right_distance 011001  left_distance 00                                    
	0x9c, // left_distance 100111  up_divided_distance 00
	0x16, // up_divided_distance 00010110
	0x42, // up_divided_distance 01000010
	0xc0, // up_divided_distance 11  down_divided_distance 000000
	0xc7, // down_divided_distance 11000111
	0xd0, // down_divided_distance 110100 right_divided_distance 00
	0x14, // right_divided_distance 00010100
	0x7b, // right_divided_distance 01111011
	0x00, // right_divided_distance 00  left_divided_distance 000000
	0xd2, // left_divided_distance 11010010
	0x0f, // left_divided_distance 000011  ascr_skin_Rr 11
	0xfc, // ascr_skin_Rr 111111  ascr_skin_Rg 00
	0x00, // ascr_skin_Rg 000000  ascr_skin_Rb 00
	0x03, // ascr_skin_Rb 000000  ascr_skin_Yr 11
	0xff, // ascr_skin_Yr 111111  ascr_skin_Yg 11
	0xfc, // ascr_skin_Yg 111111  ascr_skin_Yb 00
	0x03, // ascr_skin_Yb 000000  ascr_skin_Mr 11
	0xfc, // ascr_skin_Mr 111111  ascr_skin_Mg 00
	0x03, // ascr_skin_Mg 000000  ascr_skin_Mb 11
	0xff, // ascr_skin_Mb 111111  ascr_skin_Wr 11
	0xff, // ascr_skin_Wr 111111  ascr_skin_Wg 11
	0xff, // ascr_skin_Wg 111111  ascr_skin_Wb 11
	0xfc, // ascr_skin_Wb 111111  ascr_Cr 00
	0x03, // ascr_Cr 000000  ascr_Rr 11
	0xff, // ascr_Rr 111111  ascr_Cg 11
	0xfc, // ascr_Cg 111111  ascr_Rg 00
	0x03, // ascr_Rg 000000  ascr_Cb 11
	0xfc, // ascr_Cb 111111  ascr_Rb 00
	0x03, // ascr_Rb 000000  ascr_Mr 11
	0xfc, // ascr_Mr 111111  ascr_Gr 00
	0x00, // ascr_Gr 000000  ascr_Mg 00
	0x03, // ascr_Mg 000000  ascr_Gg 11
	0xff, // ascr_Gg 111111  ascr_Mb 11
	0xfc, // ascr_Mb 111111  ascr_Gb 00
	0x03, // ascr_Gb 000000  ascr_Yr 11
	0xfc, // ascr_Yr 111111  ascr_Br 00
	0x03, // ascr_Br 000000  ascr_Yg 11
	0xfc, // ascr_Yg 111111  ascr_Bg 00
	0x00, // ascr_Bg 000000  ascr_Yb 00
	0x03, // ascr_Yb 000000  ascr_Bb 11
	0xff, // ascr_Bb 111111  ascr_Wr 11
	0xfc, // ascr_Wr 111111  ascr_Kr 00
	0x03, // ascr_Kr 000000  ascr_Wg 11
	0xdc, // ascr_Wg 110111  ascr_Kg 00
	0x03, // ascr_Kg 000000  ascr_Wb 11
	0x98, // ascr_Wb 100110  ascr_Kb 00
	0x00, // ascr_Kb 000000  reserved 00
	0x0f, // reserved 0000  vsync_mask 1111
	/* end */
};

static unsigned char AUTO_EMAIL_1[] = {
	/* start */
	0xBA, // Start offset 0x0982, base B1h
	0x00, // data_width 00  lce_module 0  lcd_bypass 0  lcd_roi 00  algo_module 0  algo_bypass 0
	0x30, // algo_roi 00  ascr_module 1  ascr_bypass 1  ascr_roi 00  roi_type 00
	0x00, // roi0_x_start 00000000
	0x00, // roi0_x_start 0000  roi0_x_end 0000
	0x00, // roi0_x_end 00000000
	0x00, // roi0_y_start 00000000
	0x00, // roi0_y_start 0000  roi0_y_end 0000
	0x00, // roi0_y_end 00000000
	0x00, // roi1_x_start 00000000
	0x00, // roi1_x_start 0000  roi1_x_end 0000
	0x00, // roi1_x_end 00000000
	0x00, // roi1_y_start 00000000
	0x00, // roi1_y_start 0000  roi1_y_end 0000
	0x00, // roi1_y_end 00000000
	0xa0, // hsize 10100000
	0x06, // hsize 0000  vsize 0110
	0x40, // vsize 01000000
	0x62, // gain 110000 cgain 10
	0x48, // cgain 0100  scene_on 1  scene_tran 000
	0x14, // scene_tran 0  min_diff 0010100
	0xb3, // illum_gain 10110011
	0x87, // ref_offset 10000111
	0x40, // ref_offset 0  ref_gain 1000000
	0x36, // ref_gain 00  hbsize 110  vbsize 110
	0xfa, // bth 11111010
	0x5a, // bin_size_ratio 0101101  dth 0
	0xe5, // dth 11  min_ref_offset 100101
	0x83, // min_ref_offset 10  gamma 0  cs 0 sharpen 0 nr 0 mask_th 11
	0xfc, // mask_th 111111  sharpen_weight 00
	0x40, // sharpen_weight 01000000
	0x14, // max_plus 00010100
	0x02, // max_plus 000  max_minus 00010
	0x81, // max_minus 100000  cs_gain 01
	0x00, // cs_gain 00000000
	0x00, // curve_1_b
	0x20, // curve_1_a
	0x00, // curve_2_b
	0x20, // curve_2_a
	0x00, // curve_3_b
	0x20, // curve_3_a
	0x00, // curve_4_b
	0x20, // curve_4_a
	0x00, // curve_5_b
	0x20, // curve_5_a
	0x00, // curve_6_b
	0x20, // curve_6_a
	0x00, // curve_7_b
	0x20, // curve_7_a
	0x00, // curve_8_b
	0x20, // curve_8_a
	0x00, // curve_9_b
	0x20, // curve_9_a
	0x00, // curve_10_b
	0x20, // curve_10_a
	0x00, // curve_11_b
	0x20, // curve_11_a
	0x00, // curve_12_b
	0x20, // curve_12_a
	0x00, // curve_13_b
	0x20, // curve_13_a
	0x00, // curve_14_b
	0x20, // curve_14_a
	0x00, // curve_15_b
	0x20, // curve_15_a
	0x00, // curve_16_b
	0x20, // curve_16_a
	0x00, // curve_17_b
	0x20, // curve_17_a
	0x00, // curve_18_b
	0x20, // curve_18_a
	0x00, // curve_19_b
	0x20, // curve_19_a
	0x00, // curve_20_b
	0x20, // curve_20_a
	0x00, // curve_21_b
	0x20, // curve_21_a
	0x00, // curve_22_b
	0x20, // curve_22_a
	0x00, // curve_23_b
	0x20, // curve_23_a
	0x00, // curve_24_b
	0xff, // curve_24_a
};

static unsigned char AUTO_EMAIL_2[] = {
	0xBA, // Start offset 0x09D4, base B1h
	0x05, // ascr_strength 00000  ascr_on 1  skin_cb 01
	0x9e, // skin_cb 100111  skin_cr 10
	0xa4, // skin_cr 101001  up_distance 00
	0x5c, // up_distance 010111  down_distance 00                                       
	0xa4, // down_distance 101001  right_distance 00                                    
	0x64, // right_distance 011001  left_distance 00                                    
	0x9c, // left_distance 100111  up_divided_distance 00
	0x16, // up_divided_distance 00010110
	0x42, // up_divided_distance 01000010
	0xc0, // up_divided_distance 11  down_divided_distance 000000
	0xc7, // down_divided_distance 11000111
	0xd0, // down_divided_distance 110100 right_divided_distance 00
	0x14, // right_divided_distance 00010100
	0x7b, // right_divided_distance 01111011
	0x00, // right_divided_distance 00  left_divided_distance 000000
	0xd2, // left_divided_distance 11010010
	0x0f, // left_divided_distance 000011  ascr_skin_Rr 11
	0xfc, // ascr_skin_Rr 111111  ascr_skin_Rg 00
	0x00, // ascr_skin_Rg 000000  ascr_skin_Rb 00
	0x03, // ascr_skin_Rb 000000  ascr_skin_Yr 11
	0xff, // ascr_skin_Yr 111111  ascr_skin_Yg 11
	0xfc, // ascr_skin_Yg 111111  ascr_skin_Yb 00
	0x03, // ascr_skin_Yb 000000  ascr_skin_Mr 11
	0xfc, // ascr_skin_Mr 111111  ascr_skin_Mg 00
	0x03, // ascr_skin_Mg 000000  ascr_skin_Mb 11
	0xff, // ascr_skin_Mb 111111  ascr_skin_Wr 11
	0xff, // ascr_skin_Wr 111111  ascr_skin_Wg 11
	0xff, // ascr_skin_Wg 111111  ascr_skin_Wb 11
	0xfc, // ascr_skin_Wb 111111  ascr_Cr 00
	0x03, // ascr_Cr 000000  ascr_Rr 11
	0xff, // ascr_Rr 111111  ascr_Cg 11
	0xfc, // ascr_Cg 111111  ascr_Rg 00
	0x03, // ascr_Rg 000000  ascr_Cb 11
	0xfc, // ascr_Cb 111111  ascr_Rb 00
	0x03, // ascr_Rb 000000  ascr_Mr 11
	0xfc, // ascr_Mr 111111  ascr_Gr 00
	0x00, // ascr_Gr 000000  ascr_Mg 00
	0x03, // ascr_Mg 000000  ascr_Gg 11
	0xff, // ascr_Gg 111111  ascr_Mb 11
	0xfc, // ascr_Mb 111111  ascr_Gb 00
	0x03, // ascr_Gb 000000  ascr_Yr 11
	0xfc, // ascr_Yr 111111  ascr_Br 00
	0x03, // ascr_Br 000000  ascr_Yg 11
	0xfc, // ascr_Yg 111111  ascr_Bg 00
	0x00, // ascr_Bg 000000  ascr_Yb 00
	0x03, // ascr_Yb 000000  ascr_Bb 11
	0xff, // ascr_Bb 111111  ascr_Wr 11
	0xfc, // ascr_Wr 111111  ascr_Kr 00
	0x03, // ascr_Kr 000000  ascr_Wg 11
	0xe8, // ascr_Wg 111010  ascr_Kg 00
	0x03, // ascr_Kg 000000  ascr_Wb 11
	0xbc, // ascr_Wb 101111  ascr_Kb 00
	0x00, // ascr_Kb 000000  reserved 00
	0x0f, // reserved 0000  vsync_mask 1111
	/* end */
};

static unsigned char LOCAL_CE_1[] = {
	/* start */
	0xBA, // Start offset 0x0982, base B1h
	0x33, // data_width 00  lce_module 1  lcd_bypass 1  lcd_roi 00  algo_module 1  algo_bypass 1
	0x00, // algo_roi 00  ascr_module 0  ascr_bypass 0  ascr_roi 00  roi_type 00
	0x00, // roi0_x_start 00000000
	0x00, // roi0_x_start 0000  roi0_x_end 0000
	0x00, // roi0_x_end 00000000
	0x00, // roi0_y_start 00000000
	0x00, // roi0_y_start 0000  roi0_y_end 0000
	0x00, // roi0_y_end 00000000
	0x00, // roi1_x_start 00000000
	0x00, // roi1_x_start 0000  roi1_x_end 0000
	0x00, // roi1_x_end 00000000
	0x00, // roi1_y_start 00000000
	0x00, // roi1_y_start 0000  roi1_y_end 0000
	0x00, // roi1_y_end 00000000
	0xa0, // hsize 10100000
	0x06, // hsize 0000  vsize 0110
	0x40, // vsize 01000000
	0x17, // gain 000101 cgain 11
	0x00, // cgain 0000  scene_on 0  scene_tran 000
	0x14, // scene_tran 0  min_diff 0010100
	0x79, // illum_gain 01111001
	0xdf, // ref_offset 11011111
	0xac, // ref_offset 1  ref_gain 0101100
	0x3f, // ref_gain 00  hbsize 111  vbsize 111
	0xfa, // bth 11111010
	0xfe, // bin_size_ratio 1111111  dth 0
	0x10, // dth 00  min_ref_offset 010000
	0x2b, // min_ref_offset 00  gamma 1  cs 0 sharpen 1 nr 0 mask_th 11
	0xfc, // mask_th 111111  sharpen_weight 00
	0x40, // sharpen_weight 01000000
	0x14, // max_plus 00010100
	0x02, // max_plus 000  max_minus 00010
	0x81, // max_minus 100000  cs_gain 01
	0x90, // cs_gain 10010000
	0x00, // curve_1_b
	0x7b, // curve_1_a
	0x03, // curve_2_b
	0x48, // curve_2_a
	0x08, // curve_3_b
	0x32, // curve_3_a
	0x08, // curve_4_b
	0x32, // curve_4_a
	0x08, // curve_5_b
	0x32, // curve_5_a
	0x08, // curve_6_b
	0x32, // curve_6_a
	0x08, // curve_7_b
	0x32, // curve_7_a
	0x10, // curve_8_b
	0x28, // curve_8_a
	0x10, // curve_9_b
	0x28, // curve_9_a
	0x10, // curve_10_b
	0x28, // curve_10_a
	0x10, // curve_11_b
	0x28, // curve_11_a
	0x10, // curve_12_b
	0x28, // curve_12_a
	0x19, // curve_13_b
	0x22, // curve_13_a
	0x19, // curve_14_b
	0x22, // curve_14_a
	0x19, // curve_15_b
	0x22, // curve_15_a
	0x19, // curve_16_b
	0x22, // curve_16_a
	0x19, // curve_17_b
	0x22, // curve_17_a
	0x19, // curve_18_b
	0x22, // curve_18_a
	0x23, // curve_19_b
	0x1e, // curve_19_a
	0x2e, // curve_20_b
	0x1b, // curve_20_a
	0x33, // curve_21_b
	0x1a, // curve_21_a
	0x40, // curve_22_b
	0x18, // curve_22_a
	0x48, // curve_23_b
	0x17, // curve_23_a
	0x00, // curve_24_b
	0xff, // curve_24_a
};

static unsigned char LOCAL_CE_2[] = {
	0xBA, // Start offset 0x09D4, base B1h
	0x05, // ascr_strength 00000  ascr_on 1  skin_cb 01
	0x9e, // skin_cb 100111  skin_cr 10
	0xa4, // skin_cr 101001  up_distance 00
	0x5c, // up_distance 010111  down_distance 00                                       
	0xa4, // down_distance 101001  right_distance 00                                    
	0x64, // right_distance 011001  left_distance 00                                    
	0x9c, // left_distance 100111  up_divided_distance 00
	0x16, // up_divided_distance 00010110
	0x42, // up_divided_distance 01000010
	0xc0, // up_divided_distance 11  down_divided_distance 000000
	0xc7, // down_divided_distance 11000111
	0xd0, // down_divided_distance 110100 right_divided_distance 00
	0x14, // right_divided_distance 00010100
	0x7b, // right_divided_distance 01111011
	0x00, // right_divided_distance 00  left_divided_distance 000000
	0xd2, // left_divided_distance 11010010
	0x0f, // left_divided_distance 000011  ascr_skin_Rr 11
	0xfc, // ascr_skin_Rr 111111  ascr_skin_Rg 00
	0x00, // ascr_skin_Rg 000000  ascr_skin_Rb 00
	0x03, // ascr_skin_Rb 000000  ascr_skin_Yr 11
	0xff, // ascr_skin_Yr 111111  ascr_skin_Yg 11
	0xfc, // ascr_skin_Yg 111111  ascr_skin_Yb 00
	0x03, // ascr_skin_Yb 000000  ascr_skin_Mr 11
	0xfc, // ascr_skin_Mr 111111  ascr_skin_Mg 00
	0x03, // ascr_skin_Mg 000000  ascr_skin_Mb 11
	0xff, // ascr_skin_Mb 111111  ascr_skin_Wr 11
	0xff, // ascr_skin_Wr 111111  ascr_skin_Wg 11
	0xff, // ascr_skin_Wg 111111  ascr_skin_Wb 11
	0xfc, // ascr_skin_Wb 111111  ascr_Cr 00
	0x03, // ascr_Cr 000000  ascr_Rr 11
	0xff, // ascr_Rr 111111  ascr_Cg 11
	0xfc, // ascr_Cg 111111  ascr_Rg 00
	0x03, // ascr_Rg 000000  ascr_Cb 11
	0xfc, // ascr_Cb 111111  ascr_Rb 00
	0x03, // ascr_Rb 000000  ascr_Mr 11
	0xfc, // ascr_Mr 111111  ascr_Gr 00
	0x00, // ascr_Gr 000000  ascr_Mg 00
	0x03, // ascr_Mg 000000  ascr_Gg 11
	0xff, // ascr_Gg 111111  ascr_Mb 11
	0xfc, // ascr_Mb 111111  ascr_Gb 00
	0x03, // ascr_Gb 000000  ascr_Yr 11
	0xfc, // ascr_Yr 111111  ascr_Br 00
	0x03, // ascr_Br 000000  ascr_Yg 11
	0xfc, // ascr_Yg 111111  ascr_Bg 00
	0x00, // ascr_Bg 000000  ascr_Yb 00
	0x03, // ascr_Yb 000000  ascr_Bb 11
	0xff, // ascr_Bb 111111  ascr_Wr 11
	0xfc, // ascr_Wr 111111  ascr_Kr 00
	0x03, // ascr_Kr 000000  ascr_Wg 11
	0xfc, // ascr_Wg 111111  ascr_Kg 00
	0x03, // ascr_Kg 000000  ascr_Wb 11
	0xfc, // ascr_Wb 111111  ascr_Kb 00
	0x00, // ascr_Kb 000000  reserved 00
	0x0f, // reserved 0000  vsync_mask 1111
	/* end */
};

static unsigned char LEVEL1_UNLOCK[] = {
};

static unsigned char LEVEL1_LOCK[] = {
};

struct mdnie_table bypass_table[BYPASS_MAX] = {
	[BYPASS_ON] = MDNIE_SET(BYPASS)
};

struct mdnie_table accessibility_table[ACCESSIBILITY_MAX] = {
	[NEGATIVE] = MDNIE_SET(NEGATIVE),
	MDNIE_SET(COLOR_BLIND),
	MDNIE_SET(SCREEN_CURTAIN)
};

struct mdnie_table hbm_table[HBM_MAX] = {
	[HBM_ON] = MDNIE_SET(LOCAL_CE)
};

struct mdnie_table tuning_table[SCENARIO_MAX][MODE_MAX] = {
	{
		MDNIE_SET(DYNAMIC_UI),
		MDNIE_SET(STANDARD_UI),
		MDNIE_SET(NATURAL_UI),
		MDNIE_SET(MOVIE_UI),
		MDNIE_SET(AUTO_UI)
	}, {
		MDNIE_SET(DYNAMIC_VIDEO),
		MDNIE_SET(STANDARD_VIDEO),
		MDNIE_SET(NATURAL_VIDEO),
		MDNIE_SET(MOVIE_VIDEO),
		MDNIE_SET(AUTO_VIDEO)
	},
	[CAMERA_MODE] = {
		MDNIE_SET(CAMERA),
		MDNIE_SET(CAMERA),
		MDNIE_SET(CAMERA),
		MDNIE_SET(CAMERA),
		MDNIE_SET(AUTO_CAMERA)
	},
	[GALLERY_MODE] = {
		MDNIE_SET(DYNAMIC_GALLERY),
		MDNIE_SET(STANDARD_GALLERY),
		MDNIE_SET(NATURAL_GALLERY),
		MDNIE_SET(MOVIE_GALLERY),
		MDNIE_SET(AUTO_GALLERY)
	}, {
		MDNIE_SET(DYNAMIC_VT),
		MDNIE_SET(STANDARD_VT),
		MDNIE_SET(NATURAL_VT),
		MDNIE_SET(MOVIE_VT),
		MDNIE_SET(AUTO_VT)
	}, {
		MDNIE_SET(DYNAMIC_BROWSER),
		MDNIE_SET(STANDARD_BROWSER),
		MDNIE_SET(NATURAL_BROWSER),
		MDNIE_SET(MOVIE_BROWSER),
		MDNIE_SET(AUTO_BROWSER)
	}, {
		MDNIE_SET(DYNAMIC_EBOOK),
		MDNIE_SET(STANDARD_EBOOK),
		MDNIE_SET(NATURAL_EBOOK),
		MDNIE_SET(MOVIE_EBOOK),
		MDNIE_SET(AUTO_EBOOK)
	}, {
		MDNIE_SET(AUTO_EMAIL),
		MDNIE_SET(AUTO_EMAIL),
		MDNIE_SET(AUTO_EMAIL),
		MDNIE_SET(AUTO_EMAIL),
		MDNIE_SET(AUTO_EMAIL)
	}
};
#endif
