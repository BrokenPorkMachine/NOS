#pragma once
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define BOOTINFO_MAGIC_UEFI 0x55454649   // 'UEFI'
#define BOOTINFO_MAGIC_MB2  0x36d76289   // Multiboot2

#define BOOTINFO_MAX_MMAP    128
#define BOOTINFO_MAX_CPUS    32

// Physical memory region (RAM, reserved, etc)
typedef struct {
    uint64_t addr;     // Start address
    uint64_t len;      // Length in bytes
    uint32_t type;     // UEFI/MB2 type
    uint32_t reserved;
} bootinfo_memory_t;

// Framebuffer (graphics/text output)
typedef struct {
    uint64_t address;  // Phys addr of framebuffer
    uint32_t width;    // Pixels
    uint32_t height;
    uint32_t pitch;    // Bytes per row
    uint32_t bpp;      // Bits per pixel
    uint32_t type;     // 0=RGB, 1=Indexed, 2=Text, others: reserved
    uint32_t reserved;
} bootinfo_framebuffer_t;

// CPU (SMP) info, for APs/MADTs etc.
typedef struct {
    uint32_t processor_id; // ACPI/MB2 processor id
    uint32_t apic_id;      // Local APIC/cluster id
    uint32_t flags;        // enabled/online/boot
    uint32_t reserved;
} bootinfo_cpu_t;

// Main bootinfo struct, passed to kernel_main()
typedef struct bootinfo {
    uint32_t magic;            // BOOTINFO_MAGIC_*
    uint32_t size;             // Size of this struct
    const char *bootloader_name; // ASCII string, e.g. "NitrOBoot UEFI"
    const char *cmdline;         // Kernel command line (optional, may be NULL)
    bootinfo_memory_t *mmap;     // Array of mmap_entries
    uint32_t mmap_entries;
    bootinfo_framebuffer_t *framebuffer; // Optional, NULL if not set
    uint64_t acpi_rsdp;         // Physical address of ACPI RSDP, or 0 if not found
    bootinfo_cpu_t cpus[BOOTINFO_MAX_CPUS];
    uint32_t cpu_count;
    void *mb2_tags;             // Multiboot2 tag pointer (raw, optional)
    uint64_t reserved[8];       // For future expansion/alignment
    void *kernel_entry;         // Optionally: actual kernel entry (for hotswap etc)
} bootinfo_t;

#ifdef __cplusplus
}
#endif
