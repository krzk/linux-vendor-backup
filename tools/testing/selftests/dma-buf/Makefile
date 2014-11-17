CFLAGS=
LDFLAGS=-lpthread -static

all:
	${CROSS_COMPILE}gcc dmabuf-sync-test.c -o dmabuf-sync-test ${CFLAGS} ${LDFLAGS}

run_tests: all
	@./dmabuf-sync-test || echo "dmabuf-sync-test: [FAIL]"

clean:
	rm -fr ./dmabuf-sync-test
