#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <kmsg-api.h>

#include "../kselftest.h"

#include "kmsg-test.h"

#define SOME_BUFF_SIZE 4096

int kmsg_test_buffer_buf_torture(const struct kmsg_test_args *args)
{
	int i, iter;
	int fd = open(DEV_KMSG, O_RDWR);
	struct kmsg_cmd_buffer_add cmd = { 0 };
	int minors[] = { -1, -1, -1, -1 };
	FILE *fds[ARRAY_SIZE(minors)];
	FILE *log[ARRAY_SIZE(minors)];
	int retval = KSFT_PASS;
	char buff[SOME_BUFF_SIZE];
	ssize_t s;
	int logfd;
	uint32_t size, done;
	uint32_t max_size;

	memset(buff, 'A', ARRAY_SIZE(buff));
	buff[ARRAY_SIZE(buff) - 1] = 0;

	if (fd < 0) {
		printf("Failed: cannot open %s\n", DEV_KMSG);
		return KSFT_FAIL;
	}

	for (i = 0; i < ARRAY_SIZE(minors); i++) {
		fds[i] = NULL;
		log[i] = NULL;
		cmd.size = KMSG_REQUESTED_BUF_SIZE;
		cmd.mode = 0662;
		if (kmsg_cmd_buffer_add(fd, &cmd)) {
			printf("Failed to add buffer\n");
			goto error;
		}
		if (cmd.minor < 0) {
			printf("Minor number < 0\n");
			goto error;
		}
		minors[i] = cmd.minor;

		fds[i] = kmsg_get_device(minors[i], "w");
		if (!fds[i]) {
			printf("Cannot get device %d for write\n", i);
			goto error;
		}
		size = 0;
		if (kmsg_cmd_get_buf_size(fileno(fds[i]), &size)) {
			printf("Cannot get buf size on defice %d\n", i);
			goto error;
		}
		if (size != KMSG_REQUESTED_BUF_SIZE) {
			printf("Invalid buf size on device %d\n", i);
			goto error;
		}
		log[i] = kmsg_get_device(minors[i], "r");
		if (!log[i]) {
			printf("Cannot get device %d for read\n", i);
			goto error;
		}
		size = 0;
		if (kmsg_cmd_get_buf_size(fileno(log[i]), &size)) {
			printf("Cannot get buf size on defice %d\n", i);
			goto error;
		}
		if (size != KMSG_REQUESTED_BUF_SIZE) {
			printf("Invalid buf size on device %d\n", i);
			goto error;
		}

		logfd = fileno(fds[i]);
		if (kmsg_cmd_clear(logfd)) {
			printf("Cannot clear buffer on device %d\n", i);
			goto error;
		}

		iter = 0;
		while (done < (KMSG_REQUESTED_BUF_SIZE * 2)) {
			s = write(logfd, buff, ARRAY_SIZE(buff));
			if (s < 0) {
				printf("Cannot write %d to device %d, %s\n",
						    iter, i, strerror(errno));
				goto error;
			}
			done += s;

			max_size = 0;
			if (kmsg_cmd_get_read_size_max(logfd, &max_size)) {
				printf("Cannot get max_size on device %d\n", i);
				goto error;
			}
			if (!max_size) {
				printf("Expected non-zero max_size on %d\n", i);
				goto error;
			}

			iter++;
		}
	}

	goto cleanup;

error:
	retval = KSFT_FAIL;

cleanup:
	for (i = 0; i < ARRAY_SIZE(minors); i++) {
		if (minors[i] < 0)
			continue;
		if (fds[i])
			fclose(fds[i]);
		if (log[i]) {
			if (kmsg_cmd_clear(fileno(log[i]))) {
				printf("Failed to clear device %d\n", i);
				retval = KSFT_FAIL;
			}
			fclose(log[i]);
		}
		if (kmsg_drop_device(minors[i])) {
			printf("Failed to delete device file %d\n", i);
			retval = KSFT_FAIL;
		}
		if (kmsg_cmd_buffer_del(fd, &minors[i])) {
			printf("Failed to delete buffer %d\n", i);
			retval = KSFT_FAIL;
		}
	}
	close(fd);
	return retval;
}
