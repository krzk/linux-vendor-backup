/*  
 * include/linux/meizu-sys.h
 * 
 * meizu technology particular classes
 *
 */

#ifndef __H_MEIZU_PARTICULAR_CLASS_H__
#define __H_MEIZU_PARTICULAR_CLASS_H__

#include <linux/types.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/err.h>
#include <linux/sysfs.h>

extern struct class *meizu_class;

extern int meizu_sysfslink_register(struct device *dev, const char *name);
extern void meizu_sysfslink_unregister(const char *name);

#endif /*__H_MEIZU_PARTICULAR_CLASS_H__*/
