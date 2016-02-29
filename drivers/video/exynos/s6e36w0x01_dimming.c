/* linux/drivers/video/exynos/s6e36w0x01_dimming.c
 *
 * MIPI-DSI based S6E36W0X01 AMOLED 1.84 inch panel driver.
 *
 * Taeheon Kim, <th908.kim@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/kernel.h>
#include "s6e36w0x01_dimming.h"

/*#define SMART_DIMMING_DEBUG*/
#define RGB_COMPENSATION 30

static unsigned int ref_gamma[NUM_VREF][CI_MAX] = {
	{0x00, 0x00, 0x00},
	{0x80, 0x80, 0x80},
	{0x80, 0x80, 0x80},
	{0x80, 0x80, 0x80},
	{0x80, 0x80, 0x80},
	{0x80, 0x80, 0x80},
	{0x80, 0x80, 0x80},
	{0x80, 0x80, 0x80},
	{0x80, 0x80, 0x80},
	{0x100, 0x100, 0x100},
};

static const unsigned short ref_cd_tbl[MAX_GAMMA_CNT] = {
	110, 110, 198, 250, 300
};

static const int gradation_shift[MAX_GAMMA_CNT][NUM_VREF] = {
	{0, 10, 26, 26, 23, 18, 7, -2, -12, -22},	/*  2 cd */
	{0, 0, 2, 1, 0, 1, 0, 0, 0, 0},				/* 60 cd */
	{0, 0, 1, 1, 1, 1, 2, 1, 0, 0},				/* 120 cd */
	{0, 0, 1, 1, 2, 2, 2, 1, 0, 0},				/* 200 cd */
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0},				/* 300 cd */
};

static int rgb_offset[MAX_GAMMA_CNT][RGB_COMPENSATION] = {
	{0, 0, 0, 0, 0, 0, 0, 5, 0, 1, 5, 0, 0, 6, 0,0, 14, 0, 0, 11, 0, 0, 6, 0, 1, 3, 0, 0, 6, 0},		/*  2 cd */
	{0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 3, 0, 3, 4, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0},		/*  60 cd */
	{0, 0, 0, 0, 0, 0, -1, 3, 0, 3, 3, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},		/*  120 cd */
	{0, 0, 0, 0, 0, 0, 0, 1, 0, 3, 2, 0, -1, 0, 0, -2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},		/*  200 cd */
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},		/*  300 cd */
};

const unsigned int vref_index[NUM_VREF] = {
	TBL_INDEX_V0,
	TBL_INDEX_V3,
	TBL_INDEX_V11,
	TBL_INDEX_V23,
	TBL_INDEX_V35,
	TBL_INDEX_V51,
	TBL_INDEX_V87,
	TBL_INDEX_V151,
	TBL_INDEX_V203,
	TBL_INDEX_V255,
};

const int vreg_element_max[NUM_VREF] = {
	0x0f,
	0xff,
	0xff,
	0xff,
	0xff,
	0xff,
	0xff,
	0xff,
	0xff,
	0x1ff,
};

const struct v_constant fix_const[NUM_VREF] = {
	{.nu = 0,	.de = 860},
	{.nu = 64,	.de = 320},
	{.nu = 64,	.de = 320},
	{.nu = 64,	.de = 320},
	{.nu = 64,	.de = 320},
	{.nu = 64,	.de = 320},
	{.nu = 64,	.de = 320},
	{.nu = 64,	.de = 320},
	{.nu = 64,	.de = 320},
	{.nu = 72,	.de = 860},
};

const short vt_trans_volt[16] = {
	6451, 6361, 6271, 6181, 6091, 6001, 5911, 5821,
	5731, 5641, 5416, 5341, 5266, 5191, 5116, 5041,
};

