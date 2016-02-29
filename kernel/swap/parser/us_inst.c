/**
 * parser/us_inst.c
 * @author Vyacheslav Cherkashin
 *
 * @section LICENSE
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
 * @section COPYRIGHT
 *
 * Copyright (C) Samsung Electronics, 2013
 *
 * @section DESCRIPTION
 *
 * User-space instrumentation controls.
 */


#include <linux/module.h>
#include <linux/version.h>
#include <linux/errno.h>
#include <linux/namei.h>
#include <us_manager/pf/pf_group.h>
#include "msg_parser.h"
#include "us_inst.h"

/* FIXME: create get_dentry() and put_dentry() */
static struct dentry *dentry_by_path(const char *path)
{
	struct dentry *dentry;
	struct path st_path;
	if (kern_path(path, LOOKUP_FOLLOW, &st_path) != 0) {
		printk("failed to lookup dentry for path %s!\n", path);
		return NULL;
	}

	dentry = st_path.dentry;
	path_put(&st_path);
	return dentry;
}


static int mod_func_inst(struct func_inst_data *func, struct pf_group *pfg,
			 struct dentry *dentry, enum MOD_TYPE mt)
{
	int ret;

	switch (mt) {
	case MT_ADD:
		ret = pf_register_probe(pfg, dentry, func->addr, func->args,
					func->ret_type);
		break;
	case MT_DEL:
		ret = pf_unregister_probe(pfg, dentry, func->addr);
		break;
	default:
		printk("ERROR: mod_type=0x%x\n", mt);
		ret = -EINVAL;
	}

	return ret;
}

static int mod_lib_inst(struct lib_inst_data *lib, struct pf_group *pfg,
			enum MOD_TYPE mt)
{
	int ret = 0, i;
	struct dentry *dentry;

	dentry = dentry_by_path(lib->path);
	if (dentry == NULL) {
		printk("Cannot get dentry by path %s\n", lib->path);
		return -EINVAL;
	}

	for (i = 0; i < lib->cnt_func; ++i) {
		ret = mod_func_inst(lib->func[i], pfg, dentry, mt);
		if (ret) {
			printk("Cannot mod func inst, ret = %d\n", ret);
			return ret;
		}
	}

	return ret;
}

static int get_pfg_by_app_info(struct app_info_data *app_info, struct pf_group **pfg)
{
	struct dentry *dentry;

	dentry = dentry_by_path(app_info->exec_path);
	if (dentry == NULL)
		return -EINVAL;

	switch (app_info->app_type) {
	case AT_PID:
		if (app_info->tgid == 0) {
			if (app_info->exec_path[0] == '\0')
				*pfg = get_pf_group_dumb(dentry);
			else
				goto pf_dentry;
		} else
			*pfg = get_pf_group_by_tgid(app_info->tgid, dentry);
		break;
	case AT_TIZEN_WEB_APP:
		*pfg = get_pf_group_by_comm(app_info->app_id, dentry);
		break;
	case AT_TIZEN_NATIVE_APP:
	case AT_COMMON_EXEC:
 pf_dentry:
		*pfg = get_pf_group_by_dentry(dentry, dentry);
		break;
	default:
		printk("ERROR: app_type=0x%x\n", app_info->app_type);
		return -EINVAL;
	}

	return 0;
}

static int mod_us_app_inst(struct app_inst_data *app_inst, enum MOD_TYPE mt)
{
	int ret, i;
	struct pf_group *pfg;
	struct dentry *dentry;

	ret = get_pfg_by_app_info(app_inst->app_info, &pfg);
	if (ret) {
		printk("Cannot get pfg by app info, ret = %d\n", ret);
		return ret;
	}

	for (i = 0; i < app_inst->cnt_func; ++i) {
		/* TODO: */
		dentry = dentry_by_path(app_inst->app_info->exec_path);
		if (dentry == NULL) {
			printk("Cannot find dentry by path %s\n",
			       app_inst->app_info->exec_path);
			return -EINVAL;
		}

		ret = mod_func_inst(app_inst->func[i], pfg, dentry, mt);
		if (ret) {
			printk("Cannot mod func inst, ret = %d\n", ret);
			return ret;
		}
	}

	for (i = 0; i < app_inst->cnt_lib; ++i) {
		ret = mod_lib_inst(app_inst->lib[i], pfg, mt);
		if (ret) {
			printk("Cannot mod lib inst, ret = %d\n", ret);
			return ret;
		}
	}

	return 0;
}

/**
 * @brief Registers probes.
 *
 * @param us_inst Pointer to the target us_inst_data struct.
 * @param mt Modificator, indicates whether we install or remove probes.
 * @return 0 on suceess, error code on error.
 */
int mod_us_inst(struct us_inst_data *us_inst, enum MOD_TYPE mt)
{
	u32 i;
	int ret;

	for (i = 0; i < us_inst->cnt; ++i) {
		ret = mod_us_app_inst(us_inst->app_inst[i], mt);
		if (ret) {
			printk("Cannot mod us app inst, ret = %d\n", ret);
			return ret;
		}
	}

	return 0;
}
