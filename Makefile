CROSS_COMPILE ?= x86_64-linux-gnu-
CC      = $(CROSS_COMPILE)gcc
NASM    = nasm

CFLAGS  = -ffreestanding -O2 -Wall -Wextra -mno-red-zone -nostdlib

all: kernel.bin bootloader

kernel.bin: libc.o
	$(MAKE) -C Kernel CROSS_COMPILE=$(CROSS_COMPILE)
	cp Kernel/kernel.bin $@

libc.o: src/libc.c src/libc.h
	$(CC) $(CFLAGS) -c $< -o $@

bootloader:
	$(MAKE) -C bootloader

.PHONY: clean

clean:
	$(MAKE) -C Kernel clean
	$(MAKE) -C bootloader clean
	rm -f libc.o kernel.bin
