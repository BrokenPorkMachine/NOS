CROSS_COMPILE ?= x86_64-linux-gnu-
CC := $(CROSS_COMPILE)gcc
LD := $(CROSS_COMPILE)ld
OBJCOPY := $(CROSS_COMPILE)objcopy
NASM := nasm

CFLAGS := -ffreestanding -O2 -Wall -Wextra -mno-red-zone -nostdlib -DKERNEL_BUILD \
        -fno-builtin -fno-stack-protector -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=0 \
        -I include -I boot/include -no-pie
O2_CFLAGS := $(filter-out -no-pie,$(CFLAGS)) -fpie

all: libc kernel boot disk.img

# ===== Standalone Agents on Disk =====
# Build all user/agents/* EXCEPT the ones linked into the kernel.
AGENT_DIRS_ALL := $(filter-out user/agents/login,$(wildcard user/agents/*))
AGENT_DIRS_EXCL := user/agents/nosm user/agents/nosfs
AGENT_DIRS := $(filter-out $(AGENT_DIRS_EXCL),$(AGENT_DIRS_ALL))

# Keep only agent dirs that actually have at least one .c file
AGENT_DIRS_NONEMPTY := $(foreach d,$(AGENT_DIRS),$(if $(wildcard $(d)/*.c),$(d),))

# Names (dir basenames) for non-empty agents
AGENT_NAMES := $(notdir $(AGENT_DIRS_NONEMPTY))

# Convenience aggregates
AGENT_ELFS  := $(foreach n,$(AGENT_NAMES),out/agents/$(n).elf)
AGENT_BINS  := $(foreach n,$(AGENT_NAMES),out/agents/$(n).bin)

# Generic compile rule for any agent .c -> .o (objects live in source dirs)
AGENT_C_SRCS := $(foreach d,$(AGENT_DIRS_NONEMPTY),$(wildcard $(d)/*.c))
AGENT_OBJS   := $(patsubst %.c,%.o,$(AGENT_C_SRCS))

$(AGENT_OBJS): %.o : %.c
	@mkdir -p $(dir $@)
	$(CC) $(O2_CFLAGS) -static -nostdlib -pie -c $< -o $@

# Per-agent link rules with concrete deps (avoids "no input files")
define MAKE_AGENT_RULES
AGENT_$(1)_OBJS := $(patsubst %.c,%.o,$(wildcard user/agents/$(1)/*.c))

out/agents/$(1).elf: $$(AGENT_$(1)_OBJS)
	@mkdir -p $$(@D)
	$(CC) $(O2_CFLAGS) -static -nostdlib -pie $$^ -o $$@

out/agents/$(1).bin: out/agents/$(1).elf
	$(OBJCOPY) -O binary \
		--remove-section=.note.gnu.build-id \
		--remove-section=.note.gnu.property \
		$$< $$@
endef

$(foreach n,$(AGENT_NAMES),$(eval $(call MAKE_AGENT_RULES,$(n))))

# Convenience
agents: $(AGENT_ELFS) $(AGENT_BINS)

# ===== libc =====
libc:
	$(CC) $(CFLAGS) -c user/libc/libc.c -o user/libc/libc.o

# ===== kernel =====
kernel: libc agents
	$(NASM) -f elf64 kernel/n2_entry.asm -o kernel/n2_entry.o
	$(NASM) -f elf64 kernel/Task/context_switch.asm -o kernel/Task/context_switch.o
	$(CC) $(O2_CFLAGS) -c kernel/O2.c -o kernel/O2.o
	$(CC) $(CFLAGS) -c kernel/n2_main.c -o kernel/n2_main.o
	$(CC) $(CFLAGS) -c kernel/builtin_nosfs.c -o kernel/builtin_nosfs.o
	$(CC) $(CFLAGS) -c kernel/agent.c -o kernel/agent.o
	$(CC) $(CFLAGS) -c kernel/agent_loader.c -o kernel/agent_loader.o
	$(CC) $(CFLAGS) -c kernel/regx.c -o kernel/regx.o
	$(CC) $(CFLAGS) -c kernel/IPC/ipc.c -o kernel/IPC/ipc.o
	$(CC) $(CFLAGS) -c kernel/Task/thread.c -o kernel/Task/thread.o

	# Link the security gate + core service agents into the kernel:
	$(CC) $(CFLAGS) -c src/agents/regx/regx.c   -o src/agents/regx/regx.o
	$(CC) $(CFLAGS) -c user/agents/nosm/nosm.c   -o user/agents/nosm/nosm.o
	$(CC) $(CFLAGS) -c user/agents/nosfs/nosfs.c -o user/agents/nosfs/nosfs.o

	$(CC) $(CFLAGS) -c kernel/arch/CPU/smp.c -o kernel/arch/CPU/smp.o
	$(CC) $(CFLAGS) -c kernel/arch/CPU/lapic.c -o kernel/arch/CPU/lapic.o
	$(CC) $(CFLAGS) -c kernel/macho2.c -o kernel/macho2.o
	$(CC) $(CFLAGS) -c kernel/printf.c -o kernel/printf.o
	$(CC) $(CFLAGS) -c kernel/nosm.c -o kernel/nosm.o
	$(CC) $(CFLAGS) -c kernel/VM/pmm_buddy.c -o kernel/VM/pmm_buddy.o
	$(CC) $(CFLAGS) -c kernel/VM/paging_adv.c -o kernel/VM/paging_adv.o
	$(CC) $(CFLAGS) -c kernel/VM/cow.c -o kernel/VM/cow.o
	$(CC) $(CFLAGS) -c kernel/VM/numa.c -o kernel/VM/numa.o
	$(CC) $(CFLAGS) -c kernel/VM/kheap.c -o kernel/VM/kheap.o
	$(CC) $(CFLAGS) -c kernel/drivers/IO/ps2.c -o kernel/drivers/IO/ps2.o
	$(CC) $(CFLAGS) -c kernel/drivers/IO/keyboard.c -o kernel/drivers/IO/keyboard.o
	$(CC) $(CFLAGS) -c kernel/drivers/IO/mouse.c -o kernel/drivers/IO/mouse.o
	$(CC) $(CFLAGS) -c kernel/drivers/IO/serial.c -o kernel/drivers/IO/serial.o
	$(CC) $(CFLAGS) -c kernel/drivers/IO/video.c -o kernel/drivers/IO/video.o
	$(CC) $(CFLAGS) -c kernel/drivers/IO/tty.c -o kernel/drivers/IO/tty.o
	$(CC) $(CFLAGS) -c kernel/drivers/IO/block.c -o kernel/drivers/IO/block.o
	$(CC) $(CFLAGS) -c kernel/drivers/IO/sata.c -o kernel/drivers/IO/sata.o
	$(CC) $(CFLAGS) -c kernel/drivers/IO/usb.c -o kernel/drivers/IO/usb.o
	$(CC) $(CFLAGS) -c kernel/drivers/IO/usbkbd.c -o kernel/drivers/IO/usbkbd.o
	$(CC) $(CFLAGS) -c kernel/drivers/IO/pic.c -o kernel/drivers/IO/pic.o
	$(CC) $(CFLAGS) -c kernel/drivers/IO/pit.c -o kernel/drivers/IO/pit.o
	$(CC) $(CFLAGS) -c kernel/drivers/IO/pci.c -o kernel/drivers/IO/pci.o
	$(CC) $(CFLAGS) -c kernel/drivers/Net/netstack.c -o kernel/drivers/Net/netstack.o
	$(CC) $(CFLAGS) -c kernel/drivers/Net/e1000.c -o kernel/drivers/Net/e1000.o

	$(LD) -T kernel/n2.ld kernel/n2_entry.o kernel/n2_main.o kernel/builtin_nosfs.o \
	    kernel/agent.o kernel/agent_loader.o kernel/regx.o kernel/IPC/ipc.o kernel/Task/thread.o kernel/Task/context_switch.o kernel/arch/CPU/smp.o kernel/arch/CPU/lapic.o kernel/macho2.o kernel/printf.o kernel/nosm.o \
	    kernel/drivers/IO/ps2.o kernel/drivers/IO/keyboard.o \
	    kernel/drivers/IO/mouse.o kernel/drivers/IO/serial.o \
	    kernel/drivers/IO/video.o kernel/drivers/IO/tty.o \
	    kernel/drivers/IO/block.o kernel/drivers/IO/sata.o kernel/drivers/IO/usb.o kernel/drivers/IO/usbkbd.o \
	    kernel/drivers/IO/pci.o kernel/drivers/IO/pic.o \
            kernel/drivers/IO/pit.o \
            kernel/VM/pmm_buddy.o kernel/VM/paging_adv.o kernel/VM/cow.o kernel/VM/numa.o kernel/VM/kheap.o \
            kernel/drivers/Net/netstack.o kernel/drivers/Net/e1000.o \
    src/agents/regx/regx.o user/agents/nosm/nosm.o user/agents/nosfs/nosfs.o \
    user/libc/libc.o -o kernel.bin

	cp kernel.bin n2.bin

	$(CC) $(O2_CFLAGS) -static -nostdlib -pie kernel/O2.o -o O2.elf
	$(OBJCOPY) -O binary \
		--remove-section=.note.gnu.build-id \
		--remove-section=.note.gnu.property \
		O2.elf O2.bin

# ===== boot & image =====
boot:
	make -C boot

disk.img: boot kernel agents
	dd if=/dev/zero of=disk.img bs=1M count=64
	mkfs.vfat -F 32 disk.img
	mmd -i disk.img ::/EFI
	mmd -i disk.img ::/EFI/BOOT
	mcopy -i disk.img boot/nboot.efi ::/EFI/BOOT/BOOTX64.EFI
	mcopy -i disk.img O2.bin ::/
	mcopy -i disk.img n2.bin ::/
	mmd -i disk.img ::/agents || true
	$(foreach b,$(AGENT_BINS), mcopy -i disk.img $(b) ::/agents/$(notdir $(b));)

# ===== utility =====
clean:
	rm -f kernel/n2_entry.o kernel/Task/context_switch.o kernel/n2_main.o kernel/builtin_nosfs.o kernel/agent.o \
	    kernel/nosm.o kernel/agent_loader.o kernel/regx.o kernel/IPC/ipc.o kernel/Task/thread.o kernel/arch/CPU/smp.o kernel/arch/CPU/lapic.o \
            kernel/macho2.o kernel/printf.o kernel.bin n2.bin O2.elf O2.bin user/libc/libc.o disk.img \
	    kernel/drivers/IO/ps2.o kernel/drivers/IO/keyboard.o \
	    kernel/drivers/IO/mouse.o kernel/drivers/IO/serial.o \
	    kernel/drivers/IO/video.o kernel/drivers/IO/tty.o \
            kernel/drivers/IO/block.o kernel/drivers/IO/sata.o kernel/drivers/IO/usb.o kernel/drivers/IO/usbkbd.o \
            kernel/drivers/IO/pci.o kernel/drivers/IO/pic.o kernel/drivers/IO/pit.o \
            kernel/VM/pmm_buddy.o kernel/VM/paging_adv.o kernel/VM/cow.o kernel/VM/numa.o kernel/VM/kheap.o \
            kernel/drivers/Net/netstack.o kernel/drivers/Net/e1000.o \
            $(AGENT_OBJS) $(AGENT_ELFS) $(AGENT_BINS) \
            src/agents/regx/regx.o user/agents/nosm/nosm.o user/agents/nosfs/nosfs.o
	rm -rf out
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

runmac: disk.img
	qemu-system-x86_64 \
	-bios OVMF.fd \
	-drive file=disk.img,format=raw \
	-m 512M \
	-netdev user,id=n0 \
	-device e1000,netdev=n0 \
	-device i8042 \
	-serial stdio -display cocoa

.PHONY: all libc kernel boot agents clean run runmac
