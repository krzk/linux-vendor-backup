#include <stddef.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <fcntl.h>
#include <unistd.h>
#include <getopt.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>

#include "../kselftest.h"

#include "kmsg-test.h"

struct kmsg_test {
	const char	*name;
	const char	*desc;
	int (*func)(const struct kmsg_test_args *args);
};

static const struct kmsg_test tests[] = {
	{
		.name	= "buffer-add-del",
		.desc	= "create and delete kmsg devices",
		.func	= kmsg_test_buffer_add_del,
	}, {
		.name	= "buffer-add-write-read-del",
		.desc	= "create w/r and del kmsg device",
		.func	= kmsg_test_buffer_add_write_read_del,
	}, {
		.name	= "buffer-buf-torture",
		.desc	= "fill more than whole buffer can hold",
		.func	= kmsg_test_buffer_buf_torture,
	}, {
		.name	= "buffer-buf-multithreaded-torture",
		.desc	= "fill from many threads",
		.func	= kmsg_test_buffer_buf_multithreaded_torture,
	},
};

#define N_TESTS ARRAY_SIZE(tests)

FILE *kmsg_get_device(int minor, const char *mode)
{
	char path[80] = "";
	dev_t dev = makedev(1, minor);

	if (minor < 0) {
		printf("Invalid minor number %d\n", minor);
		return NULL;
	}

	snprintf(path, sizeof(path), "/tmp/kmsg-%d", minor);

	if (access(path, F_OK) < 0) {
		if (mknod(path, S_IFCHR | 0600, dev)) {
			printf("Cannot create device %s with minor %d\n",
								path, minor);
			return NULL;
		}
	}

	if (access(path, F_OK) < 0) {
		printf("Cannot access device %s\n", path);
		return NULL;
	}

	return fopen(path, mode);
}

int kmsg_drop_device(int minor)
{
	char path[80] = "";

	if (minor < 0) {
		printf("Invalid minor number %d\n", minor);
		return -1;
	}

	snprintf(path, sizeof(path), "/tmp/kmsg-%d", minor);

	return unlink(path);
}

static void usage(const char *argv0)
{
	unsigned int i, j;

	printf("Usage: %s [options]\n"
	       "Options:\n"
	       "\t-x, --loop		Run in a loop\n"
	       "\t-f, --fork		Fork before running a test\n"
	       "\t-h, --help		Print this help\n"
	       "\t-t, --test <test-id>	Run one specific test only\n"
	       "\t-w, --wait <secs>	Wait <secs> before actually starting test\n"
	       "\n", argv0);

	printf("By default, all test are run once, and a summary is printed.\n"
	       "Available tests for --test:\n\n");

	for (i = 0; i < N_TESTS; i++) {
		const struct kmsg_test *t = tests + i;

		printf("\t%s", t->name);

		for (j = 0; j < 60 - strlen(t->name); j++)
			printf(" ");

		printf("Test %s\n", t->desc);
	}

	printf("\n");
	printf("Note that some tests may, if run specifically by --test, ");
	printf("behave differently, and not terminate by themselves.\n");
}

static void print_test_result(int ret)
{
	switch (ret) {
	case KSFT_PASS:
		printf("OK");
		break;
	case KSFT_SKIP:
		printf("SKIPPED");
		break;
	case KSFT_FAIL:
		printf("ERROR");
		break;
	}
}

static int test_run(const struct kmsg_test *t,
		    const struct kmsg_test_args *kmsg_args,
		    int wait)
{
	int ret;

	if (wait > 0) {
		printf("Sleeping %d seconds before running test ...\n", wait);
		sleep(wait);
	}

	ret = t->func(kmsg_args);
	return ret;
}

static int test_run_forked(const struct kmsg_test *t,
			   const struct kmsg_test_args *kmsg_args,
			   int wait)
{
	int ret;
	pid_t pid;

	pid = fork();
	if (pid < 0) {
		return KSFT_FAIL;
	} else if (pid == 0) {
		ret = test_run(t, kmsg_args, wait);
		_exit(ret);
	}

	pid = waitpid(pid, &ret, 0);
	if (pid <= 0)
		return KSFT_FAIL;
	else if (!WIFEXITED(ret))
		return KSFT_FAIL;
	else
		return WEXITSTATUS(ret);
}

