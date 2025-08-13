#include <assert.h>
#include <stdint.h>
#include "gdt.h"

/* Stub for gdt_flush from assembly */
void gdt_flush(uint64_t ptr) { (void)ptr; }
void gdt_flush_with_tr(uint64_t ptr, uint16_t sel) { (void)ptr; (void)sel; }
void serial_printf(const char *fmt, ...) { (void)fmt; }
void panic(const char *fmt, ...) { (void)fmt; }

int main(void) {
    gdt_install();
    struct gdt_entry entry;

    /* Kernel code should be DPL0 */
    gdt_get_entry(GDT_SEL_KERNEL_CODE >> 3, &entry);
    assert((entry.access & 0x60) == 0x00);

    /* User code should be DPL3 */
    gdt_get_entry(GDT_SEL_USER_CODE >> 3, &entry);
    assert((entry.access & 0x60) == 0x60);

    /* Data segments must not set the 64-bit flag */
    gdt_get_entry(GDT_SEL_KERNEL_DATA >> 3, &entry);
    assert((entry.granularity & 0x20) == 0);
    gdt_get_entry(GDT_SEL_USER_DATA >> 3, &entry);
    assert((entry.granularity & 0x20) == 0);

    return 0;
}
