#ifndef _COMPAT_H
#define _COMPAT_H
/**
 * @file energy/compat.h
 * @author Vitaliy Cherepanov <v.cherepanov@samsung.com>
 *
 * @section LICENCE
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
 * Copyright (C) Samsung Electronics, 2013
 *
 * @section DESCRIPTION
 *
 */

unsigned long get_nr_compat_read(void);
unsigned long get_nr_compat_write(void);

#endif /* _COMPAT_H */
