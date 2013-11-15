/*
 * Adaptive Supply Voltage Driver Header File
 *
 * copyright (c) 2013 samsung electronics co., ltd.
 *		http://www.samsung.com/
 *
 * this program is free software; you can redistribute it and/or modify
 * it under the terms of the gnu general public license version 2 as
 * published by the free software foundation.
*/

#ifndef __ASV_D_H
#define __ASV_D_H __FILE__

#include <linux/power/asv.h>

struct asv_freq_table {
	unsigned int	freq;	/* KHz */
	unsigned int	volt;	/* uV */
};

/* struct asv_info - information of ASV member for intialisation
 *
 * Each member to be registered should be described using this struct
 * intialised with all required information for that member.
 *
 * @name: Name to use for member.
 * @asv_type_id: Type to identify particular member.
 * @asv_ops: Callbacks which can be used for SoC specific operations.
 * @nr_dvfs_level: Number of dvfs levels supported by member.
 * @dvfs_table: Table containing supported ASV freqs and corresponding volts.
 * @asv_grp: ASV group of member.
 * @flags: ASV flags
 */
struct asv_info {
	const char		*name;
	enum asv_type_id	type;
	struct asv_ops		*ops;
	unsigned int		nr_dvfs_level;
	struct asv_freq_table	*dvfs_table;
	unsigned int		asv_grp;
	unsigned int		flags;
};

/* struct asv_ops - SoC specific operation for ASV members
 * @get_asv_group - Calculates and initializes asv_grp of asv_info.
 * @init_asv - SoC specific initialisation (if required) based on asv_grp.
 * @init_asv_table - Initializes linear array(dvfs_table) for corresponding
 *			asv_grp.
 *
 * All ops should return 0 on sucess.
 */
struct asv_ops {
	int (*init_asv)(struct asv_info *);
	int (*get_asv_group)(struct asv_info *);
	int (*init_asv_table)(struct asv_info *);
};

/* function for registering ASV members */
void register_asv_member(struct asv_info *list, unsigned int nr_member);

#endif /* __ASV_D_H */
