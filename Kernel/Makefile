CROSS_COMPILE ?= /opt/cross/bin/x86_64-elf-
CC = $(CROSS_COMPILE)gcc
LD = $(CROSS_COMPILE)ld
NASM = nasm

CFLAGS = -ffreestanding -O2 -Wall -Wextra -mno-red-zone -nostdlib
LDFLAGS = -T kernel.ld -nostdlib

OBJS = kernel_entry.o kernel.o idt.o interrupt.o isr_stub.o

all: kernel.bin

kernel.o: kernel.c
	$(CC) $(CFLAGS) -c $< -o $@

idt.o: idt.c
	$(CC) $(CFLAGS) -c $< -o $@

interrupt.o: interrupt.c
	$(CC) $(CFLAGS) -c $< -o $@

kernel_entry.o: kernel_entry.asm
	$(NASM) -f elf64 $< -o $@

isr_stub.o: isr_stub.asm
	$(NASM) -f elf64 $< -o $@

kernel.bin: $(OBJS) kernel.ld
	$(LD) $(LDFLAGS) $(OBJS) -o $@

clean:
	rm -f *.o kernel.bin
