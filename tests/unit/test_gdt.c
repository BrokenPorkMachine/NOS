#include <assert.h>
#include <stdint.h>
#include "gdt.h"

/* Stub for gdt_flush from assembly */
void gdt_flush(uint64_t ptr) { (void)ptr; }

int main(void) {
    gdt_install();
    struct gdt_entry entry;

    gdt_get_entry(GDT_SEL_RING1_CODE >> 3, &entry);
    assert((entry.access & 0x60) == 0x20);

    /* Ensure ring 1 selectors carry the correct RPL */
    assert((GDT_SEL_RING1_CODE_R1 & 0x3) == 1);

    gdt_get_entry(GDT_SEL_RING2_CODE >> 3, &entry);
    assert((entry.access & 0x60) == 0x40);

    /* Ensure ring 2 selectors carry the correct RPL */
    assert((GDT_SEL_RING2_CODE_R2 & 0x3) == 2);

    return 0;
}
