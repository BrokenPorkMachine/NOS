CROSS_COMPILE ?= x86_64-elf-
CC      = $(CROSS_COMPILE)gcc
CFLAGS  = -ffreestanding -O2 -Wall -Wextra -nostdlib -mno-red-zone
OBJS    = audio.o server.o ../../../kernel/IPC/ipc.o ../../libc/libc.o ../../../nosm/drivers/Audio/audio.o

all: audio.bin

audio.o: audio.c audio.h
	$(CC) $(CFLAGS) -I../../../kernel/IPC -I../../libc -c audio.c -o audio.o

server.o: server.c server.h audio.h
        $(CC) $(CFLAGS) -I../../../kernel/IPC -I../../libc -I../../../nosm/drivers/Audio -c server.c -o server.o

audio.bin: $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) -o audio.bin

clean:
	rm -f *.o audio.bin
