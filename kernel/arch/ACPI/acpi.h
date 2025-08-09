#pragma once
#include <stdint.h>
#include "../../../boot/include/bootinfo.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Initialize ACPI using the RSDP pointer provided in bootinfo.
 * - Validates RSDP and selects XSDT (preferred) or RSDT.
 * - Verifies checksums and lengths on all referenced tables.
 * - Locates FADT/FACP and loads a valid DSDT (if present).
 * - Parses MADT (APIC) to discover CPUs and (optionally) a LAPIC address override.
 * - Initializes the Local APIC via lapic_init() when MADT provides an address.
 *
 * On return:
 *   bootinfo->cpu_count and bootinfo->cpus[] are populated (>=1),
 *   and acpi_get_dsdt() returns the DSDT header if found/valid.
 */
void acpi_init(bootinfo_t *bootinfo);

/**
 * Get the loaded DSDT table header (struct sdt_header*), or NULL if not available.
 * The returned pointer refers to the in-memory ACPI table; do not free it.
 */
const void *acpi_get_dsdt(void);

#ifdef __cplusplus
}
#endif
