#pragma once
#include <stdint.h>

#define IDT_ENTRIES 256

// IDT entry (x86_64)
struct idt_entry {
    uint16_t offset_low;
    uint16_t selector;
    uint8_t  ist;        // Interrupt Stack Table offset (0 disables IST)
    uint8_t  type_attr;  // Gate type, DPL, present
    uint16_t offset_mid;
    uint32_t offset_high;
    uint32_t zero;
} __attribute__((packed));

// IDT pointer structure for lidt instruction
struct idt_ptr {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed));

// Functions
void idt_install(void);
void set_idt_entry(int vec, void* isr, uint8_t type_attr);

// Common selectors (update if you change your GDT)
#define KERNEL_CS 0x08

// Handler stubs from assembly
extern void isr_default_stub(void);
extern void isr_timer_stub(void);
extern void isr_syscall_stub(void);
extern void isr_keyboard_stub(void);
extern void isr_mouse_stub(void);
extern void isr_page_fault_stub(void);
extern void isr_ipi_stub(void);
