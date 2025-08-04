CROSS_COMPILE ?= x86_64-linux-gnu-
CC = $(CROSS_COMPILE)gcc
LD = $(CROSS_COMPILE)ld
NASM = nasm

CFLAGS = -ffreestanding -O2 -Wall -Wextra -mno-red-zone -nostdlib
LDFLAGS = -T kernel.ld -nostdlib

OBJS = \
    kernel_entry.o \
    kernel.o \
    ../arch/IDT/idt.o \
    ../arch/IDT/interrupt.o \
    ../arch/IDT/isr_stub.o \
    ../drivers/IO/pic.o \
    ../drivers/IO/pit.o \
    ../drivers/IO/keyboard.o \
    ../drivers/IO/mouse.o \
    ../drivers/IO/pci.o \
    ../drivers/IO/usb.o \
    ../drivers/IO/serial.o \
    ../drivers/IO/block.o \
    ../drivers/Net/e1000.o \
    ../drivers/Net/netstack.o \
    ../VM/paging.o \
    ../VM/pmm.o \
    ../VM/cow.o \
    ../VM/numa.o \
    ../Task/thread.o \
    ../Task/context_switch.o \
    ../Task/user_mode.o \
    ../arch/GDT/gdt.o \
    ../arch/GDT/gdt_flush.o \
    ../arch/GDT/user.o \
    elf.o \
    syscall.o \
    ../../user/servers/nitrfs/nitrfs.o \
    ../../user/servers/nitrfs/server.o \
    ../../user/servers/init/init.o \
    ../../user/servers/shell/shell.o \
    ../../user/servers/vnc/vnc.o \
    ../../user/servers/ssh/ssh.o \
    ../../user/servers/ftp/ftp.o \
    ../../user/servers/login/login.o \
    ../../user/servers/pkg/pkg.o \
    ../../user/servers/pkg/server.o \
    ../../user/servers/audio/audio.o \
    ../../user/servers/audio/server.o \
    ../../user/servers/update/server.o \
    ../../user/servers/cp/cp.o \
    ../../user/servers/mv/mv.o \
    ../../user/servers/grep/grep.o \
    ../IPC/ipc.o \
    ../IPC/sharedmem.o \
    ../arch/CPU/cpu.o \
    ../arch/CPU/lapic.o \
    ../arch/CPU/smp.o \
    ../arch/ACPI/acpi.o \
    ../drivers/IO/video.o \
    ../drivers/Audio/audio.o \
    ../../user/libc/libc.o

all: kernel.bin

kernel.o: kernel.c
	$(CC) $(CFLAGS) -c $< -o $@

../%.o: ../%.c
	$(CC) $(CFLAGS) -c $< -o $@

../%.o: ../%.asm
	$(NASM) -f elf64 $< -o $@

../../%.o: ../../%.c
	$(CC) $(CFLAGS) -c $< -o $@

../../%.o: ../../%.asm
	$(NASM) -f elf64 $< -o $@

kernel_entry.o: kernel_entry.asm
	$(NASM) -f elf64 $< -o $@

../arch/GDT/gdt_flush.o: ../arch/GDT/gdt.asm
	$(NASM) -f elf64 $< -o $@

kernel.bin: $(OBJS) kernel.ld
	$(LD) $(LDFLAGS) $(OBJS) -o $@

clean:
	rm -f *.o kernel.bin \
../arch/IDT/*.o ../drivers/IO/*.o ../drivers/Net/*.o ../VM/*.o \
../Task/*.o ../arch/GDT/*.o ../../user/servers/*/*.o ../IPC/*.o ../arch/CPU/*.o ../../user/libc/libc.o
