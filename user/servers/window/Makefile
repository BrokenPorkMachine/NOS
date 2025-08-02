CROSS_COMPILE ?= x86_64-elf-
CC      = $(CROSS_COMPILE)gcc
CFLAGS  = -ffreestanding -O2 -Wall -Wextra -nostdlib -mno-red-zone
OBJS    = window.o server.o ../../libc/libc.o ../../../kernel/IPC/ipc.o \
          ../../../kernel/drivers/IO/video.o ../../../kernel/drivers/IO/keyboard.o \
          ../../../kernel/drivers/IO/pic.o

all: window_server.bin demo1.bin demo2.bin

window.o: window.c window.h ; $(CC) $(CFLAGS) -I../../../kernel/IPC -I../../libc -I../../../kernel/Task -c window.c -o window.o

server.o: server.c server.h window.h ; $(CC) $(CFLAGS) -I../../../kernel/IPC -I../../libc -I../../../kernel/drivers/IO -c server.c -o server.o

../../../kernel/drivers/IO/video.o: ; $(CC) $(CFLAGS) -I../../../boot/include -c ../../../kernel/drivers/IO/video.c -o ../../../kernel/drivers/IO/video.o

../../../kernel/drivers/IO/keyboard.o: ; $(CC) $(CFLAGS) -c ../../../kernel/drivers/IO/keyboard.c -o ../../../kernel/drivers/IO/keyboard.o

../../../kernel/drivers/IO/pic.o: ; $(CC) $(CFLAGS) -c ../../../kernel/drivers/IO/pic.c -o ../../../kernel/drivers/IO/pic.o

../../../kernel/drivers/IO/serial.o: ; $(CC) $(CFLAGS) -c ../../../kernel/drivers/IO/serial.c -o ../../../kernel/drivers/IO/serial.o

window_server.bin: $(OBJS) ; $(CC) $(CFLAGS) $(OBJS) -o window_server.bin

# Demo clients

demo1.o: demo1.c demo1.h window.h ; $(CC) $(CFLAGS) -I../../../kernel/IPC -I../../libc -c demo1.c -o demo1.o

demo2.o: demo2.c demo2.h window.h ; $(CC) $(CFLAGS) -I../../../kernel/IPC -I../../libc -c demo2.c -o demo2.o

demo1.bin: demo1.o window.o ../../libc/libc.o ../../../kernel/IPC/ipc.o ../../../kernel/drivers/IO/serial.o ; $(CC) $(CFLAGS) demo1.o window.o ../../libc/libc.o ../../../kernel/IPC/ipc.o ../../../kernel/drivers/IO/serial.o -o demo1.bin

demo2.bin: demo2.o window.o ../../libc/libc.o ../../../kernel/IPC/ipc.o ../../../kernel/drivers/IO/serial.o ; $(CC) $(CFLAGS) demo2.o window.o ../../libc/libc.o ../../../kernel/IPC/ipc.o ../../../kernel/drivers/IO/serial.o -o demo2.bin

clean: ; rm -f *.o *.bin ../../../kernel/drivers/IO/video.o ../../../kernel/drivers/IO/keyboard.o ../../../kernel/drivers/IO/pic.o ../../../kernel/drivers/IO/serial.o
