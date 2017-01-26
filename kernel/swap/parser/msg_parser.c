/**
 * parser/msg_parser.c
 *
 * @author Vyacheslav Cherkashin
 * @author Vitaliy Cherepanov <v.cherepanov@samsung.com>
 *
 * @sectionLICENSE
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
 * Message parsing implementation.
 */


#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <us_manager/probes/probes.h>
#include "msg_parser.h"
#include "msg_buf.h"
#include "parser_defs.h"

/* ============================================================================
 * ==                               APP_INFO                                 ==
 * ============================================================================
 */

/**
 * @brief Creates and fills app_info_data struct.
 *
 * @param mb Pointer to the message buffer.
 * @param ai Pointer to the target app_inst_data.
 * @return 0 on success, error code on error.
 */
int create_app_info(struct msg_buf *mb, struct app_inst_data *ai)
{
	int ret;
	u32 app_type;
	char *ta_id, *exec_path;

	print_parse_debug("app_info:\n");

	print_parse_debug("type:");
	ret = get_u32(mb, &app_type);
	if (ret) {
		print_err("failed to read target application type\n");
		return -EINVAL;
	}

	print_parse_debug("id:");
	ret = get_string(mb, &ta_id);
	if (ret) {
		print_err("failed to read target application ID\n");
		return -EINVAL;
	}

	print_parse_debug("exec path:");
	ret = get_string(mb, &exec_path);
	if (ret) {
		print_err("failed to read executable path\n");
		goto free_ta_id;
	}

	switch (app_type) {
	case AT_TIZEN_NATIVE_APP:
	case AT_TIZEN_WEB_APP:
	case AT_COMMON_EXEC:
		ai->tgid = 0;
		break;
	case AT_PID: {
		u32 tgid = 0;

		if (*ta_id != '\0') {
			ret = kstrtou32(ta_id, 10, &tgid);
			if (ret) {
				print_err("converting string to PID, "
					  "str='%s'\n", ta_id);
				goto free_exec_path;
			}
		}

		ai->tgid = tgid;
		break;
	}
	default:
		print_err("wrong application type(%u)\n", app_type);
		ret = -EINVAL;
		goto free_exec_path;
	}

	ai->type = (enum APP_TYPE)app_type;
	ai->id = ta_id;
	ai->path = exec_path;

	return 0;

free_exec_path:
	put_string(exec_path);

free_ta_id:
	put_string(ta_id);

	return -EINVAL;
}



/* ============================================================================
 * ==                                CONFIG                                  ==
 * ============================================================================
 */

/**
 * @brief Creates and fills conf_data struct.
 *
 * @param mb Pointer to the message buffer.
 * @return Pointer to the filled conf_data struct on success;\n
 * NULL on error.
 */
struct conf_data *create_conf_data(struct msg_buf *mb)
{
	struct conf_data *conf;
	u64 use_features0, use_features1;
	u32 stp, dmp;

	print_parse_debug("conf_data:\n");

	print_parse_debug("features:");
	if (get_u64(mb, &use_features0)) {
		print_err("failed to read use_features\n");
		return NULL;
	}

	if (get_u64(mb, &use_features1)) {
		print_err("failed to read use_features\n");
		return NULL;
	}

	print_parse_debug("sys trace period:");
	if (get_u32(mb, &stp)) {
		print_err("failed to read sys trace period\n");
		return NULL;
	}

	print_parse_debug("data msg period:");
	if (get_u32(mb, &dmp)) {
		print_err("failed to read data message period\n");
		return NULL;
	}

	conf = kmalloc(sizeof(*conf), GFP_KERNEL);
	if (conf == NULL) {
		print_err("out of memory\n");
		return NULL;
	}

	conf->use_features0 = use_features0;
	conf->use_features1 = use_features1;
	conf->sys_trace_period = stp;
	conf->data_msg_period = dmp;

	return conf;
}

/**
 * @brief conf_data cleanup.
 *
 * @param conf Pointer to the target conf_data.
 * @return Void.
 */
void destroy_conf_data(struct conf_data *conf)
{
	kfree(conf);
}

static struct conf_data config;

/**
 * @brief Saves config to static config variable.
 *
 * @param conf Variable to save.
 * @return Void.
 */
