#pragma once
#include <stdint.h>

// 64-bit IDT entry (x86_64)
struct idt_entry {
    uint16_t offset_low;
    uint16_t selector;
    uint8_t  ist;        // Interrupt Stack Table offset
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

// Installs/loads the IDT
void idt_install(void);
