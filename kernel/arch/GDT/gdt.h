#pragma once

#include <stdint.h>
#include "segments.h"

/* Public GDT entry structure for inspection/testing */
struct gdt_entry {
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t  base_middle;
    uint8_t  access;
    uint8_t  granularity;
    uint8_t  base_high;
} __attribute__((packed));

void gdt_install(void);
void gdt_get_entry(int n, struct gdt_entry *out);
