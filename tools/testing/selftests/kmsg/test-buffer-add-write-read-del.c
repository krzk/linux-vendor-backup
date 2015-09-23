#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <kmsg-api.h>

#include "../kselftest.h"

#include "kmsg-test.h"

static const char *message(char *buff, size_t size, int i, int j)
{
	snprintf(buff, size, "Test message (%d, %d)", i, j);
	return buff;
}

int kmsg_test_buffer_add_write_read_del(const struct kmsg_test_args *args)
{
	int i, j;
	int fd = open(DEV_KMSG, O_RDWR);
	struct kmsg_cmd_buffer_add cmd = { 0 };
	int minors[] = { -1, -1, -1, -1 };
	FILE *fds[ARRAY_SIZE(minors)];
	FILE *log[ARRAY_SIZE(minors)];
	int logfd;
	int retval = KSFT_PASS;
	uint32_t size;
	char txt[80] = "";
	char *buff = NULL;
	const char *msg;
	char *msgend;

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

		for (j = 0; j <= i; j++) {
			if (kmsg_cmd_clear(fileno(fds[j]))) {
				printf("Cannot clear buffer on device %d\n", j);
				goto error;
			}
			fprintf(fds[j], "%s\n", message(txt, ARRAY_SIZE(txt),
									i, j));
			fflush(fds[j]);
		}

		for (j = 0; j <= i; j++) {
			logfd = fileno(log[j]);
			size = 0;
			if (kmsg_cmd_get_read_size_max(logfd, &size)) {
				printf("Cannot get buf size on device %d\n", j);
				goto error;
			}
			if (!size) {
				printf("Expected non-zero buf size on %d\n", j);
				goto error;
			}
			buff = malloc(size);
			if (!buff) {
				printf("Out of memory\n");
				goto error;
			}
			if (read(logfd, buff, size) <= 0) {
				printf("Could not read from buffer %d\n", j);
				goto error;
			}
			msg = strchr(buff, ';');
			msgend = strchr(buff, '\n');
			if ((!msg) || (!msgend)) {
				printf("Could not read stored log on %d\n", j);
				goto error;
			}
			msg++;
			*msgend = 0;
			if (strcmp(msg, message(txt, ARRAY_SIZE(txt), i, j))) {
				printf("Messages do not match on %d\n", j);
				goto error;
			}
			free(buff);
			buff = NULL;
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
	if (buff)
		free(buff);
	return retval;
}