void save_config(const struct conf_data *conf)
{
	memcpy(&config, conf, sizeof(config));
}

/**
 * @brief Restores config from static config variable.
 *
 * @param conf Variable to restore.
 * @return Void.
 */
void restore_config(struct conf_data *conf)
{
	memcpy(conf, &config, sizeof(*conf));
}



/* ============================================================================
 * ==                             PROBES PARSING                             ==
 * ============================================================================
 */

/**
 * @brief Gets retprobe data and puts it to the probe_info struct.
 *
 * @param mb Pointer to the message buffer.
 * @param pd Pointer to the probe_desc struct.
 * @return 0 on success, error code on error.
 */
int get_retprobe(struct msg_buf *mb, struct probe_desc *pd)
{
	char *args;
	char ret_type;

	print_parse_debug("funct args:");
	if (get_string(mb, &args)) {
		print_err("failed to read data function arguments\n");
		return -EINVAL;
	}

	print_parse_debug("funct ret type:");
	if (get_u8(mb, (u8 *)&ret_type)) {
		print_err("failed to read data function arguments\n");
		goto free_args;
	}

	pd->type = SWAP_RETPROBE;
	pd->info.rp_i.args = args;
	pd->info.rp_i.ret_type = ret_type;

	return 0;

free_args:
	put_string(args);
	return -EINVAL;
}

/**
 * @brief Gets webprobe data and puts it to the probe_info struct.
 *
 * @param mb Pointer to the message buffer.
 * @param pd Pointer to the probe_desc struct.
 * @return 0 on success, error code on error.
 */
int get_webprobe(struct msg_buf *mb, struct probe_desc *pd)
{
	pd->type = SWAP_WEBPROBE;

	return 0;
}

/**
 * @brief Retprobe data cleanup.
 *
 * @param pi Pointer to the probe_info comprising retprobe.
 * @return Void.
 */
void put_retprobe(struct probe_info *pi)
{
	put_string(pi->rp_i.args);
}

/**
 * @brief Gets preload data and puts it to the probe_info struct.
 *
 * @param mb Pointer to the message buffer.
 * @param pd Pointer to the probe_desc struct.
 * @return 0 on success, error code on error.
 */
int get_preload_probe(struct msg_buf *mb, struct probe_desc *pd)
{
	u64 handler;
	u8 flags;

	print_parse_debug("funct handler:");
	if (get_u64(mb, &handler)) {
		print_err("failed to read function handler\n");
		return -EINVAL;
	}

	print_parse_debug("collect events flag:");
	if (get_u8(mb, &flags)) {
		print_err("failed to read collect events type\n");
		return -EINVAL;
	}

	pd->type = SWAP_PRELOAD_PROBE;
	pd->info.pl_i.handler = handler;
	pd->info.pl_i.flags = flags;

	return 0;
}

/**
 * @brief Preload probe data cleanup.
 *
 * @param pi Pointer to the probe_info struct.
 * @return Void.
 */
void put_preload_probe(struct probe_info *pi)
{
}

/**
 * @brief Gets preload get_caller and puts it to the probe_info struct.
 *
 * @param mb Pointer to the message buffer.
 * @param pd Pointer to the probe_desc struct.
 * @return 0 on success, error code on error.
 */

int get_get_caller_probe(struct msg_buf *mb, struct probe_desc *pd)
{
	pd->type = SWAP_GET_CALLER;

	return 0;
}

/**
 * @brief Preload get_caller probe data cleanup.
 *
 * @param pi Pointer to the probe_info struct.
 * @return Void.
 */
void put_get_caller_probe(struct probe_info *pi)
{
}

/**
 * @brief Gets preload get_call_type and puts it to the probe_info struct.
 *
 * @param mb Pointer to the message buffer.
 * @param pd Pointer to the probe_desc struct.
 * @return 0 on success, error code on error.
 */
int get_get_call_type_probe(struct msg_buf *mb, struct probe_desc *pd)
{
	pd->type = SWAP_GET_CALL_TYPE;

	return 0;
}

/**
 * @brief Preload get_call type probe data cleanup.
 *
 * @param pi Pointer to the probe_info struct.
 * @return Void.
 */
void put_get_call_type_probe(struct probe_info *pi)
{
}

