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
  "name": "nitrfs",
  "version": "1.0.3",
  "abi": "nitr-1",
  "author": "Core Team",
  "deps": ["vfs>=1.0"],
  "exports": ["nitrfs_mount"],
  "imports": ["kmalloc", "klog"],
  "security": {
    "caps": ["disk_io"],
    "sandbox": { "fs": "ro", "net": false }
  }
}
```

## NitrFS Filesystem

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
} nitrfs_superblock_t;
```

## System Calls & Tools

The kernel exposes a small syscall surface that userland tools build upon.

```c
int sys_nosm_load(const char *path);
int sys_nosm_unload(const char *name);
int sys_nitrfs_snap(const char *path, uint64_t *snap_id);
```

### `nosmctl`
```
$ nosmctl list
nitrfs@1.0.3  [loaded]
```

### `nitrfsctl`
```
$ nitrfsctl snapshot /
Snapshot 42 created.
```

## Security & Extensibility

- **Signed Everything** – bootloader, kernel, modules, and filesystem metadata.
- **Hot Reload** – modules can be swapped live via `nosmctl`.
- **Manifest‑based Capabilities** – fine‑grained privileges and sandboxing.
- **Language Agnostic** – modules may be authored in C, Rust, or WebAssembly.

This blueprint forms the foundation for the next generation of NitrOS.