const short v255_trans_volt[512] = {
	5911, 5904, 5896, 5889, 5881, 5874, 5866, 5859,
	5851, 5844, 5836, 5829, 5821, 5814, 5806, 5799,
	5791, 5784, 5776, 5769, 5761, 5754, 5746, 5739,
	5731, 5724, 5716, 5709, 5701, 5694, 5686, 5679,
	5671, 5664, 5656, 5649, 5641, 5634, 5626, 5619,
	5611, 5604, 5596, 5589, 5581, 5574, 5566, 5559,
	5551, 5544, 5536, 5529, 5521, 5514, 5506, 5499,
	5491, 5484, 5476, 5469, 5461, 5454, 5446, 5439,
	5431, 5424, 5416, 5409, 5401, 5394, 5386, 5379,
	5371, 5363, 5356, 5348, 5341, 5333, 5326, 5318,
	5311, 5303, 5296, 5288, 5281, 5273, 5266, 5258,
	5251, 5243, 5236, 5228, 5221, 5213, 5206, 5198,
	5191, 5183, 5176, 5168, 5161, 5153, 5146, 5138,
	5131, 5123, 5116, 5108, 5101, 5093, 5086, 5078,
	5071, 5063, 5056, 5048, 5041, 5033, 5026, 5018,
	5011, 5003, 4996, 4988, 4981, 4973, 4966, 4958,
	4951, 4943, 4936, 4928, 4921, 4913, 4906, 4898,
	4891, 4883, 4876, 4868, 4861, 4853, 4846, 4838,
	4831, 4823, 4816, 4808, 4801, 4793, 4786, 4778,
	4771, 4763, 4756, 4748, 4741, 4733, 4726, 4718,
	4711, 4703, 4696, 4688, 4681, 4673, 4666, 4658,
	4651, 4643, 4636, 4628, 4621, 4613, 4606, 4598,
	4591, 4583, 4576, 4568, 4561, 4553, 4546, 4538,
	4531, 4523, 4516, 4508, 4501, 4493, 4486, 4478,
	4471, 4463, 4456, 4448, 4441, 4433, 4426, 4418,
	4411, 4403, 4396, 4388, 4381, 4373, 4366, 4358,
	4351, 4343, 4336, 4328, 4321, 4313, 4306, 4298,
	4291, 4283, 4276, 4268, 4261, 4253, 4246, 4238,
	4231, 4223, 4216, 4208, 4201, 4193, 4186, 4178,
	4171, 4163, 4156, 4148, 4141, 4133, 4126, 4118,
	4111, 4103, 4096, 4088, 4081, 4073, 4066, 4058,
	4051, 4043, 4036, 4028, 4021, 4013, 4006, 3998,
	3991, 3983, 3976, 3968, 3961, 3953, 3946, 3938,
	3931, 3923, 3916, 3908, 3901, 3893, 3886, 3878,
	3871, 3863, 3856, 3848, 3841, 3833, 3826, 3818,
	3811, 3803, 3796, 3788, 3781, 3773, 3766, 3758,
	3751, 3743, 3736, 3728, 3721, 3713, 3706, 3698,
	3691, 3683, 3676, 3668, 3661, 3653, 3646, 3638,
	3631, 3623, 3616, 3608, 3601, 3593, 3586, 3578,
	3571, 3563, 3556, 3548, 3541, 3533, 3526, 3518,
	3511, 3503, 3496, 3488, 3481, 3473, 3466, 3458,
	3451, 3443, 3436, 3428, 3421, 3413, 3406, 3398,
	3391, 3383, 3376, 3368, 3361, 3353, 3346, 3338,
	3331, 3323, 3316, 3308, 3301, 3293, 3286, 3278,
	3271, 3263, 3256, 3248, 3241, 3233, 3226, 3218,
	3211, 3203, 3196, 3188, 3181, 3173, 3166, 3158,
	3151, 3143, 3136, 3128, 3121, 3113, 3106, 3098,
	3091, 3083, 3076, 3068, 3061, 3053, 3046, 3038,
	3031, 3023, 3016, 3008, 3001, 2993, 2986, 2978,
	2971, 2963, 2956, 2948, 2941, 2933, 2926, 2918,
	2911, 2903, 2896, 2888, 2881, 2873, 2866, 2858,
	2851, 2843, 2836, 2828, 2821, 2813, 2806, 2798,
	2791, 2783, 2776, 2768, 2761, 2753, 2746, 2738,
	2731, 2723, 2716, 2708, 2701, 2693, 2685, 2678,
	2670, 2663, 2655, 2648, 2640, 2633, 2625, 2618,
	2610, 2603, 2595, 2588, 2580, 2573, 2565, 2558,
	2550, 2543, 2535, 2528, 2520, 2513, 2505, 2498,
	2490, 2483, 2475, 2468, 2460, 2453, 2445, 2438,
	2430, 2423, 2415, 2408, 2400, 2393, 2385, 2378,
	2370, 2363, 2355, 2348, 2340, 2333, 2325, 2318,
	2310, 2303, 2295, 2288, 2280, 2273, 2265, 2258,
	2250, 2243, 2235, 2228, 2220, 2213, 2205, 2198,
	2190, 2183, 2175, 2168, 2160, 2153, 2145, 2138,
	2130, 2123, 2115, 2108, 2100, 2093, 2085, 2078,
};

const short v3_v203_trans_volt[256] = {
	205, 208, 211, 214, 218, 221, 224, 227,
	230, 234, 237, 240, 243, 246, 250, 253,
	256, 259, 262, 266, 269, 272, 275, 278,
	282, 285, 288, 291, 294, 298, 301, 304,
	307, 310, 314, 317, 320, 323, 326, 330,
	333, 336, 339, 342, 346, 349, 352, 355,
	358, 362, 365, 368, 371, 374, 378, 381,
	384, 387, 390, 394, 397, 400, 403, 406,
	410, 413, 416, 419, 422, 426, 429, 432,
	435, 438, 442, 445, 448, 451, 454, 458,
	461, 464, 467, 470, 474, 477, 480, 483,
	486, 490, 493, 496, 499, 502, 506, 509,
	512, 515, 518, 522, 525, 528, 531, 534,
	538, 541, 544, 547, 550, 554, 557, 560,
	563, 566, 570, 573, 576, 579, 582, 586,
	589, 592, 595, 598, 602, 605, 608, 611,
	614, 618, 621, 624, 627, 630, 634, 637,
	640, 643, 646, 650, 653, 656, 659, 662,
	666, 669, 672, 675, 678, 682, 685, 688,
	691, 694, 698, 701, 704, 707, 710, 714,
	717, 720, 723, 726, 730, 733, 736, 739,
	742, 746, 749, 752, 755, 758, 762, 765,
	768, 771, 774, 778, 781, 784, 787, 790,
	794, 797, 800, 803, 806, 810, 813, 816,
	819, 822, 826, 829, 832, 835, 838, 842,
	845, 848, 851, 854, 858, 861, 864, 867,
	870, 874, 877, 880, 883, 886, 890, 893,
	896, 899, 902, 906, 909, 912, 915, 918,
	922, 925, 928, 931, 934, 938, 941, 944,
	947, 950, 954, 957, 960, 963, 966, 970,
	973, 976, 979, 982, 986, 989, 992, 995,
	998, 1002, 1005, 1008, 1011, 1014, 1018, 1021,
};

