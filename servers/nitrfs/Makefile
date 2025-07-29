CROSS_COMPILE ?= x86_64-elf-
CC      = $(CROSS_COMPILE)gcc
CFLAGS  = -ffreestanding -O2 -Wall -Wextra -nostdlib -mno-red-zone
OBJS    = nitrfs.o server.o ../../IPC/ipc.o ../../src/libc.o

all: nitrfs_server.bin

nitrfs.o: nitrfs.c nitrfs.h
	$(CC) $(CFLAGS) -c nitrfs.c -o nitrfs.o

server.o: server.c nitrfs.h ../../IPC/ipc.h
	$(CC) $(CFLAGS) -I../../IPC -I../../src -c server.c -o server.o

nitrfs_server.bin: $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) -o nitrfs_server.bin

clean:
		rm -f *.o nitrfs_server.bin
