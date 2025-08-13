#include "arch/IDT/idt.h"
#include <string.h>

#ifndef kprintf
#include <stdio.h>
#define kprintf printf
#endif

static struct idt_entry idt[IDT_ENTRIES];
static struct idt_ptr   idtp;

#if defined(__GNUC__) || defined(__clang__)
_Static_assert((KERNEL_CS & 0x3) == 0, "KERNEL_CS must be ring 0 (DPL=0)");
#endif

static inline void set_idt_entry_ex(int vec, void *isr, uint8_t type_attr, uint8_t ist_slot) {
    uintptr_t addr = (uintptr_t)isr;

    idt[vec].offset_low  = (uint16_t)(addr & 0xFFFFu);
    idt[vec].selector    = (uint16_t)KERNEL_CS;
    idt[vec].ist         = (uint8_t)(ist_slot & 0x7);
    idt[vec].type_attr   = type_attr;
    idt[vec].offset_mid  = (uint16_t)((addr >> 16) & 0xFFFFu);
    idt[vec].offset_high = (uint32_t)((addr >> 32) & 0xFFFFFFFFu);
    idt[vec].zero        = 0;
}

void idt_set_with_ist(int vec, void *isr, uint8_t type_attr, uint8_t ist_slot) { set_idt_entry_ex(vec, isr, type_attr, ist_slot); }
void idt_set_interrupt_gate(int vec, void *isr) { set_idt_entry_ex(vec, isr, IDT_INTERRUPT_GATE, 0); }
void idt_set_trap_gate(int vec, void *isr)      { set_idt_entry_ex(vec, isr, IDT_TRAP_GATE, 0); }
void idt_set_user_interrupt_gate(int vec, void *isr) { set_idt_entry_ex(vec, isr, IDT_USER_GATE, 0); }
void idt_set_user_trap_gate(int vec, void *isr)      { set_idt_entry_ex(vec, isr, IDT_USER_TRAP_GATE, 0); }

static void idt_populate_all(void) {
    memset(idt, 0, sizeof(idt));

    for (int i = 0; i < IDT_ENTRIES; ++i) {
        uint8_t type = IDT_INTERRUPT_GATE;
        uint8_t ist  = 0;

        if (i <= 31) {
            switch (i) {
                case 2:  /* NMI */
                    type = IDT_TRAP_GATE;
#if IST_NMI
                    ist  = IST_NMI;
#endif
                    break;
                case 3:  /* #BP */
                case 4:  /* #OF */
                    type = IDT_USER_TRAP_GATE;
                    break;
                case 8:  /* #DF */
                    type = IDT_TRAP_GATE;
#if IST_DF
                    ist  = IST_DF;
#endif
                    break;
                default:
                    type = IDT_TRAP_GATE;
                    break;
            }
        } else {
            type = IDT_INTERRUPT_GATE;
        }

        if (i == 0x80) type = IDT_USER_GATE;

        set_idt_entry_ex(i, (void*)isr_stub_table[i], type, ist);
    }
}

void idt_dump_vec(int v) {
    uint64_t off = ((uint64_t)idt[v].offset_high << 32) |
                   ((uint64_t)idt[v].offset_mid  << 16) |
                   (uint64_t)idt[v].offset_low;
    kprintf("[idt] vec=%3d sel=%#06x off=%016llx attr=%02x ist=%u\n",
            v, idt[v].selector, (unsigned long long)off, idt[v].type_attr, idt[v].ist & 7);
}

void idt_install(void) {
    idt_populate_all();

    /* Explicitly set critical vectors you care about */
    idt_set_interrupt_gate(6,  isr_ud_stub);     /* #UD */
    idt_set_interrupt_gate(32, isr_timer_stub);  /* APIC timer */

    idtp.limit = (uint16_t)(sizeof(idt) - 1);
    idtp.base  = (uint64_t)(uintptr_t)&idt;

    asm volatile ("lidt %0" : : "m"(idtp) : "memory");

    idt_dump_vec(6);
    idt_dump_vec(13);
    idt_dump_vec(32);
}

void idt_reload(void) {
    asm volatile ("lidt %0" : : "m"(idtp) : "memory");
}
