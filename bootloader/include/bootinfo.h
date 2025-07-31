// acpi.c
#include <stdint.h>
#include "bootinfo.h"

// Only basic MADT parsing. Improve as needed!
void acpi_parse_madt(uint64_t rsdp_addr, bootinfo_t *info) {
    if (!rsdp_addr || !info) return;
    // For brevity: only ACPI 2.0 XSDT supported.
    typedef struct { char sig[8]; uint8_t chksum; char oemid[6]; uint8_t rev; uint32_t rsdt; uint32_t len; uint64_t xsdt; } __attribute__((packed)) RSDP2;
    RSDP2 *rsdp = (RSDP2 *)(uintptr_t)rsdp_addr;
    if (rsdp->xsdt == 0) return; // fallback needed for ACPI 1.0 systems
    uint64_t *entries = (uint64_t *)((uintptr_t)rsdp->xsdt + 36); // XSDT starts after 36 bytes header
    uint32_t n = (*((uint32_t *)((uintptr_t)rsdp->xsdt + 4)) - 36) / 8;
    for (uint32_t i = 0; i < n; ++i) {
        uint8_t *sdt = (uint8_t *)(uintptr_t)entries[i];
        if (sdt[0] == 'A' && sdt[1] == 'P' && sdt[2] == 'I' && sdt[3] == 'C') {
            uint32_t len = *(uint32_t *)(sdt + 4);
            uint8_t *end = sdt + len;
            uint8_t *ptr = sdt + 44;
            uint32_t cpu_count = 0;
            while (ptr + 2 <= end) {
                uint8_t type = ptr[0], plen = ptr[1];
                if (type == 0 && plen >= 8) { // Processor Local APIC
                    uint8_t proc_id = ptr[2], apic_id = ptr[3];
                    uint32_t flags = *(uint32_t *)(ptr + 4);
                    if (flags & 1) {
                        info->cpus[cpu_count].processor_id = proc_id;
                        info->cpus[cpu_count].apic_id = apic_id;
                        info->cpus[cpu_count].flags = flags;
                        cpu_count++;
                        if (cpu_count >= BOOTINFO_MAX_CPUS) break;
                    }
                }
                ptr += plen;
            }
            info->cpu_count = cpu_count;
            return;
        }
    }
}
