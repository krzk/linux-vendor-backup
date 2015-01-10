/*
    M88DC2800/M88TC2800  - DVB-C demodulator and tuner from Montage

    Copyright (C) 2012 Max Nibble <nibble.max@gmail.com>
    Copyright (C) 2011 Montage Technology - www.montage-tech.com

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#ifndef M88DC2800_H
#define M88DC2800_H

#include <linux/dvb/frontend.h>

struct m88dc2800_config {
	u8 demod_address;
	u8 ts_mode;
};

#if defined(CONFIG_DVB_M88DC2800) || (defined(CONFIG_DVB_M88DC2800_MODULE) && defined(MODULE))
extern struct dvb_frontend* m88dc2800_attach(const struct m88dc2800_config* config,
					    struct i2c_adapter* i2c);
#else
static inline struct dvb_frontend* m88dc2800_attach(const struct m88dc2800_config* config,
					    struct i2c_adapter* i2c)
{
	printk(KERN_WARNING "%s: driver disabled by Kconfig\n", __func__);
	return NULL;
}
#endif /* CONFIG_DVB_M88DC2800 */
#endif /* M88DC2800_H */
