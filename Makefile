# ========= Config =========
CROSS_COMPILE ?= x86_64-unknown-linux-gnu-
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
          -I include -I boot/include -I nosm -I loader -I src/agents/regx -I user/agents/nosfs \
          -no-pie -fcf-protection=none -I kernel
O2_CFLAGS := $(filter-out -no-pie,$(CFLAGS)) -fpie
AGENT_CFLAGS := $(filter-out -no-pie,$(CFLAGS)) -fPIE

# ========= Source Discovery =========
KERNEL_SRCS := $(wildcard kernel/**/*.c kernel/*.c loader/*.c src/agents/regx/*.c user/agents/nosfs/*.c user/agents/nosm/*.c nosm/drivers/**/*.c)
KERNEL_ASM  := $(wildcard kernel/**/*.S kernel/**/*.asm)
KERNEL_OBJS := $(patsubst %.c,$(BUILD_DIR)/%.o,$(KERNEL_SRCS)) $(patsubst %.S,$(BUILD_DIR)/%.o,$(KERNEL_ASM)) $(patsubst %.asm,$(BUILD_DIR)/%.o,$(KERNEL_ASM))

AGENT_DIRS := user/agents/init user/agents/login
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

$(BUILD_DIR)/%.o: %.asm
	@mkdir -p $(dir $@)
	$(NASM) -f elf64 $< -o $@

# ========= Build Targets =========
all: boot kernel agents bins modules image

boot:
	make -C boot

kernel: $(KERNEL_OBJS)
	$(LD) -T kernel/n2.ld -Map $(OUT_DIR)/kernel.map $(KERNEL_OBJS) -o kernel.bin
	cp kernel.bin n2.bin
	$(CC) $(O2_CFLAGS) -static -nostdlib -pie kernel/O2.c -o O2.elf
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