/**
 * @brief Gets preload write_msg and puts it to the probe_info struct.
 *
 * @param mb Pointer to the message buffer.
 * @param pi Pointer to the probe_info struct.
 * @return 0 on success, error code on error.
 */
int get_write_msg_probe(struct msg_buf *mb, struct probe_desc *pd)
{
	pd->type = SWAP_WRITE_MSG;

	return 0;
}

/**
 * @brief Preload write_msg type probe data cleanup.
 *
 * @param pi Pointer to the probe_info comprising retprobe.
 * @return Void.
 */
void put_write_msg_probe(struct probe_info *pi)
{
}




/**
 * @brief Gets FBI probe data and puts it to the probe_info struct.
 *
 * @param mb Pointer to the message buffer.
 * @param pi Pointer to the probe_info struct.
 * @return 0 on success, error code on error.
 */
int get_fbi_data(struct msg_buf *mb, struct fbi_var_data *vd)
{
	u64 var_id;
	u64 reg_offset;
	u8 reg_n;
	u32 data_size;
	u8 steps_count, i;
	struct fbi_step *steps = NULL;

	print_parse_debug("var ID:");
	if (get_u64(mb, &var_id)) {
		print_err("failed to read var ID\n");
		return -EINVAL;
	}

	print_parse_debug("register offset:");
	if (get_u64(mb, &reg_offset)) {
		print_err("failed to read register offset\n");
		return -EINVAL;
	}

	print_parse_debug("register number:");
	if (get_u8(mb, &reg_n)) {
		print_err("failed to read number of the register\n");
		return -EINVAL;
	}

	print_parse_debug("data size:");
	if (get_u32(mb, &data_size)) {
		print_err("failed to read data size\n");
		return -EINVAL;
	}

	print_parse_debug("steps count:");
	if (get_u8(mb, &steps_count)) {
		print_err("failed to read steps count\n");
		return -EINVAL;
	}

	if (steps_count > 0) {
		steps = kmalloc(steps_count * sizeof(*vd->steps),
				GFP_KERNEL);
		if (steps == NULL) {
			print_err("MALLOC FAIL\n");
			return -ENOMEM;
		}

		for (i = 0; i != steps_count; i++) {
			print_parse_debug("steps #%d ptr_order:", i);
			if (get_u8(mb, &(steps[i].ptr_order))) {
				print_err("failed to read pointer order(step #%d)\n",
					  i);
				goto free_steps;
			}
			print_parse_debug("steps #%d data_offset:", i);
			if (get_u64(mb, &(steps[i].data_offset))){
				print_err("failed to read offset (steps #%d)\n",
					  i);
				goto free_steps;
			}
		}
	}

	vd->reg_n = reg_n;
	vd->reg_offset = reg_offset;
	vd->data_size = data_size;
	vd->var_id = var_id;
	vd->steps_count = steps_count;
	vd->steps = steps;

	return 0;

free_steps:
	kfree(steps);
	return -EINVAL;
}

int get_fbi_probe(struct msg_buf *mb, struct probe_desc *pd)
{
	uint8_t var_count, i;
	struct fbi_var_data *vars;

	print_parse_debug("var count:");
	if (get_u8(mb, &var_count)) {
		print_err("failed to read var ID\n");
		return -EINVAL;
	}

	vars = kmalloc(var_count * sizeof(*vars), GFP_KERNEL);
	if (vars == NULL) {
		print_err("alloc vars error\n");
		goto err;
	}

	for (i = 0; i != var_count; i++) {
		if (get_fbi_data(mb, &vars[i]) != 0)
			goto free_vars;
	}

	pd->type = SWAP_FBIPROBE;
	pd->info.fbi_i.var_count = var_count;
	pd->info.fbi_i.vars = vars;
	return 0;

free_vars:
	kfree(vars);

err:
	return -EINVAL;

}

/**
 * @brief FBI probe data cleanup.
 *
 * @param pi Pointer to the probe_info comprising FBI probe.
 * @return Void.
 */
void put_fbi_probe(struct probe_info *pi)
{
	return;
}


/* ============================================================================
 * ==                               FUNC_INST                                ==
 * ============================================================================
 */

/**
 * @brief Creates and fills func_inst_data struct.
 *
 * @param mb Pointer to the message buffer.
 * @return Pointer to the filled func_inst_data struct on success;\n
 * 0 on error.
 */
