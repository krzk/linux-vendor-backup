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

enum asv_type_id {
	ASV_ARM,
	ASV_INT,
	ASV_MIF,
	ASV_G3D,
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
