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

    gdt_get_entry(GDT_SEL_RING2_CODE >> 3, &entry);
    assert((entry.access & 0x60) == 0x40);

    return 0;
}