struct func_inst_data *create_func_inst_data(struct msg_buf *mb)
{
	struct func_inst_data *fi;
	struct probe_desc *pd;
	u64 addr;
	u8 type;

	print_parse_debug("func addr:");
	if (get_u64(mb, &addr)) {
		print_err("failed to read data function address\n");
		return NULL;
	}

	print_parse_debug("probe type:");
	if (get_u8(mb, &type)) {
		print_err("failed to read data probe type\n");
		return NULL;
	}

	fi = kmalloc(sizeof(*fi), GFP_KERNEL);
	if (fi == NULL) {
		print_err("out of memory\n");
		return NULL;
	}
	INIT_LIST_HEAD(&fi->list);
	fi->registered = 0;

	pd = &fi->p_desc;

	switch (type) {
	case SWAP_RETPROBE:
		if (get_retprobe(mb, pd) != 0)
			goto err;
		break;
	case SWAP_WEBPROBE:
		if (get_webprobe(mb, pd) != 0)
			goto err;
		break;
	case SWAP_PRELOAD_PROBE:
		if (get_preload_probe(mb, pd) != 0)
			goto err;
		break;
	case SWAP_GET_CALLER:
		if (get_get_caller_probe(mb, pd) != 0)
			goto err;
		break;
	case SWAP_GET_CALL_TYPE:
		if (get_get_call_type_probe(mb, pd) != 0)
			goto err;
		break;
	case SWAP_FBIPROBE:
		if (get_fbi_probe(mb, pd) != 0)
			goto err;
		break;
	case SWAP_WRITE_MSG:
		if (get_write_msg_probe(mb, pd) != 0)
			goto err;
		break;
	default:
		printk(KERN_WARNING "SWAP PARSER: Wrong probe type %d!\n",
		       type);
		goto err;
	}

	fi->addr = addr;
	return fi;
err:

	kfree(fi);
	return NULL;
}

/**
 * @brief func_inst_data cleanup.
 *
 * @param fi Pointer to the target func_inst_data.
 * @return Void.
 */
void destroy_func_inst_data(struct func_inst_data *fi)
{
	switch (fi->p_desc.type) {
	case SWAP_RETPROBE:
		put_retprobe(&(fi->p_desc.info));
		break;
	case SWAP_WEBPROBE:
		break;
	case SWAP_PRELOAD_PROBE:
		put_preload_probe(&(fi->p_desc.info));
		break;
	case SWAP_GET_CALLER:
		put_get_caller_probe(&(fi->p_desc.info));
		break;
	case SWAP_GET_CALL_TYPE:
		put_get_call_type_probe(&(fi->p_desc.info));
		break;
	case SWAP_FBIPROBE:
		put_fbi_probe(&(fi->p_desc.info));
		break;
	case SWAP_WRITE_MSG:
		put_write_msg_probe(&(fi->p_desc.info));
		break;
	default:
		printk(KERN_WARNING "SWAP PARSER: Wrong probe type %d!\n",
		   fi->p_desc.type);
	}

	kfree(fi);
}

/**
 * @brief func_inst_data find.
 *
 * @param head Pointer to the list head with func_inst_data.
 * @param func Pointer to the func_inst_data looking for.
 * @return Pointer to the found func_inst_data struct on success;\n
 * NULL on error.
 */
struct func_inst_data *func_inst_data_find(struct list_head *head,
					   struct func_inst_data *func)
{
	struct func_inst_data *f;

	list_for_each_entry(f, head, list) {
		if (func->addr == f->addr)
			return f;
	}

	return NULL;
}

/**
 * @brief func_inst_data lists splice
 *
 * @param dst Pointer to the destination list head.
 * @param src Pointer to the source list head.
 * @return u32 count of spliced elements.
 */
u32 func_inst_data_splice(struct list_head *dst,
			  struct list_head *src)
{
	struct func_inst_data *f, *n, *s;
	u32 cnt = 0;

	list_for_each_entry_safe(f, n, src, list) {
		s = func_inst_data_find(dst, f);
		if (s) {
			printk(KERN_WARNING "duplicate func probe\n");
			continue;
		}

		list_del(&f->list);
		list_add_tail(&f->list, dst);
		cnt++;
	}

