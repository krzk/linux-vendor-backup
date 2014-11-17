#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/mman.h>

#include "../../../../include/uapi/linux/dma-buf-test.h"

#define DMA_BUF_DEV	"/dev/dmabuf"

/* Global variables */
int thread_count = 100;
int iterations = 10;
int dmabuf_fd;
int verify = 0, buf_sync = 0;
void *vaddr;
int buf_size = 4 * 1024;
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

struct dmabuf_sync_thread_data {
	pthread_t thread;
	int thread_no;
};

static int check_results()
{
	int i;

	char *data = vaddr;
	for (i = 0; i < buf_size; i++) {
		if (data[0] != data[i])
			return -1;
	}
	return 0;
}

void *dmabuf_sync_thread(void *data)
{
	int i, err_count = 0;
	struct dmabuf_sync_thread_data *sync_data = data;
	struct flock lock;

	srand(time(0));
	memset(&lock, 0, sizeof(struct flock));
	lock.l_whence = SEEK_CUR;

	for (i = 0; i < iterations; i++) {
		char val = rand() % 256;

		if (buf_sync) {
			if (val % 2 == 0)
				lock.l_type = F_RDLCK;
			else
				lock.l_type = F_WRLCK;
			if (fcntl(dmabuf_fd, F_SETLKW, &lock) == -1) {
				perror("Cannot set lock");
				return NULL;
			}
		}

		if (val % 2 != 0)
			memset(vaddr, val, buf_size);

		if (verify && (val % 2 != 0) && check_results())
			err_count++;

		if (buf_sync) {
			lock.l_type = F_UNLCK;
			if (fcntl(dmabuf_fd, F_SETLKW, &lock) == -1) {
				perror("Cannot set unlock");
				return NULL;
			}
		}
	}

	printf("thread:%d error_count:%d\n",  sync_data->thread_no, err_count);

	return NULL;
}

static void usage(char *name)
{
	fprintf(stderr, "usage: %s [-s]\n", name);
	fprintf(stderr, "-t N : the number of threads\n");
	fprintf(stderr, "-v : verification results\n");
	fprintf(stderr, "-b : use buf-sync\n");
	fprintf(stderr, "-s : size of buffer\n");
	exit(0);
}

extern char *optarg;
static const char optstr[] = "t:vbsh";

int main(int argc, char **argv)
{
	int fd, c, err = 0;
	int i;
	struct dmabuf_create buf;
	struct dmabuf_sync_thread_data *data;

	while ((c = getopt(argc, argv, optstr)) != -1) {
		switch (c) {
		case 't':
			if (sscanf(optarg, "%d", &thread_count) != 1)
				usage(argv[0]);
			break;
		case 'v':
			verify = 1;
			break;
		case 'b':
			buf_sync = 1;
			break;
		case 's':
			if (sscanf(optarg, "%d", &buf_size) != 1)
				usage(argv[0]);
			break;
		case 'h':
			usage(argv[0]);
			return 0;
		}
	}

	fd = open(DMA_BUF_DEV, O_RDWR);
	if (fd < 0) {
		perror("cannot open device\n");
		return -1;
	}

	buf.flags = 0;
	buf.size = buf_size;

	err = ioctl(fd, DMABUF_IOCTL_CREATE, &buf);
	if (err < 0) {
		perror("ioctl DMABUF_IOCTL_CREATE error\n");
		goto err_ioctl;
	}

	err = ioctl(fd, DMABUF_IOCTL_EXPORT, &buf);
	if (err < 0) {
		perror("ioctl DMABUF_IOCTL_EXPORT error\n");
		goto err_alloc;
	}

	dmabuf_fd = buf.fd;

	vaddr = mmap(NULL, buf.size, PROT_READ | PROT_WRITE, MAP_PRIVATE,
		     dmabuf_fd, 0);
	if (vaddr == MAP_FAILED) {
		perror("mmap error\n");
		goto err_alloc;
	}

	data = malloc(sizeof(struct dmabuf_sync_thread_data) * thread_count);
	if (!data) {
		perror("cannot allocate memory\n");
		close(fd);
		return -1;
	}

	for (i = 0; i < thread_count; i++) {
		data[i].thread_no = i;
		pthread_create(&data[i].thread, NULL, dmabuf_sync_thread,
			       &data[i]);
	}

	for (i = 0; i < thread_count; i++) {
		void *val;
		pthread_join(data[i].thread, &val);
	}

err_alloc:
	ioctl(fd, DMABUF_IOCTL_DELETE, &buf);

err_ioctl:
	close(fd);

	return err;
}
