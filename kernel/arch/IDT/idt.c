#include "idt.h"
#include <string.h> // Use standard string.h, not libc.h unless you need it

static struct idt_entry idt[IDT_ENTRIES];
static struct idt_ptr idtp;

void set_idt_entry(int vec, void* isr, uint8_t type_attr) {
    uintptr_t addr = (uintptr_t)isr;
    idt[vec].offset_low  = addr & 0xFFFF;
    idt[vec].selector    = KERNEL_CS;
    idt[vec].ist         = 0;         // Set if using IST (Interrupt Stack Table)
    idt[vec].type_attr   = type_attr; // Present, DPL, Gate type
    idt[vec].offset_mid  = (addr >> 16) & 0xFFFF;
    idt[vec].offset_high = (addr >> 32) & 0xFFFFFFFF;
    idt[vec].zero        = 0;
}

void idt_install(void) {
    memset(idt, 0, sizeof(idt));

    // Default all entries to the default stub
    for (int i = 0; i < IDT_ENTRIES; ++i)
        set_idt_entry(i, isr_default_stub, 0x8E);

    // IRQs and exceptions
    set_idt_entry(32, isr_timer_stub,    0x8E); // IRQ0: Timer
    set_idt_entry(33, isr_keyboard_stub, 0x8E); // IRQ1: Keyboard
    set_idt_entry(44, isr_mouse_stub,    0x8E); // IRQ12: Mouse
    set_idt_entry(14, isr_page_fault_stub,0x8E); // Page Fault
    set_idt_entry(0xF0, isr_ipi_stub,    0x8E); // IPI (inter-processor)

    // Syscall: INT 0x80, accessible from ring 3 (user)
    set_idt_entry(0x80, isr_syscall_stub, 0xEE); // DPL=3, present, int gate

    // Set up the IDT pointer
    idtp.limit = sizeof(idt) - 1;
    idtp.base  = (uintptr_t)&idt;

    // Load IDT
    asm volatile ("lidt %0" : : "m"(idtp));
}
