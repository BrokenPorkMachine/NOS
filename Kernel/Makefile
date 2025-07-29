CC = x86_64-elf-gcc
LD = x86_64-elf-ld

CFLAGS = -ffreestanding -O2 -Wall -Wextra -mno-red-zone -nostdlib
LDFLAGS = -T kernel.ld -nostdlib

all: kernel.bin

kernel.o: kernel.c
	$(CC) $(CFLAGS) -c $< -o $@

kernel_entry.o: kernel_entry.asm
	nasm -f elf64 kernel_entry.asm -o kernel_entry.o

kernel.bin: kernel_entry.o kernel.o kernel.ld
	$(LD) $(LDFLAGS) kernel_entry.o kernel.o -o kernel.bin

clean:
	rm -f *.o kernel.bin
