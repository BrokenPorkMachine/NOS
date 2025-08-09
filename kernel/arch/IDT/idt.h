#pragma once

#include <stdint.h>
#include "context.h"  // if you export regs/context types from ISRs

#ifdef __cplusplus
extern "C" {
#endif

/* ---------------------------- Constants ---------------------------- */

#define IDT_ENTRIES 256

/* Segment selector for kernel code (must be a 64-bit code desc: L=1, D=0, DPL=0) */
#ifndef KERNEL_CS
# define KERNEL_CS 0x08
#endif

/* ---- IDT gate attribute helpers (64-bit) ---- */
#define IDT_ATTR_PRESENT        0x80
#define IDT_ATTR_TRAP_GATE      0x0F  /* type=1111b */
#define IDT_ATTR_INT_GATE       0x0E  /* type=1110b */
#define IDT_ATTR_DPL(n)         (((n) & 0x3) << 5)

/* Common presets */
#ifndef IDT_INTERRUPT_GATE      /* DPL=0 interrupt gate */
# define IDT_INTERRUPT_GATE (IDT_ATTR_PRESENT | IDT_ATTR_INT_GATE  | IDT_ATTR_DPL(0))
#endif
#ifndef IDT_TRAP_GATE           /* DPL=0 trap gate */
# define IDT_TRAP_GATE      (IDT_ATTR_PRESENT | IDT_ATTR_TRAP_GATE | IDT_ATTR_DPL(0))
#endif
#ifndef IDT_USER_GATE           /* DPL=3 interrupt gate (e.g., int 0x80) */
# define IDT_USER_GATE      (IDT_ATTR_PRESENT | IDT_ATTR_INT_GATE  | IDT_ATTR_DPL(3))
#endif
#ifndef IDT_USER_TRAP_GATE      /* DPL=3 trap gate (e.g., #BP/#OF from ring3) */
# define IDT_USER_TRAP_GATE (IDT_ATTR_PRESENT | IDT_ATTR_TRAP_GATE | IDT_ATTR_DPL(3))
#endif

/* Optional IST defaults (0..7 valid; 0 = disabled). You can override at build time. */
#ifndef IST_NMI
# define IST_NMI 1
#endif
#ifndef IST_DF
# define IST_DF  2
#endif

/* ------------------------- Descriptor Types ------------------------ */

/* x86_64 IDT entry */
struct idt_entry {
    uint16_t offset_low;     /* bits 0..15 of ISR address */
    uint16_t selector;       /* code segment selector */
    uint8_t  ist;            /* bits 0..2 = IST index, bits 3..7 = zero */
    uint8_t  type_attr;      /* P | DPL | type */
    uint16_t offset_mid;     /* bits 16..31 */
    uint32_t offset_high;    /* bits 32..63 */
    uint32_t zero;           /* reserved */
} __attribute__((packed));

struct idt_ptr {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed));

/* --------------------------- Public API ---------------------------- */

void idt_install(void);
void idt_reload(void);

/* Low-level setters */
void set_idt_entry(int vec, void *isr, uint8_t type_attr);
void idt_set_with_ist(int vec, void *isr, uint8_t type_attr, uint8_t ist_slot);

/* Convenience helpers */
void idt_set_interrupt_gate(int vec, void *isr);
void idt_set_trap_gate(int vec, void *isr);
void idt_set_user_interrupt_gate(int vec, void *isr);
void idt_set_user_trap_gate(int vec, void *isr);

/* ------------------------ ISR Stub Table --------------------------- */
/* Assembly provides a table of 256 ISR entry points (one per vector). */
extern void (*isr_stub_table[IDT_ENTRIES])(void);

/* Named stubs if you still use them directly somewhere */
extern void isr_timer_stub(void);
extern void isr_syscall_stub(void);
extern void isr_keyboard_stub(void);
extern void isr_mouse_stub(void);
extern void isr_page_fault_stub(void);
extern void isr_ipi_stub(void);

/* ------------------------- Sanity Checks --------------------------- */
#if defined(__GNUC__) || defined(__clang__)
_Static_assert((KERNEL_CS & 0x3) == 0, "KERNEL_CS must be ring 0 (DPL=0)");
#endif

#ifdef __cplusplus
} // extern "C"
#endif