static int start_all_tests(const struct kmsg_test_args *kmsg_args)
{
	int retval;
	int ret = KSFT_PASS;
	unsigned int i, n;
	const struct kmsg_test *t;

	for (i = 0; i < N_TESTS; i++) {
		t = tests + i;

		printf("Testing %s (%s) ", t->desc, t->name);
		for (n = 0; n < 60 - strlen(t->desc) - strlen(t->name); n++)
			printf(".");
		printf(" ");

		retval = test_run_forked(t, kmsg_args, 0);
		switch (retval) {
		case KSFT_PASS:
			ksft_inc_pass_cnt();
			break;
		case KSFT_SKIP:
			ksft_inc_xskip_cnt();
			break;
		case KSFT_FAIL:
		default:
			ret = KSFT_FAIL;
			ksft_inc_fail_cnt();
			break;
		}

		print_test_result(retval);
		printf("\n");
	}

	return ret;
}

static int start_one_test(const struct kmsg_test_args *kmsg_args)
{
	int i, ret = KSFT_PASS;
	bool test_found = false;
	const struct kmsg_test *t;

	for (i = 0; i < N_TESTS; i++) {
		t = tests + i;

		if (strcmp(t->name, kmsg_args->test))
			continue;

		do {
			test_found = true;
			if (kmsg_args->fork)
				ret = test_run_forked(t, kmsg_args,
						      kmsg_args->wait);
			else
				ret = test_run(t, kmsg_args,
					       kmsg_args->wait);

			printf("Testing %s: ", t->desc);
			print_test_result(ret);
			printf("\n");

			if ((ret != KSFT_PASS) && (ret != KSFT_SKIP))
				break;
		} while (kmsg_args->loop);

		return ret;
	}

	if (!test_found) {
		printf("Unknown test-id '%s'\n", kmsg_args->test);
		return KSFT_FAIL;
	}

	return ret;
}

static int start_tests(const struct kmsg_test_args *kmsg_args)
{
	int retval;
	int ret = KSFT_PASS;

	if (kmsg_args->test) {
		retval = start_one_test(kmsg_args);
		switch (retval) {
		case KSFT_PASS:
			ksft_inc_pass_cnt();
			break;
		case KSFT_SKIP:
			ksft_inc_xskip_cnt();
			break;
		case KSFT_FAIL:
		default:
			ret = KSFT_FAIL;
			ksft_inc_fail_cnt();
			break;
		}
	} else  {
		do {
			ret = start_all_tests(kmsg_args);
			if ((ret != KSFT_PASS) && (ret != KSFT_SKIP))
				break;
		} while (kmsg_args->loop);
	}

	return ret;
}

int main(int argc, char *argv[])
{
	int t, ret = 0;
	struct kmsg_test_args *kmsg_args;
	char *exec = basename(argv[0]);

	kmsg_args = malloc(sizeof(*kmsg_args));
	if (!kmsg_args) {
		printf("unable to malloc() kmsg_args\n");
		return ksft_exit_fail();
	}

	memset(kmsg_args, 0, sizeof(*kmsg_args));

	static const struct option options[] = {
		{ "loop",	no_argument,		NULL, 'x' },
		{ "help",	no_argument,		NULL, 'h' },
		{ "test",	required_argument,	NULL, 't' },
		{ "wait",	required_argument,	NULL, 'w' },
		{ "fork",	no_argument,		NULL, 'f' },
		{}
	};

	if (strcmp(exec, "kmsg-test") != 0)
		kmsg_args->test = exec;

	while ((t = getopt_long(argc, argv, "hxfm:r:t:b:w:a",
						options, NULL)) >= 0) {
		switch (t) {
		case 'x':
			kmsg_args->loop = 1;
			break;

		case 't':
			kmsg_args->test = optarg;
			break;

		case 'w':
			kmsg_args->wait = strtol(optarg, NULL, 10);
			break;

		case 'f':
			kmsg_args->fork = 1;
			break;

		default:
		case 'h':
			usage(argv[0]);
			return ksft_exit_fail();
		}
	}

	ret = start_tests(kmsg_args);

	free(kmsg_args);

	ksft_print_cnts();

	if ((ret != KSFT_PASS) && (ret != KSFT_SKIP))
		return ksft_exit_fail();

	return ksft_exit_pass();
}
