#pragma once
#include <stdint.h>
#include <stddef.h>
#include "../GDT/gdt_selectors.h"

#ifndef IDT_ENTRIES
#define IDT_ENTRIES 256
#endif

/* Your 64-bit kernel code selector must be DPL=0, L=1, D=0 */
#ifndef KERNEL_CS
# error "KERNEL_CS must be defined (64-bit code segment, DPL=0, L=1, D=0)"
#endif

/* Attribute helpers */
#define IDT_ATTR_PRESENT      0x80
#define IDT_ATTR_TRAP_GATE    0x0F
#define IDT_ATTR_INT_GATE     0x0E
#define IDT_ATTR_DPL(n)       (((n) & 0x3) << 5)

#define IDT_INTERRUPT_GATE    (IDT_ATTR_PRESENT | IDT_ATTR_INT_GATE | IDT_ATTR_DPL(0))
#define IDT_TRAP_GATE         (IDT_ATTR_PRESENT | IDT_ATTR_TRAP_GATE | IDT_ATTR_DPL(0))
#define IDT_USER_GATE         (IDT_ATTR_PRESENT | IDT_ATTR_INT_GATE | IDT_ATTR_DPL(3))
#define IDT_USER_TRAP_GATE    (IDT_ATTR_PRESENT | IDT_ATTR_TRAP_GATE | IDT_ATTR_DPL(3))

/* Optional IST picks (0..7; 0 = off) */
#ifndef IST_NMI
# define IST_NMI  1
#endif
#ifndef IST_DF
# define IST_DF   2
#endif

/* 16-byte IDT entry (packed) */
struct __attribute__((packed)) idt_entry {
    uint16_t offset_low;    // 0..15
    uint16_t selector;      // CS
    uint8_t  ist;           // bits 0..2 used
    uint8_t  type_attr;
    uint16_t offset_mid;    // 16..31
    uint32_t offset_high;   // 32..63
    uint32_t zero;
};

/* 10-byte IDT pointer (packed) */
struct __attribute__((packed)) idt_ptr {
    uint16_t limit;
    uint64_t base;
};

_Static_assert(sizeof(struct idt_entry) == 16, "IDT entry must be 16 bytes");
_Static_assert(offsetof(struct idt_entry, offset_low)  == 0,  "off_low");
_Static_assert(offsetof(struct idt_entry, selector)    == 2,  "selector");
_Static_assert(offsetof(struct idt_entry, ist)         == 4,  "ist");
_Static_assert(offsetof(struct idt_entry, type_attr)   == 5,  "type_attr");
_Static_assert(offsetof(struct idt_entry, offset_mid)  == 6,  "off_mid");
_Static_assert(offsetof(struct idt_entry, offset_high) == 8,  "off_high");
_Static_assert(offsetof(struct idt_entry, zero)        == 12, "zero");

_Static_assert(sizeof(struct idt_ptr) == 10, "IDT pointer must be 10 bytes");

#ifdef __cplusplus
extern "C" {
#endif

/* Populated by your assembly stubs */
extern void (*isr_stub_table[IDT_ENTRIES])(void);

/* Specific stubs you might override explicitly */
extern void isr_ud_stub(void);     // #UD
extern void isr_timer_stub(void);  // APIC timer (vector 32 typically)

/* API */
void idt_install(void);
void idt_reload(void);

/* Helpers for overriding entries */
void idt_set_interrupt_gate(int vec, void *isr);
void idt_set_trap_gate(int vec, void *isr);
void idt_set_user_interrupt_gate(int vec, void *isr);
void idt_set_user_trap_gate(int vec, void *isr);
void idt_set_with_ist(int vec, void *isr, uint8_t type_attr, uint8_t ist_slot);

/* Debug */
void idt_dump_vec(int v);

#ifdef __cplusplus
}
#endif
