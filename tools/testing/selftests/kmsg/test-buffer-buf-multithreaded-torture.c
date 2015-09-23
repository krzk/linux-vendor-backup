#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <pthread.h>
#include <kmsg-api.h>

#include "../kselftest.h"

#include "kmsg-test.h"

#define SOME_BUFF_SIZE 4096
#define THREADS_PER_DEVICE 10

static bool ok = true;
static bool nok = !true;

static void *kmsg_test_thread_func(void *data)
{
	char buff[SOME_BUFF_SIZE];
	int minor = *((int *)data);
	FILE *f = kmsg_get_device(minor, "w");
	int fd;
	void *retval = &ok;
	int iter;
	ssize_t s;
	uint32_t size, done;
	uint32_t max_size;

	memset(buff, 'A', ARRAY_SIZE(buff));
	buff[ARRAY_SIZE(buff) - 1] = 0;

	if (!f) {
		printf("Cannot get device for write\n");
		return &nok;
	}
	fd = fileno(f);

	size = 0;
	if (kmsg_cmd_get_buf_size(fd, &size)) {
		printf("Cannot get buf size\n");
		goto error;
	}
	if (size != KMSG_REQUESTED_BUF_SIZE) {
		printf("Invalid buf size\n");
		goto error;
	}

	if (kmsg_cmd_clear(fd)) {
		printf("Cannot clear buffer\n");
		goto error;
	}

	iter = 0;
	while (done < (KMSG_REQUESTED_BUF_SIZE * 2)) {
		s = write(fd, buff, ARRAY_SIZE(buff));
		if (s < 0) {
			printf("Cannot write iteration %d\n", iter);
			goto error;
		}
		done += s;

		max_size = 0;
		if (kmsg_cmd_get_read_size_max(fd, &max_size)) {
			printf("Cannot get max_size\n");
			goto error;
		}
		if (!max_size) {
			printf("Expected non-zero max_size\n");
			goto error;
		}

		iter++;
	}

	goto cleanup;

error:
	retval = &nok;

cleanup:
	fclose(f);

	return retval;
}

int kmsg_test_buffer_buf_multithreaded_torture(
					const struct kmsg_test_args *args)
{
	int i, j;
	int fd = open(DEV_KMSG, O_RDWR);
	struct kmsg_cmd_buffer_add cmd = { 0 };
	int minors[] = { -1, -1, -1, -1 };
	FILE *log[ARRAY_SIZE(minors)];
	int retval = KSFT_PASS;
	pthread_t threads[ARRAY_SIZE(minors)][THREADS_PER_DEVICE];
	bool started[ARRAY_SIZE(minors)][THREADS_PER_DEVICE];
	uint32_t size;
	uint32_t max_size;
	void *retptr;

	for (i = 0; i < ARRAY_SIZE(minors); i++)
		for (j = 0; j < THREADS_PER_DEVICE; j++)
			started[i][j] = false;

	if (fd < 0) {
		printf("Failed: cannot open %s\n", DEV_KMSG);
		return KSFT_FAIL;
	}

	for (i = 0; i < ARRAY_SIZE(minors); i++) {
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

		for (j = 0; j < THREADS_PER_DEVICE; j++) {
			if (pthread_create(&threads[i][j], NULL,
					  kmsg_test_thread_func, &minors[i])) {
				printf("Cannot create thread %d for dev %d\n",
									j, i);
				goto error;
			}
			started[i][j] = true;
		}
	}

	goto cleanup;

error:
	retval = KSFT_FAIL;

cleanup:
	for (i = 0; i < ARRAY_SIZE(minors); i++) {
		for (j = 0; j < THREADS_PER_DEVICE; j++)
			if (started[i][j]) {
				if (pthread_join(threads[i][j], &retptr)) {
					printf("pthread_join() failed %d:%d\n",
									i, j);
					retval = KSFT_FAIL;
				}
				if (!(*((bool *)retptr)))
					retval = KSFT_FAIL;
			}
		if (minors[i] < 0)
			continue;
		if (log[i]) {
			max_size = 0;
			if (kmsg_cmd_get_read_size_max(fileno(log[i]),
								&max_size)) {
				printf("Cannot get max_size\n");
				retval = KSFT_FAIL;
			}
			if (!max_size) {
				printf("Expected non-zero max_size\n");
				retval = KSFT_FAIL;
			}
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
