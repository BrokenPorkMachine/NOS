#ifndef BOOTINFO_H
#define BOOTINFO_H

#include <stdint.h>
#define BOOTINFO_MAGIC_UEFI 0x4E49545255454649ULL // "NITRUEFI"
#define BOOTINFO_MAX_MMAP 128
#define BOOTINFO_MAX_SEGS 32
#define BOOTINFO_MAX_MODULES 16
#define KERNEL_FMT_ELF64 1
#define KERNEL_FMT_MACHO64 2

typedef struct {
    uint64_t addr, len;
    uint32_t type;
    uint32_t reserved;
} bootinfo_memory_t;

typedef struct {
    uint64_t address;
    uint32_t width;
    uint32_t height;
    uint32_t pitch;
    uint32_t bpp;
    uint32_t type;
    uint32_t reserved;
} bootinfo_framebuffer_t;

typedef struct {
    uint32_t fmt; // 1=ELF64, 2=MachO
    uint32_t nsegs;
    struct {
        uint64_t vaddr;
        uint64_t paddr;
        uint64_t fileoff;
        uint64_t filesz;
        uint64_t memsz;
        uint32_t prot;
        uint32_t flags;
    } seg[BOOTINFO_MAX_SEGS];
    uint64_t file_base;   // Pointer to kernel file mapping, for on-demand paging.
    uint64_t file_size;
} bootinfo_kernel_segments_t;

typedef struct {
    uint64_t base;
    uint64_t size;
    char     name[64];
    uint8_t  sha256[32];
} bootinfo_module_t;

typedef struct {
    uint64_t magic;
    uint64_t size;
    char*    bootloader_name;
    char*    cmdline;
    uint32_t cpu_count;
    struct { uint32_t processor_id, apic_id, flags; } cpus[8];
    bootinfo_memory_t *mmap;
    uint32_t mmap_entries;
    uint64_t acpi_rsdp;
    void*    framebuffer;
    void*    kernel_entry;
    bootinfo_kernel_segments_t kernel_segs;
    uint32_t module_count;
    bootinfo_module_t modules[BOOTINFO_MAX_MODULES];
    uint8_t  reserved[256];
} bootinfo_t;

#endif
