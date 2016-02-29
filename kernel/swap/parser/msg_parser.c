/**
 * parser/msg_parser.c
 * @author Vyacheslav Cherkashin
 * @author Vitaliy Cherepanov
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
#include "msg_parser.h"
#include "msg_buf.h"
#include "parser_defs.h"


static int str_to_u32(const char* str, u32 *val)
{
	u32 result;
	if(!str || !*str)
		return -EINVAL;

	for (result = 0 ; *str; ++str) {
		if (*str < '0' || *str> '9')
			return -EINVAL;

		result = result * 10 + (*str - '0');
	}

	*val = result;

	return 0;
}





/* ============================================================================
 * ==                               APP_INFO                                 ==
 * ============================================================================
 */

/**
 * @brief Creates and fills app_info_data struct.
 *
 * @param mb Pointer to the message buffer.
 * @return Pointer to the filled app_info_data struct on success;\n
 * NULL on error.
 */
struct app_info_data *create_app_info(struct msg_buf *mb)
{
	int ret;
	struct app_info_data *ai;
	u32 app_type;
	char *ta_id, *exec_path;

	print_parse_debug("app_info:\n");

	print_parse_debug("type:");
	ret = get_u32(mb, &app_type);
	if (ret) {
		print_err("failed to read target application type\n");
		return NULL;
	}

	print_parse_debug("id:");
	ret = get_string(mb, &ta_id);
	if (ret) {
		print_err("failed to read target application ID\n");
		return NULL;
	}

	print_parse_debug("exec path:");
	ret = get_string(mb, &exec_path);
	if (ret) {
		print_err("failed to read executable path\n");
		goto free_ta_id;
	}

	ai = kmalloc(sizeof(*ai), GFP_KERNEL);
	if (ai == NULL) {
		print_err("out of memory\n");
		goto free_exec_path;
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
			ret = str_to_u32(ta_id, &tgid);
			if (ret) {
				print_err("converting string to PID, "
					  "str='%s'\n", ta_id);
				goto free_ai;
			}
		}

		ai->tgid = tgid;
		break;
	}
	default:
		print_err("wrong application type(%u)\n", app_type);
		ret = -EINVAL;
		goto free_ai;
	}

	ai->app_type = (enum APP_TYPE)app_type;
	ai->app_id = ta_id;
	ai->exec_path = exec_path;

	return ai;

free_ai:
	kfree(ai);

free_exec_path:
	put_string(exec_path);

free_ta_id:
	put_string(ta_id);

	return NULL;
}

/**
 * @brief app_info_data cleanup.
 *
 * @param ai Pointer to the target app_info_data.
 * @return Void.
 */