	return cnt;
}

/**
 * @brief func_inst_data move from one list to another
 *
 * @param dst Pointer to the destination list head.
 * @param src Pointer to the source list head.
 * @return u32 Counter of moved elements.
 */
u32 func_inst_data_move(struct list_head *dst,
			struct list_head *src)
{
	struct func_inst_data *f, *n, *s;
	u32 cnt = 0;

	list_for_each_entry_safe(f, n, src, list) {
		s = func_inst_data_find(dst, f);
		if (s) {
			print_parse_debug("move func 0x%016llX\n", f->addr);
			list_del(&f->list);
			list_del(&s->list);
			destroy_func_inst_data(s);
			list_add_tail(&f->list, dst);
			cnt++;
		}
	}

	return cnt;
}



/* ============================================================================
 * ==                               LIB_INST                                 ==
 * ============================================================================
 */

/**
 * @brief Creates and fills lib_inst_data struct.
 *
 * @param mb Pointer to the message buffer.
 * @return Pointer to the filled lib_inst_data struct on success;\n
 * 0 on error.
 */
struct lib_inst_data *create_lib_inst_data(struct msg_buf *mb)
{
	struct lib_inst_data *li;
	struct func_inst_data *fi, *fin;
	char *path;
	u32 cnt, i = 0;

	print_parse_debug("bin path:");
	if (get_string(mb, &path)) {
		print_err("failed to read path of binary\n");
		return NULL;
	}

	print_parse_debug("func count:");
	if (get_u32(mb, &cnt)) {
		print_err("failed to read count of functions\n");
		goto free_path;
	}

	if (remained_mb(mb) / MIN_SIZE_FUNC_INST < cnt) {
		print_err("to match count of functions(%u)\n", cnt);
		goto free_path;
	}

	li = kmalloc(sizeof(*li), GFP_KERNEL);
	if (li == NULL) {
		print_err("out of memory\n");
		goto free_path;
	}
	INIT_LIST_HEAD(&li->list);
	INIT_LIST_HEAD(&li->f_head);

	if (cnt) {
		for (i = 0; i < cnt; ++i) {
			print_parse_debug("func #%d:\n", i + 1);
			fi = create_func_inst_data(mb);
			if (fi == NULL)
				goto free_func;
			list_add_tail(&fi->list, &li->f_head);
		}
	}

	li->path = path;
	li->cnt_func = cnt;

	return li;

free_func:
	list_for_each_entry_safe(fi, fin, &li->f_head, list) {
		list_del(&fi->list);
		destroy_func_inst_data(fi);
	}
	kfree(li);

free_path:
	put_string(path);

	return NULL;
}

/**
 * @brief lib_inst_data cleanup.
 *
 * @param li Pointer to the target lib_inst_data.
 * @return Void.
 */
void destroy_lib_inst_data(struct lib_inst_data *li)
{
	struct func_inst_data *fi, *fin;

	list_for_each_entry_safe(fi, fin, &li->f_head, list) {
		list_del(&fi->list);
		destroy_func_inst_data(fi);
	}

	put_string(li->path);
	kfree(li);
}

/**
 * @brief lib_inst_data find.
 *
 * @param head Pointer to the list head with lib_inst_data.
 * @param lib Pointer to the lib_inst_data looking for.
 * @return Pointer to the found lib_inst_data struct on success;\n
 * NULL on error.
 */
struct lib_inst_data *lib_inst_data_find(struct list_head *head,
					 struct lib_inst_data *lib)
{
	struct lib_inst_data *l;

	list_for_each_entry(l, head, list) {
		if (!strcmp(l->path, lib->path))
			return l;
	}

	return NULL;
}

/**
 * @brief lib_inst_data lists splice
 *
 * @param dst Pointer to the destination list head.
 * @param src Pointer to the source list head.
 * @return u32 count of spliced elements.
 */
u32 lib_inst_data_splice(struct list_head *dst, struct list_head *src)
{
	struct lib_inst_data *l, *n, *s;
	u32 cnt = 0;

	list_for_each_entry_safe(l, n, src, list) {
		s = lib_inst_data_find(dst, l);

		if (s) {
			print_parse_debug("update lib %s\n", s->path);
			s->cnt_func += func_inst_data_splice(&s->f_head,
							     &l->f_head);

		} else {
			print_parse_debug("add new lib %s\n", s->path);

			list_del(&l->list);
			list_add_tail(&l->list, dst);
			cnt++;
		}
	}

	return cnt;
}

