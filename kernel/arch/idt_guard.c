// kernel/arch/idt_guard.c
#include <stdint.h>
#include "drivers/IO/serial.h"

struct __attribute__((packed)) idtr { uint16_t limit; uint64_t base; };

static inline void sidt(struct idtr* d) { __asm__ volatile("sidt %0" : "=m"(*d)); }

void idt_guard_init_once(void) {
    struct idtr d; sidt(&d);

    // Peek UD(6) and GP(13) entries; format is arch-dependent; we just print addresses.
    // If your IDT entry layout differs, adjust these reads or just omit them.
    uint64_t *tbl = (uint64_t*)d.base;
    uint64_t vec6  = tbl ? tbl[6*2]  : 0;
    uint64_t vec13 = tbl ? tbl[13*2] : 0;

    serial_printf("[idt] pre-guard base=%016lx lim=%04x vec6=%016lx vec13=%016lx\n",
                  (unsigned long)d.base, d.limit, (unsigned long)vec6, (unsigned long)vec13);

    // This file intentionally *does not* modify the IDT; trap_init() must install it.
    // We just log state to confirm you’re off the firmware IDT after trap_init().
    sidt(&d);
    tbl = (uint64_t*)d.base;
    vec6  = tbl ? tbl[6*2]  : 0;
    vec13 = tbl ? tbl[13*2] : 0;
    serial_printf("[idt] post-guard base=%016lx lim=%04x vec6=%016lx vec13=%016lx\n",
                  (unsigned long)d.base, d.limit, (unsigned long)vec6, (unsigned long)vec13);
}
