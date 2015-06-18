/*
 * This is ioctl include for kmsg* devices
 */

#ifndef _KMSG_IOCTL_H_
#define _KMSG_IOCTL_H_

#include <linux/ioctl.h>
#include <linux/types.h>

struct kmsg_cmd_buffer_add {
	size_t size;
	unsigned short mode;
	int minor;
} __attribute__((packed));

#define KMSG_IOCTL_MAGIC	0xBB

/*
 * A ioctl interface for kmsg device.
 *
 * KMSG_CMD_BUFFER_ADD:	Creates additional kmsg device based on its size
 *			and mode. Minor of created device is put.
 * KMSG_CMD_BUFFER_DEL:	Removes additional kmsg device based on its minor
 */
#define KMSG_CMD_BUFFER_ADD		_IOWR(KMSG_IOCTL_MAGIC, 0x00, \
					      struct kmsg_cmd_buffer_add)
#define KMSG_CMD_BUFFER_DEL		_IOW(KMSG_IOCTL_MAGIC, 0x01, int)

#endif