const short int_tbl_v0_v3[2] = {
	341, 683,
};

const short int_tbl_v3_v11[7] = {
	128, 256, 384, 512, 640, 768, 896,
};

const short int_tbl_v11_v23[11] = {
	85, 171, 256, 341, 427, 512, 597, 683, 768, 853, 939,
};

const short int_tbl_v23_v35[11] = {
	85, 171, 256, 341, 427, 512, 597, 683, 768, 853, 939,
};

const short int_tbl_v35_v51[15] = {
	64, 128, 192, 256, 320, 384, 448, 512,
	576, 640, 704, 768, 832, 896, 960,
};

const short int_tbl_v51_v87[35] = {
	28, 57, 85, 114, 142, 171, 199, 228,
	256, 284, 313, 341, 370, 398, 427, 455,
	484, 512, 540, 569, 597, 626, 654, 683,
	711, 740, 768, 796, 825, 853, 882, 910,
	939, 967, 996,
};

const short int_tbl_v87_v151[63] = {
	16, 32, 48, 64, 80, 96, 112, 128,
	144, 160, 176, 192, 208, 224, 240, 256,
	272, 288, 304, 320, 336, 352, 368, 384,
	400, 416, 432, 448, 464, 480, 496, 512,
	528, 544, 560, 576, 592, 608, 624, 640,
	656, 672, 688, 704, 720, 736, 752, 768,
	784, 800, 816, 832, 848, 864, 880, 896,
	912, 928, 944, 960, 976, 992, 1008,
};

const short int_tbl_v151_v203[51] = {
	20, 39, 59, 79, 98, 118, 138, 158,
	177, 197, 217, 236, 256, 276, 295, 315,
	335, 354, 374, 394, 414, 433, 453, 473,
	492, 512, 532, 551, 571, 591, 610, 630,
	650, 670, 689, 709, 729, 748, 768, 788,
	807, 827, 847, 866, 886, 906, 926, 945,
	965, 985, 1004,
};

const short int_tbl_v203_v255[51] = {
	20, 39, 59, 79, 98, 118, 138, 158,
	177, 197, 217, 236, 256, 276, 295, 315,
	335, 354, 374, 394, 414, 433, 453, 473,
	492, 512, 532, 551, 571, 591, 610, 630,
	650, 670, 689, 709, 729, 748, 768, 788,
	807, 827, 847, 866, 886, 906, 926, 945,
	965, 985, 1004,
};

const int gamma_tbl[256] = {
	0, 5, 24, 60, 112, 184, 274, 385,
	516, 669, 844, 1041, 1260, 1503, 1769, 2059,
	2373, 2711, 3075, 3463, 3877, 4316, 4781, 5272,
	5790, 6334, 6905, 7503, 8128, 8780, 9460, 10167,
	10903, 11667, 12458, 13279, 14128, 15006, 15912, 16848,
	17813, 18808, 19832, 20885, 21969, 23082, 24226, 25400,
	26604, 27838, 29104, 30399, 31726, 33084, 34473, 35893,
	37344, 38827, 40342, 41888, 43465, 45075, 46717, 48391,
	50097, 51835, 53605, 55408, 57244, 59113, 61014, 62948,
	64915, 66915, 68948, 71014, 73114, 75247, 77414, 79614,
	81848, 84116, 86418, 88753, 91123, 93526, 95964, 98436,
	100942, 103483, 106058, 108668, 111313, 113992, 116706, 119455,
	122239, 125057, 127911, 130800, 133725, 136684, 139679, 142710,
	145776, 148877, 152014, 155187, 158396, 161640, 164921, 168237,
	171590, 174978, 178403, 181864, 185361, 188895, 192465, 196072,
	199715, 203395, 207111, 210864, 214654, 218481, 222345, 226245,
	230183, 234158, 238170, 242219, 246306, 250429, 254590, 258789,
	263025, 267299, 271610, 275959, 280345, 284770, 289232, 293732,
	298270, 302846, 307459, 312111, 316802, 321530, 326296, 331101,
	335944, 340826, 345746, 350704, 355701, 360737, 365811, 370924,
	376076, 381266, 386496, 391764, 397071, 402417, 407802, 413226,
	418689, 424192, 429733, 435314, 440935, 446594, 452293, 458031,
	463809, 469627, 475484, 481380, 487316, 493292, 499308, 505364,
	511459, 517594, 523769, 529984, 536240, 542535, 548870, 555245,
	561661, 568117, 574613, 581149, 587726, 594343, 601001, 607699,
	614437, 621216, 628036, 634896, 641797, 648739, 655721, 662745,
	669809, 676914, 684060, 691246, 698474, 705743, 713053, 720404,
	727796, 735230, 742704, 750220, 757777, 765375, 773015, 780697,
	788419, 796183, 803989, 811836, 819725, 827655, 835628, 843641,
	851697, 859794, 867933, 876114, 884337, 892602, 900909, 909258,
	917648, 926081, 934556, 943073, 951632, 960234, 968877, 977563,
	986291, 995062, 1003875, 1012730, 1021628, 1030568, 1039551, 1048576,
};

