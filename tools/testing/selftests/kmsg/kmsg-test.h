#ifndef _KMSG_TEST_H_
#define _KMSG_TEST_H_

#include <stdio.h>

#define DEV_KMSG "/dev/kmsg"

#define ARRAY_SIZE(a) (sizeof(a) / sizeof(a[0]))

#define KMSG_REQUESTED_BUF_SIZE (1024 * 256)

struct kmsg_test_args {
	int loop;
	int wait;
	int fork;
	const char *test;
};

FILE *kmsg_get_device(int minor, const char *mode);
int kmsg_drop_device(int minor);

int kmsg_test_buffer_add_del(const struct kmsg_test_args *args);
int kmsg_test_buffer_add_write_read_del(const struct kmsg_test_args *args);
int kmsg_test_buffer_buf_torture(const struct kmsg_test_args *args);
int kmsg_test_buffer_buf_multithreaded_torture(
					const struct kmsg_test_args *args);

#endif /* _KMSG_TEST_H_ */
