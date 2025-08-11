CROSS_COMPILE ?= x86_64-linux-gnu-
CC      := $(CROSS_COMPILE)gcc
LD      := $(CROSS_COMPILE)ld
AS      := $(CROSS_COMPILE)as
AR      := $(CROSS_COMPILE)ar
OBJCOPY := $(CROSS_COMPILE)objcopy
NASM    := nasm

CFLAGS := -ffreestanding -O2 -Wall -Wextra -mno-red-zone -nostdlib -DKERNEL_BUILD \
	  -fno-builtin -fno-stack-protector -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=0 \
	  -I include -I boot/include -I nosm -no-pie
O2_CFLAGS := $(filter-out -no-pie,$(CFLAGS)) -fpie

all: libc kernel boot disk.img

# ===== Standalone Agents on Disk =====
# Build the core user agents that ship on the disk image. Additional
# agents can be added here as they are migrated to the new architecture.
AGENT_DIRS := user/agents/init user/agents/login
AGENT_DIRS_NONEMPTY := $(AGENT_DIRS)
AGENT_NAMES := $(notdir $(AGENT_DIRS))
AGENT_ELFS  := $(foreach n,$(AGENT_NAMES),out/agents/$(n).elf)
AGENT_BINS  := $(foreach n,$(AGENT_NAMES),out/agents/$(n).bin)
AGENT_BINS  := $(filter-out out/agents/init.bin,$(AGENT_BINS)) out/agents/init.mo2
AGENT_C_SRCS := $(foreach d,$(AGENT_DIRS_NONEMPTY),$(wildcard $(d)/*.c))
AGENT_OBJS   := $(patsubst %.c,%.o,$(AGENT_C_SRCS))

$(AGENT_OBJS): %.o : %.c
	@mkdir -p $(dir $@)
	$(CC) $(O2_CFLAGS) -static -nostdlib -pie -c $< -o $@

define MAKE_AGENT_RULES
AGENT_$(1)_OBJS := $(patsubst %.c,%.o,$(wildcard user/agents/$(1)/*.c))

out/agents/$(1).elf: $$(AGENT_$(1)_OBJS) user/rt/rt0_agent.o user/libc/libc.o
	@mkdir -p $$(@D)
	$(CC) $(O2_CFLAGS) -static -nostdlib -pie $$^ -o $$@

out/agents/$(1).bin: out/agents/$(1).elf
	cp $$< $$@
endef
$(foreach n,$(AGENT_NAMES),$(eval $(call MAKE_AGENT_RULES,$(n))))

out/agents/init.mo2: out/agents/init.elf
	cp $< $@

agents: $(AGENT_ELFS) $(AGENT_BINS)

# ===== NOSM modules =====
nosm/drivers/example/hello/hello_nmod.o: nosm/drivers/example/hello/hello_nmod.c
	$(CC) $(O2_CFLAGS) -static -nostdlib -pie -c $< -o $@

out/modules/hello.mo2: nosm/drivers/example/hello/hello_nmod.o user/libc/libc.o
	@mkdir -p $(@D)
	$(CC) $(O2_CFLAGS) -static -nostdlib -pie $^ -o $(@:.mo2=.elf)
	$(OBJCOPY) -O binary --remove-section=.note.gnu.build-id --remove-section=.note.gnu.property $(@:.mo2=.elf) $@

modules: out/modules/hello.mo2

# ===== /bin user programs (single C file each under ./bin) =====
# Provide a tiny crt0 so programs have a proper _start.
user/rt/rt0_user.o: user/rt/rt0_user.S
	@mkdir -p $(dir $@)
	$(NASM) -f elf64 $< -o $@

user/rt/rt0_agent.o: user/rt/rt0_agent.c
	@mkdir -p $(dir $@)
	$(CC) $(O2_CFLAGS) -static -nostdlib -pie -c $< -o $@

BIN_SRCS  := $(wildcard bin/*.c)      # e.g., bin/cp.c, bin/grep.c, bin/mv.c
BIN_NAMES := $(basename $(notdir $(BIN_SRCS)))
BIN_ELFS  := $(foreach n,$(BIN_NAMES),out/bin/$(n).elf)
BIN_BINS  := $(foreach n,$(BIN_NAMES),out/bin/$(n).bin)
BIN_OBJS  := $(patsubst %.c,%.o,$(BIN_SRCS))

$(BIN_OBJS): %.o : %.c
	@mkdir -p $(dir $@)
	$(CC) $(O2_CFLAGS) -static -nostdlib -pie -c $< -o $@

# Link each tool with crt0 + libc
define MAKE_BIN_RULES
out/bin/$(1).elf: user/rt/rt0_user.o bin/$(1).o user/libc/libc.o
	@mkdir -p $$(@D)
	$(CC) $(O2_CFLAGS) -static -nostdlib -pie $$^ -o $$@

out/bin/$(1).bin: out/bin/$(1).elf
	$(OBJCOPY) -O binary \
		--remove-section=.note.gnu.build-id \
		--remove-section=.note.gnu.property \
		$$< $$@
endef
$(foreach n,$(BIN_NAMES),$(eval $(call MAKE_BIN_RULES,$(n))))

bins: user/rt/rt0_user.o $(BIN_ELFS) $(BIN_BINS)

# ===== libc =====
libc:
	$(CC) $(CFLAGS) -c user/libc/libc.c -o user/libc/libc.o

# ===== kernel =====
kernel: libc agents bins
	$(NASM) -f elf64 kernel/n2_entry.asm -o kernel/n2_entry.o
	$(NASM) -f elf64 kernel/Task/context_switch.asm -o kernel/Task/context_switch.o
	$(CC) $(O2_CFLAGS) -c kernel/O2.c -o kernel/O2.o
	$(CC) $(CFLAGS) -c kernel/n2_main.c -o kernel/n2_main.o
	$(CC) $(CFLAGS) -c kernel/builtin_nosfs.c -o kernel/builtin_nosfs.o
	$(CC) $(CFLAGS) -c kernel/agent.c -o kernel/agent.o
	$(CC) $(CFLAGS) -c kernel/agent_loader.c -o kernel/agent_loader.o
	$(CC) $(CFLAGS) -c kernel/regx.c -o kernel/regx.o
	$(CC) $(CFLAGS) -c kernel/trap.c -o kernel/trap.o
	$(CC) $(CFLAGS) -c kernel/symbols.c -o kernel/symbols.o
	$(CC) $(CFLAGS) -c kernel/uaccess.c -o kernel/uaccess.o
	$(CC) $(CFLAGS) -c kernel/proc_launch.c -o kernel/proc_launch.o
	$(CC) $(CFLAGS) -c kernel/IPC/ipc.c -o kernel/IPC/ipc.o
	$(CC) $(CFLAGS) -c kernel/Task/thread.c -o kernel/Task/thread.o
	xxd -i out/agents/init.mo2 | \
	sed 's/unsigned char/static unsigned char/; s/unsigned int/static unsigned int/; s/out_agents_init_mo2/init_bin/g; s/out_agents_init_mo2_len/init_bin_len/' > kernel/init_bin.h
	xxd -i out/agents/login.bin | \
///	sed 's/unsigned char/static unsigned char/; s/unsigned int/static unsigned int/; s/out_agents_login_bin/login_bin/g; s/out_agents_login_bin_len/login_bin_len/' > kernel/login_bin.h
//	sed 's/out_agents_init_mo2/init_bin/g; s/out_agents_init_mo2_len/init_bin_len/' > kernel/init_bin.h
//	sed 's/out_agents_login_bin/login_bin/g; s/out_agents_login_bin_len/login_bin_len/' > kernel/login_bin.h
	$(CC) $(CFLAGS) -c kernel/stubs.c -o kernel/stubs.o
	$(CC) $(CFLAGS) -c nosm/drivers/IO/serial.c -o nosm/drivers/IO/serial.o
n56# Linked-in security gate + core agents:
	$(CC) $(CFLAGS) -c src/agents/regx/regx.c   -o src/agents/regx/regx.o
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

	$(LD) -T kernel/n2.ld kernel/n2_entry.o kernel/n2_main.o kernel/builtin_nosfs.o \
	    kernel/agent.o kernel/agent_loader.o kernel/regx.o kernel/IPC/ipc.o kernel/Task/thread.o kernel/Task/context_switch.o kernel/arch/CPU/smp.o kernel/arch/CPU/lapic.o kernel/macho2.o kernel/printf.o kernel/nosm.o \
	kernel/VM/pmm_buddy.o kernel/VM/paging_adv.o kernel/VM/cow.o kernel/VM/numa.o kernel/VM/kheap.o kernel/uaccess.o kernel/proc_launch.o kernel/trap.o kernel/symbols.o nosm/drivers/IO/serial.o kernel/stubs.o \
	src/agents/regx/regx.o user/agents/nosfs/nosfs.o \
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

disk.img: boot kernel agents bins modules
	dd if=/dev/zero of=disk.img bs=1M count=64
	mkfs.vfat -F 32 disk.img
	mmd -i disk.img ::/EFI
	mmd -i disk.img ::/EFI/BOOT
	mcopy -i disk.img boot/nboot.efi ::/EFI/BOOT/BOOTX64.EFI
	mcopy -i disk.img O2.bin ::/
	mcopy -i disk.img n2.bin ::/
	mmd -i disk.img ::/agents || true
	$(foreach b,$(AGENT_BINS), mcopy -i disk.img $(b) ::/agents/$(notdir $(b));)
	mmd -i disk.img ::/bin || true
	$(foreach b,$(BIN_BINS), mcopy -i disk.img $(b) ::/bin/$(notdir $(b));)
	mmd -i disk.img ::/modules || true
	mcopy -i disk.img out/modules/hello.mo2 ::/modules/hello.mo2

# ===== utility =====
clean:
	rm -f kernel/n2_entry.o kernel/Task/context_switch.o kernel/n2_main.o kernel/builtin_nosfs.o kernel/agent.o \
	    kernel/nosm.o kernel/agent_loader.o kernel/regx.o kernel/IPC/ipc.o kernel/Task/thread.o kernel/stubs.o kernel/arch/CPU/smp.o kernel/arch/CPU/lapic.o \
	    kernel/macho2.o kernel/printf.o kernel.bin n2.bin O2.elf O2.bin user/libc/libc.o disk.img \
	    kernel/VM/pmm_buddy.o kernel/VM/paging_adv.o kernel/VM/cow.o kernel/VM/numa.o kernel/VM/kheap.o kernel/uaccess.o kernel/proc_launch.o kernel/trap.o \
	    kernel/symbols.o \
	    $(AGENT_OBJS) $(AGENT_ELFS) $(AGENT_BINS) $(BIN_OBJS) $(BIN_ELFS) $(BIN_BINS) \
	    src/agents/regx/regx.o user/agents/nosfs/nosfs.o \
	    user/rt/rt0_user.o user/rt/rt0_agent.o \
	    nosm/drivers/IO/serial.o nosm/drivers/example/hello/hello_nmod.o out/modules/hello.elf out/modules/hello.mo2 \
	    kernel/login_bin.h
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

.PHONY: all libc kernel boot agents bins clean run runmac
