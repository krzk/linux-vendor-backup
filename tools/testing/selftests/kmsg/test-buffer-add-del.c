#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <kmsg-api.h>

#include "../kselftest.h"

#include "kmsg-test.h"

int kmsg_test_buffer_add_del(const struct kmsg_test_args *args)
{
	int i;
	int fd = open(DEV_KMSG, O_RDWR);
	struct kmsg_cmd_buffer_add cmd = { 0 };
	int minors[] = { -1, -1, -1, -1 };
	FILE *fds[ARRAY_SIZE(minors)];
	int retval = KSFT_PASS;
	uint32_t size;

	if (fd < 0) {
		printf("Failed: cannot open %s\n", DEV_KMSG);
		return KSFT_FAIL;
	}

	for (i = 0; i < ARRAY_SIZE(minors); i++) {
		fds[i] = NULL;
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
		fds[i] = kmsg_get_device(minors[i], "r");
		if (!fds[i]) {
			printf("Cannot get device %d\n", i);
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
