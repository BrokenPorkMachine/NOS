CROSS_COMPILE ?= /opt/cross/bin/x86_64-elf-
CC      = $(CROSS_COMPILE)gcc
CFLAGS  = -ffreestanding -O2 -Wall -Wextra -nostdlib -mno-red-zone
OBJS    = nsh.o ../../../kernel/IPC/ipc.o ../../libc/libc.o ../nosfs/nosfs.o ../nosfs/nosfs_server.o

all: nsh.bin

nsh.o: nsh.c ../../../kernel/IPC/ipc.h ../../include/nosfs.h
        $(CC) $(CFLAGS) -I../../../kernel/IPC -I../../libc -I../pkg -I../update -I../../include -c nsh.c -o nsh.o

nsh.bin: $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) -o nsh.bin

clean:
	rm -f *.o nsh.bin
