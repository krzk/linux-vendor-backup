#ifndef KMSG_API_H
#define KMSG_API_H

#include <stdint.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <linux/kmsg_ioctl.h>

static inline int kmsg_cmd_buffer_add(int fd, struct kmsg_cmd_buffer_add *cmd)
{
	int ret = ioctl(fd, KMSG_CMD_BUFFER_ADD, cmd);

	return (ret < 0) ? (errno > 0 ? -errno : -EINVAL) : 0;
}

static inline int kmsg_cmd_buffer_del(int fd, int *minor)
{
	int ret = ioctl(fd, KMSG_CMD_BUFFER_DEL, minor);

	return (ret < 0) ? (errno > 0 ? -errno : -EINVAL) : 0;
}

static inline int kmsg_cmd_get_buf_size(int fd, uint32_t *size)
{
	int ret = ioctl(fd, KMSG_CMD_GET_BUF_SIZE, size);

	return (ret < 0) ? (errno > 0 ? -errno : -EINVAL) : 0;
}

static inline int kmsg_cmd_get_read_size_max(int fd, uint32_t *max_size)
{
	int ret = ioctl(fd, KMSG_CMD_GET_READ_SIZE_MAX, max_size);

	return (ret < 0) ? (errno > 0 ? -errno : -EINVAL) : 0;
}

static inline int kmsg_cmd_clear(int fd)
{
	int ret = ioctl(fd, KMSG_CMD_CLEAR);

	return (ret < 0) ? (errno > 0 ? -errno : -EINVAL) : 0;
}

#endif /* KMSG_API_H */
