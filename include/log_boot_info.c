#include "../../user/libc/libc.h" // For print/serial_puts, adjust as needed
#include "../../boot/include/bootinfo.h"

// Helper to print hex addresses
static void print_hex64(uint64_t val) {
    char buf[20];
    snprintf(buf, sizeof(buf), "0x%016llx", (unsigned long long)val);
    serial_puts(buf);
}

void log_bootinfo(const bootinfo_t *bootinfo) {
    if (!bootinfo) {
        serial_puts("[bootinfo] NULL pointer passed!\n");
        return;
    }

    serial_puts("[bootinfo] Kernel load base: ");
    print_hex64(bootinfo->kernel_load_base);
    serial_puts("\n[bootinfo] Kernel load size: ");
    print_hex64(bootinfo->kernel_load_size);

    serial_puts("\n[bootinfo] Module count: ");
    print_hex64(bootinfo->module_count);

    serial_puts("\n[bootinfo] Memory map entries: ");
    print_hex64(bootinfo->mmap_entries);
    serial_puts("\n");

    for (uint32_t i = 0; i < bootinfo->mmap_entries; ++i) {
        const mmap_entry_t *entry = &bootinfo->mmap[i];
        serial_puts("[mmap] Region ");
        print_hex64(i);
        serial_puts(": addr=");
        print_hex64(entry->addr);
        serial_puts(" len=");
        print_hex64(entry->len);
        serial_puts(" type=");
        print_hex64(entry->type);
        serial_puts("\n");
    }

    for (uint32_t i = 0; i < bootinfo->module_count; ++i) {
        serial_puts("[module] Module ");
        print_hex64(i);
        serial_puts(": base=");
        print_hex64((uint64_t)bootinfo->modules[i].base);
        serial_puts(" size=");
        print_hex64((uint64_t)bootinfo->modules[i].size);
        serial_puts("\n");
    }
}

