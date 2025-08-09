CROSS_COMPILE ?= x86_64-linux-gnu-
CC := $(CROSS_COMPILE)gcc
LD := $(CROSS_COMPILE)ld
OBJCOPY := $(CROSS_COMPILE)objcopy
NASM := nasm

CFLAGS := -ffreestanding -O2 -Wall -Wextra -mno-red-zone -nostdlib -DKERNEL_BUILD \
        -fno-builtin -fno-stack-protector -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=0 \
        -I include -I boot/include -no-pie
O2_CFLAGS := $(filter-out -no-pie,$(CFLAGS)) -fpie

# ===== Standalone Agents =====
# Each subdir under user/agents/* becomes one agent binary: <name>.elf and <name>.bin
AGENT_DIRS := $(filter-out user/agents/login,$(wildcard user/agents/*))
# Optional: explicitly choose which to package (uncomment to pin)
# AGENT_DIRS := user/agents/regx user/agents/nosm user/agents/nosfs user/agents/init

# All .c files for each agent dir
AGENT_SRCS := $(foreach d,$(AGENT_DIRS),$(wildcard $(d)/*.c))
# Per-file objects for those sources
AGENT_OBJS := $(AGENT_SRCS:.c=.o)

# Agent names (basename of directory path)
AGENT_NAMES := $(notdir $(AGENT_DIRS))

# One ELF and BIN per agent name, linked from that agent's objects
AGENT_ELFS := $(foreach n,$(AGENT_NAMES),out/agents/$(n).elf)
AGENT_BINS := $(foreach n,$(AGENT_NAMES),out/agents/$(n).bin)

# Per-agent object list variable:
# e.g. for user/agents/regx/*.c -> $(call AGENT_OBJ_LIST,regx) yields obj list
define AGENT_OBJ_LIST
$(patsubst %.c,%.o,$(wildcard user/agents/$(1)/*.c))
endef

# ===== Targets =====
all: libc kernel boot disk.img

libc:
	$(CC) $(CFLAGS) -c user/libc/libc.c -o user/libc/libc.o

# Generic compile rule for agent .c files
$(AGENT_OBJS): %.o : %.c
	@mkdir -p $(dir $@)
	$(CC) $(O2_CFLAGS) -static -nostdlib -pie -c $< -o $@

# Link each agent ELF, then strip to .bin
# We reuse the same approach as O2.elf → O2.bin (no notes, flat text at start)
$(AGENT_ELFS): out/agents/%.elf : $(call AGENT_OBJ_LIST,%)
	@mkdir -p $(dir $@)
	$(CC) $(O2_CFLAGS) -static -nostdlib -pie $^ -o $@

$(AGENT_BINS): out/agents/%.bin : out/agents/%.elf
	$(OBJCOPY) -O binary \
		--remove-section=.note.gnu.build-id \
		--remove-section=.note.gnu.property \
		$< $@

# Build kernel as before (no agents linked into the kernel)
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
	$(CC) $(CFLAGS) -c user/agents/login/login.c -o user/agents/login/login.o
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
    user/agents/login/login.o user/libc/libc.o -o kernel.bin
	cp kernel.bin n2.bin
	$(CC) $(O2_CFLAGS) -static -nostdlib -pie kernel/O2.o -o O2.elf
	# Strip ELF note sections so O2.bin begins with executable code
	$(OBJCOPY) -O binary \
		--remove-section=.note.gnu.build-id \
		--remove-section=.note.gnu.property \
		O2.elf O2.bin

# Convenience: build all agents (ELF+BIN)
agents: $(AGENT_ELFS) $(AGENT_BINS)

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
	# Place standalone agent binaries in /agents on the image
	mmd   -i disk.img ::/agents || true
	$(foreach b,$(AGENT_BINS), mcopy -i disk.img $(b) ::/agents/$(notdir $(b));)

clean:
	rm -f kernel/n2_entry.o kernel/Task/context_switch.o kernel/n2_main.o kernel/builtin_nosfs.o kernel/agent.o \
	    kernel/nosm.o kernel/agent_loader.o kernel/regx.o kernel/IPC/ipc.o kernel/Task/thread.o kernel/arch/CPU/smp.o kernel/arch/CPU/lapic.o \
            kernel/macho2.o kernel/printf.o kernel.bin n2.bin O2.elf O2.bin user/libc/libc.o user/agents/login/login.o disk.img \
	    kernel/drivers/IO/ps2.o kernel/drivers/IO/keyboard.o \
	    kernel/drivers/IO/mouse.o kernel/drivers/IO/serial.o \
	    kernel/drivers/IO/video.o kernel/drivers/IO/tty.o \
            kernel/drivers/IO/block.o kernel/drivers/IO/sata.o kernel/drivers/IO/usb.o kernel/drivers/IO/usbkbd.o \
            kernel/drivers/IO/pci.o kernel/drivers/IO/pic.o kernel/drivers/IO/pit.o \
            kernel/VM/pmm_buddy.o kernel/VM/paging_adv.o kernel/VM/cow.o kernel/VM/numa.o kernel/VM/kheap.o \
            kernel/drivers/Net/netstack.o kernel/drivers/Net/e1000.o
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
