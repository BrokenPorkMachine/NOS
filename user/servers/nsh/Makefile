CROSS_COMPILE ?= x86_64-linux-gnu-
CC      = $(CROSS_COMPILE)gcc
CFLAGS  = -ffreestanding -O2 -Wall -Wextra -nostdlib -mno-red-zone
OBJS    = nsh.o ../../../kernel/IPC/ipc.o ../../libc/libc.o \
          ../../../kernel/drivers/IO/tty.o \
          ../../../kernel/drivers/IO/video.o \
          ../../../kernel/drivers/IO/serial.o \
          ../../../kernel/drivers/IO/keyboard.o \
          ../../../kernel/drivers/IO/pic.o

all: nsh.bin

nsh.o: nsh.c ../../../kernel/IPC/ipc.h ../../libc/libc.h ../../../include/nosfs.h
	$(CC) $(CFLAGS) -I../../../kernel/IPC -I../../libc -I../../../include -I../../../kernel/drivers/IO -c nsh.c -o nsh.o

nsh.bin: $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) -o nsh.bin

clean:
	rm -f *.o nsh.bin
