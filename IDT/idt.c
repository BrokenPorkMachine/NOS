#include "idt.h"
#include <string.h>

#define IDT_ENTRIES 256

static struct idt_entry idt[IDT_ENTRIES];
static struct idt_ptr idtp;

extern void isr_stub_table(void); // Provided by assembly stubs

void set_idt_entry(int vec, void* isr, uint8_t type_attr) {
    uint64_t addr = (uint64_t)isr;
    idt[vec].offset_low  = addr & 0xFFFF;
    idt[vec].selector    = 0x08; // Kernel code segment
    idt[vec].ist         = 0;
    idt[vec].type_attr   = type_attr;
    idt[vec].offset_mid  = (addr >> 16) & 0xFFFF;
    idt[vec].offset_high = (addr >> 32) & 0xFFFFFFFF;
    idt[vec].zero        = 0;
}

// You'll install real handlers next

void idt_install(void) {
    memset(idt, 0, sizeof(idt));
    // For now, install dummy handler for all vectors
    extern void isr_default_stub(void);
    for (int i = 0; i < IDT_ENTRIES; ++i)
        set_idt_entry(i, isr_default_stub, 0x8E); // Present, DPL=0, 64-bit int gate

    idtp.limit = sizeof(idt) - 1;
    idtp.base  = (uint64_t)&idt;
    // Load IDT
    asm volatile ("lidt %0" : : "m"(idtp));
}
