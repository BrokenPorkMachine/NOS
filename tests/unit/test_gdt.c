#include <assert.h>
#include <stdint.h>
#include "gdt.h"

/* Stub for gdt_flush from assembly */
void gdt_flush(uint64_t ptr) { (void)ptr; }

int main(void) {
    gdt_install();
    struct gdt_entry entry;

    /* Kernel data segment must not set the 64-bit flag */
    gdt_get_entry(GDT_SEL_KERNEL_DATA >> 3, &entry);
    assert((entry.granularity & 0x20) == 0);

    /* User code/data should have DPL=3 and data segment L flag clear */
    gdt_get_entry(GDT_SEL_USER_CODE >> 3, &entry);
    assert((entry.access & 0x60) == 0x60);

    gdt_get_entry(GDT_SEL_USER_DATA >> 3, &entry);
    assert((entry.access & 0x60) == 0x60);
    assert((entry.granularity & 0x20) == 0);

    return 0;
}
