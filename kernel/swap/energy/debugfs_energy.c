/*
 *  Dynamic Binary Instrumentation Module based on KProbes
 *  energy/debugfs_energy.c
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 * Copyright (C) Samsung Electronics, 2013
 *
 * 2013         Vyacheslav Cherkashin <v.cherkashin@samsung.com>
 *
 */


#include <linux/fs.h>
#include <linux/module.h>
#include <linux/debugfs.h>
#include <linux/math64.h>
#include <master/swap_debugfs.h>
#include "energy.h"
#include "debugfs_energy.h"
#include "rational_debugfs.h"
#include "lcd/lcd_debugfs.h"


/* CPU running */
static DEFINE_RATIONAL(cpu0_running_coef); /* boot core uses distinct coeff */
static DEFINE_RATIONAL(cpuN_running_coef);

static u64 __energy_cpu(enum parameter_energy pe)
{
	u64 times[NR_CPUS] = { 0 };
	u64 val = 0;
	int i;

	if (get_parameter_energy(pe, times, sizeof(times)) == 0) {
		val = div_u64(times[0] * cpu0_running_coef.num,
			      cpu0_running_coef.denom);
		for (i = 1; i < NR_CPUS; i++)
			val += div_u64(times[i] * cpuN_running_coef.num,
				       cpuN_running_coef.denom);
	}

	return val;
}

static u64 cpu_system(void)
{
	return __energy_cpu(PE_TIME_SYSTEM);
}

static u64 cpu_apps(void)
{
	return __energy_cpu(PE_TIME_APPS);
}


/* CPU idle */
static DEFINE_RATIONAL(cpu_idle_coef);

static u64 cpu_idle_system(void)
{
	u64 time = 0;

	get_parameter_energy(PE_TIME_IDLE, &time, sizeof(time));
	return div_u64(time * cpu_idle_coef.num, cpu_idle_coef.denom);
}


/* flash read */
static DEFINE_RATIONAL(fr_coef);

static u64 fr_system(void)
{
	u64 byte = 0;

	get_parameter_energy(PE_READ_SYSTEM, &byte, sizeof(byte));
	return div_u64(byte * fr_coef.num, fr_coef.denom);
}

static u64 fr_apps(void)
{
	u64 byte = 0;

	get_parameter_energy(PE_READ_APPS, &byte, sizeof(byte));
	return div_u64(byte * fr_coef.num, fr_coef.denom);
}


/* flash write */
static DEFINE_RATIONAL(fw_coef);

static u64 fw_system(void)
{
	u64 byte = 0;

	get_parameter_energy(PE_WRITE_SYSTEM, &byte, sizeof(byte));
	return div_u64(byte * fw_coef.num, fw_coef.denom);
}

static u64 fw_apps(void)
{
	u64 byte = 0;

	get_parameter_energy(PE_WRITE_APPS, &byte, sizeof(byte));
	return div_u64(byte * fw_coef.num, fw_coef.denom);
}





/* ============================================================================
 * ===                             PARAMETERS                               ===
 * ============================================================================
 */
static int get_func_u64(void *data, u64 *val)
{
	u64 (*func)(void) = data;
	*val = func();
	return 0;
}

SWAP_DEFINE_SIMPLE_ATTRIBUTE(fops_get_u64, get_func_u64, NULL, "%llu\n");


struct param_data {
	char *name;
	struct rational *coef;
	u64 (*system)(void);
	u64 (*apps)(void);
};

static struct dentry *create_parameter(struct dentry *parent,
				       struct param_data *param)
{
	struct dentry *name, *system, *apps = NULL;

	name = debugfs_create_dir(param->name, parent);
	if (name == NULL)
		return NULL;

	system = debugfs_create_file("system", 0600, name, param->system,
				     &fops_get_u64);
	if (system == NULL)
		goto rm_name;

	if (param->apps) {
		apps = debugfs_create_file("apps", 0600, name, param->apps,
					   &fops_get_u64);
		if (apps == NULL)
			goto rm_system;
	}

	if (create_rational_files(name, param->coef,
				  "numerator", "denominator"))
		goto rm_apps;

	return name;

rm_apps:
	if (param->apps)
		debugfs_remove(apps);
rm_system:
	debugfs_remove(system);
rm_name:
	debugfs_remove(name);

	return NULL;
}

struct param_data parameters[] = {
	{
		.name = "cpu_running",
		.coef = &cpu0_running_coef,
		.system = cpu_system,
		.apps = cpu_apps
	},
	{
		.name = "cpuN_running",
		.coef = &cpuN_running_coef,
		.system = cpu_system,
		.apps = cpu_apps
	},
	{
		.name = "cpu_idle",
		.coef = &cpu_idle_coef,
		.system = cpu_idle_system,
		.apps = NULL
	},
	{
		.name = "flash_read",
		.coef = &fr_coef,
		.system = fr_system,
		.apps = fr_apps
	},
	{
		.name = "flash_write",
		.coef = &fw_coef,
		.system = fw_system,
		.apps = fw_apps
	}
};

enum {
	parameters_cnt = sizeof(parameters) / sizeof(struct param_data)
};





/* ============================================================================
 * ===                              INIT/EXIT                               ===
 * ============================================================================
 */
static struct dentry *energy_dir = NULL;

/**
 * @brief Destroy debugfs for LCD
 *
 * @return Dentry of energy debugfs
 */
struct dentry *get_energy_dir(void)
{
	return energy_dir;
}

/**
 * @brief Destroy debugfs for energy
 *
 * @return Void
 */
void exit_debugfs_energy(void)
{
	if (energy_dir)
		debugfs_remove_recursive(energy_dir);

	energy_dir = NULL;
}

/**
 * @brief Create debugfs for energy
 *
 * @return Error code
 */
int init_debugfs_energy(void)
{
	int i;
	struct dentry *swap_dir, *dentry;

	swap_dir = swap_debugfs_getdir();
	if (swap_dir == NULL)
		return -ENOENT;

	energy_dir = debugfs_create_dir("energy", swap_dir);
	if (energy_dir == NULL)
		return -ENOMEM;

	for (i = 0; i < parameters_cnt; ++i) {
		dentry = create_parameter(energy_dir, &parameters[i]);
		if (dentry == NULL)
			goto fail;
	}

	return 0;

fail:
	exit_debugfs_energy();
	return -ENOMEM;
}