/**
 * @brief lib_inst_data move from one list to another
 *
 * @param dst Pointer to the destination list head.
 * @param src Pointer to the source list head.
 * @return u32 Counter of moved elements.
 */
u32 lib_inst_data_move(struct list_head *dst, struct list_head *src)
{
	struct lib_inst_data *l, *n, *s;
	u32 cnt = 0;

	list_for_each_entry_safe(l, n, src, list) {
		s = lib_inst_data_find(dst, l);

		if (s) {
			print_parse_debug("update lib %s\n", s->path);
			l->cnt_func -= func_inst_data_move(&s->f_head,
							     &l->f_head);
			if (list_empty(&l->f_head)) {
				list_del(&l->list);
				destroy_lib_inst_data(l);
				cnt++;
			}
		}
	}

	return cnt;
}


/* ============================================================================
 * ==                               APP_INST                                 ==
 * ============================================================================
 */

/**
 * @brief Creates and fills app_inst_data struct.
 *
 * @param mb Pointer to the message buffer.
 * @return Pointer to the filled app_inst_data struct on success;\n
 * 0 on error.
 */
struct app_inst_data *create_app_inst_data(struct msg_buf *mb)
{
	struct app_inst_data *app_inst;
	struct func_inst_data *func, *func_n;
	struct lib_inst_data *lib, *lib_n;
	u32 cnt_func, i_func = 0, cnt_lib, i_lib = 0;

	app_inst = kmalloc(sizeof(*app_inst), GFP_KERNEL);
	if (app_inst == NULL) {
		print_err("out of memory\n");
		return NULL;
	}

	INIT_LIST_HEAD(&app_inst->list);
	INIT_LIST_HEAD(&app_inst->f_head);
	INIT_LIST_HEAD(&app_inst->l_head);

	if (create_app_info(mb, app_inst))
		goto err;

	if (get_u32(mb, &cnt_func)) {
		print_err("failed to read count of functions\n");
		goto free_app_inst;
	}
	print_parse_debug("func count:%d", cnt_func);

	if (remained_mb(mb) / MIN_SIZE_FUNC_INST < cnt_func) {
		print_err("to match count of functions(%u)\n", cnt_func);
		goto free_app_inst;
	}

	if (cnt_func) {
		for (i_func = 0; i_func < cnt_func; ++i_func) {
			print_parse_debug("func #%d:\n", i_func + 1);
			func = create_func_inst_data(mb);
			if (func == NULL)
				goto free_func;
			list_add_tail(&func->list, &app_inst->f_head);
		}
	}

	if (get_u32(mb, &cnt_lib)) {
		print_err("failed to read count of libraries\n");
		goto free_func;
	}
	print_parse_debug("lib count:i%d", cnt_lib);

	if (remained_mb(mb) / MIN_SIZE_LIB_INST < cnt_lib) {
		print_err("to match count of libraries(%u)\n", cnt_lib);
		goto free_func;
	}

	if (cnt_lib) {
		for (i_lib = 0; i_lib < cnt_lib; ++i_lib) {
			print_parse_debug("lib #%d:\n", i_lib + 1);
			lib = create_lib_inst_data(mb);
			if (lib == NULL)
				goto free_lib;

			list_add_tail(&lib->list, &app_inst->l_head);
		}
	}

	app_inst->cnt_func = cnt_func;
	app_inst->cnt_lib = cnt_lib;

	return app_inst;

free_lib:
	list_for_each_entry_safe(lib, lib_n, &app_inst->l_head, list) {
		list_del(&lib->list);
		destroy_lib_inst_data(lib);
	}

free_func:
	list_for_each_entry_safe(func, func_n, &app_inst->f_head, list) {
		list_del(&func->list);
		destroy_func_inst_data(func);
	}

free_app_inst:
	put_string(app_inst->path);
	put_string(app_inst->id);

err:
	kfree(app_inst);

	return NULL;
}

/**
 * @brief app_inst_data cleanup.
 *
 * @param ai Pointer to the target app_inst_data.
 * @return Void.
 */
