#include <stdio.h>
#include <string.h>
#include <time.h>
#include <fcntl.h>
#include <stdlib.h>
/* Use in conjunction with test-kdbus-daemon */

#include <unistd.h>
#include <stddef.h>
#include <stdint.h>
#include <errno.h>
#include <assert.h>
#include <poll.h>
#include <sys/ioctl.h>

#include "kdbus-test.h"
#include "kdbus-util.h"
#include "kdbus-enum.h"

int get_file(const char *fname, int flags, const char *content)
{
	FILE *f;

	if (access(fname, F_OK) < 0) {
		f = fopen(fname, "w");
		if (!f)
			return -1;
		fprintf(f, "%s\n", content);
		fclose(f);
	}

	return open(fname, flags);
}

int kdbus_test_send(struct kdbus_test_env *env)
{
	int ret;
	int serial = 1;
	int fds[3];
	size_t i;

	if (!env->conn)
		return EXIT_FAILURE;

	fds[0] = get_file("/tmp/kdbus-test-send.rd", O_RDONLY, "foo");
	fds[1] = get_file("/tmp/kdbus-test-send.wr", O_WRONLY, "bar");
	fds[2] = get_file("/tmp/kdbus-test-send.rdwr", O_RDWR, "baz");

	for (i = 0; i < ELEMENTSOF(fds); i++) {
		if (fds[i] < 0) {
			fprintf(stderr, "Unable to open data/fileN file(s)\n");
			return EXIT_FAILURE;
		}
	}

	ret = kdbus_msg_send(env->conn, "com.example.kdbus-test", serial++,
			     0, 0, 0, 0, 0, NULL);
	if (ret < 0)
		fprintf(stderr, "error sending simple message: %d (%m)\n",
			ret);

	ret = kdbus_msg_send(env->conn, "com.example.kdbus-test", serial++,
			     0, 0, 0, 0, 1, fds);
	if (ret < 0)
		fprintf(stderr, "error sending message with 1 fd: %d (%m)\n",
			ret);

	ret = kdbus_msg_send(env->conn, "com.example.kdbus-test", serial++,
			     0, 0, 0, 0, 2, fds);
	if (ret < 0)
		fprintf(stderr, "error sending message with 2 fds: %d (%m)\n",
			ret);

	ret = kdbus_msg_send(env->conn, "com.example.kdbus-test", serial++,
			     0, 0, 0, 0, 3, fds);
	if (ret < 0)
		fprintf(stderr, "error sending message with 3 fds: %d (%m)\n",
			ret);

	for (i = 0; i < ELEMENTSOF(fds); i++)
		close(fds[i]);

	return EXIT_SUCCESS;
}
