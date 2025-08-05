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

// Global Descriptor Table with explicit entries for all privilege rings
// 0x00: null, 0x08: kernel code, 0x10: kernel data,
// 0x18: ring1 code, 0x20: ring1 data,
// 0x28: ring2 code, 0x30: ring2 data,
// 0x38: user code,  0x40: user data
#define GDT_ENTRY_COUNT 9
static struct gdt_entry gdt[GDT_ENTRY_COUNT];
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
    gp.limit = sizeof(gdt) - 1;
    gp.base  = (uint64_t)&gdt;

    gdt_set_gate(0, 0, 0, 0, 0);                           // Null
    gdt_set_gate(GDT_SEL_KERNEL_CODE >> 3, 0, 0xFFFFF, 0x9A, 0xA0);
    gdt_set_gate(GDT_SEL_KERNEL_DATA >> 3, 0, 0xFFFFF, 0x92, 0xA0);
    gdt_set_gate(GDT_SEL_RING1_CODE >> 3,  0, 0xFFFFF, 0x9A | 0x20, 0xA0);
    gdt_set_gate(GDT_SEL_RING1_DATA >> 3,  0, 0xFFFFF, 0x92 | 0x20, 0xA0);
    gdt_set_gate(GDT_SEL_RING2_CODE >> 3,  0, 0xFFFFF, 0x9A | 0x40, 0xA0);
    gdt_set_gate(GDT_SEL_RING2_DATA >> 3,  0, 0xFFFFF, 0x92 | 0x40, 0xA0);
    gdt_set_gate(GDT_SEL_USER_CODE >> 3,   0, 0xFFFFF, 0x9A | 0x60, 0xA0);
    gdt_set_gate(GDT_SEL_USER_DATA >> 3,   0, 0xFFFFF, 0x92 | 0x60, 0xA0);

    gdt_flush((uint64_t)&gp);
}
