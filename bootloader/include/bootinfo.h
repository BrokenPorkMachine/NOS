// include/bootinfo.h
#pragma once
#include <stdint.h>

#define BOOTINFO_MAGIC_UEFI 0x55454649   // 'UEFI'
#define BOOTINFO_MAGIC_MB2  0x36d76289   // Multiboot2

#define BOOTINFO_MAX_MMAP    128
#define BOOTINFO_MAX_CPUS    32

typedef struct {
    uint64_t addr, len;
    uint32_t type; // UEFI/MB2 type
    uint32_t reserved;
} bootinfo_memory_t;

typedef struct {
    uint64_t address;
    uint32_t width, height, pitch, bpp;
    uint32_t type; // 0=RGB, 1=Indexed, 2=Text
} bootinfo_framebuffer_t;

typedef struct {
    uint32_t processor_id;
    uint32_t apic_id;
    uint32_t flags;
} bootinfo_cpu_t;

typedef struct bootinfo {
    uint32_t magic;
    uint32_t size;
    const char *bootloader_name;
    const char *cmdline;
    bootinfo_memory_t *mmap;
    uint32_t mmap_entries;
    bootinfo_framebuffer_t *framebuffer;
    uint64_t acpi_rsdp;
    bootinfo_cpu_t cpus[BOOTINFO_MAX_CPUS];
    uint32_t cpu_count;
    void *mb2_tags;   // Optionally: multiboot2 raw tag pointer
    uint64_t reserved[8];
    void *kernel_entry;
} bootinfo_t;
