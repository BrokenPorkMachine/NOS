#include "idt.h"
#include "../src/libc.h"

#define IDT_ENTRIES 256

static struct idt_entry idt[IDT_ENTRIES];
static struct idt_ptr idtp;

extern void isr_default_stub(void); // Assembly default ISR handler
extern void isr_timer_stub(void);   // Timer IRQ0 handler
extern void isr_syscall_stub(void); // Syscall handler (int 0x80)

void set_idt_entry(int vec, void* isr, uint8_t type_attr) {
    uintptr_t addr = (uintptr_t)isr;
    idt[vec].offset_low  = addr & 0xFFFF;
    idt[vec].selector    = 0x08; // Kernel code segment selector
    idt[vec].ist         = 0;
    idt[vec].type_attr   = type_attr;
    idt[vec].offset_mid  = (addr >> 16) & 0xFFFF;
    idt[vec].offset_high = (addr >> 32) & 0xFFFFFFFF;
    idt[vec].zero        = 0;
}

void idt_install(void) {
    memset(idt, 0, sizeof(idt));
    // Install default handler for all vectors
    for (int i = 0; i < IDT_ENTRIES; ++i)
        set_idt_entry(i, isr_default_stub, 0x8E); // Present, DPL=0, 64-bit int gate

    // Override with actual handlers for timer and syscall
    set_idt_entry(32, isr_timer_stub, 0x8E);   // IRQ0
    set_idt_entry(0x80, isr_syscall_stub, 0xEE); // Ring 3 syscall (DPL=3)

    idtp.limit = sizeof(idt) - 1;
    idtp.base  = (uintptr_t)&idt;

    asm volatile ("lidt %0" : : "m"(idtp));
}
