/* drivers/video/fbdev/exynos/dpu_9110/panels/s6e36w3x01_dimming.c
 *
 * MIPI-DSI based S6E36W1X01 AMOLED panel driver.
 *
 * Taeheon Kim, <th908.kim@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/kernel.h>
#include "s6e36w3x01_s_dimming.h"
#include "s6e36w3x01_s_param.h"

/*#define SMART_DIMMING_DEBUG*/
#define RGB_COMPENSATION 33

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

const static int vreg_element_max[NUM_VREF] = {
	0xff,
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

const static struct v_constant fix_const[NUM_VREF] = {
	{.nu = 0,	.de = 256},
	{.nu = 64,	.de = 320},
	{.nu = 64,	.de = 320},
	{.nu = 64,	.de = 320},
	{.nu = 64,	.de = 320},
	{.nu = 64,	.de = 320},
	{.nu = 64,	.de = 320},
	{.nu = 64,	.de = 320},
	{.nu = 64,	.de = 320},
	{.nu = 129,	.de = 860},
};

static const unsigned int vt_trans_volt[16] = {
	6000000, 5916279, 5832558, 5748837, 5665116, 5581395, 5497674, 5413953,
	5330233, 5246512, 5037209, 4967442, 4897674, 4827907, 4758140, 4702326
};

static const unsigned int v0_trans_volt[16] = {
	6000000, 5958140, 5916279, 5874419, 5832558, 5790698, 5748837, 5706977,
	5665116, 5623256, 5581395, 5539535, 5497674, 5455814, 5413953, 5372093
};

static const int gamma_tbl[256] = {
	0, 7, 31, 75, 138, 224, 331, 461,
	614, 791, 992, 1218, 1468, 1744, 2045, 2372,
	2725, 3105, 3511, 3943, 4403, 4890, 5404, 5946,
	6516, 7114, 7740, 8394, 9077, 9788, 10528, 11297,
	12095, 12922, 13779, 14665, 15581, 16526, 17501, 18507,
	19542, 20607, 21703, 22829, 23986, 25174, 26392, 27641,
	28921, 30232, 31574, 32947, 34352, 35788, 37255, 38754,
	40285, 41847, 43442, 45068, 46727, 48417, 50140, 51894,
	53682, 55501, 57353, 59238, 61155, 63105, 65088, 67103,
	69152, 71233, 73348, 75495, 77676, 79890, 82138, 84418,
	86733, 89080, 91461, 93876, 96325, 98807, 101324, 103874,
	106458, 109075, 111727, 114414, 117134, 119888, 122677, 125500,
	128358, 131250, 134176, 137137, 140132, 143163, 146227, 149327,
	152462, 155631, 158835, 162074, 165348, 168657, 172002, 175381,
	178796, 182246, 185731, 189251, 192807, 196398, 200025, 203688,
	207385, 211119, 214888, 218693, 222533, 226410, 230322, 234270,
	238254, 242274, 246330, 250422, 254550, 258714, 262914, 267151,
	271423, 275732, 280078, 284459, 288878, 293332, 297823, 302351,
	306915, 311516, 316153, 320827, 325538, 330285, 335069, 339890,
	344748, 349643, 354575, 359544, 364549, 369592, 374672, 379789,
	384943, 390134, 395363, 400629, 405932, 411272, 416650, 422065,
	427517, 433007, 438534, 444099, 449702, 455342, 461020, 466735,
	472488, 478279, 484107, 489973, 495878, 501819, 507799, 513817,
	519872, 525966, 532098, 538267, 544475, 550721, 557005, 563327,
	569687, 576085, 582522, 588997, 595510, 602062, 608651, 615280,
	621946, 628652, 635395, 642177, 648998, 655857, 662755, 669691,
	676667, 683680, 690733, 697824, 704954, 712122, 719330, 726576,
	733861, 741186, 748549, 755951, 763391, 770871, 778390, 785948,
	793545, 801182, 808857, 816571, 824325, 832118, 839950, 847821,
	855732, 863682, 871671, 879700, 887768, 895875, 904022, 912208,
	920434, 928699, 937004, 945349, 953733, 962156, 970619, 979122,
	987665, 996247, 1004869, 1013531, 1022233, 1030974, 1039755, 1048576
};

const static int gamma_multi_tbl[256] = {
  0, 2, 8, 20, 39, 63, 94, 132, 177, 230,
  290, 357, 433, 516, 607, 707, 815, 931, 1056, 1189,
  1331, 1482, 1642, 1810, 1988, 2175, 2371, 2576, 2790, 3014,
  3248, 3491, 3743, 4005, 4277, 4559, 4850, 5152, 5463, 5784,
  6116, 6457, 6809, 7170, 7542, 7925, 8317, 8720, 9134, 9558,
  9992, 10437, 10892, 11359, 11835, 12323, 12821, 13330, 13850, 14381,
  14923, 15475, 16039, 16614, 17199, 17796, 18404, 19023, 19653, 20295,
  20947, 21611, 22287, 22973, 23671, 24381, 25102, 25834, 26578, 27333,
  28100, 28879, 29669, 30471, 31284, 32110, 32947, 33795, 34656, 35528,
  36412, 37308, 38216, 39136, 40068, 41012, 41967, 42935, 43915, 44907,
  45911, 46927, 47955, 48995, 50048, 51113, 52190, 53279, 54381, 55495,
  56621, 57760, 58911, 60074, 61250, 62438, 63639, 64852, 66078, 67316,
  68567, 69830, 71106, 72394, 73696, 75009, 76336, 77675, 79027, 80392,
  81769, 83159, 84562, 85978, 87407, 88848, 90302, 91770, 93250, 94743,
  96249, 97768, 99300, 100845, 102403, 103974, 105558, 107155, 108765, 110389,
  112025, 113675, 115337, 117013, 118702, 120405, 122120, 123849, 125591, 127347,
  129115, 130897, 132693, 134501, 136323, 138159, 140008, 141870, 143746, 145635,
  147537, 149453, 151383, 153326, 155282, 157253, 159236, 161234, 163244, 165269,
  167307, 169358, 171424, 173503, 175595, 177702, 179822, 181956, 184103, 186264,
  188440, 190628, 192831, 195047, 197278, 199522, 201780, 204052, 206337, 208637,
  210950, 213278, 215619, 217974, 220344, 222727, 225124, 227535, 229961, 232400,
  234853, 237321, 239802, 242298, 244807, 247331, 249869, 252421, 254987, 257568,
  260162, 262771, 265394, 268031, 270682, 273348, 276028, 278722, 281430, 284153,
  286890, 289641, 292407, 295187, 297981, 300790, 303613, 306451, 309302, 312169,
  315050, 317945, 320854, 323778, 326717, 329670, 332638, 335620, 338616, 341627,
  344653, 347693, 350748, 353817, 356901, 360000
};

const static int gamma_double_multi_tbl[256] = {
  0, 1828, 8399, 20492, 38588, 63045, 94156, 132170, 177302, 229746,
  289678, 357255, 432627, 515929, 607290, 706830, 814664, 930898, 1055635, 1188974,
  1331007, 1481824, 1641512, 1810154, 1987829, 2174614, 2370585, 2575814, 2790370, 3014323,
  3247738, 3490679, 3743209, 4005390, 4277281, 4558940, 4850423, 5151787, 5463086, 5784373,
  6115699, 6457117, 6808675, 7170422, 7542408, 7924679, 8317281, 8720260, 9133660, 9557526,
  9991901, 10436828, 10892347, 11358501, 11835331, 12322875, 12821174, 13330266, 13850189, 14380982,
  14922681, 15475324, 16038946, 16613584, 17199272, 17796045, 18403939, 19022986, 19653220, 20294676,
  20947384, 21611379, 22286692, 22973354, 23671397, 24380853, 25101751, 25834122, 26577996, 27333404,
  28100373, 28878934, 29669115, 30470944, 31284451, 32109663, 32946608, 33795312, 34655805, 35528112,
  36412260, 37308275, 38216185, 39136014, 40067790, 41011537, 41967280, 42935045, 43914857, 44906741,
  45910721, 46926821, 47955065, 48995478, 50048084, 51112905, 52189966, 53279289, 54380897, 55494814,
  56621063, 57759664, 58910642, 60074018, 61249815, 62438054, 63638756, 64851945, 66077640, 67315864,
  68566637, 69829981, 71105916, 72394463, 73695643, 75009477, 76335984, 77675184, 79027099, 80391748,
  81769150, 83159325, 84562294, 85978075, 87406688, 88848152, 90302486, 91769709, 93249840, 94742898,
  96248902, 97767870, 99299821, 100844772, 102402743, 103973751, 105557814, 107154951, 108765179, 110388516,
  112024980, 113674588, 115337358, 117013307, 118702452, 120404811, 122120401, 123849239, 125591341, 127346725,
  129115407, 130897404, 132692732, 134501409, 136323451, 138158873, 140007693, 141869926, 143745588, 145634697,
  147537266, 149453313, 151382854, 153325903, 155282477, 157252592, 159236262, 161233504, 163244332, 165268762,
  167306809, 169358489, 171423816, 173502806, 175595473, 177701833, 179821900, 181955689, 184103215, 186264493,
  188439536, 190628361, 192830980, 195047409, 197277661, 199521752, 201779695, 204051505, 206337196, 208636781,
  210950275, 213277691, 215619045, 217974348, 220343616, 222726862, 225124100, 227535343, 229960605, 232399899,
  234853238, 237320638, 239802109, 242297667, 244807324, 247331093, 249868988, 252421021, 254987206, 257567556,
  260162084, 262770802, 265393724, 268030862, 270682230, 273347839, 276027703, 278721834, 281430244, 284152948,
  286889956, 289641282, 292406937, 295186934, 297981287, 300790005, 303613103, 306450593, 309302486, 312168794,
  315049530, 317944706, 320854334, 323778425, 326716992, 329670047, 332637601, 335619667, 338616255, 341627378,
  344653048, 347693276, 350748073, 353817452, 356901424, 360000000,
};

const static unsigned char lookup_tbl[361] = {
  0, 18, 24, 29, 33, 36, 40, 43, 45, 48,
  50, 52, 54, 56, 58, 60, 62, 64, 65, 67,
  69, 70, 72, 73, 74, 76, 77, 79, 80, 81,
  82, 84, 85, 86, 87, 88, 90, 91, 92, 93,
  94, 95, 96, 97, 98, 99, 100, 101, 102, 103,
  104, 105, 106, 107, 108, 109, 109, 110, 111, 112,
  113, 114, 115, 115, 116, 117, 118, 119, 120, 120,
  121, 122, 123, 123, 124, 125, 126, 126, 127, 128,
  129, 129, 130, 131, 132, 132, 133, 134, 134, 135,
  136, 136, 137, 138, 139, 139, 140, 140, 141, 142,
  142, 143, 144, 144, 145, 146, 146, 147, 148, 148,
  149, 149, 150, 151, 151, 152, 152, 153, 154, 154,
  155, 155, 156, 157, 157, 158, 158, 159, 159, 160,
  160, 161, 162, 162, 163, 163, 164, 164, 165, 165,
  166, 167, 167, 168, 168, 169, 169, 170, 170, 171,
  171, 172, 172, 173, 173, 174, 174, 175, 175, 176,
  176, 177, 177, 178, 178, 179, 179, 180, 180, 181,
  181, 182, 182, 183, 183, 184, 184, 185, 185, 186,
  186, 187, 187, 187, 188, 188, 189, 189, 190, 190,
  191, 191, 192, 192, 193, 193, 193, 194, 194, 195,
  195, 196, 196, 197, 197, 197, 198, 198, 199, 199,
  200, 200, 200, 201, 201, 202, 202, 203, 203, 203,
  204, 204, 205, 205, 206, 206, 206, 207, 207, 208,
  208, 208, 209, 209, 210, 210, 210, 211, 211, 212,
  212, 212, 213, 213, 214, 214, 214, 215, 215, 216,
  216, 216, 217, 217, 218, 218, 218, 219, 219, 220,
  220, 220, 221, 221, 221, 222, 222, 223, 223, 223,
  224, 224, 224, 225, 225, 226, 226, 226, 227, 227,
  227, 228, 228, 229, 229, 229, 230, 230, 230, 231,
  231, 231, 232, 232, 233, 233, 233, 234, 234, 234,
  235, 235, 235, 236, 236, 236, 237, 237, 238, 238,
  238, 239, 239, 239, 240, 240, 240, 241, 241, 241,
  242, 242, 242, 243, 243, 243, 244, 244, 244, 245,
  245, 245, 246, 246, 246, 247, 247, 247, 248, 248,
  248, 249, 249, 249, 250, 250, 250, 251, 251, 251,
  252, 252, 252, 253, 253, 253, 254, 254, 254, 255,
  255,
};

static const unsigned int v255_trans_volt [513] = {
	5100000, 5093023, 5086047, 5079070, 5072093, 5065116, 5058140, 5051163, 5044186, 5037209,
	5030233, 5023256, 5016279, 5009302, 5002326, 4995349, 4988372, 4981395, 4974419, 4967442,
	4960465, 4953488, 4946512, 4939535, 4932558, 4925581, 4918605, 4911628, 4904651, 4897674,
	4890698, 4883721, 4876744, 4869767, 4862791, 4855814, 4848837, 4841860, 4834884, 4827907,
	4820930, 4813953, 4806977, 4800000, 4793023, 4786047, 4779070, 4772093, 4765116, 4758140,
	4751163, 4744186, 4737209, 4730233, 4723256, 4716279, 4709302, 4702326, 4695349, 4688372,
	4681395, 4674419, 4667442, 4660465, 4653488, 4646512, 4639535, 4632558, 4625581, 4618605,
	4611628, 4604651, 4597674, 4590698, 4583721, 4576744, 4569767, 4562791, 4555814, 4548837,
	4541860, 4534884, 4527907, 4520930, 4513953, 4506977, 4500000, 4493023, 4486047, 4479070,
	4472093, 4465116, 4458140, 4451163, 4444186, 4437209, 4430233, 4423256, 4416279, 4409302,
	4402326, 4395349, 4388372, 4381395, 4374419, 4367442, 4360465, 4353488, 4346512, 4339535,
	4332558, 4325581, 4318605, 4311628, 4304651, 4297674, 4290698, 4283721, 4276744, 4269767,
	4262791, 4255814, 4248837, 4241860, 4234884, 4227907, 4220930, 4213953, 4206977, 4200000,
	4193023, 4186047, 4179070, 4172093, 4165116, 4158140, 4151163, 4144186, 4137209, 4130233,
	4123256, 4116279, 4109302, 4102326, 4095349, 4088372, 4081395, 4074419, 4067442, 4060465,
	4053488, 4046512, 4039535, 4032558, 4025581, 4018605, 4011628, 4004651, 3997674, 3990698,
	3983721, 3976744, 3969767, 3962791, 3955814, 3948837, 3941860, 3934884, 3927907, 3920930,
	3913953, 3906977, 3900000, 3893023, 3886047, 3879070, 3872093, 3865116, 3858140, 3851163,
	3844186, 3837209, 3830233, 3823256, 3816279, 3809302, 3802326, 3795349, 3788372, 3781395,
	3774419, 3767442, 3760465, 3753488, 3746512, 3739535, 3732558, 3725581, 3718605, 3711628,
	3704651, 3697674, 3690698, 3683721, 3676744, 3669767, 3662791, 3655814, 3648837, 3641860,
	3634884, 3627907, 3620930, 3613953, 3606977, 3600000, 3593023, 3586047, 3579070, 3572093,
	3565116, 3558140, 3551163, 3544186, 3537209, 3530233, 3523256, 3516279, 3509302, 3502326,
	3495349, 3488372, 3481395, 3474419, 3467442, 3460465, 3453488, 3446512, 3439535, 3432558,
	3425581, 3418605, 3411628, 3404651, 3397674, 3390698, 3383721, 3376744, 3369767, 3362791,
	3355814, 3348837, 3341860, 3334884, 3327907, 3320930, 3313953, 3306977, 3300000, 3293023,
	3286047, 3279070, 3272093, 3265116, 3258140, 3251163, 3244186, 3237209, 3230233, 3223256,
	3216279, 3209302, 3202326, 3195349, 3188372, 3181395, 3174419, 3167442, 3160465, 3153488,
	3146512, 3139535, 3132558, 3125581, 3118605, 3111628, 3104651, 3097674, 3090698, 3083721,
	3076744, 3069767, 3062791, 3055814, 3048837, 3041860, 3034884, 3027907, 3020930, 3013953,
	3006977, 3000000, 2993023, 2986047, 2979070, 2972093, 2965116, 2958140, 2951163, 2944186,
	2937209, 2930233, 2923256, 2916279, 2909302, 2902326, 2895349, 2888372, 2881395, 2874419,
	2867442, 2860465, 2853488, 2846512, 2839535, 2832558, 2825581, 2818605, 2811628, 2804651,
	2797674, 2790698, 2783721, 2776744, 2769767, 2762791, 2755814, 2748837, 2741860, 2734884,
	2727907, 2720930, 2713953, 2706977, 2700000, 2693023, 2686047, 2679070, 2672093, 2665116,
	2658140, 2651163, 2644186, 2637209, 2630233, 2623256, 2616279, 2609302, 2602326, 2595349,
	2588372, 2581395, 2574419, 2567442, 2560465, 2553488, 2546512, 2539535, 2532558, 2525581,
	2518605, 2511628, 2504651, 2497674, 2490698, 2483721, 2476744, 2469767, 2462791, 2455814,
	2448837, 2441860, 2434884, 2427907, 2420930, 2413953, 2406977, 2400000, 2393023, 2386047,
	2379070, 2372093, 2365116, 2358140, 2351163, 2344186, 2337209, 2330233, 2323256, 2316279,
	2309302, 2302326, 2295349, 2288372, 2281395, 2274419, 2267442, 2260465, 2253488, 2246512,
	2239535, 2232558, 2225581, 2218605, 2211628, 2204651, 2197674, 2190698, 2183721, 2176744,
	2169767, 2162791, 2155814, 2148837, 2141860, 2134884, 2127907, 2120930, 2113953, 2106977,
	2100000, 2093023, 2086047, 2079070, 2072093, 2065116, 2058140, 2051163, 2044186, 2037209,
	2030233, 2023256, 2016279, 2009302, 2002326, 1995349, 1988372, 1981395, 1974419, 1967442,
	1960465, 1953488, 1946512, 1939535, 1932558, 1925581, 1918605, 1911628, 1904651, 1897674,
	1890698, 1883721, 1876744, 1869767, 1862791, 1855814, 1848837, 1841860, 1834884, 1827907,
	1820930, 1813953, 1806977, 1800000, 1793023, 1786047, 1779070, 1772093, 1765116, 1758140,
	1751163, 1744186, 1737209, 1730233, 1723256, 1716279, 1709302, 1702326, 1695349, 1688372,
	1681395, 1674419, 1667442, 1660465, 1653488, 1646512, 1639535, 1632558, 1625581, 1618605,
	1611628, 1604651, 1597674, 1590698, 1583721, 1576744, 1569767, 1562791, 1555814, 1548837,
	1541860, 1534884, 1527907,
};

static const unsigned int v203_trans_volt [256] = {
	200, 203, 206, 209, 213, 216, 219, 222,
	225, 228, 231, 234, 238, 241, 244, 247,
	250, 253, 256, 259, 263, 266, 269, 272,
	275, 278, 281, 284, 288, 291, 294, 297,
	300, 303, 306, 309, 313, 316, 319, 322,
	325, 328, 331, 334, 338, 341, 344, 347,
	350, 353, 356, 359, 363, 366, 369, 372,
	375, 378, 381, 384, 388, 391, 394, 397,
	400, 403, 406, 409, 413, 416, 419, 422,
	425, 428, 431, 434, 438, 441, 444, 447,
	450, 453, 456, 459, 463, 466, 469, 472,
	475, 478, 481, 484, 488, 491, 494, 497,
	500, 503, 506, 509, 513, 516, 519, 522,
	525, 528, 531, 534, 538, 541, 544, 547,
	550, 553, 556, 559, 563, 566, 569, 572,
	575, 578, 581, 584, 588, 591, 594, 597,
	600, 603, 606, 609, 613, 616, 619, 622,
	625, 628, 631, 634, 638, 641, 644, 647,
	650, 653, 656, 659, 663, 666, 669, 672,
	675, 678, 681, 684, 688, 691, 694, 697,
	700, 703, 706, 709, 713, 716, 719, 722,
	725, 728, 731, 734, 738, 741, 744, 747,
	750, 753, 756, 759, 763, 766, 769, 772,
	775, 778, 781, 784, 788, 791, 794, 797,
	800, 803, 806, 809, 813, 816, 819, 822,
	825, 828, 831, 834, 838, 841, 844, 847,
	850, 853, 856, 859, 863, 866, 869, 872,
	875, 878, 881, 884, 888, 891, 894, 897,
	900, 903, 906, 909, 913, 916, 919, 922,
	925, 928, 931, 934, 938, 941, 944, 947,
	950, 953, 956, 959, 963, 966, 969, 972,
	975, 978, 981, 984, 988, 991, 994, 997,
};

static const unsigned int v1_trans_volt [256] = {
	0, 4, 8, 12, 16, 20, 23, 27,
	31, 35, 39, 43, 47,	51,	55, 59,
	63, 66, 70, 74, 78,	82, 86, 90,
	94, 98, 102, 105, 109, 113, 117, 121,
	125, 129, 133, 137, 141, 145, 148, 152,
	156, 160, 164, 168, 172, 176, 180, 184,
	188, 191, 195, 199, 203, 207, 211, 215,
	219, 223, 227, 230, 234, 238, 242, 246,
	250, 254, 258, 262, 266, 270, 273, 277,
	281, 285, 289, 293, 297, 301, 305, 309,
	313, 316, 320, 324, 328, 332, 336, 340,
	344, 348, 352, 355, 359, 363, 367, 371,
	375, 379, 383, 387, 391, 395, 398, 402,
	406, 410, 414, 418, 422, 426, 430, 434,
	438, 441, 445, 449, 453, 457, 461, 465,
	469, 473, 477, 480, 484, 488, 492, 496,
	500, 504, 508, 512, 516, 520, 523, 527,
	531, 535, 539, 543, 547, 551, 555, 559,
	563, 566, 570, 574, 578, 582, 586, 590,
	594, 598, 602, 605, 609, 613, 617, 621,
	625, 629, 633, 637, 641, 645, 648, 652,
	656, 660, 664, 668, 672, 676, 680, 684,
	688, 691, 695, 699, 703, 707, 711, 715,
	719, 723, 727, 730, 734, 738, 742, 746,
	750, 754, 758, 762, 766, 770, 773, 777,
	781, 785, 789, 793, 797, 801, 805, 809,
	813, 816, 820, 824, 828, 832, 836, 840,
	844, 848, 852, 855, 859, 863, 867, 871,
	875, 879, 883, 887, 891, 895, 898, 902,
	906, 910, 914, 918, 922, 926, 930, 934,
	938, 941, 945, 949, 953, 957, 961, 965,
	969, 973, 977, 980, 984, 988, 992, 996,
};

static const short int_tbl_v1_v7[5] = {
	171, 341, 512, 683, 853,
};


static const short int_tbl_v7_v11[3] = {
	256, 512, 768
};


static const short int_tbl_v11_v23[11] = {
	85, 171, 256, 341, 427, 512, 597, 683,
	768, 853, 939,
};


static const short int_tbl_v23_v35[11] = {
	85, 171, 256, 341, 427, 512, 597, 683,
	768, 853, 939,
};


static const short int_tbl_v35_v51[15] = {
	64, 128, 192, 256, 320, 384, 448, 512,
	576, 640, 704, 768, 832, 896, 960,
};


static const short int_tbl_v51_v87[35] = {
	28,	57,	85,	114, 142, 171, 199, 228,
	256, 284, 313, 341, 370, 398, 427, 455,
	484, 512, 540, 569, 597, 626, 654, 683,
	711, 740, 768, 796, 825, 853, 882, 910,
	939, 967, 996,
};


static const short int_tbl_v87_v151[63] = {
	16,	32, 48, 64, 80, 96, 112, 128,
	144, 160, 176, 192, 208, 224, 240, 256,
	272, 288, 304, 320, 336, 352, 368, 384,
	400, 416, 432, 448, 464, 480, 496, 512,
	528, 544, 560, 576, 592, 608, 624, 640,
	656, 672, 688, 704, 720, 736, 752, 768,
	784, 800, 816, 832, 848, 864, 880, 896,
	912, 928, 944, 960, 976, 992, 1008,
};


static const short int_tbl_v151_v203[51] = {
	20, 39, 59, 79, 98, 118, 138, 158,
	177, 197, 217, 236, 256, 276, 295, 315,
	335, 354, 374, 394, 414, 433, 453, 473,
	492, 512, 532, 551, 571, 591, 610, 630,
	650, 670, 689, 709, 729, 748, 768, 788,
	807, 827, 847, 866, 886, 906, 926, 945,
	965, 985, 1004,
};


static const short int_tbl_v203_v255[51] = {
	20, 39, 59, 79, 98, 118, 138, 158,
	177, 197, 217, 236, 256, 276, 295, 315,
	335, 354, 374, 394, 414, 433, 453, 473,
	492, 512, 532, 551, 571, 591, 610, 630,
	650, 670, 689, 709, 729, 748, 768, 788,
	807, 827, 847, 866, 886, 906, 926, 945,
	965, 985, 1004,
};

void s6e36w3x01_read_gamma(struct smart_dimming *dimming, const unsigned char *data)
{
	int i, j;
	int temp, tmp;
	u8 pos = 0;
	u8 s_v255[3]={0,};

	tmp = (data[31] & 0xf0) >> 4;
	s_v255[0] = (tmp >> 3) & 0x1;
	s_v255[1] = (tmp >> 2) & 0x1;
	s_v255[2] = (tmp >> 1) & 0x1;

	for (j = 0; j < CI_MAX; j++) {
		temp = ((s_v255[j] & 0x01) ? -1 : 1) * data[pos];
		dimming->gamma[V255][j] = ref_gamma[V255][j] + temp;
		dimming->mtp[V255][j] = temp;
		pos ++;
	}

	for (i = V203; i >= V1; i--) {
		for (j = 0; j < CI_MAX; j++) {
			temp = ((data[pos] & 0x80) ? -1 : 1) *
					(data[pos] & 0x7f);
			dimming->gamma[i][j] = ref_gamma[i][j] + temp;
			dimming->mtp[i][j] = temp;
			pos++;
		}
	}

	temp =data[pos] & 0xf;
	dimming->gamma[V1][CI_RED] = ref_gamma[V1][CI_RED] + temp;
	dimming->mtp[V1][CI_RED] = temp;

	temp = (data[pos] & 0xf0) >> 4;
	dimming->gamma[V1][CI_GREEN] = ref_gamma[V1][CI_GREEN] + temp;
	dimming->mtp[V1][CI_GREEN] = temp;

	temp =data[++pos] & 0xf;
	dimming->gamma[V1][CI_BLUE] = ref_gamma[V1][CI_BLUE] + temp;
	dimming->mtp[V1][CI_BLUE] = temp;

	pr_info("%s:MTP OFFSET\n", __func__);
	for (i = V1; i<= V255; i++)
		pr_info("%d %d %d\n", dimming->mtp[i][0],
				dimming->mtp[i][1],dimming->mtp[i][2]);

	pr_debug("MTP+ Center gamma\n");
	for (i = V1; i<= V255; i++)
		pr_debug("%d %d %d\n", dimming->gamma[i][0],
			dimming->gamma[i][1], dimming->gamma[i][2]);
}

static int calc_vt_volt(int gamma)
{
	int max;

	max = (sizeof(vt_trans_volt) >> 2) - 1;
	if (gamma > max) {
		pr_err(" %s exceed gamma value\n", __func__);
		gamma = max;
	}

	return (int)vt_trans_volt[gamma];
}

static int calc_v0_volt(int gamma)
{
	int max;

	max = (sizeof(v0_trans_volt) >> 2) - 1;
	if (gamma > max) {
		pr_err(" %s exceed gamma value\n", __func__);
		gamma = max;
	}

	return (int)v0_trans_volt[gamma];
}

static int calc_v1_volt(struct dim_data *data, int color)
{
	int gap;
	int ret, v7, gamma;
	unsigned long temp;

	gamma = data->t_gamma[V1][color];

	if (gamma > vreg_element_max[V1]) {
		pr_err("%s : gamma overflow : %d\n", __FUNCTION__, gamma);
		gamma = vreg_element_max[V1];
	}
	if (gamma < 0) {
		pr_err(":%s : gamma undeflow : %d\n", __FUNCTION__, gamma);
		gamma = 0;
	}

	v7 = data->volt[TBL_INDEX_V7][color];
	gap = (DOUBLE_MULTIPLE_VREGOUT - v7);
	temp = (unsigned long)gap * (unsigned long)v1_trans_volt[gamma];
	temp /= MULTIPLY_VALUE;

	ret = DOUBLE_MULTIPLE_VREGOUT - (int)temp;
	//printk("[LCD] calc_v1_volt : ret : %d, gamma : %d\n", ret, gamma);

	return ret;
}

static int calc_v7_volt(struct dim_data *data, int color)
{
	int gap;
	int ret, v11, gamma;
	unsigned long temp;

	gamma = data->t_gamma[V7][color];

	if (gamma > vreg_element_max[V7]) {
		pr_err("%s : gamma overflow : %d\n", __FUNCTION__, gamma);
		gamma = vreg_element_max[V7];
	}
	if (gamma < 0) {
		pr_err(":%s : gamma undeflow : %d\n", __FUNCTION__, gamma);
		gamma = 0;
	}

	v11 = data->volt[TBL_INDEX_V11][color];

	gap = (DOUBLE_MULTIPLE_VREGOUT - v11);
	temp = (unsigned long)gap * (unsigned long)v203_trans_volt[gamma];
	temp /= MULTIPLY_VALUE;

	ret = DOUBLE_MULTIPLE_VREGOUT - (int)temp;
	//printk("[LCD] calc_v7_volt : ret : %d\n", ret);

	return ret;
}

static int calc_v11_volt(struct dim_data *data, int color)
{
	int gap;
	int vt, ret, v23, gamma;
	unsigned long temp;

	gamma = data->t_gamma[V11][color];

	if (gamma > vreg_element_max[V11]) {
		pr_err("%s : gamma overflow : %d\n", __FUNCTION__, gamma);
		gamma = vreg_element_max[V11];
	}
	if (gamma < 0) {
		pr_err("%s : gamma undeflow : %d\n", __FUNCTION__, gamma);
		gamma = 0;
	}

	vt = data->volt_vt[color];
	v23 = data->volt[TBL_INDEX_V23][color];

	gap = vt - v23;
	temp = (unsigned long)gap * (unsigned long)v203_trans_volt[gamma];
	temp /= MULTIPLY_VALUE;

	ret = vt - (int)temp;
	//printk("[LCD] calc_v11_volt : ret : %d\n", ret);

	return ret ;
}

static int calc_v23_volt(struct dim_data *data, int color)
{
	int gap;
	int vt, ret, v35, gamma;
	unsigned long temp;

	gamma = data->t_gamma[V23][color];

	if (gamma > vreg_element_max[V23]) {
		pr_err("%s : gamma overflow : %d\n", __FUNCTION__, gamma);
		gamma = vreg_element_max[V23];
	}
	if (gamma < 0) {
		pr_err("%s : gamma undeflow : %d\n", __FUNCTION__, gamma);
		gamma = 0;
	}


	vt = data->volt_vt[color];
	v35 = data->volt[TBL_INDEX_V35][color];

	gap = vt - v35;
	temp = (unsigned long)gap * (unsigned long)v203_trans_volt[gamma];
	temp /= MULTIPLY_VALUE;

	ret = vt - (int)temp;
	//printk("[LCD] calc_v23_volt : ret : %d\n", ret);

	return ret;

}

static int calc_v35_volt(struct dim_data *data, int color)
{
	int gap;
	int vt, ret, v51, gamma;
	unsigned long temp;

	gamma = data->t_gamma[V35][color];

	if (gamma > vreg_element_max[V35]) {
		pr_err("%s : gamma overflow : %d\n", __FUNCTION__, gamma);
		gamma = vreg_element_max[V35];
	}
	if (gamma < 0) {
		pr_err("%s : gamma undeflow : %d\n", __FUNCTION__, gamma);
		gamma = 0;
	}

	vt = data->volt_vt[color];
	v51 = data->volt[TBL_INDEX_V51][color];

	gap = vt - v51;
	temp = (unsigned long)gap * (unsigned long)v203_trans_volt[gamma];
	temp /= MULTIPLY_VALUE;

	ret = vt - (int)temp;
	//printk("[LCD] calc_v35_volt : ret : %d\n", ret);

	return ret;
}

static int calc_v51_volt(struct dim_data *data, int color)
{
	int gap;
	int vt, ret, v87, gamma;
	unsigned long temp;

	gamma = data->t_gamma[V51][color];

	if (gamma > vreg_element_max[V51]) {
		pr_err("%s : gamma overflow : %d\n", __FUNCTION__, gamma);
		gamma = vreg_element_max[V51];
	}
	if (gamma < 0) {
		pr_err("%s : gamma undeflow : %d\n", __FUNCTION__, gamma);
		gamma = 0;
	}

	vt = data->volt_vt[color];
	v87 = data->volt[TBL_INDEX_V87][color];

	gap = vt - v87;
	temp = (unsigned long)gap * (unsigned long)v203_trans_volt[gamma];
	temp /= MULTIPLY_VALUE;

	ret = vt - (int)temp;
	//printk("[LCD] calc_v51_volt : ret : %d\n", ret);

	return ret;
}

static int calc_v87_volt(struct dim_data *data, int color)
{
	int gap;
	int vt, ret, v151, gamma;
	unsigned long temp;

	gamma = data->t_gamma[V87][color];

	if (gamma > vreg_element_max[V87]) {
		pr_err("%s : gamma overflow : %d\n", __FUNCTION__, gamma);
		gamma = vreg_element_max[V87];
	}
	if (gamma < 0) {
		pr_err("%s : gamma undeflow : %d\n", __FUNCTION__, gamma);
		gamma = 0;
	}

	vt = data->volt_vt[color];
	v151 = data->volt[TBL_INDEX_V151][color];
	//printk("[LCD] calc_v87_volt : vt : %d, v151 : %d\n", vt, v151);

	gap = vt - v151;
	temp = (unsigned long)gap * (unsigned long)v203_trans_volt[gamma];
	temp /= MULTIPLY_VALUE;

	ret = vt - (int)temp;
	//printk("[LCD] calc_v87_volt : ret : %d\n", ret);

	return ret;
}

static int calc_v151_volt(struct dim_data *data, int color)
{
	int gap;
	int vt, ret, v203, gamma;
	unsigned long temp;

	gamma = data->t_gamma[V151][color];

	if (gamma > vreg_element_max[V151]) {
		pr_err("%s : gamma overflow : %d\n", __FUNCTION__, gamma);
		gamma = vreg_element_max[V151];
	}
	if (gamma < 0) {
		pr_err("%s : gamma undeflow : %d\n", __FUNCTION__, gamma);
		gamma = 0;
	}

	vt = data->volt_vt[color];
	v203 = data->volt[TBL_INDEX_V203][color];

	gap = vt - v203;
	temp = (unsigned long)gap * (unsigned long)v203_trans_volt[gamma];
	temp /= MULTIPLY_VALUE;
	ret = vt - (int)temp;

	return ret;
}

static int calc_v203_volt(struct dim_data *data, int color)
{
	int gap;
	int vt, ret, v255, gamma;
	unsigned long temp;

	gamma = data->t_gamma[V203][color];

	if (gamma > vreg_element_max[V203]) {
		pr_err("%s : gamma overflow : %d\n", __FUNCTION__, gamma);
		gamma = vreg_element_max[V203];
	}
	if (gamma < 0) {
		pr_err("%s : gamma undeflow : %d\n", __FUNCTION__, gamma);
		gamma = 0;
	}

	vt = data->volt_vt[color];
	v255 = data->volt[TBL_INDEX_V255][color];

	gap = vt - v255;
	temp = (unsigned long)gap * (unsigned long)v203_trans_volt[gamma];
	temp /= MULTIPLY_VALUE;
	ret = vt - (int)temp;

	return ret;
}

static int calc_v255_volt(struct dim_data *data, int color)
{
	int ret, gamma;

	gamma = data->t_gamma[V255][color];

	if (gamma > vreg_element_max[V255]) {
		pr_err("%s : gamma overflow : %d\n", __FUNCTION__, gamma);
		gamma = vreg_element_max[V255];
	}
	if (gamma < 0) {
		pr_err("%s : gamma undeflow : %d\n", __FUNCTION__, gamma);
		gamma = 0;
	}

	ret = (int)v255_trans_volt[gamma];

	return ret;
}

static int calc_inter_v1_v7(struct dim_data *data, int gray, int color)
{
	int ret = 0;
	int v1, v7, ratio, temp;

	ratio = (int)int_tbl_v1_v7[gray];

	v1 = data->volt[TBL_INDEX_V1][color];
	v7 = data->volt[TBL_INDEX_V7][color];

	temp = (v1 - v7) * ratio;
	temp >>= 10;
	ret = v1 - temp;

	return ret;
}

static int calc_inter_v7_v11(struct dim_data *data, int gray, int color)
{
	int ret = 0;
	int v7, v11, ratio, temp;

	ratio = (int)int_tbl_v7_v11[gray];
	v7 = data->volt[TBL_INDEX_V7][color];
	v11 = data->volt[TBL_INDEX_V11][color];

	temp = (v7 - v11) * ratio;
	temp >>= 10;
	ret = v7 - temp;
	//printk("[LCD] ret: %d\n", ret);

	return ret;
}

static int calc_inter_v11_v23(struct dim_data *data, int gray, int color)
{
	int ret = 0;
	int v11, v23, ratio, temp;

	ratio = (int)int_tbl_v11_v23[gray];
	v11 = data->volt[TBL_INDEX_V11][color];
	v23 = data->volt[TBL_INDEX_V23][color];

	temp = (v11 - v23) * ratio;
	temp >>= 10;
	ret = v11 - temp;

	return ret;
}

static int calc_inter_v23_v35(struct dim_data *data, int gray, int color)
{
	int ret = 0;
	int v23, v35, ratio, temp;

	ratio = (int)int_tbl_v23_v35[gray];
	v23 = data->volt[TBL_INDEX_V23][color];
	v35 = data->volt[TBL_INDEX_V35][color];

	temp = (v23 - v35) * ratio;
	temp >>= 10;
	ret = v23 - temp;

	return ret;
}

static int calc_inter_v35_v51(struct dim_data *data, int gray, int color)
{
	int ret = 0;
	int v35, v51, ratio, temp;

	ratio = (int)int_tbl_v35_v51[gray];
	v35 = data->volt[TBL_INDEX_V35][color];
	v51 = data->volt[TBL_INDEX_V51][color];

	temp = (v35 - v51) * ratio;
	temp >>= 10;
	ret = v35 - temp;

	return ret;
}

static int calc_inter_v51_v87(struct dim_data *data, int gray, int color)
{
	int ret = 0;
	int v51, v87, ratio, temp;

	ratio = (int)int_tbl_v51_v87[gray];
	v51 = data->volt[TBL_INDEX_V51][color];
	v87 = data->volt[TBL_INDEX_V87][color];

	temp = (v51 - v87) * ratio;
	temp >>= 10;
	ret = v51 - temp;

	return ret;
}

static int calc_inter_v87_v151(struct dim_data *data, int gray, int color)
{
	int ret = 0;
	int v87, v151, ratio, temp;

	ratio = (int)int_tbl_v87_v151[gray];
	v87 = data->volt[TBL_INDEX_V87][color];
	v151 = data->volt[TBL_INDEX_V151][color];

	temp = (v87 - v151) * ratio;
	temp >>= 10;
	ret = v87 - temp;
	return ret;
}

static int calc_inter_v151_v203(struct dim_data *data, int gray, int color)
{
	int ret = 0;
	int v151, v203, ratio, temp;

	ratio = (int)int_tbl_v151_v203[gray];
	v151 = data->volt[TBL_INDEX_V151][color];
	v203 = data->volt[TBL_INDEX_V203][color];

	temp = (v151 - v203) * ratio;
	temp >>= 10;
	ret = v151 - temp;

	return ret;
}

static int calc_inter_v203_v255(struct dim_data *data, int gray, int color)
{
	int ret = 0;
	int v203, v255, ratio, temp;

	ratio = (int)int_tbl_v203_v255[gray];
	v203 = data->volt[TBL_INDEX_V203][color];
	v255 = data->volt[TBL_INDEX_V255][color];

	temp = (v203 - v255) * ratio;
	temp >>= 10;
	ret = v203 - temp;

	return ret;
}

static int calc_reg_v1(struct dim_data *data, int color)
{
	int ret;
	int t1, t2;
	unsigned long temp;

	t1 = DOUBLE_MULTIPLE_VREGOUT - data->look_volt[V1][color];
	t2 = DOUBLE_MULTIPLE_VREGOUT - data->look_volt[V7][color];

	temp = ((unsigned long)t1 * (unsigned long)fix_const[V1].de) / (unsigned long)t2;
	ret =  temp - fix_const[V1].nu;

	return ret;
}


static int calc_reg_v7(struct dim_data *data, int color)
{
	int ret;
	int t1, t2;
	unsigned long temp;

	t1 = DOUBLE_MULTIPLE_VREGOUT - data->look_volt[V7][color];
	t2 = DOUBLE_MULTIPLE_VREGOUT - data->look_volt[V11][color];

	temp = ((unsigned long)t1 * (unsigned long)fix_const[V7].de) / (unsigned long)t2;
	ret =  temp - fix_const[V7].nu;

	return ret;
}


static int calc_reg_v11(struct dim_data *data, int color)
{
	int ret;
	int t1, t2;
	unsigned long temp;

	t1 = data->volt_vt[color] - data->look_volt[V11][color];
	t2 = data->volt_vt[color] - data->look_volt[V23][color];

	temp = ((unsigned long)t1 * (unsigned long)fix_const[V11].de) / (unsigned long)t2;
	ret =  (int)temp - fix_const[V11].nu;

	return ret;

}

static int calc_reg_v23(struct dim_data *data, int color)
{
	int ret;
	int t1, t2;
	unsigned long temp;

	t1 = data->volt_vt[color] - data->look_volt[V23][color];
	t2 = data->volt_vt[color] - data->look_volt[V35][color];

	temp = ((unsigned long)t1 * (unsigned long)fix_const[V23].de) / (unsigned long)t2;
	ret = (int)temp - fix_const[V23].nu;

	return ret;

}

static int calc_reg_v35(struct dim_data *data, int color)
{
	int ret;
	int t1, t2;
	unsigned long temp;

	t1 = data->volt_vt[color] - data->look_volt[V35][color];
	t2 = data->volt_vt[color] - data->look_volt[V51][color];

	temp = ((unsigned long)t1 * (unsigned long)fix_const[V35].de)/ (unsigned long)t2;
	ret = (int)temp - fix_const[V35].nu;

	return ret;
}


static int calc_reg_v51(struct dim_data *data, int color)
{
	int ret;
	int t1, t2;
	unsigned long temp;

	t1 = data->volt_vt[color] - data->look_volt[V51][color];
	t2 = data->volt_vt[color] - data->look_volt[V87][color];

	temp = ((unsigned long)t1 * (unsigned long)fix_const[V51].de) / (unsigned long)t2;
	ret = (int)temp - fix_const[V51].nu;

	return ret;
}


static int calc_reg_v87(struct dim_data *data, int color)
{
	int ret;
	int t1, t2;
	unsigned long temp;

	t1 = data->volt_vt[color] - data->look_volt[V87][color];
	t2 = data->volt_vt[color] - data->look_volt[V151][color];

	temp = ((unsigned long)t1 * (unsigned long)fix_const[V87].de) / (unsigned long)t2;

	ret = (int)temp - fix_const[V87].nu;

	return ret;
}

static int calc_reg_v151(struct dim_data *data, int color)
{
	int ret;
	int t1, t2;
	unsigned long temp;

	t1 = data->volt_vt[color] - data->look_volt[V151][color];
	t2 = data->volt_vt[color] - data->look_volt[V203][color];

	temp = ((unsigned long)t1 * (unsigned long)fix_const[V151].de) / (unsigned long)t2;
	ret = (int)temp - fix_const[V151].nu;

	return ret;
}



static int calc_reg_v203(struct dim_data *data, int color)
{
	int ret;
	int t1, t2;
	unsigned long temp;

	t1 = data->volt_vt[color] - data->look_volt[V203][color];
	t2 = data->volt_vt[color] - data->look_volt[V255][color];

	temp = ((unsigned long)t1 * (unsigned long)fix_const[V203].de) / (unsigned long)t2;
	ret = (int)temp - fix_const[V203].nu;

	return ret;
}

static int calc_reg_v255(struct dim_data *data, int color)
{
	int ret;
	int t1;
	unsigned long temp;

	t1 = DOUBLE_MULTIPLE_VREGOUT -  data->look_volt[V255][color];
	temp = ((unsigned long)t1 * (unsigned long)fix_const[V255].de) / DOUBLE_MULTIPLE_VREGOUT;
	ret = (int)temp - fix_const[V255].nu;

	return ret;
}

int generate_volt_table(struct dim_data *data)
{
	int i, j;
	int seq, index, gray;
	int ret = 0;

	int calc_seq[NUM_VREF] = {V255, V203, V151, V87, V51, V35, V23, V11, V7, V1};
	int (*calc_volt_point[NUM_VREF])(struct dim_data *, int) = {
		calc_v1_volt,
		calc_v7_volt,
		calc_v11_volt,
		calc_v23_volt,
		calc_v35_volt,
		calc_v51_volt,
		calc_v87_volt,
		calc_v151_volt,
		calc_v203_volt,
		calc_v255_volt,
	};
	int (*calc_inter_volt[NUM_VREF])(struct dim_data *, int, int)  = {
		NULL,
		calc_inter_v1_v7,
		calc_inter_v7_v11,
		calc_inter_v11_v23,
		calc_inter_v23_v35,
		calc_inter_v35_v51,
		calc_inter_v51_v87,
		calc_inter_v87_v151,
		calc_inter_v151_v203,
		calc_inter_v203_v255,
	};

	for (i = 0; i < CI_MAX; i++)
		data->volt_vt[i] = calc_vt_volt(data->vt_mtp[i]);

	/* calculate voltage for V0 */
	for (i = 0; i < CI_MAX; i++)
		data->volt[0][i] = calc_v0_volt(data->v0_mtp[i]);

	/* calculate voltage for every vref point */
	for (j = 0; j < NUM_VREF; j++) {
		seq = calc_seq[j];
		index = vref_index[seq];
		if (calc_volt_point[seq] != NULL) {
			for (i = 0; i < CI_MAX; i++)
				data->volt[index][i] = calc_volt_point[seq](data ,i);
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
			if (calc_inter_volt[index] != NULL) {
				data->volt[i][j] = calc_inter_volt[index](data, gray, j);
			}
		}

	}
#if defined (SMART_DIMMING_DEBUG)
	printk("=========================== VT Voltage ===========================\n");

	printk("R : %05d : G: %05d : B : %05d\n",
					data->volt_vt[0], data->volt_vt[1], data->volt_vt[2]);

	printk("\n=================================================================\n");

	for (i = 0; i < MAX_GRADATION; i++) {
		printk("V%03d R : %05d : G : %05d B : %05d\n", i,
					data->volt[i][CI_RED], data->volt[i][CI_GREEN], data->volt[i][CI_BLUE]);
	}
#endif
	return ret;
}

static int lookup_volt_index(struct dim_data *data, int gray)
{
	int ret, i;
	int temp;
	int index;
	int index_l, index_h, exit;
	int cnt_l, cnt_h;
	int p_delta, delta;

	temp = gray / (MULTIPLY_VALUE * MULTIPLY_VALUE);

	index = (int)lookup_tbl[temp];

	exit = 1;
	i = 0;
	while(exit) {
		index_l = temp - i;
		index_h = temp + i;
		if (index_l < 0)
			index_l = 0;
		if (index_h > MAX_BRIGHTNESS_COUNT)
			index_h = MAX_BRIGHTNESS_COUNT;
		cnt_l = (int)lookup_tbl[index] - (int)lookup_tbl[index_l];
		cnt_h = (int)lookup_tbl[index_h] - (int)lookup_tbl[index];

		if (cnt_l + cnt_h) {
			exit = 0;
		}
		i++;
	}

	p_delta = 0;
	index = (int)lookup_tbl[index_l];
	ret = index;

	temp = gamma_double_multi_tbl[index];
	//temp = gamma_multi_tbl[index] * MULTIPLY_VALUE;

	if (gray > temp)
		p_delta = gray - temp;
	else
		p_delta = temp - gray;

	for (i = 0; i <= (cnt_l + cnt_h); i++) {
		temp = gamma_double_multi_tbl[index + i];
		//temp = gamma_multi_tbl[index + i] * MULTIPLY_VALUE;
		if (gray > temp)
			delta = gray - temp;
		else
			delta = temp - gray;

		if (delta < p_delta) {
			p_delta = delta;
			ret = index + i;
		}
	}

	return ret;
}

int cal_gamma_from_index(struct dim_data *data, struct SmtDimInfo *brInfo)
{
	int i, j;
	int ret = 0;
	int gray, index;
	signed int shift, c_shift;
	int gamma_int[NUM_VREF][CI_MAX];
	int br, temp;
	unsigned char *result;
	int (*calc_reg[NUM_VREF])(struct dim_data *, int)  = {
		calc_reg_v1,
		calc_reg_v7,
		calc_reg_v11,
		calc_reg_v23,
		calc_reg_v35,
		calc_reg_v51,
		calc_reg_v87,
		calc_reg_v151,
		calc_reg_v203,
		calc_reg_v255,
	};

	br = brInfo->refBr;

	result = brInfo->gamma;

	if (br > MAX_BRIGHTNESS_COUNT) {
		printk("Warning Exceed Max brightness : %d\n", br);
		br = MAX_BRIGHTNESS_COUNT;
	}
	for (i = V1; i < NUM_VREF; i++) {
		/* get reference shift value */
		if (brInfo->rTbl == NULL) {
			shift = 0;
		}
		else {
			shift = (signed int)brInfo->rTbl[i];
		}

		gray = brInfo->cGma[vref_index[i]] * br;
		index = lookup_volt_index(data, gray);
		index = index + shift;

		if(i == V1)
			index = 1;
		for (j = 0; j < CI_MAX; j++) {
			if (calc_reg[i] != NULL) {
				data->look_volt[i][j] = data->volt[index][j];
			}
		}
	}

	for (i = V1; i < NUM_VREF; i++) {
		for (j = 0; j < CI_MAX; j++) {
			if (calc_reg[i] != NULL) {
				index = (i * CI_MAX) + j;

				if (brInfo->cTbl == NULL)
					c_shift = 0;
				else
					c_shift = (signed int)brInfo->cTbl[index];

				temp = calc_reg[i](data, j);
				gamma_int[i][j] = (temp + c_shift) - data->mtp[i][j];

				if (gamma_int[i][j] >= vreg_element_max[i])
					gamma_int[i][j] = vreg_element_max[i];
				if (gamma_int[i][j] < 0)
					gamma_int[i][j] = 0;
			}
		}
	}

	index = 0;
	result[index++] = OLED_CMD_GAMMA;

	for (i = V255; i >= V1; i--) {
		if (i == V255) {
			result[index] = (gamma_int[i][CI_RED] > 0xff ? 1 : 0)<<2;
			result[index] |= (gamma_int[i][CI_GREEN] > 0xff ? 1 : 0)<<1;
			result[index++] |= (gamma_int[i][CI_BLUE] > 0xff ? 1 : 0)<<0;
		}
		for (j = 0; j < CI_MAX; j++) {
			//printk("%3d, ", gamma_int[i][j]);
			if (i == V255) {
				//result[index++] = gamma_int[i][j] > 0xff ? 1 : 0;
				result[index++] = gamma_int[i][j] & 0xff;
			} else
				result[index++] = (unsigned char)gamma_int[i][j];
		}
	}
		//printk("\n");
	result[index++] = 0x00;
	result[index++] = 0x00;

	return ret;
}


