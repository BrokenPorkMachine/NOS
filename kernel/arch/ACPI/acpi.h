#pragma once
#include <stdint.h>
#include "../../../boot/include/bootinfo.h"

// Initializes ACPI tables using boot-provided RSDP pointer.
// Sets up DSDT and parses MADT/LAPICs for processor info.
void acpi_init(bootinfo_t *bootinfo);

// Returns a pointer to the loaded DSDT table header, or NULL if not available.
const void *acpi_get_dsdt(void);
