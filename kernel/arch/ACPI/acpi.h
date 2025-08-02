#pragma once
#include <stdint.h>
#include "../../../boot/include/bootinfo.h"

void acpi_init(bootinfo_t *bootinfo);
const void *acpi_get_dsdt(void);