void destroy_app_info(struct app_info_data *ai)
{
	put_string(ai->exec_path);
	put_string(ai->app_id);
	kfree(ai);
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
 * 0 on error.
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
	u64 addr;
	char *args;
	char ret_type;

	print_parse_debug("func addr:");
	if (get_u64(mb, &addr)) {
		print_err("failed to read data function address\n");
		return NULL;
	}

	print_parse_debug("funct args:");
	if (get_string(mb, &args)) {
		print_err("failed to read data function arguments\n");
		return NULL;
	}

	print_parse_debug("funct ret type:");
	if (get_u8(mb, (u8 *)&ret_type)) {
		print_err("failed to read data function arguments\n");
		goto free_args;
	}

	fi = kmalloc(sizeof(*fi), GFP_KERNEL);
	if (fi == NULL) {
		print_err("out of memory\n");
		goto free_args;
	}

	fi->addr = addr;
	fi->args = args;
	fi->ret_type = ret_type;

	return fi;

free_args:
	put_string(args);
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
	put_string(fi->args);
	kfree(fi);
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
	struct func_inst_data *fi;
	char *path;
	u32 cnt, j, i = 0;

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
	if (li == NULL)
	if (li == NULL) {
		print_err("out of memory\n");
		goto free_path;
	}

	li->func = kmalloc(sizeof(struct func_inst_data *) * cnt, GFP_KERNEL);
	if (li->func == NULL)
	if (li->func == NULL) {
		print_err("out of memory\n");
		goto free_li;
	}

	for (i = 0; i < cnt; ++i) {
		print_parse_debug("func #%d:\n", i + 1);
		fi = create_func_inst_data(mb);
		if (fi == NULL)
			goto free_func;

		li->func[i] = fi;
	}

	li->path = path;
	li->cnt_func = cnt;

	return li;

free_func:
	for (j = 0; j < i; ++j)
		destroy_func_inst_data(li->func[j]);
	kfree(li->func);

free_li:
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
	int i;

	put_string(li->path);

	for (i = 0; i < li->cnt_func; ++i)
		destroy_func_inst_data(li->func[i]);

	kfree(li->func);
	kfree(li);
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
	struct app_info_data *app_info;
	struct func_inst_data *func;
	struct lib_inst_data *lib;
	u32 cnt_func, i_func = 0, cnt_lib, i_lib = 0, i;

	app_info = create_app_info(mb);
	if (app_info == NULL)
		return NULL;

	print_parse_debug("func count:");
	if (get_u32(mb, &cnt_func)) {
		print_err("failed to read count of functions\n");
		goto free_app_info;
	}

	if (remained_mb(mb) / MIN_SIZE_FUNC_INST < cnt_func) {
		print_err("to match count of functions(%u)\n", cnt_func);
		goto free_app_info;
	}

	app_inst = kmalloc(sizeof(*app_inst), GFP_KERNEL);
	if (app_inst == NULL) {
		print_err("out of memory\n");
		goto free_app_info;
	}

	app_inst->func = kmalloc(sizeof(struct func_inst_data *) * cnt_func,
				 GFP_KERNEL);
	if (app_inst->func == NULL) {
		print_err("out of memory\n");
		goto free_app_inst;
	}

	for (i_func = 0; i_func < cnt_func; ++i_func) {
		print_parse_debug("func #%d:\n", i_func + 1);
		func = create_func_inst_data(mb);
		if (func == NULL)
			goto free_func;

		app_inst->func[i_func] = func;
	}

	print_parse_debug("lib count:");
	if (get_u32(mb, &cnt_lib)) {
		print_err("failed to read count of libraries\n");
		goto free_func;
	}

	if (remained_mb(mb) / MIN_SIZE_LIB_INST < cnt_lib) {
		print_err("to match count of libraries(%u)\n", cnt_lib);
		goto free_func;
	}

	app_inst->lib = kmalloc(sizeof(struct lib_inst_data *) * cnt_lib,
				GFP_KERNEL);
	if (app_inst->lib == NULL) {
		print_err("out of memory\n");
		goto free_func;
	}

	for (i_lib = 0; i_lib < cnt_lib; ++i_lib) {
		print_parse_debug("lib #%d:\n", i_lib + 1);
		lib = create_lib_inst_data(mb);
		if (lib == NULL)
			goto free_lib;

		app_inst->lib[i_lib] = lib;
	}

	app_inst->app_info = app_info;
	app_inst->cnt_func = cnt_func;
	app_inst->cnt_lib = cnt_lib;

	return app_inst;

free_lib:
	for (i = 0; i < i_lib; ++i)
		destroy_lib_inst_data(app_inst->lib[i]);
	kfree(app_inst->lib);

free_func:
	for (i = 0; i < i_func; ++i)
		destroy_func_inst_data(app_inst->func[i]);
	kfree(app_inst->func);

free_app_inst:
	kfree(app_inst);

free_app_info:
	destroy_app_info(app_info);

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
	int i;

	for (i = 0; i < ai->cnt_lib; ++i)
		destroy_lib_inst_data(ai->lib[i]);
	kfree(ai->lib);

	for (i = 0; i < ai->cnt_func; ++i)
		destroy_func_inst_data(ai->func[i]);
	kfree(ai->func);

	destroy_app_info(ai->app_info);
	kfree(ai);
}





/* ============================================================================
 * ==                                US_INST                                 ==
 * ============================================================================
 */

/**
 * @brief Creates and fills us_inst_data struct.
 *
 * @param mb Pointer to the message buffer.
 * @return Pointer to the filled us_inst_data struct on success;\n
 * 0 on error.
 */
struct us_inst_data *create_us_inst_data(struct msg_buf *mb)
{
	struct us_inst_data *ui;
	struct app_inst_data *ai;
	u32 cnt, j, i = 0;

	print_parse_debug("us_inst_data:\n");

	print_parse_debug("app count:");
	if (get_u32(mb, &cnt)) {
		print_err("failed to read count of applications\n");
		return NULL;
	}

	if (remained_mb(mb) / MIN_SIZE_APP_INST < cnt) {
		print_err("to match count of applications(%u)\n", cnt);
		return NULL;
	}

	ui = kmalloc(sizeof(struct us_inst_data), GFP_KERNEL);
	if (ui == NULL) {
		print_err("out of memory\n");
		return NULL;
	}

	ui->app_inst = kmalloc(sizeof(struct app_inst_data *) * cnt,
			       GFP_KERNEL);
	if (ui->app_inst == NULL) {
		print_err("out of memory\n");
		goto free_ui;
	}

	for (i = 0; i < cnt; ++i) {
		print_parse_debug("app #%d:\n",i+1);
		ai = create_app_inst_data(mb);
		if (ai == NULL)
			goto free_app_inst;

		ui->app_inst[i] = ai;
	}

	ui->cnt = cnt;

	return ui;

free_app_inst:
	for (j = 0; j < i; ++j)
		destroy_app_inst_data(ui->app_inst[j]);
	kfree(ui->app_inst);

free_ui:
	kfree(ui);

	return NULL;
}

/**
 * @brief us_inst_data cleanup.
 *
 * @param ui Pointer to the target us_inst_data.
 * @return Void.
 */
void destroy_us_inst_data(struct us_inst_data *ui)
{
	int i;

	for (i = 0; i < ui->cnt; ++i)
		destroy_app_inst_data(ui->app_inst[i]);

	kfree(ui->app_inst);
	kfree(ui);
}
