CROSS_COMPILE ?= x86_64-elf-
CC      = $(CROSS_COMPILE)gcc
CFLAGS  = -ffreestanding -O2 -Wall -Wextra -nostdlib -mno-red-zone \
          -I../../../include
OBJS    = nosfs.o nosfs_server.o ../../../kernel/IPC/ipc.o ../../libc/libc.o

all: nosfs_server.bin

nosfs.o: nosfs.c ../../../include/nosfs.h
        $(CC) $(CFLAGS) -c nosfs.c -o nosfs.o

nosfs_server.o: nosfs_server.c ../../../include/nosfs.h ../../../kernel/IPC/ipc.h
        $(CC) $(CFLAGS) -I../../../kernel/IPC -I../../libc -c nosfs_server.c -o nosfs_server.o

nosfs_server.bin: $(OBJS)
        $(CC) $(CFLAGS) $(OBJS) -o nosfs_server.bin

clean:
        rm -f *.o nosfs_server.bin
