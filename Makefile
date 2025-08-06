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
	$(LD) -T kernel/n2.ld kernel/n2_entry.o kernel/n2_main.o \
	    kernel/agent.o kernel/nosm.o user/libc/libc.o -o kernel.bin

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
	    kernel/nosm.o kernel.bin user/libc/libc.o disk.img
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