void destroy_app_inst_data(struct app_inst_data *ai)
{
	struct func_inst_data *func, *func_n;
	struct lib_inst_data *lib, *lib_n;

	list_for_each_entry_safe(lib, lib_n, &ai->l_head, list) {
		list_del(&lib->list);
		destroy_lib_inst_data(lib);
	}

	list_for_each_entry_safe(func, func_n, &ai->f_head, list) {
		list_del(&func->list);
		destroy_func_inst_data(func);
	}

	put_string(ai->path);
	put_string(ai->id);

	kfree(ai);
}

/**
 * @brief find app_inst_data.
 *
 * @param head Pointer to the list head with app_inst_data.
 * @param ai Pointer to the target app_inst_data.
 * @return Pointer to the target app_inst_data.
 */
struct app_inst_data *app_inst_data_find(struct list_head *head,
					 struct app_inst_data *ai)
{
	struct app_inst_data *p;

	list_for_each_entry(p, head, list) {

		print_parse_debug("app1: %d, %d, %s, %s\n",
				p->type, p->tgid, p->id, p->path);

		print_parse_debug("app2: %d, %d, %s, %s\n",
				ai->type, ai->tgid, ai->id, ai->path);

		if ((p->type == ai->type) &&
		    (p->tgid == ai->tgid) &&
		    !strcmp(p->id, ai->id) &&
		    !strcmp(p->path, ai->path)) {
			return p;
		}
	}

	return NULL;
}

/**
 * @brief app_inst_data splice
 *
 * @param dst Pointer to the destination app_inst_data.
 * @param src Pointer to the source app_inst_data..
 * @return void.
 */
void app_inst_data_splice(struct app_inst_data *dst,
			  struct app_inst_data *src)
{
	print_parse_debug("find app, splice func and lib to %s\n",
				        dst->path);

	dst->cnt_func += func_inst_data_splice(&dst->f_head, &src->f_head);
	dst->cnt_lib += lib_inst_data_splice(&dst->l_head, &src->l_head);

	return;
}

/**
 * @brief app_inst_data move from one to another
 *
 * @param dst Pointer to the destination app_inst_data.
 * @param src Pointer to the source app_inst_data.
 * @return void.
 */
void app_inst_data_move(struct app_inst_data *dst,
			struct app_inst_data *src)
{
	print_parse_debug("find app, delete func and lib from %s\n",
			dst->path);

	dst->cnt_func -= func_inst_data_move(&dst->f_head, &src->f_head);
	dst->cnt_lib -= lib_inst_data_move(&dst->l_head, &src->l_head);

	return;
}

/* ============================================================================
 * ==                                US_INST                                 ==
 * ============================================================================
 */

/**
 * @brief Creates and fills us_inst_data struct.
 *
 * @param mb Pointer to the message buffer.
 * @param head Pointer to the list head.
 * @return u32 count of created elements.
 */
u32 create_us_inst_data(struct msg_buf *mb,
			struct list_head *head)
{
	struct app_inst_data *ai, *n;
	u32 cnt, i = 0;

	print_parse_debug("us_inst_data:\n");

	print_parse_debug("app count:");
	if (get_u32(mb, &cnt)) {
		print_err("failed to read count of applications\n");
		return 0;
	}

	if (remained_mb(mb) / MIN_SIZE_APP_INST < cnt) {
		print_err("to match count of applications(%u)\n", cnt);
		return 0;
	}

	for (i = 0; i < cnt; ++i) {
		print_parse_debug("app #%d:\n", i + 1);
		ai = create_app_inst_data(mb);
		if (ai == NULL)
			goto err;

		list_add_tail(&ai->list, head);
	}

	return cnt;

err:
	list_for_each_entry_safe(ai, n, head, list) {
		list_del(&ai->list);
		destroy_app_inst_data(ai);
	}

	return 0;
}

/**
 * @brief us_inst_data cleanup.
 *
 * @param head Pointer to the list head.
 * @return Void.
 */
void destroy_us_inst_data(struct list_head *head)
{
	struct app_inst_data *ai, *n;
	list_for_each_entry_safe(ai, n, head, list) {
		list_del(&ai->list);
		destroy_app_inst_data(ai);
	}
}
