#include "idt.h"
#include <string.h>

static struct idt_entry idt[IDT_ENTRIES];
static struct idt_ptr idtp;

void set_idt_entry(int vec, void *isr, uint8_t type_attr) {
    uintptr_t addr = (uintptr_t)isr;
    idt[vec].offset_low  = addr & 0xFFFF;
    idt[vec].selector    = KERNEL_CS;
    idt[vec].ist         = 0;
    idt[vec].type_attr   = type_attr;
    idt[vec].offset_mid  = (addr >> 16) & 0xFFFF;
    idt[vec].offset_high = (addr >> 32) & 0xFFFFFFFF;
    idt[vec].zero        = 0;
}

void idt_install(void) {
    memset(idt, 0, sizeof(idt));
    for (int i = 0; i < IDT_ENTRIES; ++i) {
        uint8_t type = (i == 0x80) ? 0xEE : 0x8E;
        set_idt_entry(i, isr_stub_table[i], type);
    }

    idtp.limit = sizeof(idt) - 1;
    idtp.base  = (uintptr_t)&idt;

    asm volatile ("lidt %0" : : "m"(idtp));
}
