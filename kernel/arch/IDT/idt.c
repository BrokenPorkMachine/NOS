#include "idt.h"
#include <stdint.h>
#include <string.h>
#include "../../../nosm/drivers/IO/serial.h"
#include "../../VM/paging_adv.h"

/*
 * Robust IDT setup:
 * - Uses explicit attribute helpers (present bit, DPL, gate type)
 * - Installs all vectors 0..255
 * - Trap/interrupt gate choices for exceptions are sensible defaults
 * - Assigns DPL=3 to INT3 (breakpoint), OF (overflow), and 0x80 syscall gate
 * - Optionally assigns IST slots to NMI and Double Fault (set IST_NMI/IST_DF below)
 * - Loads with lidt and leaves IF state unchanged
 */

#ifndef KERNEL_CS
# error "KERNEL_CS selector must be defined (64-bit code segment, DPL=0, L=1, D=0)"
#endif

/* -------- Gate attribute helpers (Intel SDM Vol.3A, IDT Gate Descriptors) -------- */
#ifndef IDT_ATTR_PRESENT
# define IDT_ATTR_PRESENT      0x80
#endif
#ifndef IDT_ATTR_TRAP_GATE
# define IDT_ATTR_TRAP_GATE    0x0F  /* 64-bit trap gate (type = 1111b)   */
#endif
#ifndef IDT_ATTR_INT_GATE
# define IDT_ATTR_INT_GATE     0x0E  /* 64-bit interrupt gate (type=1110b)*/
#endif
#define IDT_ATTR_DPL(n)        (((n) & 0x3) << 5)

/* Common presets */
#ifndef IDT_INTERRUPT_GATE   /* DPL=0 interrupt gate */
# define IDT_INTERRUPT_GATE   (IDT_ATTR_PRESENT | IDT_ATTR_INT_GATE | IDT_ATTR_DPL(0))
#endif
#ifndef IDT_TRAP_GATE        /* DPL=0 trap gate */
# define IDT_TRAP_GATE        (IDT_ATTR_PRESENT | IDT_ATTR_TRAP_GATE | IDT_ATTR_DPL(0))
#endif
#ifndef IDT_USER_GATE        /* DPL=3 interrupt gate (for int 0x80, etc.) */
# define IDT_USER_GATE        (IDT_ATTR_PRESENT | IDT_ATTR_INT_GATE | IDT_ATTR_DPL(3))
#endif
#ifndef IDT_USER_TRAP_GATE   /* DPL=3 trap gate (for #BP, #OF) */
# define IDT_USER_TRAP_GATE   (IDT_ATTR_PRESENT | IDT_ATTR_TRAP_GATE | IDT_ATTR_DPL(3))
#endif

/* Optional: pick IST slots for certain critical exceptions (0..7 valid; 0=off) */
#ifndef IST_NMI
# define IST_NMI  1  /* set to 0 to disable IST for NMI */
#endif
#ifndef IST_DF
# define IST_DF   2  /* set to 0 to disable IST for #DF (double fault) */
#endif

/* Stubs provided by your ISR/IRQ assembly table */
extern void isr_ud_stub(void);

/* IDT and descriptor */
static struct idt_entry idt[IDT_ENTRIES];
static struct idt_ptr   idtp;

/* Basic sanity: KERNEL_CS must be ring 0 */
#if defined(__GNUC__) || defined(__clang__)
_Static_assert((KERNEL_CS & 0x3) == 0, "KERNEL_CS must be ring 0 (DPL=0)");
#endif

static inline void set_idt_entry_ex(int vec, void *isr, uint8_t type_attr, uint8_t ist_slot) {
    uintptr_t addr = (uintptr_t)isr;

    idt[vec].offset_low  = (uint16_t)(addr & 0xFFFFu);
    idt[vec].selector    = (uint16_t)KERNEL_CS;
    idt[vec].ist         = (uint8_t)(ist_slot & 0x7); /* 3 bits valid */
    idt[vec].type_attr   = type_attr;
    idt[vec].offset_mid  = (uint16_t)((addr >> 16) & 0xFFFFu);
    idt[vec].offset_high = (uint32_t)((addr >> 32) & 0xFFFFFFFFu);
    idt[vec].zero        = 0;
}

void set_idt_entry(int vec, void *isr, uint8_t type_attr) {
    set_idt_entry_ex(vec, isr, type_attr, 0);
}

