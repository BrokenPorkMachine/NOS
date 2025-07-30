#!/bin/bash

set -e

IMG=disk.img
IMG_MB=16
EFI_DIR=EFI/BOOT
BOOTLOADER=bootloader/bootx64.efi
KERNEL=kernel.bin

# 1. Clean and build everything
echo "[*] Building bootloader..."
make -C bootloader clean && make -C bootloader
echo "[*] Building kernel..."
make clean && make

# 2. Make FAT disk image
echo "[*] Creating FAT${IMG_MB}MB image and populating UEFI directory..."
rm -f $IMG
dd if=/dev/zero of=$IMG bs=1M count=$IMG_MB
mkfs.vfat $IMG

# 3. Create EFI/BOOT directory in image and copy files
MNT=$(mktemp -d)
sudo mount -o loop $IMG $MNT
sudo mkdir -p $MNT/$EFI_DIR
sudo cp $BOOTLOADER $MNT/$EFI_DIR/BOOTX64.EFI
sudo cp $KERNEL $MNT/$EFI_DIR/kernel.bin
sync
sudo umount $MNT
rmdir $MNT

echo "[*] Boot image ready: $IMG"

# 4. Optionally run in QEMU
echo
read -p "Run in QEMU now? (y/N): " runqemu
if [[ "$runqemu" =~ ^[Yy]$ ]]; then
qemu-system-x86_64 \
  -drive if=pflash,format=raw,readonly=on,file=/usr/share/OVMF/OVMF_CODE_4M.fd \
  -drive if=pflash,format=raw,file=/usr/share/OVMF/OVMF_VARS_4M.fd \
  -drive format=raw,file=disk.img
  
fi
