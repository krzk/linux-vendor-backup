/**
 * parser/msg_cmd.c
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
 * Module's messages parsing implementation.
 */


#include <linux/errno.h>
#include <writer/swap_writer_module.h>
#include "msg_parser.h"
#include "msg_buf.h"
#include "features.h"
#include "parser_defs.h"
#include "us_inst.h"
#include <us_manager/us_manager.h>

static int set_config(struct conf_data *conf)
{
	int ret;

	ret = set_features(conf);

	return ret;
}

/**
 * @brief Message "keep alive" handling.
 *
 * @param mb Pointer to the message buffer.
 * @return 0 on success, negative error code on error.
 */
int msg_keep_alive(struct msg_buf *mb)
{
	if (!is_end_mb(mb)) {
		print_err("to long message, remained=%u", remained_mb(mb));
		return -EINVAL;
	}

	return 0;
}

/**
 * @brief Message "start" handling.
 *
 * @param mb Pointer to the message buffer.
 * @return 0 on success, negative error code on error.
 */
int msg_start(struct msg_buf *mb)
{
	int ret = 0;
	struct us_inst_data *us_inst;
	struct conf_data conf;

	reset_seq_num();
	reset_discarded();

	us_inst = create_us_inst_data(mb);
	if (us_inst == NULL) {
		return -EINVAL;
	}

	if (!is_end_mb(mb)) {
		print_err("to long message, remained=%u", remained_mb(mb));
		ret = -EINVAL;
		goto free_us_inst;
	}

	ret = mod_us_inst(us_inst, MT_ADD);
	if (ret) {
		printk("Cannot mod us inst, ret = %d\n", ret);
		ret = -EINVAL;
		goto free_us_inst;
	}

	ret = usm_start();
	if (ret)
		goto free_us_inst;

	restore_config(&conf);
	set_config(&conf);

free_us_inst:
	destroy_us_inst_data(us_inst);

	return ret;
}

/**
 * @brief Message "stop" handling.
 *
 * @param mb Pointer to the message buffer.
 * @return 0 on success, negative error code on error.
 */
int msg_stop(struct msg_buf *mb)
{
	int ret = 0;
	struct conf_data conf;
	unsigned int discarded;

	if (!is_end_mb(mb)) {
		print_err("to long message, remained=%u", remained_mb(mb));
		return -EINVAL;
	}

	ret = usm_stop();
	if (ret)
		return ret;

	conf.use_features0 = 0;
	conf.use_features1 = 0;
	ret = set_config(&conf);
	if (ret)
		printk("Cannot set config, ret = %d\n", ret);

	discarded = get_discarded_count();
	printk("discarded messages: %d\n", discarded);
	reset_discarded();

	return ret;
}

/**
 * @brief Message "config" handling.
 *
 * @param mb Pointer to the message buffer.
 * @return 0 on success, negative error code on error.
 */
int msg_config(struct msg_buf *mb)
{
	int ret = 0;
	struct conf_data *conf;
	enum status_type st;

	conf = create_conf_data(mb);
	if (conf == NULL)
		return -EINVAL;

	if (!is_end_mb(mb)) {
		print_err("to long message, remained=%u", remained_mb(mb));
		ret = -EINVAL;
		goto free_conf_data;
	}

	st = usm_get_status();
	if (st == ST_ON)
		set_config(conf);

	save_config(conf);
	usm_put_status(st);

free_conf_data:
	destroy_conf_data(conf);

	return ret;
}

/**
 * @brief Message "swap inst add" handling.
 *
 * @param mb Pointer to the message buffer.
 * @return 0 on success, negative error code on error.
 */
int msg_swap_inst_add(struct msg_buf *mb)
{
	int ret = 0;
	struct us_inst_data *us_inst;

	us_inst = create_us_inst_data(mb);
	if (us_inst == NULL) {
		return -EINVAL;
	}

	if (!is_end_mb(mb)) {
		print_err("to long message, remained=%u", remained_mb(mb));
		ret = -EINVAL;
		goto free_us_inst;
	}

	ret = mod_us_inst(us_inst, MT_ADD);

free_us_inst:
	destroy_us_inst_data(us_inst);

	return ret;
}

/**
 * @brief Message "swap inst remove" handling.
 *
 * @param mb Pointer to the message buffer.
 * @return 0 on success, negative error code on error.
 */
int msg_swap_inst_remove(struct msg_buf *mb)
{
	int ret = 0;
	struct us_inst_data *us_inst;

	us_inst = create_us_inst_data(mb);
	if (us_inst == NULL) {
		return -EINVAL;
	}

	if (!is_end_mb(mb)) {
		print_err("to long message, remained=%u", remained_mb(mb));
		ret = -EINVAL;
		goto free_us_inst;
	}

	ret = mod_us_inst(us_inst, MT_DEL);

free_us_inst:
	destroy_us_inst_data(us_inst);

	return ret;
}

/**
 * @brief Initializes commands handling.
 *
 * @return Initialization results.
 */
int once_cmd(void)
{
	return once_features();
}