const int gamma_multi_tbl[256] = {
	0, 2, 7, 17, 33, 54, 80, 113,
	151, 196, 247, 305, 369, 440, 518, 603,
	695, 794, 901, 1015, 1136, 1264, 1401, 1545,
	1696, 1856, 2023, 2198, 2381, 2572, 2771, 2979,
	3194, 3418, 3650, 3890, 4139, 4396, 4662, 4936,
	5219, 5510, 5810, 6119, 6436, 6762, 7097, 7441,
	7794, 8156, 8526, 8906, 9295, 9693, 10099, 10516,
	10941, 11375, 11819, 12272, 12734, 13206, 13687, 14177,
	14677, 15186, 15705, 16233, 16771, 17318, 17875, 18442,
	19018, 19604, 20200, 20805, 21420, 22045, 22680, 23325,
	23979, 24643, 25318, 26002, 26696, 27400, 28114, 28839,
	29573, 30317, 31072, 31836, 32611, 33396, 34191, 34997,
	35812, 36638, 37474, 38320, 39177, 40044, 40922, 41809,
	42708, 43616, 44535, 45465, 46405, 47356, 48317, 49288,
	50270, 51263, 52267, 53280, 54305, 55340, 56386, 57443,
	58510, 59588, 60677, 61777, 62887, 64008, 65140, 66283,
	67436, 68601, 69776, 70963, 72160, 73368, 74587, 75817,
	77058, 78310, 79573, 80847, 82132, 83429, 84736, 86054,
	87384, 88724, 90076, 91439, 92813, 94198, 95595, 97002,
	98421, 99851, 101293, 102745, 104209, 105685, 107171, 108669,
	110178, 111699, 113231, 114775, 116329, 117896, 119473, 121062,
	122663, 124275, 125898, 127533, 129180, 130838, 132508, 134189,
	135882, 137586, 139302, 141029, 142768, 144519, 146282, 148056,
	149841, 151639, 153448, 155269, 157101, 158946, 160802, 162670,
	164549, 166440, 168344, 170259, 172185, 174124, 176074, 178037,
	180011, 181997, 183995, 186005, 188027, 190060, 192106, 194163,
	196233, 198315, 200408, 202514, 204631, 206761, 208902, 211056,
	213222, 215399, 217589, 219791, 222005, 224231, 226469, 228720,
	230982, 233257, 235544, 237843, 240154, 242477, 244813, 247161,
	249521, 251893, 254277, 256674, 259083, 261505, 263938, 266384,
	268842, 271313, 273796, 276291, 278799, 281318, 283851, 286395,
	288953, 291522, 294104, 296698, 299305, 301924, 304556, 307200,
};

const unsigned char lookup_tbl[301] = {
	0, 19, 26, 31, 36, 40, 43, 46,
	49, 52, 54, 57, 59, 61, 63, 65,
	67, 69, 71, 73, 74, 76, 78, 79,
	81, 82, 84, 85, 87, 88, 90, 91,
	92, 93, 95, 96, 97, 98, 100, 101,
	102, 103, 104, 105, 107, 108, 109, 110,
	111, 112, 113, 114, 115, 116, 117, 118,
	119, 120, 121, 122, 123, 124, 125, 125,
	126, 127, 128, 129, 130, 131, 132, 132,
	133, 134, 135, 136, 137, 137, 138, 139,
	140, 141, 141, 142, 143, 144, 145, 145,
	146, 147, 148, 148, 149, 150, 150, 151,
	152, 153, 153, 154, 155, 155, 156, 157,
	158, 158, 159, 160, 160, 161, 162, 162,
	163, 164, 164, 165, 166, 166, 167, 167,
	168, 169, 169, 170, 171, 171, 172, 173,
	173, 174, 174, 175, 176, 176, 177, 177,
	178, 179, 179, 180, 180, 181, 182, 182,
	183, 183, 184, 184, 185, 186, 186, 187,
	187, 188, 188, 189, 189, 190, 191, 191,
	192, 192, 193, 193, 194, 194, 195, 195,
	196, 196, 197, 198, 198, 199, 199, 200,
	200, 201, 201, 202, 202, 203, 203, 204,
	204, 205, 205, 206, 206, 207, 207, 208,
	208, 209, 209, 210, 210, 211, 211, 212,
	212, 213, 213, 214, 214, 214, 215, 215,
	216, 216, 217, 217, 218, 218, 219, 219,
	220, 220, 221, 221, 221, 222, 222, 223,
	223, 224, 224, 225, 225, 226, 226, 226,
	227, 227, 228, 228, 229, 229, 230, 230,
	230, 231, 231, 232, 232, 233, 233, 233,
	234, 234, 235, 235, 236, 236, 236, 237,
	237, 238, 238, 239, 239, 239, 240, 240,
	241, 241, 241, 242, 242, 243, 243, 243,
	244, 244, 245, 245, 246, 246, 246, 247,
	247, 248, 248, 248, 249, 249, 250, 250,
	250, 251, 251, 251, 252, 252, 253, 253,
	253, 254, 254, 255, 255,
};

static int calc_vt_volt(int gamma)
{
	int max;

	max = sizeof(vt_trans_volt) >> 2;
	if (gamma > max) {
		pr_warn("%s: exceed gamma value\n", __func__);
		gamma = max;
	}

	return (int)vt_trans_volt[gamma];
}

static int calc_v0_volt(struct smart_dimming *dimming, int color)
{
	return MULTIPLE_VREGOUT;
}

static int calc_v3_volt(struct smart_dimming *dimming, int color)
{
	int ret, v11, gamma;

	gamma = dimming->gamma[V3][color];

	if (gamma > vreg_element_max[V3]) {
		pr_warn("%s : gamma overflow : %d\n", __func__, gamma);
		gamma = vreg_element_max[V3];
	}
	if (gamma < 0) {
		pr_warn("%s : gamma undeflow : %d\n", __func__, gamma);
		gamma = 0;
	}

	v11 = dimming->volt[TBL_INDEX_V11][color];

	ret = (MULTIPLE_VREGOUT << 10) -
		((MULTIPLE_VREGOUT - v11) * (int)v3_v203_trans_volt[gamma]);

	return ret >> 10;
}

