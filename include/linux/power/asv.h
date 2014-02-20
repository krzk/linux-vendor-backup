/*
 * Adaptive Supply Voltage Header File
 *
 * copyright (c) 2013 samsung electronics co., ltd.
 *		http://www.samsung.com/
 *
 * this program is free software; you can redistribute it and/or modify
 * it under the terms of the gnu general public license version 2 as
 * published by the free software foundation.
*/

#ifndef __ASV_H
#define __ASV_H __FILE__

#define ABB_X060        0
#define ABB_X065        1
#define ABB_X070        2
#define ABB_X075        3
#define ABB_X080        4
#define ABB_X085        5
#define ABB_X090        6
#define ABB_X095        7
#define ABB_X100        8
#define ABB_X105        9
#define ABB_X110        10
#define ABB_X115        11
#define ABB_X120        12
#define ABB_X125        13
#define ABB_X130        14
#define ABB_X135        15
#define ABB_X140        16
#define ABB_X145        17
#define ABB_X150        18
#define ABB_X155        19
#define ABB_X160        20
#define ABB_BYPASS      255

#define ABB_INIT        (0x80000080)
#define ABB_INIT_BYPASS (0x80000000)

static inline void set_abb(void __iomem *target_reg, unsigned int target_value)
{
    unsigned int tmp;

    if (target_value == ABB_BYPASS)
        tmp = ABB_INIT_BYPASS;
    else
        tmp = (ABB_INIT | target_value);

    writel_relaxed(tmp, target_reg);
}

enum asv_type_id {
	ASV_ARM,
	ASV_INT,
	ASV_MIF,
	ASV_G3D,
	ASV_KFC,
	ASV_INT_MIF_L0,
	ASV_INT_MIF_L1,
	ASV_INT_MIF_L2,
	ASV_INT_MIF_L3,
};

#ifdef CONFIG_POWER_ASV
/* asv_get_volt - get the ASV for target_freq for particular target_type.
 *	returns 0 if target_freq is not supported
 */
extern unsigned int asv_get_volt(enum asv_type_id target_type,
					unsigned int target_freq);
extern int asv_init_opp_table(struct device *dev,
					enum asv_type_id target_type);
#else
static inline unsigned int asv_get_volt(enum asv_type_id target_type,
				unsigned int target_freq) { return 0; }
static int asv_init_opp_table(struct device *dev, enum asv_type_id target_type)
	{ return 0; }

#endif /* CONFIG_POWER_EXYNOS_AVS */
#endif /* __ASV_H */
