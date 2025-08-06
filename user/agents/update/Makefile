CROSS_COMPILE ?= x86_64-elf-
CC      = $(CROSS_COMPILE)gcc
CFLAGS  = -ffreestanding -O2 -Wall -Wextra -nostdlib -mno-red-zone
OBJS    = server.o ../../../kernel/IPC/ipc.o ../../libc/libc.o

all: update_server.bin

server.o: server.c update.h ../pkg/pkg.h
$(CC) $(CFLAGS) -I../../../kernel/IPC -I../../libc -I../pkg -c server.c -o server.o

update_server.bin: $(OBJS)
$(CC) $(CFLAGS) $(OBJS) -o update_server.bin

clean:
rm -f *.o update_server.bin
