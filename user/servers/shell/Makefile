CROSS_COMPILE ?= /opt/cross/bin/x86_64-elf-
CC      = $(CROSS_COMPILE)gcc
CFLAGS  = -ffreestanding -O2 -Wall -Wextra -nostdlib -mno-red-zone
OBJS    = shell.o ../../../kernel/IPC/ipc.o ../../libc/libc.o ../nosfs/nosfs.o ../nosfs/nosfs_server.o

all: shell.bin

shell.o: shell.c ../../../kernel/IPC/ipc.h ../../include/nosfs.h
        $(CC) $(CFLAGS) -I../../../kernel/IPC -I../../libc -I../pkg -I../update -I../../include -c shell.c -o shell.o

shell.bin: $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) -o shell.bin

clean:
	rm -f *.o shell.bin
