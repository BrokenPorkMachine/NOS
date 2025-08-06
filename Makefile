CROSS_COMPILE ?= x86_64-linux-gnu-
CC := $(CROSS_COMPILE)gcc
LD := $(CROSS_COMPILE)ld
NASM := nasm
CFLAGS := -ffreestanding -O2 -Wall -Wextra -mno-red-zone -nostdlib -DKERNEL_BUILD \
	-fno-builtin -fno-stack-protector -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=0 \
	-I include -I boot/include

all: libc kernel boot disk.img

libc:
	$(CC) $(CFLAGS) -c user/libc/libc.c -o user/libc/libc.o

kernel: libc
	$(NASM) -f elf64 kernel/n2_entry.asm -o kernel/n2_entry.o
	$(CC) $(CFLAGS) -c kernel/n2_main.c -o kernel/n2_main.o
	$(CC) $(CFLAGS) -c kernel/agent.c -o kernel/agent.o
	$(CC) $(CFLAGS) -c kernel/nosm.c -o kernel/nosm.o
	$(CC) $(CFLAGS) -c kernel/drivers/IO/ps2.c -o kernel/drivers/IO/ps2.o
	$(CC) $(CFLAGS) -c kernel/drivers/IO/keyboard.c -o kernel/drivers/IO/keyboard.o
	$(CC) $(CFLAGS) -c kernel/drivers/IO/mouse.c -o kernel/drivers/IO/mouse.o
	$(CC) $(CFLAGS) -c kernel/drivers/IO/serial.c -o kernel/drivers/IO/serial.o
	$(CC) $(CFLAGS) -c kernel/drivers/IO/video.c -o kernel/drivers/IO/video.o
	$(CC) $(CFLAGS) -c kernel/drivers/IO/tty.c -o kernel/drivers/IO/tty.o
	$(CC) $(CFLAGS) -c kernel/drivers/IO/block.c -o kernel/drivers/IO/block.o
	$(CC) $(CFLAGS) -c kernel/drivers/IO/sata.c -o kernel/drivers/IO/sata.o
	$(CC) $(CFLAGS) -c kernel/drivers/IO/pic.c -o kernel/drivers/IO/pic.o
	$(CC) $(CFLAGS) -c kernel/drivers/IO/pit.c -o kernel/drivers/IO/pit.o
	$(CC) $(CFLAGS) -c kernel/drivers/IO/pci.c -o kernel/drivers/IO/pci.o
	$(CC) $(CFLAGS) -c kernel/drivers/Net/netstack.c -o kernel/drivers/Net/netstack.o
	$(CC) $(CFLAGS) -c kernel/drivers/Net/e1000.c -o kernel/drivers/Net/e1000.o
	$(LD) -T kernel/n2.ld kernel/n2_entry.o kernel/n2_main.o \
	kernel/agent.o kernel/nosm.o \
	kernel/drivers/IO/ps2.o kernel/drivers/IO/keyboard.o \
	kernel/drivers/IO/mouse.o kernel/drivers/IO/serial.o \
	kernel/drivers/IO/video.o kernel/drivers/IO/tty.o \
	kernel/drivers/IO/block.o kernel/drivers/IO/sata.o \
	kernel/drivers/IO/pci.o kernel/drivers/IO/pic.o \
	kernel/drivers/IO/pit.o \
	kernel/drivers/Net/netstack.o kernel/drivers/Net/e1000.o \
	user/libc/libc.o -o kernel.bin

boot:
	make -C boot

disk.img: boot kernel
		dd if=/dev/zero of=disk.img bs=1M count=64
		mkfs.vfat -F 32 disk.img
		mmd -i disk.img ::/EFI
		mmd -i disk.img ::/EFI/BOOT
	        mcopy -i disk.img boot/O2.efi ::/EFI/BOOT/BOOTX64.EFI
		mcopy -i disk.img kernel.bin ::/

clean:
	        rm -f kernel/n2_entry.o kernel/n2_main.o kernel/agent.o \
	            kernel/nosm.o kernel.bin user/libc/libc.o disk.img \
	            kernel/drivers/IO/ps2.o kernel/drivers/IO/keyboard.o \
	            kernel/drivers/IO/mouse.o kernel/drivers/IO/serial.o \
	            kernel/drivers/IO/video.o kernel/drivers/IO/tty.o \
	            kernel/drivers/IO/block.o kernel/drivers/IO/sata.o \
	                  kernel/drivers/IO/pci.o \
	            kernel/drivers/IO/pic.o kernel/drivers/IO/pit.o \
	            kernel/drivers/Net/netstack.o kernel/drivers/Net/e1000.o
	        make -C boot clean

run: disk.img
	qemu-system-x86_64 \
		-bios OVMF.fd \
		-drive file=disk.img,format=raw \
		-m 512M \
		-netdev user,id=n0 \
 		-device e1000,netdev=n0 \
	 	-device i8042 \
  		-serial stdio -display sdl

.PHONY: all libc kernel boot clean run
