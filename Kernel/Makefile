CROSS_COMPILE ?= x86_64-linux-gnu-
CC = $(CROSS_COMPILE)gcc
LD = $(CROSS_COMPILE)ld
NASM = nasm

CFLAGS = -ffreestanding -O2 -Wall -Wextra -mno-red-zone -nostdlib
LDFLAGS = -T kernel.ld -nostdlib

OBJS = \
    kernel_entry.o \
    kernel.o \
    ../IDT/idt.o \
    ../IDT/interrupt.o \
    ../IDT/isr_stub.o \
    ../IO/pic.o \
    ../IO/pit.o \
    ../IO/keyboard.o \
    ../IO/mouse.o \
    ../IO/pci.o \
    ../IO/serial.o \
    ../Net/e1000.o \
    ../Net/netstack.o \
    ../VM/paging.o \
    ../VM/pmm.o \
    ../VM/cow.o \
    ../VM/numa.o \
    ../Task/thread.o \
    ../Task/context_switch.o \
    ../Task/user_mode.o \
    ../GDT/gdt.o \
    ../GDT/gdt_flush.o \
    ../GDT/user.o \
    syscall.o \
    ../servers/nitrfs/nitrfs.o \
    ../servers/nitrfs/server.o \
    ../servers/shell/shell.o \
    ../servers/vnc/vnc.o \
    ../servers/ssh/ssh.o \
    ../servers/ftp/ftp.o \
    ../IPC/ipc.o \
    ../IPC/sharedmem.o \
    ../CPU/cpu.o \
    ../ACPI/acpi.o \
    ../IO/video.o \
    ../libc.o

all: kernel.bin

kernel.o: kernel.c
	$(CC) $(CFLAGS) -c $< -o $@

../%.o: ../%.c
	$(CC) $(CFLAGS) -c $< -o $@

../%.o: ../%.asm
	$(NASM) -f elf64 $< -o $@

../GDT/gdt_flush.o: ../GDT/gdt.asm
	$(NASM) -f elf64 $< -o $@

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
	rm -f *.o kernel.bin \
../IDT/*.o ../IO/*.o ../Net/*.o ../VM/*.o \
../Task/*.o ../GDT/*.o ../servers/*/*.o ../IPC/*.o ../CPU/*.o ../libc.o
