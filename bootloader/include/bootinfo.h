#ifndef BOOTINFO_H
#define BOOTINFO_H

#include <stdint.h>

#define BOOTINFO_MAGIC_UEFI  0x55454649 // "UEFI"
#define BOOTINFO_MAGIC_MB2   0x36d76289 // Multiboot2
#define BOOTINFO_MAGIC_LIMINE 0x4C494D49 // "LIMI" (example for extensibility)

// -------- Memory Map Types --------
typedef enum {
    BOOTINFO_MMAP_USABLE = 1,
    BOOTINFO_MMAP_RESERVED = 2,
    BOOTINFO_MMAP_ACPI_RECLAIMABLE = 3,
    BOOTINFO_MMAP_NVS = 4,
    BOOTINFO_MMAP_BAD = 5,
    BOOTINFO_MMAP_BOOTLOADER_RECLAIMABLE = 0x1000,
    BOOTINFO_MMAP_KERNEL_AND_MODULES = 0x1001
} bootinfo_memory_type_t;

typedef struct {
    uint64_t addr;
    uint64_t len;
    uint32_t type; // see bootinfo_memory_type_t
    uint32_t reserved;
} bootinfo_memory_t;

// -------- Framebuffer Info --------
typedef struct {
    uint64_t address;     // Physical address of framebuffer
    uint32_t width;
    uint32_t height;
    uint32_t pitch;
    uint8_t  bpp;         // Bits per pixel
    uint8_t  type;        // 0 = RGB, 1 = indexed, etc
    uint8_t  reserved[6];
} bootinfo_framebuffer_t;

// -------- SMP / Multiprocessor Info --------
#define MAX_CPU_COUNT 256
typedef struct {
    uint32_t count;
    uint32_t bsp_lapic_id;         // Local APIC ID of the BSP
    uint32_t lapic_ids[MAX_CPU_COUNT]; // All logical processor APIC IDs
    uint32_t reserved[7];
} bootinfo_smp_t;

// -------- Module/Initrd --------
#define MAX_MODULES 8
typedef struct {
    uint64_t begin;
    uint64_t end;
    char     name[64];
} bootinfo_module_t;

// -------- Main bootinfo struct --------
typedef struct bootinfo {
    // --- Bootloader flags ---
    uint32_t magic;          // e.g. BOOTINFO_MAGIC_UEFI, _MB2, _LIMINE
    uint32_t size;           // total struct size
    uint64_t kernel_phys_base;
    uint64_t kernel_phys_end;

    // --- Memory map ---
    bootinfo_memory_t* mmap;
    uint32_t mmap_entries;

    // --- Framebuffer (graphics) ---
    bootinfo_framebuffer_t* framebuffer;

    // --- ACPI (RSDP pointer) ---
    uint64_t acpi_rsdp;      // Physical pointer to ACPI RSDP, or 0

    // --- SMP info ---
    bootinfo_smp_t* smp;

    // --- Boot modules (initrd, etc) ---
    bootinfo_module_t modules[MAX_MODULES];
    uint32_t module_count;

    // --- Kernel command line ---
    char* cmdline;

    // --- Bootloader name/version ---
    char* bootloader_name;

    // --- Extensibility: Custom tags, pointers, etc ---
    void*  extra;

    // --- (you can add more here as your OS grows!) ---
} bootinfo_t;

#endif
