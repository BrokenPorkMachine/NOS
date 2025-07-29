CC = x86_64-elf-gcc
LD = x86_64-elf-ld
NASM = nasm

CFLAGS  = -ffreestanding -O2 -Wall -Wextra -mno-red-zone -nostdlib -fstack-protector-strong -D_FORTIFY_SOURCE=2
LDFLAGS = -T kernel.ld -nostdlib

# List all your object files here
OBJS = \
    kernel_entry.o \
    kernel.o \
    idt.o \
    interrupt.o \
    paging.o \
    pic.o \
    pit.o \
    thread.o \
    context_switch.o \
    isr_stub.o

all: kernel.bin

kernel.o: kernel.c
	$(CC) $(CFLAGS) -c $< -o $@

idt.o: idt.c
	$(CC) $(CFLAGS) -c $< -o $@

interrupt.o: interrupt.c
	$(CC) $(CFLAGS) -c $< -o $@

paging.o: paging.c
	$(CC) $(CFLAGS) -c $< -o $@

pic.o: pic.c
	$(CC) $(CFLAGS) -c $< -o $@

pit.o: pit.c
	$(CC) $(CFLAGS) -c $< -o $@

thread.o: thread.c
	$(CC) $(CFLAGS) -c $< -o $@

context_switch.o: context_switch.S
	$(CC) $(CFLAGS) -c $< -o $@

kernel_entry.o: kernel_entry.asm
	$(NASM) -f elf64 $< -o $@

isr_stub.o: isr_stub.asm
	$(NASM) -f elf64 $< -o $@

kernel.bin: $(OBJS) kernel.ld
	$(LD) $(LDFLAGS) $(OBJS) -o $@

clean:
	rm -f *.o *.bin
