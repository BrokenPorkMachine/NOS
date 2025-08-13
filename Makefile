		# ========= Config =========
#
# Default to the host toolchain unless a cross compiler is explicitly
# provided.  The previous default expected the cross prefix
# `x86_64-unknown-linux-gnu-` to be installed which isn't available in
# many environments and caused the build to fail before any of the
# project sources were compiled.  Falling back to an empty prefix allows
# ordinary `gcc`, `ld`, etc. to be used out of the box while still
# supporting cross compilation when `CROSS_COMPILE` is set by the user.
CROSS_COMPILE ?=
CC      := $(CROSS_COMPILE)gcc
LD      := $(CROSS_COMPILE)ld
AS      := $(CROSS_COMPILE)as
AR      := $(CROSS_COMPILE)ar
OBJCOPY := $(CROSS_COMPILE)objcopy
NASM    := nasm

BUILD_DIR := build
OUT_DIR   := out

CFLAGS := -ffreestanding -O2 -Wall -Wextra -mno-red-zone -nostdlib -DKERNEL_BUILD \
          -fno-builtin -fno-stack-protector -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=0 \
          -I include -I boot/include -I nosm -I loader -I src/agents/regx -I user/agents/nosfs -I user/libc \
          -fPIE -fcf-protection=none -I kernel
O2_CFLAGS := $(filter-out -no-pie,$(CFLAGS)) -fPIE
AGENT_CFLAGS := $(filter-out -no-pie,$(CFLAGS)) -fPIE

# ========= Source Discovery =========
# The kernel now links against the real driver and agent implementations instead
# of stub placeholders.  Explicitly list the required sources so the linker sees
# the concrete implementations.
KERNEL_SRCS := $(filter-out kernel/O2.c,$(shell find kernel loader src/agents/regx -name '*.c')) \
               user/agents/nosfs/nosfs.c user/agents/nosfs/nosfs_server.c \
               user/agents/nosm/nosm.c \
               nosm/drivers/IO/serial.c nosm/drivers/IO/usb.c \
               nosm/drivers/IO/usbkbd.c nosm/drivers/IO/video.c \
               nosm/drivers/IO/tty.c nosm/drivers/IO/keyboard.c \
               nosm/drivers/IO/mouse.c nosm/drivers/IO/ps2.c \
               nosm/drivers/IO/block.c nosm/drivers/IO/sata.c \
               nosm/drivers/IO/pci.c nosm/drivers/IO/pic.c \
               nosm/drivers/Net/netstack.c nosm/drivers/Net/e1000.c \
               user/libc/libc.c
KERNEL_ASM_S   := $(shell find kernel -name '*.S')
KERNEL_ASM_ASM := $(shell find kernel -name '*.asm')
# Convert each source type to its object path without leaving the original
# source names in the list, ensuring the linker only sees object files.  Use a
# distinct suffix for `.asm` objects so files sharing the same stem as a C
# source (e.g. `context_switch.c` and `context_switch.asm`) do not collide.
KERNEL_OBJS := \
    $(patsubst %.c,$(BUILD_DIR)/%.o,$(KERNEL_SRCS)) \
    $(patsubst %.S,$(BUILD_DIR)/%.o,$(KERNEL_ASM_S)) \
    $(patsubst %.asm,$(BUILD_DIR)/%.asm.o,$(KERNEL_ASM_ASM))