static int calc_v11_volt(struct smart_dimming *dimming, int color)
{
	int vt, ret, v23, gamma;

	gamma = dimming->gamma[V11][color];

	if (gamma > vreg_element_max[V11]) {
		pr_warn("%s : gamma overflow : %d\n", __func__, gamma);
		gamma = vreg_element_max[V11];
	}
	if (gamma < 0) {
		pr_warn("%s : gamma undeflow : %d\n", __func__, gamma);
		gamma = 0;
	}

	vt = dimming->volt_vt[color];
	v23 = dimming->volt[TBL_INDEX_V23][color];

	ret = (vt << 10) - ((vt - v23) * (int)v3_v203_trans_volt[gamma]);

	return ret >> 10;
}

static int calc_v23_volt(struct smart_dimming *dimming, int color)
{
	int vt, ret, v35, gamma;

	gamma = dimming->gamma[V23][color];

	if (gamma > vreg_element_max[V23]) {
		pr_warn("%s : gamma overflow : %d\n", __func__, gamma);
		gamma = vreg_element_max[V23];
	}
	if (gamma < 0) {
		pr_warn("%s : gamma undeflow : %d\n", __func__, gamma);
		gamma = 0;
	}

	vt = dimming->volt_vt[color];
	v35 = dimming->volt[TBL_INDEX_V35][color];

	ret = (vt << 10) - ((vt - v35) * (int)v3_v203_trans_volt[gamma]);

	return ret >> 10;
}

static int calc_v35_volt(struct smart_dimming *dimming, int color)
{
	int vt, ret, v51, gamma;

	gamma = dimming->gamma[V35][color];

	if (gamma > vreg_element_max[V35]) {
		pr_warn("%s : gamma overflow : %d\n", __func__, gamma);
		gamma = vreg_element_max[V35];
	}
	if (gamma < 0) {
		pr_warn("%s : gamma undeflow : %d\n", __func__, gamma);
		gamma = 0;
	}

	vt = dimming->volt_vt[color];
	v51 = dimming->volt[TBL_INDEX_V51][color];

	ret = (vt << 10) - ((vt - v51) * (int)v3_v203_trans_volt[gamma]);

	return ret >> 10;
}

static int calc_v51_volt(struct smart_dimming *dimming, int color)
{
	int vt, ret, v87, gamma;

	gamma = dimming->gamma[V51][color];

	if (gamma > vreg_element_max[V51]) {
		pr_warn("%s : gamma overflow : %d\n", __func__, gamma);
		gamma = vreg_element_max[V51];
	}
	if (gamma < 0) {
		pr_warn("%s : gamma undeflow : %d\n", __func__, gamma);
		gamma = 0;
	}

	vt = dimming->volt_vt[color];
	v87 = dimming->volt[TBL_INDEX_V87][color];

	ret = (vt << 10) - ((vt - v87) * (int)v3_v203_trans_volt[gamma]);

	return ret >> 10;
}

static int calc_v87_volt(struct smart_dimming *dimming, int color)
{
	int vt, ret, v151, gamma;

	gamma = dimming->gamma[V87][color];

	if (gamma > vreg_element_max[V87]) {
		pr_warn("%s : gamma overflow : %d\n", __func__, gamma);
		gamma = vreg_element_max[V87];
	}
	if (gamma < 0) {
		pr_warn("%s : gamma undeflow : %d\n", __func__, gamma);
		gamma = 0;
	}

	vt = dimming->volt_vt[color];
	v151 = dimming->volt[TBL_INDEX_V151][color];

	ret = (vt << 10) -
		((vt - v151) * (int)v3_v203_trans_volt[gamma]);

	return ret >> 10;
}

static int calc_v151_volt(struct smart_dimming *dimming, int color)
{
	int vt, ret, v203, gamma;

	gamma = dimming->gamma[V151][color];

	if (gamma > vreg_element_max[V151]) {
		pr_warn("%s : gamma overflow : %d\n", __func__, gamma);
		gamma = vreg_element_max[V151];
	}
	if (gamma < 0) {
		pr_warn("%s : gamma undeflow : %d\n", __func__, gamma);
		gamma = 0;
	}

	vt = dimming->volt_vt[color];
	v203 = dimming->volt[TBL_INDEX_V203][color];

	ret = (vt << 10) - ((vt - v203) * (int)v3_v203_trans_volt[gamma]);

	return ret >> 10;
}

static int calc_v203_volt(struct smart_dimming *dimming, int color)
{
	int vt, ret, v255, gamma;

	gamma = dimming->gamma[V203][color];

	if (gamma > vreg_element_max[V203]) {
		pr_warn("%s : gamma overflow : %d\n", __func__, gamma);
		gamma = vreg_element_max[V203];
	}
	if (gamma < 0) {
		pr_warn("%s : gamma undeflow : %d\n", __func__, gamma);
		gamma = 0;
	}

	vt = dimming->volt_vt[color];
	v255 = dimming->volt[TBL_INDEX_V255][color];

	ret = (vt << 10) - ((vt - v255) * (int)v3_v203_trans_volt[gamma]);

	return ret >> 10;
}

static int calc_v255_volt(struct smart_dimming *dimming, int color)
{
	int ret, gamma;

	gamma = dimming->gamma[V255][color];

	if (gamma > vreg_element_max[V255]) {
		pr_warn("%s : gamma overflow : %d\n", __func__, gamma);
		gamma = vreg_element_max[V255];
	}
	if (gamma < 0) {
		pr_warn("%s : gamma undeflow : %d\n", __func__, gamma);
		gamma = 0;
	}

	ret = (int)v255_trans_volt[gamma];

	return ret;
}

