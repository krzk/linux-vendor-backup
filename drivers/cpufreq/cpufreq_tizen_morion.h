/*
 * Header file for CPUFreq Morion governor and related code.
 *
 * Copyright (C) 2016, Intel Corporation
 * Author: Rafael J. Wysocki <rafael.j.wysocki@intel.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include "cpufreq_governor.h"

struct morion_policy_dbs_info {
	struct policy_dbs_info policy_dbs;
	unsigned int freq_lo;
	unsigned int freq_lo_delay_us;
	unsigned int freq_hi_delay_us;
	unsigned int sample_type:1;
};

static inline struct morion_policy_dbs_info *to_dbs_info(struct policy_dbs_info *policy_dbs)
{
	return container_of(policy_dbs, struct morion_policy_dbs_info, policy_dbs);
}

struct morion_dbs_tuners {
	unsigned int powersave_bias;
};