AGENT_DIRS := user/agents/init
AGENT_NAMES := $(notdir $(AGENT_DIRS))
AGENT_SRCS := $(wildcard $(foreach d,$(AGENT_DIRS),$(d)/*.c))
AGENT_OBJS := $(patsubst %.c,$(BUILD_DIR)/%.o,$(AGENT_SRCS))

BIN_SRCS := $(wildcard bin/*.c)
BIN_NAMES := $(basename $(notdir $(BIN_SRCS)))
BIN_OBJS := $(patsubst %.c,$(BUILD_DIR)/%.o,$(BIN_SRCS))

MODULE_SRCS := $(wildcard nosm/drivers/example/hello/*.c)
MODULE_OBJS := $(patsubst %.c,$(BUILD_DIR)/%.o,$(MODULE_SRCS))

# ========= Pattern Rules =========
$(BUILD_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/%.o: %.S
	@mkdir -p $(dir $@)
	$(NASM) -f elf64 $< -o $@

$(BUILD_DIR)/%.asm.o: %.asm
	@mkdir -p $(dir $@)
	$(NASM) -f elf64 $< -o $@

# ========= Build Targets =========
all: boot kernel agents bins modules image

boot:
	make -C boot

kernel: $(KERNEL_OBJS)
	@mkdir -p $(OUT_DIR)
	$(LD) -T kernel/n2.ld -Map $(OUT_DIR)/kernel.map $(KERNEL_OBJS) -o kernel.bin
	cp kernel.bin n2.bin
	$(CC) $(O2_CFLAGS) -static -nostdlib -pie kernel/O2.c -fPIE -o O2.elf 
	$(OBJCOPY) -O binary --remove-section=.note.gnu.build-id --remove-section=.note.gnu.property O2.elf O2.bin

agents: $(AGENT_NAMES:%=$(OUT_DIR)/agents/%.mo2)

$(OUT_DIR)/agents/%.elf: $(BUILD_DIR)/user/rt/rt0_user.o $(BUILD_DIR)/user/rt/rt_stubs.o $(BUILD_DIR)/user/libc/libc.o $(BUILD_DIR)/user/agents/%/*.o
	@mkdir -p $(dir $@)
	$(CC) $(AGENT_CFLAGS) -nostdlib -Wl,-pie -Wl,-e,_start $^ -o $@

$(OUT_DIR)/agents/%.mo2: $(OUT_DIR)/agents/%.elf
	cp $< $@

bins: $(BIN_NAMES:%=$(OUT_DIR)/bin/%.bin)

$(OUT_DIR)/bin/%.elf: $(BUILD_DIR)/user/rt/rt0_user.o $(BUILD_DIR)/user/rt/rt_stubs.o $(BUILD_DIR)/user/libc/libc.o $(BUILD_DIR)/bin/%.o
	@mkdir -p $(dir $@)
	$(CC) $(O2_CFLAGS) -static -nostdlib -pie $^ -o $@

$(OUT_DIR)/bin/%.bin: $(OUT_DIR)/bin/%.elf
	$(OBJCOPY) -O binary --remove-section=.note.gnu.build-id --remove-section=.note.gnu.property $< $@

modules: $(OUT_DIR)/modules/hello.mo2

$(OUT_DIR)/modules/hello.mo2: $(MODULE_OBJS) $(BUILD_DIR)/user/libc/libc.o
	@mkdir -p $(dir $@)
	$(CC) $(O2_CFLAGS) -static -nostdlib -pie $^ -o $(OUT_DIR)/modules/hello.elf
	$(OBJCOPY) -O binary --remove-section=.note.gnu.build-id --remove-section=.note.gnu.property $(OUT_DIR)/modules/hello.elf $@

image: disk.img

disk.img: boot kernel agents bins modules
	dd if=/dev/zero of=disk.img bs=1M count=64
	mkfs.vfat -F 32 disk.img
	mmd -i disk.img ::/EFI ::/EFI/BOOT
	mcopy -i disk.img boot/nboot.efi ::/EFI/BOOT/BOOTX64.EFI
	mcopy -i disk.img O2.bin ::/
	mcopy -i disk.img n2.bin ::/
	mmd -i disk.img ::/agents
	mcopy -i disk.img $(OUT_DIR)/agents/*.mo2 ::/agents/
	mmd -i disk.img ::/bin
	mcopy -i disk.img $(OUT_DIR)/bin/*.bin ::/bin/
	mmd -i disk.img ::/modules
	mcopy -i disk.img $(OUT_DIR)/modules/*.mo2 ::/modules/

# ========= Utility =========
clean:
	rm -rf $(BUILD_DIR) $(OUT_DIR) disk.img kernel.bin n2.bin O2.elf O2.bin
	make -C boot clean

run: disk.img
	qemu-system-x86_64 -cpu max -bios OVMF.fd -drive file=disk.img,format=raw \
	    -m 512M -netdev user,id=n0 -device e1000,netdev=n0 \
	    -device i8042 -serial stdio -display sdl

runmac: disk.img
	qemu-system-x86_64 -cpu max -bios OVMF.fd -drive file=disk.img,format=raw \
	-m 512M -netdev user,id=n0 -device e1000,netdev=n0 \
	-device i8042 -serial stdio -display cocoa

.PHONY: boot