static int calc_inter_v0_v3(struct smart_dimming *dimming,
		int gray, int color)
{
	int ret = 0;
	int v0, v3, ratio;

	ratio = (int)int_tbl_v0_v3[gray];

	v0 = dimming->volt[TBL_INDEX_V0][color];
	v3 = dimming->volt[TBL_INDEX_V3][color];

	ret = (v0 << 10) - ((v0 - v3) * ratio);

	return ret >> 10;
}

static int calc_inter_v3_v11(struct smart_dimming *dimming,
		int gray, int color)
{
	int ret = 0;
	int v3, v11, ratio;

	ratio = (int)int_tbl_v3_v11[gray];
	v3 = dimming->volt[TBL_INDEX_V3][color];
	v11 = dimming->volt[TBL_INDEX_V11][color];

	ret = (v3 << 10) - ((v3 - v11) * ratio);

	return ret >> 10;
}

static int calc_inter_v11_v23(struct smart_dimming *dimming,
		int gray, int color)
{
	int ret = 0;
	int v11, v23, ratio;

	ratio = (int)int_tbl_v11_v23[gray];
	v11 = dimming->volt[TBL_INDEX_V11][color];
	v23 = dimming->volt[TBL_INDEX_V23][color];

	ret = (v11 << 10) - ((v11 - v23) * ratio);

	return ret >> 10;
}

static int calc_inter_v23_v35(struct smart_dimming *dimming,
		int gray, int color)
{
	int ret = 0;
	int v23, v35, ratio;

	ratio = (int)int_tbl_v23_v35[gray];
	v23 = dimming->volt[TBL_INDEX_V23][color];
	v35 = dimming->volt[TBL_INDEX_V35][color];

	ret = (v23 << 10) - ((v23 - v35) * ratio);

	return ret >> 10;
}

static int calc_inter_v35_v51(struct smart_dimming *dimming,
		int gray, int color)
{
	int ret = 0;
	int v35, v51, ratio;

	ratio = (int)int_tbl_v35_v51[gray];
	v35 = dimming->volt[TBL_INDEX_V35][color];
	v51 = dimming->volt[TBL_INDEX_V51][color];

	ret = (v35 << 10) - ((v35 - v51) * ratio);

	return ret >> 10;
}

static int calc_inter_v51_v87(struct smart_dimming *dimming,
		int gray, int color)
{
	int ret = 0;
	int v51, v87, ratio;

	ratio = (int)int_tbl_v51_v87[gray];
	v51 = dimming->volt[TBL_INDEX_V51][color];
	v87 = dimming->volt[TBL_INDEX_V87][color];

	ret = (v51 << 10) - ((v51 - v87) * ratio);

	return ret >> 10;
}

static int calc_inter_v87_v151(struct smart_dimming *dimming,
		int gray, int color)
{
	int ret = 0;
	int v87, v151, ratio;

	ratio = (int)int_tbl_v87_v151[gray];
	v87 = dimming->volt[TBL_INDEX_V87][color];
	v151 = dimming->volt[TBL_INDEX_V151][color];

	ret = (v87 << 10) - ((v87 - v151) * ratio);

	return ret >> 10;
}

static int calc_inter_v151_v203(struct smart_dimming *dimming,
		int gray, int color)
{
	int ret = 0;
	int v151, v203, ratio;

	ratio = (int)int_tbl_v151_v203[gray];
	v151 = dimming->volt[TBL_INDEX_V151][color];
	v203 = dimming->volt[TBL_INDEX_V203][color];

	ret = (v151 << 10) - ((v151 - v203) * ratio);

	return ret >> 10;
}

static int calc_inter_v203_v255(struct smart_dimming *dimming,
		int gray, int color)
{
	int ret = 0;
	int v203, v255, ratio;

	ratio = (int)int_tbl_v203_v255[gray];
	v203 = dimming->volt[TBL_INDEX_V203][color];
	v255 = dimming->volt[TBL_INDEX_V255][color];

	ret = (v203 << 10) - ((v203 - v255) * ratio);

	return ret >> 10;
}

void panel_read_gamma(struct smart_dimming *dimming, const unsigned char *data)
{
	int i, j;
	int temp;
	u8 pos = 0;

	for (j = 0; j < CI_MAX; j++) {
		temp = ((data[pos] & 0x01) ? -1 : 1) * data[pos+1];
		dimming->gamma[V255][j] = ref_gamma[V255][j] + temp;
		dimming->mtp[V255][j] = temp;
		pos += 2;
	}

	for (i = V203; i >= V0; i--) {
		for (j = 0; j < CI_MAX; j++) {
			temp = ((data[pos] & 0x80) ? -1 : 1) *
					(data[pos] & 0x7f);
			dimming->gamma[i][j] = ref_gamma[i][j] + temp;
			dimming->mtp[i][j] = temp;
			pos++;
		}
	}
}

