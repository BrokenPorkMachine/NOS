CROSS_COMPILE ?= x86_64-elf-
CC      = $(CROSS_COMPILE)gcc
CFLAGS  = -ffreestanding -O2 -Wall -Wextra -nostdlib -mno-red-zone
OBJS    = pkg.o server.o ../../../kernel/IPC/ipc.o ../../libc/libc.o

all: pkg_server.bin

pkg.o: pkg.c pkg.h
$(CC) $(CFLAGS) -I../../libc -c pkg.c -o pkg.o

server.o: server.c pkg.h server.h ../../../kernel/IPC/ipc.h
$(CC) $(CFLAGS) -I../../../kernel/IPC -I../../libc -c server.c -o server.o

pkg_server.bin: $(OBJS)
$(CC) $(CFLAGS) $(OBJS) -o pkg_server.bin

clean:
rm -f *.o pkg_server.bin
