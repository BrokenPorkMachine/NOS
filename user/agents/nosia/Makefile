CROSS_COMPILE ?= x86_64-linux-gnu-
CC      = $(CROSS_COMPILE)gcc
CFLAGS  = -ffreestanding -O2 -Wall -Wextra -nostdlib -mno-red-zone

OBJS    = nosia.o ../../libc/libc.o

all: nosia.bin

nosia.o: nosia.c
	$(CC) $(CFLAGS) -I../../libc -I../../../nosm/drivers/IO -c nosia.c -o nosia.o

nosia.bin: $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) -o nosia.bin

clean:
	rm -f *.o nosia.bin