int panel_generate_volt_tbl(struct smart_dimming *dimming)
{
	int i, j;
	int seq, index, gray;
	int ret = 0;
	int calc_seq[NUM_VREF] = {
		V0, V255, V203, V151, V87, V51, V35, V23, V11, V3};
	int (*calc_volt_point[NUM_VREF])(struct smart_dimming *, int) = {
		calc_v0_volt,
		calc_v3_volt,
		calc_v11_volt,
		calc_v23_volt,
		calc_v35_volt,
		calc_v51_volt,
		calc_v87_volt,
		calc_v151_volt,
		calc_v203_volt,
		calc_v255_volt,
	};
	int (*calc_inter_volt[NUM_VREF])(struct smart_dimming *, int, int)  = {
		NULL,
		calc_inter_v0_v3,
		calc_inter_v3_v11,
		calc_inter_v11_v23,
		calc_inter_v23_v35,
		calc_inter_v35_v51,
		calc_inter_v51_v87,
		calc_inter_v87_v151,
		calc_inter_v151_v203,
		calc_inter_v203_v255,
	};
#ifdef SMART_DIMMING_DEBUG
	long temp[CI_MAX];
#endif
	for (i = 0; i < CI_MAX; i++)
		dimming->volt_vt[i] =
			calc_vt_volt(dimming->gamma[VT][i]);

	/* calculate voltage for every vref point */
	for (j = 0; j < NUM_VREF; j++) {
		seq = calc_seq[j];
		index = vref_index[seq];
		if (calc_volt_point[seq] != NULL) {
			for (i = 0; i < CI_MAX; i++)
				dimming->volt[index][i] =
					calc_volt_point[seq](dimming, i);
		}
	}

	index = 0;
	for (i = 0; i < MAX_GRADATION; i++) {
		if (i == vref_index[index]) {
			index++;
			continue;
		}
		gray = (i - vref_index[index - 1]) - 1;
		for (j = 0; j < CI_MAX; j++) {
			if (calc_inter_volt[index] != NULL)
				dimming->volt[i][j] =
				calc_inter_volt[index](dimming, gray, j);
		}
	}
#ifdef SMART_DIMMING_DEBUG
	pr_info("============= VT Voltage ===============\n");
	for (i = 0; i < CI_MAX; i++)
		temp[i] = dimming->volt_vt[i] >> 10;

	pr_info("R : %d : %ld G : %d : %ld B : %d : %ld.\n",
				dimming->gamma[VT][0], temp[0],
				dimming->gamma[VT][1], temp[1],
				dimming->gamma[VT][2], temp[2]);

	pr_info("=================================\n");

	for (i = 0; i < MAX_GRADATION; i++) {
		for (j = 0; j < CI_MAX; j++)
			temp[j] = dimming->volt[i][j] >> 10;

		pr_info("V%d R : %d : %ld G : %d : %ld B : %d : %ld\n", i,
					dimming->volt[i][0], temp[0],
					dimming->volt[i][1], temp[1],
					dimming->volt[i][2], temp[2]);
	}
#endif
	return ret;
}


static int lookup_volt_index(struct smart_dimming *dimming, int gray)
{
	int ret, i;
	int temp;
	int index;
	int index_l, index_h, exit;
	int cnt_l, cnt_h;
	int p_delta, delta;

	temp = gray >> 20;
	index = (int)lookup_tbl[temp];
#ifdef SMART_DIMMING_DEBUG
	pr_info("============== look up index ================\n");
	pr_info("gray : %d : %d, index : %d\n", gray, temp, index);
#endif
	exit = 1;
	i = 0;
	while (exit) {
		index_l = temp - i;
		index_h = temp + i;
		if (index_l < 0)
			index_l = 0;
		if (index_h > MAX_GAMMA)
			index_h = MAX_GAMMA;
		cnt_l = (int)lookup_tbl[index] - (int)lookup_tbl[index_l];
		cnt_h = (int)lookup_tbl[index_h] - (int)lookup_tbl[index];

		if (cnt_l + cnt_h)
			exit = 0;
		i++;
	}
#ifdef SMART_DIMMING_DEBUG
	pr_info("base index : %d, cnt : %d\n",
			lookup_tbl[index_l], cnt_l + cnt_h);
#endif
	p_delta = 0;
	index = (int)lookup_tbl[index_l];
	ret = index;
	temp = gamma_multi_tbl[index] << 10;

	if (gray > temp)
		p_delta = gray - temp;
	else
		p_delta = temp - gray;
#ifdef SMART_DIMMING_DEBUG
	pr_info("temp : %d, gray : %d, p_delta : %d\n", temp, gray, p_delta);
#endif
	for (i = 0; i <= (cnt_l + cnt_h); i++) {
		temp = gamma_multi_tbl[index + i] << 10;
		if (gray > temp)
			delta = gray - temp;
		else
			delta = temp - gray;
#ifdef SMART_DIMMING_DEBUG
		pr_info("temp : %d, gray : %d, delta : %d\n",
				temp, gray, delta);
#endif
		if (delta < p_delta) {
			p_delta = delta;
			ret = index + i;
		}
	}
#ifdef SMART_DIMMING_DEBUG
	pr_info("ret : %d\n", ret);
#endif
	return ret;
}

static int calc_reg_v3(struct smart_dimming *dimming, int color)
{
	int ret;
	int t1, t2;

	t1 = dimming->look_volt[V3][color] - MULTIPLE_VREGOUT;
	t2 = dimming->look_volt[V11][color] - MULTIPLE_VREGOUT;

	ret = ((t1 * fix_const[V3].de) / t2) - fix_const[V3].nu;

	return ret;
}

static int calc_reg_v11(struct smart_dimming *dimming, int color)
{
	int ret;
	int t1, t2;

	t1 = dimming->look_volt[V11][color] - dimming->volt_vt[color];
	t2 = dimming->look_volt[V23][color] - dimming->volt_vt[color];

	ret = (((t1) * (fix_const[V11].de)) / t2) - fix_const[V11].nu;

	return ret;
}

