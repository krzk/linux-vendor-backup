CFLAGS += -I../../../../usr/include/
CFLAGS += -I../../../../samples/kmsg/
CFLAGS += -I../../../../include/uapi/
CFLAGS += -std=gnu99 -Wall
CFLAGS += -DKBUILD_MODNAME=\"kmsg\" -D_GNU_SOURCE
CFLAGS += -pthread
LDLIBS += -pthread

OBJS= \
	kmsg-test.o				\
	test-buffer-add-del.o			\
	test-buffer-add-write-read-del.o	\
	test-buffer-buf-torture.o		\
	test-buffer-buf-multithreaded-torture.o

all: kmsg-test

include ../lib.mk

%.o: %.c kmsg-test.h
	$(CC) $(CFLAGS) -c $< -o $@

kmsg-test: $(OBJS)
	$(CC) $(CFLAGS) $^ $(LDLIBS) -o $@

run_tests:
	./kmsg-test

clean:
	rm -f *.o kmsg-test
