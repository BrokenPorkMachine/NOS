#ifndef BOOTINFO_H
#define BOOTINFO_H

#include <stdint.h>
#define BOOTINFO_MAGIC_UEFI 0x55454649   // 'UEFI'
#define BOOTINFO_MAGIC_MB2  0x36d76289

typedef struct bootinfo_memory {
    uint64_t addr;
    uint64_t len;
    uint32_t type;
    uint32_t reserved;
} bootinfo_memory_t;

typedef struct bootinfo {
    uint32_t magic;
    uint32_t size;
    uint64_t kernel_phys_base;
    uint64_t kernel_phys_end;
    uint32_t cpu_count;
    uint32_t reserved;
    bootinfo_memory_t* mmap;
    uint32_t mmap_entries;
    // Extend as needed!
} bootinfo_t;

#endif
