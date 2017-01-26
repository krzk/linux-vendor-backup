/**
 * @file driver/driver_to_buffer.h
 * @author Alexander Aksenov <a.aksenov@samsung.com>
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
 * Driver and buffer interaction interface declaration.
 */

#ifndef __SWAP_DRIVER_DRIVER_TO_BUFFER__
#define __SWAP_DRIVER_DRIVER_TO_BUFFER__

int driver_to_buffer_initialize(size_t size, unsigned int count);
int driver_to_buffer_uninitialize(void);
ssize_t driver_to_buffer_read(char __user *buf, size_t count);
int driver_to_buffer_fill_spd(struct splice_pipe_desc *spd);
int driver_to_buffer_buffer_to_read(void);
int driver_to_buffer_next_buffer_to_read(void);
int driver_to_buffer_flush(void);


#endif /* __SWAP_DRIVER_DRIVER_TO_BUFFER__ */
