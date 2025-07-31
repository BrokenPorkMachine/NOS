#ifndef BOOTINFO_H
#define BOOTINFO_H

#include <stdint.h>

#define BOOTINFO_MAGIC_UEFI      0x55454649  // 'UEFI'
#define BOOTINFO_MAGIC_MB2       0x36d76289  // Multiboot2 standard magic

typedef struct bootinfo_memory {
    uint64_t addr;
    uint64_t len;
    uint32_t type;
    uint32_t reserved;
} bootinfo_memory_t;

typedef struct bootinfo {
    uint32_t magic;             // BOOTINFO_MAGIC_UEFI or BOOTINFO_MAGIC_MB2
    uint32_t size;              // sizeof(struct bootinfo)
    uint64_t kernel_phys_base;  // (optional) where kernel is loaded
    uint64_t kernel_phys_end;   // (optional) kernel end
    uint32_t cpu_count;         // (optional, if SMP)
    uint32_t reserved;          // align
    // --- memory map ---
    bootinfo_memory_t* mmap;
    uint32_t mmap_entries;
    // --- modules, framebuffers, etc, can go here
    // char bootloader[32];   // optional
    // void* framebuffer;     // optional
    // etc
} bootinfo_t;

#endif
