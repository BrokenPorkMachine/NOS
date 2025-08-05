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
    for (int i = 0; i < IDT_ENTRIES; ++i)
        set_idt_entry(i, isr_default_stub, 0x8E); // Present, ring0, 64-bit int gate

    set_idt_entry(32, isr_timer_stub,    0x8E); // Timer IRQ0
    set_idt_entry(33, isr_keyboard_stub, 0x8E); // Keyboard IRQ1
    set_idt_entry(44, isr_mouse_stub,    0x8E); // Mouse IRQ12
    set_idt_entry(14, isr_page_fault_stub, 0x8E); // Page fault
    set_idt_entry(0x80, isr_syscall_stub, 0xEE);  // Syscall gate, ring3
    set_idt_entry(0xF0, isr_ipi_stub,    0x8E); // IPI (example)

    idtp.limit = sizeof(idt) - 1;
    idtp.base  = (uintptr_t)&idt;

    asm volatile ("lidt %0" : : "m"(idtp));
}
