#pragma once
#include <stdint.h>
#define BOOTINFO_MAGIC_UEFI 0x4F324255
#define FBINFO_MAGIC 0xF00DBA66

typedef struct {
    uint64_t address;
    uint32_t width, height, pitch, bpp;
    uint32_t type;
    uint32_t reserved;
} bootinfo_framebuffer_t;

typedef struct {
    uint64_t addr;
    uint64_t len;
    uint32_t type;
    uint32_t reserved;
} bootinfo_memory_t;

typedef struct {
    uint32_t magic;
    uint32_t size;
    const char *bootloader_name;

    void *kernel_entry;
    uint64_t kernel_load_base;
    uint64_t kernel_load_size;
    const char *cmdline;

    // ACPI info
    uint64_t acpi_rsdp, acpi_xsdt, acpi_rsdt, acpi_dsdt;

    // CPU/LAPIC/IOAPIC info (MADT)
    uint64_t lapic_addr;
    uint32_t cpu_count;
    struct {
        uint32_t apic_id;
        uint32_t acpi_id;
        uint8_t online;
        uint8_t is_bsp;
        uint8_t reserved[6];
        // room for x2APIC, etc.
    } cpus[256];
    struct {
        uint32_t ioapic_id;
        uint32_t ioapic_addr;
        uint32_t gsi_base;
    } ioapics[8];
    uint32_t ioapic_count;

    bootinfo_memory_t *mmap;
    uint64_t mmap_entries, mmap_desc_size;
    uint32_t mmap_desc_ver;

    // Framebuffer info
    bootinfo_framebuffer_t fb;

    // Modules
    struct {
        void *base;
        uint64_t size;
        const char *name;
    } modules[16];
    uint32_t module_count;

    // Boot device
    uint32_t boot_device_type, boot_partition;

    // SMBIOS
    uint64_t smbios_entry;

    // RTC
    uint16_t current_year, current_month, current_day;
    uint16_t current_hour, current_minute, current_second;

    void *uefi_system_table;

    uint64_t reserved[32];
} bootinfo_t;
