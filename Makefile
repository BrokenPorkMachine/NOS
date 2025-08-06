CROSS_COMPILE ?= x86_64-linux-gnu-
CC := $(CROSS_COMPILE)gcc
CFLAGS := -ffreestanding -O2 -Wall -Wextra -mno-red-zone -nostdlib -DKERNEL_BUILD

all: libc kernel boot disk.img

libc:
	$(CC) $(CFLAGS) -c user/libc/libc.c -o user/libc/libc.o

kernel: libc
	make -C kernel/Kernel CROSS_COMPILE=$(CROSS_COMPILE)
	cp kernel/Kernel/kernel.bin kernel.bin

boot:
	make -C boot

disk.img: boot kernel
	dd if=/dev/zero of=disk.img bs=1M count=64
	mkfs.vfat -F 32 disk.img
	mmd -i disk.img ::/EFI
	mmd -i disk.img ::/EFI/BOOT
	mcopy -i disk.img boot/NitrOBoot.efi ::/EFI/BOOT/BOOTX64.EFI
	mcopy -i disk.img kernel.bin ::/

clean:
	make -C kernel/Kernel clean
	make -C boot clean
	rm -f kernel.bin user/libc/libc.o disk.img

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
