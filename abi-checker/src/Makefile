PROGRAM = abi-checker
GCC = gcc
LDD = gcc

CFLAGS = $(shell pkg-config --cflags glib-2.0 libelf) -I.

LDFLAGS = $(shell pkg-config --libs glib-2.0 libelf)

SRC = kernel_abi_checker.c kernel_abi_checker_elf.c

HEADER = kernel_abi_checker.h

DEST_DIR = /usr/local/bin

OBJECTS = kernel_abi_checker.o kernel_abi_checker_elf.o

all : $(PROGRAM)

.c.o : $(HEADER)
	$(GCC) -c $(CFLAGS) $< -o $@

$(PROGRAM) : $(OBJECTS)
	$(LDD) $(OBJECTS) -o $@ $(LDFLAGS)

install :
	mkdir -p $(DEST_DIR)
	cp $(PROGRAM) $(DEST_DIR)

clean :
	rm -f $(PROGRAM)
	rm -f $(OBJECTS)
