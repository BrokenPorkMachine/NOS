#include "gdt.h"
#include "segments.h"

struct gdt_ptr {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed));

static struct gdt_entry gdt[9];
static struct gdt_ptr gp;

extern void gdt_flush(uint64_t);

static void gdt_set_gate(int n, uint32_t base, uint32_t limit,
                         uint8_t access, uint8_t gran) {
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

    gdt_set_gate(0, 0, 0, 0, 0);                                      // Null
    gdt_set_gate(GDT_SEL_KERNEL_CODE >> 3, 0, 0xFFFFF, 0x9A, 0xA0);   // Ring0 code
    gdt_set_gate(GDT_SEL_KERNEL_DATA >> 3, 0, 0xFFFFF, 0x92, 0x80);   // Ring0 data
    gdt_set_gate(GDT_SEL_RING1_CODE >> 3,  0, 0xFFFFF, 0x9A|0x20, 0xA0); // Ring1 code
    gdt_set_gate(GDT_SEL_RING1_DATA >> 3,  0, 0xFFFFF, 0x92|0x20, 0x80); // Ring1 data
    gdt_set_gate(GDT_SEL_RING2_CODE >> 3,  0, 0xFFFFF, 0x9A|0x40, 0xA0); // Ring2 code
    gdt_set_gate(GDT_SEL_RING2_DATA >> 3,  0, 0xFFFFF, 0x92|0x40, 0x80); // Ring2 data
    gdt_set_gate(GDT_SEL_USER_CODE >> 3,   0, 0xFFFFF, 0x9A|0x60, 0xA0); // Ring3 code
    gdt_set_gate(GDT_SEL_USER_DATA >> 3,   0, 0xFFFFF, 0x92|0x60, 0x80); // Ring3 data

    gdt_flush((uint64_t)&gp);
}

void gdt_get_entry(int n, struct gdt_entry *out) {
    if (n >= 0 && n < 9 && out) {
        *out = gdt[n];
    }
}
