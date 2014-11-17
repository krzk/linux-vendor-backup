#include <linux/ioctl.h>
#include <linux/types.h>

struct dmabuf_create {
	__u32 flags;
	__u32 size;
	__u32 fd;
};

#define DMABUF_IOCTL_BASE	'D'
#define DMABUF_IOCTL_CREATE	_IOWR(DMABUF_IOCTL_BASE, 0, struct dmabuf_create)
#define DMABUF_IOCTL_DELETE	_IOWR(DMABUF_IOCTL_BASE, 1, int)
#define DMABUF_IOCTL_EXPORT	_IOWR(DMABUF_IOCTL_BASE, 2, int)
