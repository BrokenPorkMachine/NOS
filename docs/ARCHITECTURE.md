# NitrOS Architecture Blueprint

NitrOS is evolving toward a unified, secure architecture where every component is verified, versioned, and hot‑swappable. This document captures the high‑level design for the new stack.

## UEFI Bootloader

- Loads the kernel and optional early NOSM modules.
- Provides a rich `bootinfo` structure (memory map, modules, framebuffer, ACPI).
- Verifies signatures for all loaded components before jumping to the kernel.

**Bootloader Layout**

```
/EFI/NitrOS/
 ├── bootloader.efi
 ├── nitrboot.cfg
 └── modules/*.nosm
```

**`bootinfo` Structure**

```c
typedef struct {
    uint32_t magic;
    uint32_t version;
    EFI_MEMORY_DESCRIPTOR *mmap;
    uint64_t mmap_entries;
    struct {
        uint64_t addr;
        uint64_t size;
        char     name[64];
        uint8_t  sha256[32];
    } modules[16];
    struct {
        uint64_t fb_base;
        uint32_t width, height, pitch, bpp;
    } framebuffer;
    uint64_t acpi_rsdp;
} nitr_bootinfo_t;
```

## Kernel & NOSM Modules

- Microkernel/hybrid design with strong process isolation.
- Modules are packaged as `.nosm` files with a manifest and signature.
- Kernel can load/unload modules at runtime with sandboxing and symbol resolution.

**NOSM Layout**

```
+-------------------+
| NOSM Header       |
+-------------------+
| Manifest          |
+-------------------+
| Code / Data       |
+-------------------+
| Relocs / Tables   |
+-------------------+
| Signature Block   |
+-------------------+
```

**Sample Manifest**

```json
{
  "name": "nosfs",
  "version": "1.0.3",
  "abi": "nitr-1",
  "author": "Core Team",
  "deps": ["vfs>=1.0"],
  "exports": ["nosfs_mount"],
  "imports": ["kmalloc", "klog"],
  "security": {
    "caps": ["disk_io"],
    "sandbox": { "fs": "ro", "net": false }
  }
}
```

## NOSFS Filesystem

- Transactional, versioned, and cryptographically verifiable.
- Copy‑on‑write blocks with journaling and deduplication.
- Metadata and ACLs are signed and discoverable.

**Superblock**

```c
typedef struct {
    uint32_t magic;       // 'NFS1'
    uint32_t block_size;  // e.g. 4096
    uint64_t total_blocks;
    uint64_t uuid[2];
    uint64_t checkpoint_lba;
    uint8_t  fs_pubkey[32];
    uint8_t  reserved[4008];
} nosfs_superblock_t;
```

## Memory Management

The N2 kernel relies on a two-tiered memory system. A physical memory manager
initializes from the UEFI map and hands out frames via a buddy allocator, while
the virtual memory manager builds per-task page tables with NX, SMEP and SMAP
protection. 4 KiB and 2 MiB pages are mixed to balance granularity with TLB
pressure. Copy‑on‑write and NUMA‑aware placement are planned to support large
multi-socket machines. See [MEMORY_MANAGEMENT.md](MEMORY_MANAGEMENT.md) for the
full design.

## CPU Handling and Caches

CPUs are enumerated through ACPI and `CPUID` and brought online with the local
APIC. Each core maintains its own scheduler data and per‑CPU structure. Memory
operations use explicit barriers and shootdowns to keep caches coherent; TLBs
are flushed on context switches and inter‑processor interrupts coordinate page
table updates. Future revisions will explore cache coloring and non‑temporal
loads for IO‑heavy agents.

## Threading & Tasking

Threads run inside task address spaces managed by the scheduler. Each CPU has a
run queue with round‑robin time slicing, and tasks may spawn multiple threads
that share an IPC mail box. Context switches save and restore full register
state, including FPU and SIMD units. Priorities and more advanced policies such
as affinity or fair scheduling are planned. Additional details live in
[CPU_THREADING_TASKING.md](CPU_THREADING_TASKING.md).

## System Calls & Tools

The kernel exposes a small syscall surface that userland tools build upon.

```c
int sys_nosm_load(const char *path);
int sys_nosm_unload(const char *name);
int sys_nosfs_snap(const char *path, uint64_t *snap_id);
```

### `nosmctl`

```
$ nosmctl list
nosfs@1.0.3  [loaded]
```

### `nosfsctl`

```
$ nosfsctl snapshot /
Snapshot 42 created.
```

## Security & Extensibility

- **Signed Everything** – bootloader, kernel, modules, and filesystem metadata.
- **Hot Reload** – modules can be swapped live via `nosmctl`.
- **Manifest‑based Capabilities** – fine‑grained privileges and sandboxing.
- **Language Agnostic** – modules may be authored in C, Rust, or WebAssembly.

This blueprint forms the foundation for the next generation of NitrOS.
