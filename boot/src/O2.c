#pragma once
#include <stdint.h>

#define BOOTINFO_MAGIC_UEFI 0x4F324255
#define FBINFO_MAGIC 0xF00DBA66

typedef struct {
    uint32_t magic;
    uint32_t size;
    const char *bootloader_name;

    // Kernel image info
    void *kernel_entry;
    uint64_t kernel_load_base;
    uint64_t kernel_load_size;

    // Command line
    const char *cmdline;

    // ACPI
    uint64_t acpi_rsdp;   // Physical address of RSDP
    uint64_t acpi_xsdt;   // Physical address of XSDT (if available)
    uint64_t acpi_rsdt;   // Physical address of RSDT (legacy)
    uint64_t acpi_dsdt;   // Physical address of DSDT (optional)

    // LAPIC/CPU info (from MADT)
    uint64_t lapic_addr;
    uint32_t cpu_count;
    struct {
        uint32_t apic_id;
        uint8_t  is_bsp;
        uint8_t  reserved[7];
    } cpus[256]; // Up to 256 CPUs, expand if needed

    // IOAPICs (from MADT)
    struct {
        uint32_t ioapic_id;
        uint32_t ioapic_addr;
        uint32_t gsi_base;
    } ioapics[8];
    uint32_t ioapic_count;

    // Memory map
    void *mmap;
    uint64_t mmap_entries;
    uint64_t mmap_desc_size;
    uint32_t mmap_desc_ver;

    // Framebuffer info
    struct {
        uint32_t magic;
        void *base;
        uint32_t width;
        uint32_t height;
        uint32_t pitch;
        uint32_t bpp;
    } fb;

    // Modules (e.g., initrd)
    struct {
        void *base;
        uint64_t size;
        const char *name;
    } modules[16];
    uint32_t module_count;

    // Boot device
    uint32_t boot_device_type; // e.g., 0=unknown, 1=AHCI, 2=NVMe, 3=USB, etc.
    uint32_t boot_partition;

    // SMBIOS
    uint64_t smbios_entry;     // Physical address of SMBIOS entry point (optional)

    // Real-time clock (RTC)
    uint16_t current_year, current_month, current_day;
    uint16_t current_hour, current_minute, current_second;

    // UEFI system table (optional, rarely used)
    void *uefi_system_table;

    // Additional reserved fields for future expansion
    uint64_t reserved[32];
} bootinfo_t;
