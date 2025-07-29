CC = x86_64-linux-gnu-gcc
LD = x86_64-linux-gnu-ld
NASM = nasm

	CFLAGS = -ffreestanding -O2 -Wall -Wextra -mno-red-zone -nostdlib
LDFLAGS = -T Kernel/kernel.ld -nostdlib

OBJS = \
Kernel/kernel_entry.o \
Kernel/kernel.o \
IDT/idt.o \
IDT/interrupt.o \
IDT/isr_stub.o \
IO/pic.o \
IO/pit.o \
VM/paging.o \
Task/thread.o \
Task/context_switch.o \
Task/user_mode.o \
GDT/gdt.o \
GDT/gdt_flush.o \
GDT/user.o \
Kernel/syscall.o

all: kernel.bin bootloader

bootloader:
	$(MAKE) -C Bootloader

Kernel/kernel_entry.o: Kernel/kernel_entry.asm
	$(NASM) -f elf64 $< -o $@

Kernel/kernel.o: Kernel/kernel.c
	$(CC) $(CFLAGS) -c $< -o $@
Kernel/syscall.o: Kernel/syscall.c
	$(CC) $(CFLAGS) -c $< -o $@


IDT/idt.o: IDT/idt.c
	$(CC) $(CFLAGS) -c $< -o $@

IDT/interrupt.o: IDT/interrupt.c
	$(CC) $(CFLAGS) -c $< -o $@

IDT/isr_stub.o: IDT/isr_stub.asm
	$(NASM) -f elf64 $< -o $@

IO/pic.o: IO/pic.c
	$(CC) $(CFLAGS) -c $< -o $@

IO/pit.o: IO/pit.c
	$(CC) $(CFLAGS) -c $< -o $@

VM/paging.o: VM/paging.c
	$(CC) $(CFLAGS) -c $< -o $@

Task/thread.o: Task/thread.c
	$(CC) $(CFLAGS) -c $< -o $@

Task/context_switch.o: Task/context_switch.asm
	$(NASM) -f elf64 $< -o $@

Task/user_mode.o: Task/user_mode.asm
	$(NASM) -f elf64 $< -o $@

GDT/gdt.o: GDT/gdt.c
	$(CC) $(CFLAGS) -c $< -o $@

GDT/gdt_flush.o: GDT/gdt.asm
	$(NASM) -f elf64 $< -o $@

GDT/user.o: GDT/user.c
	$(CC) $(CFLAGS) -c $< -o $@

kernel.bin: $(OBJS)
	$(LD) $(LDFLAGS) $(OBJS) -o $@

clean:
	$(MAKE) -C Bootloader clean
	rm -f $(OBJS) kernel.bin