static int calc_reg_v23(struct smart_dimming *dimming, int color)
{
	int ret;
	int t1, t2;

	t1 = dimming->look_volt[V23][color] - dimming->volt_vt[color];
	t2 = dimming->look_volt[V35][color] - dimming->volt_vt[color];

	ret = (((t1) * (fix_const[V23].de)) / t2) - fix_const[V23].nu;

	return ret;
}

static int calc_reg_v35(struct smart_dimming *dimming, int color)
{
	int ret;
	int t1, t2;

	t1 = dimming->look_volt[V35][color] - dimming->volt_vt[color];
	t2 = dimming->look_volt[V51][color] - dimming->volt_vt[color];

	ret = (((t1) * (fix_const[V35].de)) / t2) - fix_const[V35].nu;

	return ret;
}

static int calc_reg_v51(struct smart_dimming *dimming, int color)
{
	int ret;
	int t1, t2;

	t1 = dimming->look_volt[V51][color] - dimming->volt_vt[color];
	t2 = dimming->look_volt[V87][color] - dimming->volt_vt[color];

	ret = (((t1) * (fix_const[V51].de)) / t2) - fix_const[V51].nu;

	return ret;
}

static int calc_reg_v87(struct smart_dimming *dimming, int color)
{
	int ret;
	int t1, t2;

	t1 = dimming->look_volt[V87][color] - dimming->volt_vt[color];
	t2 = dimming->look_volt[V151][color] - dimming->volt_vt[color];

	ret = (((t1) * (fix_const[V87].de)) / t2) - fix_const[V87].nu;

	return ret;
}

static int calc_reg_v151(struct smart_dimming *dimming, int color)
{
	int ret;
	int t1, t2;

	t1 = dimming->look_volt[V151][color] - dimming->volt_vt[color];
	t2 = dimming->look_volt[V203][color] - dimming->volt_vt[color];

	ret = (((t1) * (fix_const[V151].de)) / t2) - fix_const[V151].nu;

	return ret;
}

static int calc_reg_v203(struct smart_dimming *dimming, int color)
{
	int ret;
	int t1, t2;

	t1 = dimming->look_volt[V203][color] - dimming->volt_vt[color];
	t2 = dimming->look_volt[V255][color] - dimming->volt_vt[color];

	ret = (((t1) * (fix_const[V203].de)) / t2) - fix_const[V203].nu;

	return ret;
}

static int calc_reg_v255(struct smart_dimming *dimming, int color)
{
	int ret;
	int t1;

	t1 = MULTIPLE_VREGOUT - dimming->look_volt[V255][color];

	ret = ((t1 * fix_const[V255].de) / MULTIPLE_VREGOUT) -
			fix_const[V255].nu;

	return ret;
}

int panel_get_gamma(struct smart_dimming *dimming,
				int index_br, unsigned char *result)
{
	int i, j;
	int ret = 0;
	int gray, index, shift, c_shift;
	int gamma_int[NUM_VREF][CI_MAX];
	int br;
	int *color_shift_table = NULL;
	int (*calc_reg[NUM_VREF])(struct smart_dimming *, int)  = {
		NULL,
		calc_reg_v3,
		calc_reg_v11,
		calc_reg_v23,
		calc_reg_v35,
		calc_reg_v51,
		calc_reg_v87,
		calc_reg_v151,
		calc_reg_v203,
		calc_reg_v255,
	};

	br = ref_cd_tbl[index_br];

	if (br > MAX_GAMMA)
		br = MAX_GAMMA;

	for (i = V3; i < NUM_VREF; i++) {
		/* get reference shift value */
		shift = gradation_shift[index_br][i];
		gray = gamma_tbl[vref_index[i]] * br;
		index = lookup_volt_index(dimming, gray);
		index = index + shift;
#ifdef SMART_DIMMING_DEBUG
		pr_info("index : %d\n", index);
#endif
		for (j = 0; j < CI_MAX; j++) {
			if (calc_reg[i] != NULL) {
				dimming->look_volt[i][j] =
					dimming->volt[index][j];
#ifdef SMART_DIMMING_DEBUG
				pr_info("volt : %d : %d\n",
					dimming->look_volt[i][j],
					dimming->look_volt[i][j] >> 10);
#endif
			}
		}
	}

	for (i = V3; i < NUM_VREF; i++) {
		for (j = 0; j < CI_MAX; j++) {
			if (calc_reg[i] != NULL) {
				index = (i * CI_MAX) + j;
				color_shift_table = rgb_offset[index_br];
				c_shift = color_shift_table[index];
				gamma_int[i][j] =
					(calc_reg[i](dimming, j) + c_shift) -
					dimming->mtp[i][j];
#ifdef SMART_DIMMING_DEBUG
				pr_info("gamma : %d, shift : %d\n",
						gamma_int[i][j], c_shift);
#endif
				if (gamma_int[i][j] >= vreg_element_max[i])
					gamma_int[i][j] = vreg_element_max[i];
				if (gamma_int[i][j] < 0)
					gamma_int[i][j] = 0;
			}
		}
	}

	for (j = 0; j < CI_MAX; j++)
		gamma_int[VT][j] = dimming->gamma[VT][j] - dimming->mtp[VT][j];

	index = 0;

	for (i = V255; i >= VT; i--) {
		for (j = 0; j < CI_MAX; j++) {
			if (i == V255) {
				result[index++] =
					gamma_int[i][j] > 0xff ? 1 : 0;
				result[index++] = gamma_int[i][j] & 0xff;
			} else {
				result[index++] =
					(unsigned char)gamma_int[i][j];
			}
		}
	}

	return ret;
}

