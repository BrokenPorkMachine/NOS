CROSS_COMPILE ?= /opt/cross/bin/x86_64-elf-

all: libc kernel bootloader disk.img

libc:
	$(CROSS_COMPILE)gcc -ffreestanding -O2 -Wall -Wextra -mno-red-zone -nostdlib -c src/libc.c -o libc.o

kernel:
	make -C Kernel CROSS_COMPILE=$(CROSS_COMPILE)
	cp Kernel/kernel.bin kernel.bin

bootloader:
	make -C bootloader

disk.img: bootloader kernel
	dd if=/dev/zero of=disk.img bs=1M count=64
	mkfs.vfat -F 32 disk.img
	mmd -i disk.img ::/EFI
	mmd -i disk.img ::/EFI/BOOT
	mcopy -i disk.img bootloader/NitrOBoot.efi ::/EFI/BOOT/BOOTX64.EFI
	mcopy -i disk.img kernel.bin ::/

clean:
	make -C Kernel clean
	make -C bootloader clean
	rm -f kernel.bin libc.o disk.img

run: disk.img
	qemu-system-x86_64 \
		-bios OVMF.fd \
		-drive file=disk.img,format=raw \
		-m 512M \
		-enable-kvm

.PHONY: all libc kernel bootloader clean run
