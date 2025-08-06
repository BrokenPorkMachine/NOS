#pragma once
#include <stdint.h>
#include "context.h"

// Number of IDT entries (x86_64)
#define IDT_ENTRIES 256
#define KERNEL_CS   0x08   // Kernel code segment selector

// IDT entry for x86_64
struct idt_entry {
    uint16_t offset_low;
    uint16_t selector;
    uint8_t  ist;
    uint8_t  type_attr;
    uint16_t offset_mid;
    uint32_t offset_high;
    uint32_t zero;
} __attribute__((packed));

// Pointer for lidt
struct idt_ptr {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed));

// Setup functions
void idt_install(void);
void set_idt_entry(int vec, void *isr, uint8_t type_attr);

// Assembly ISR stubs
extern void isr_timer_stub(void);
extern void isr_syscall_stub(void);
extern void isr_keyboard_stub(void);
extern void isr_mouse_stub(void);
extern void isr_page_fault_stub(void);
extern void isr_ipi_stub(void);
extern void *isr_stub_table[];
