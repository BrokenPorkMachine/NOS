CC      = /opt/cross/bin/x86_64-elf-gcc
CFLAGS  = -ffreestanding -O2 -Wall -Wextra -nostdlib -mno-red-zone
OBJS    = shell.o ../../IPC/ipc.o ../../src/libc.o ../nitrfs/nitrfs.o ../nitrfs/server.o

all: shell.bin

shell.o: shell.c ../../IPC/ipc.h ../nitrfs/nitrfs.h
	$(CC) $(CFLAGS) -I../../IPC -I../../src -c shell.c -o shell.o

shell.bin: $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) -o shell.bin

clean:
	rm -f *.o shell.bin
