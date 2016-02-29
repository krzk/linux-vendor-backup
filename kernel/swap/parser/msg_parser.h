/**
 * @file parser/msg_parser.h
 * @author Vyacheslav Cherkashin
 * @author Vitaliy Cherepanov
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
 * Message parsing interface declaration.
 */


#ifndef _MSG_PARSER_H
#define _MSG_PARSER_H

#include <linux/types.h>

struct msg_buf;

/**
 * @enum APP_TYPE
 * Supported application types.
 */
enum APP_TYPE {
	AT_TIZEN_NATIVE_APP	= 0x01,     /**< Tizen native application. */
	AT_PID			= 0x02,         /**< App with specified PID. */
	AT_COMMON_EXEC		= 0x03,     /**< Common application. */
	AT_TIZEN_WEB_APP	= 0x04      /**< Tizen web application. */
};

/**
 * @brief App type size defenition.
 */
enum {
	SIZE_APP_TYPE = 4
};

/**
 * @struct app_info_data
 * @brief Basic application information.
 */
struct app_info_data {
	enum APP_TYPE app_type;     /**< Application type. */
	pid_t tgid;                 /**< Application PID. */
	char *app_id;               /**< Application ID */
	char *exec_path;            /**< Application execution path. */
};

/**
 * @struct conf_data
 * @brief Configuration struct.
 */
struct conf_data {
	u64 use_features0;          /**< Feature flags. */
	u64 use_features1;          /**< Feature flags. */
	u32 sys_trace_period;       /**< Trace period. */
	u32 data_msg_period;        /**< Data message period. */
};

/**
 * @struct func_inst_data
 * @brief Application and library functions to set probes.
 */
struct func_inst_data {
	u64 addr;                   /**< Function address. */
	char *args;                 /**< Function args format string. */
	char ret_type;              /**< Function return type. */
};

/**
 * @struct lib_inst_data
 * @brief Library struct.
 */
struct lib_inst_data {
	char *path;                 /**< Library path. */
	u32 cnt_func;               /**< Function probes count in this library. */
	struct func_inst_data **func;    /**< Pointer to the probes array. */
};

/**
 * @struct app_inst_data
 * @brief Application struct.
 */
struct app_inst_data {
	struct app_info_data *app_info;     /**< Pointer to app_info struct. */
	u32 cnt_func;                       /**< Function probes count in app. */
	struct func_inst_data **func;       /**< Pointer to the probes array. */
	u32 cnt_lib;                        /**< Libs count. */
	struct lib_inst_data **lib;         /**< Pointer to the libs array. */
};

/**
 * @struct us_inst_data
 * @brief User space instrumentation struct.
 */
struct us_inst_data {
	u32 cnt;                            /**< Apps count. */
	struct app_inst_data **app_inst;    /**< Pointer to the apps array. */
};


struct app_info_data *create_app_info(struct msg_buf *mb);
void destroy_app_info(struct app_info_data *app_info);

struct conf_data *create_conf_data(struct msg_buf *mb);
void destroy_conf_data(struct conf_data *conf);

void save_config(const struct conf_data *conf);
void restore_config(struct conf_data *conf);

struct func_inst_data *create_func_inst_data(struct msg_buf *mb);
void destroy_func_inst_data(struct func_inst_data *func_inst);

struct lib_inst_data *create_lib_inst_data(struct msg_buf *mb);
void destroy_lib_inst_data(struct lib_inst_data *lib_inst);

struct app_inst_data *create_app_inst_data(struct msg_buf *mb);
void destroy_app_inst_data(struct app_inst_data *app_inst);

struct us_inst_data *create_us_inst_data(struct msg_buf *mb);
void destroy_us_inst_data(struct us_inst_data *us_inst);


/* empty functions for calculating size fields in structures */
struct func_inst_data make_func_inst_data(void);
struct lib_inst_data make_lib_inst_data(void);
struct app_inst_data make_app_inst_data(void);
struct us_inst_data make_us_inst_data(void);

/**
 * @brief Constant defenitions.
 */
enum {
	MIN_SIZE_STRING = 1,
	MIN_SIZE_FUNC_INST = sizeof(make_func_inst_data().addr) +
			     MIN_SIZE_STRING,
	MIN_SIZE_LIB_INST = MIN_SIZE_STRING +
			    sizeof(make_lib_inst_data().cnt_func),
	MIN_SIZE_APP_INFO = SIZE_APP_TYPE + MIN_SIZE_STRING + MIN_SIZE_STRING,
	MIN_SIZE_APP_INST = MIN_SIZE_APP_INFO +
			    sizeof(make_app_inst_data().cnt_func) +
			    sizeof(make_app_inst_data().cnt_lib),
	MIN_SIZE_US_INST = sizeof(make_us_inst_data().cnt)
};

#endif /* _MSG_PARSER_H */
