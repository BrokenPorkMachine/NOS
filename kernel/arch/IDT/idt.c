#include "idt.h"
#include <string.h>
#include "../GDT/gdt_selectors.h"
#include "drivers/IO/serial.h"
#ifndef kprintf
#define kprintf serial_printf
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
    serial_printf("[idt] vec=%3d sel=%#06x off=%016lx attr=%02x ist=%u\n",
                  v,
                  (unsigned)idt[v].selector,
                  (unsigned long)off,
                  (unsigned)idt[v].type_attr,
                  (unsigned)(idt[v].ist & 7));
}

/* Self-test with proper masks (no -Woverflow) */
static int check_vec(int v, uint16_t allow_mask) {
    uint64_t off = ((uint64_t)idt[v].offset_high << 32) |
                   ((uint64_t)idt[v].offset_mid  << 16) |
                   (uint64_t)idt[v].offset_low;

    if (idt[v].selector != KERNEL_CS) {
        kprintf("[idt][FAIL] vec=%d selector=%#x expected=%#x\n",
                v, idt[v].selector, KERNEL_CS);
        return -1;
    }
    if ((idt[v].type_attr & 0x80) == 0) {
        kprintf("[idt][FAIL] vec=%d not present (attr=%#x)\n", v, idt[v].type_attr);
        return -1;
    }
    uint8_t gtype = (uint8_t)(idt[v].type_attr & 0x0F);
    if (((uint16_t)(1u << gtype) & allow_mask) == 0) {
        kprintf("[idt][WARN] vec=%d unexpected gate=%#x\n", v, gtype);
    }
    if (off < 0x10000) {
        kprintf("[idt][FAIL] vec=%d offset suspicious: %016llx\n",
                v, (unsigned long long)off);
        return -1;
    }
    return 0;
}

void idt_selftest(void) {
    const uint16_t ALLOW_INT  = (uint16_t)(1u << 0xE);
    const uint16_t ALLOW_TRAP = (uint16_t)(1u << 0xF);

    int ok = 0;
    ok += check_vec(6,  (uint16_t)(ALLOW_INT | ALLOW_TRAP));
    ok += check_vec(13, (uint16_t)(ALLOW_INT | ALLOW_TRAP));
    ok += check_vec(32, (uint16_t)(ALLOW_INT));

    kprintf("[idt] selftest: %s\n", ok == 0 ? "OK" : "FAILED");
}

void idt_install(void) {
    idt_populate_all();

    /* Pin key vectors */
    idt_set_interrupt_gate(6,  isr_ud_stub);     /* #UD */
    idt_set_interrupt_gate(32, isr_timer_stub);  /* APIC timer */

    idtp.limit = (uint16_t)(sizeof(idt) - 1);
    idtp.base  = (uint64_t)(uintptr_t)&idt;

    asm volatile ("lidt %0" : : "m"(idtp) : "memory");

    idt_dump_vec(6);
    idt_dump_vec(13);
    idt_dump_vec(32);
    idt_selftest();
}

void idt_reload(void) {
    asm volatile ("lidt %0" : : "m"(idtp) : "memory");
}