/* Select sensible defaults per vector */
static void idt_populate_all(void) {
    /* Clear the table first */
    memset(idt, 0, sizeof(idt));

    for (int i = 0; i < IDT_ENTRIES; ++i) {
        uint8_t type = IDT_INTERRUPT_GATE;
        uint8_t ist  = 0;

        /* Exception vectors 0..31: choose trap/interrupt appropriately */
        if (i <= 31) {
            switch (i) {
                case 2:  /* NMI */
                    type = IDT_TRAP_GATE;  /* no IF masking differences for NMI; trap gate is fine */
#if IST_NMI
                    ist  = IST_NMI;
#endif
                    break;
                case 3:  /* Breakpoint (#BP) — callable from user */
                    type = IDT_USER_TRAP_GATE;
                    break;
                case 4:  /* Overflow (#OF) — callable from user */
                    type = IDT_USER_TRAP_GATE;
                    break;
                case 8:  /* Double Fault (#DF) */
                    type = IDT_TRAP_GATE;
#if IST_DF
                    ist  = IST_DF;
#endif
                    break;
                default:
                    /* Most exceptions: trap gate is convenient (doesn't clear IF on entry) */
                    type = IDT_TRAP_GATE;
                    break;
            }
        } else {
            /* Remap PIC/APIC IRQs or other software ints as INT gates by default */
            type = IDT_INTERRUPT_GATE;
        }

        /* Syscall compat vector 0x80: user-callable INT gate */
        if (i == 0x80) {
            type = IDT_USER_GATE;
        }

        set_idt_entry_ex(i, isr_stub_table[i], type, ist);
    }
}

static void dump_idt_gate(int vec, const char *name) {
    struct idt_entry *e = &idt[vec];
    uint64_t off = ((uint64_t)e->offset_high << 32) |
                   ((uint64_t)e->offset_mid << 16) |
                   e->offset_low;
    serial_printf("[idt] vec=%d %s sel=%#04x off=%016lx low=%04x mid=%04x high=%08x attr=%02x ist=%u expected=%p\n",
                  vec, name, e->selector, off, e->offset_low, e->offset_mid,
                  e->offset_high, e->type_attr, e->ist, isr_stub_table[vec]);
}

/* Public API: install IDT with sane defaults */
void idt_install(void) {
    idt_populate_all();

    set_idt_entry(6, isr_ud_stub, IDT_INTERRUPT_GATE);   /* #UD */

    idtp.limit = (uint16_t)(sizeof(idt) - 1);
    idtp.base  = (uintptr_t)&idt;

    /* Load IDT. 'memory' clobber ensures table is written before lidt */
    asm volatile ("lidt %0" : : "m"(idtp) : "memory");

    /* Dump key gates to verify 64-bit offsets */
    dump_idt_gate(6, "#UD");
    dump_idt_gate(13, "#GP");
    dump_idt_gate(32, "timer");

    /* Sanity-check mapping around 0xB0000 */
    uint64_t phys, flags;
    if (paging_lookup_adv(0x00000000000B0000ULL, &phys, &flags)) {
        serial_printf("[map] 0xB0000 -> phys=%016lx flags=%016lx NX=%d\n",
                      phys, flags, (int)((flags >> 63) & 1));
    } else {
        serial_puts("[map] 0xB0000 unmapped\n");
    }
}

/* Optional helpers if you want to override individual vectors at runtime */

void idt_set_interrupt_gate(int vec, void *isr) {
    set_idt_entry(vec, isr, IDT_INTERRUPT_GATE);
}

void idt_set_trap_gate(int vec, void *isr) {
    set_idt_entry(vec, isr, IDT_TRAP_GATE);
}

void idt_set_user_interrupt_gate(int vec, void *isr) {
    set_idt_entry(vec, isr, IDT_USER_GATE);
}

void idt_set_user_trap_gate(int vec, void *isr) {
    set_idt_entry(vec, isr, IDT_USER_TRAP_GATE);
}

void idt_set_with_ist(int vec, void *isr, uint8_t type_attr, uint8_t ist_slot) {
    set_idt_entry_ex(vec, isr, type_attr, ist_slot);
}

void idt_reload(void) {
    asm volatile ("lidt %0" : : "m"(idtp) : "memory");
}
