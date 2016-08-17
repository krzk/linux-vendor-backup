/*
 * Copyright 2014 Broadcom Corporation
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2, as
 * published by the Free Software Foundation (the "GPL").
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *  
 * A copy of the GPL is available at 
 * http://www.broadcom.com/licenses/GPLv2.php, or by writing to the Free
 * Software Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 *
 * The BBD (Broadcom Bridge Driver)
 */

#ifndef BBD_UTILS_H_ /* { */
#define BBD_UTILS_H_

#include <linux/types.h>

#include <linux/kernel.h>
#include <linux/string.h>

#define ASSERT_COMPILE(test_) \
    int __static_assert(int assert[(test_) ? 1 : -1])

#ifndef _DIM
/** Array dimension
*/
#define _DIM(x) ((unsigned int)(sizeof(x)/sizeof(*(x))))
#endif

#endif /* } BBD_UTILS_H_ */
