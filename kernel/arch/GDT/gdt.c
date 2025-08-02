#include <stdint.h>
#include "gdt.h"

struct gdt_entry {
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t  base_middle;
    uint8_t  access;
    uint8_t  granularity;
    uint8_t  base_high;
} __attribute__((packed));

struct gdt_ptr {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed));

// 0x00: null, 0x08: kernel code, 0x10: kernel data,
// 0x18: user code,  0x20: user data
static struct gdt_entry gdt[5];
static struct gdt_ptr gp;

extern void gdt_flush(uint64_t);

void gdt_set_gate(int n, uint32_t base, uint32_t limit, uint8_t access, uint8_t gran) {
    gdt[n].base_low    = (base & 0xFFFF);
    gdt[n].base_middle = (base >> 16) & 0xFF;
    gdt[n].base_high   = (base >> 24) & 0xFF;
    gdt[n].limit_low   = (limit & 0xFFFF);
    gdt[n].granularity = ((limit >> 16) & 0x0F) | (gran & 0xF0);
    gdt[n].access      = access;
}

void gdt_install(void) {
    gp.limit = (sizeof(struct gdt_entry) * 5) - 1;
    gp.base  = (uint64_t)&gdt;

    gdt_set_gate(0, 0, 0, 0, 0);                // Null
    gdt_set_gate(1, 0, 0xFFFFF, 0x9A, 0xA0);    // Kernel code 0x08
    gdt_set_gate(2, 0, 0xFFFFF, 0x92, 0xA0);    // Kernel data 0x10
    gdt_set_gate(3, 0, 0xFFFFF, 0xFA, 0xA0);    // User code   0x18
    gdt_set_gate(4, 0, 0xFFFFF, 0xF2, 0xA0);    // User data   0x20

    gdt_flush((uint64_t)&gp);
}
