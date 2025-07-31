# NitrOS (NOS)

## Overview

**NitrOS (NOS)** is a new experimental operating system inspired by the original CMU Mach microkernel and modern security, flexibility, and developer-centric features. It is written from scratch for x86\_64 and is designed as a pure microkernel: the kernel manages only scheduling, memory, IPC, and hardware abstraction; all higher-level services run in user space as isolated, message-passing servers.

---

## Features

* Custom UEFI bootloader (no GRUB, no 3rd-party loaders)
* True x86\_64 long mode kernel
* Clean kernel/user separation (ring 0/3)
* Full paging and memory protection
* Modular, timer-driven preemptive multitasking
* Interrupt handling (IDT, PIT, PIC)
* Basic thread and task abstractions
* Mach-style IPC message passing (prototype queue implementation)
* NitrFS secure in-memory filesystem server with optional block storage
* Simple secure heap allocator for user-space memory
* All device drivers, filesystems, and networking to run as user-mode agents

---

## Quick Build & Run (QEMU, Linux/WSL recommended)

1. **Dependencies:**

   * `x86_64-elf-gcc`, `nasm`, `qemu`, `make`, `mtools`
   * For UEFI boot: FAT image tools (`mkfs.vfat`, `mcopy`)
   * For gnu-efi bootloader: `gnu-efi` (for bootloader only)
   * Optionally set the `CROSS_COMPILE` environment variable if your
     cross compiler prefix differs from the default `/opt/cross/bin/x86_64-elf-`.
2. **Build the kernel:**

   ```sh
   make        # use CROSS_COMPILE if your toolchain uses a different prefix
   ```
3. **Prepare boot image:**

   * Create a FAT-formatted image and copy `BOOTX64.EFI` (bootloader) and `kernel.bin`:

   ```sh
   mkfs.vfat -C disk.img 4096
   mmd -i disk.img ::/EFI
   mmd -i disk.img ::/EFI/BOOT
   mcopy -i disk.img bootx64.efi ::/EFI/BOOT/BOOTX64.EFI
   mcopy -i disk.img kernel.bin ::/EFI/BOOT/kernel.bin
   ```
4. **Run in QEMU:**

   ```sh
   qemu-system-x86_64 -drive format=raw,file=disk.img \
     -bios /usr/share/OVMF/OVMF_CODE.fd -serial stdio
   ```

   The `-serial stdio` option attaches COM1 to your terminal so early boot
   logs appear even before the framebuffer is initialized.
   **Important:** the shell's VGA output and PS/2 keyboard require a graphical
   window. Remove the `-display none` argument (or explicitly use
   `-display sdl`) otherwise no shell will appear and keyboard input will seem
   unresponsive. See
   [docs/SERIAL_CONSOLE.md](docs/SERIAL_CONSOLE.md) for more details.

---

## Directory Structure

```
/Bootloader    # UEFI bootloader source (main.c, Makefile)
/kernel        # Kernel sources (kernel.c, idt.c, gdt.c, ...)
/servers/nitrfs # NitrFS filesystem server
/servers/shell  # Simple demonstration shell
/agents        # Reference docs and AGENTS.md
/              # Root: Makefile, linker scripts, README.md
```

---

## User Environment

NitrOS boots directly into a minimal set of user-mode services. A small
shell task communicates with the **NitrFS** filesystem server purely
through IPC to showcase how higher level applications will interact with
the kernel and each other.

### Shell Commands

When the shell starts it prints:

```
NOS shell ready
type 'help' for commands
```

The shell understands a handful of simple text commands. Examples:

```
> create hello.txt
> write hello.txt hello
> ls
> cat hello.txt
> rm hello.txt
```

Available commands include:

* `cd` – change the working directory (not yet implemented)
* `ls` or `dir` – list directory contents
* `mkdir` – create a new directory (not yet implemented)
* `mv` – move or rename a file
* `crc` – compute CRC32 of a file
* `verify` – verify file integrity using CRC32
* `rm` – remove a file or directory
* `cat` – display file contents
* `create` – create a new empty file
* `write` – write data to a file
* `help` – show available commands



### File System Navigation

Once the shell exposes these common commands you can move around the
NitrFS directory structure similarly to a regular Unix environment. A
typical session might look like:

```sh
# list the current directory
ls

# create and enter a new folder
mkdir demo
cd demo

# display a file and return to the parent directory
cat ../README.md
cd ..

# rename then remove the folder
mv demo testdemo
rm -r testdemo
```

---

## How It Works

1. **UEFI bootloader** loads the kernel from `/EFI/BOOT/kernel.bin` into RAM and jumps to its entry point in long mode.
2. **Kernel** sets up the GDT, paging, IDT, and enables timer IRQ.
3. **Threads** and **user tasks** are created, with kernel-mode and user-mode stacks and code.
4. **Preemptive scheduling** happens via timer interrupt, with context switching between threads/tasks.
5. **System calls** and **IPC** are handled via traps (e.g., `int $0x80`), building the foundation for a full Mach-style message-passing environment.

---

## Agents & Architecture

See [AGENTS.md](./AGENTS.md) for a detailed breakdown of all core system agents (kernel, bootloader, user-mode servers, IPC framework, etc).

---

## Roadmap

* [x] User/kernel context switching (improved register save/restore)
* [x] System call interface with dispatcher and validation
* [ ] Interactive user task/server framework (done)
* [x] Basic IPC primitives (prototype)
* [x] NitrFS filesystem server (block storage capable)
* [ ] Window server and networking agents
* [x] Shell and developer tools (prototype shell)

---

## Contributing

PRs, issues, and kernel/module proposals are welcome! This OS is a developer playground for microkernel, secure-by-default, and modular OS research. Please see `AGENTS.md` and code comments before contributing subsystems or new agents.

---

## License

(C) 2025 failbr34k. All rights reserved. Released for research, educational, and experimental use only.
