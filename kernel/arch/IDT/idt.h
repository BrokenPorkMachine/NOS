#pragma once

#include <stdint.h>
#include "context.h"

// Number of entries in the descriptor table
#define IDT_ENTRIES 256

// Segment selector for kernel code
#define KERNEL_CS 0x08

// Common gate types
#define IDT_INTERRUPT_GATE 0x8E
#define IDT_TRAP_GATE      0x8F
#define IDT_USER_GATE      0xEE  // Interrupt gate accessible from ring3

// Descriptor for a single IDT entry on x86_64
struct idt_entry {
    uint16_t offset_low;
    uint16_t selector;
    uint8_t  ist;
    uint8_t  type_attr;
    uint16_t offset_mid;
    uint32_t offset_high;
    uint32_t zero;
} __attribute__((packed));

struct idt_ptr {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed));

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
