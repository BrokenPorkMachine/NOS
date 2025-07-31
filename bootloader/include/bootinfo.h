#pragma once
#include <stdint.h>
#include <stddef.h>

#pragma pack(push,1)
#define BOOTINFO_MAGIC_UEFI 0x55454649
#define BOOTINFO_MAGIC_MB2  0x36d76289
#define BOOTINFO_MAX_MMAP    128
#define BOOTINFO_MAX_CPUS    32

typedef struct {
    uint64_t addr, len;
    uint32_t type;
    uint32_t reserved;
} bootinfo_memory_t;

typedef struct {
    uint64_t address;
    uint32_t width, height, pitch, bpp;
    uint32_t type;
    uint32_t reserved;
} bootinfo_framebuffer_t;

typedef struct {
    uint32_t processor_id, apic_id, flags, reserved;
} bootinfo_cpu_t;

typedef struct bootinfo {
    uint32_t magic, size;
    const char *bootloader_name;
    const char *cmdline;
    bootinfo_memory_t *mmap;
    uint32_t mmap_entries;
    bootinfo_framebuffer_t *framebuffer;
    uint64_t acpi_rsdp;
    bootinfo_cpu_t cpus[BOOTINFO_MAX_CPUS];
    uint32_t cpu_count;
    void *mb2_tags;
    uint64_t reserved[8];
    void *kernel_entry;
} bootinfo_t;

#pragma pack(pop)

_Static_assert(sizeof(bootinfo_memory_t) == 24, "bootinfo_memory_t size mismatch");
_Static_assert(sizeof(bootinfo_framebuffer_t) == 32, "bootinfo_framebuffer_t size mismatch");
_Static_assert(sizeof(bootinfo_cpu_t) == 16, "bootinfo_cpu_t size mismatch");
_Static_assert(sizeof(bootinfo_t) == 648 || sizeof(bootinfo_t) == 656,
               "bootinfo_t unexpected size");
