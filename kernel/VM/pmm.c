#include <stddef.h>
#include <stdint.h>
#include "pmm.h"
#include "pmm_buddy.h"
#include "numa.h"

#define PROT_EXEC  0x1
#define PROT_WRITE 0x2
#define PROT_READ  0x4

extern void vmm_prot(void *va, size_t size, int prot);

extern char __text_start[] __attribute__((weak));
extern char __text_end[] __attribute__((weak));
extern char __rodata_start[] __attribute__((weak));
extern char __rodata_end[] __attribute__((weak));

static void protect_kernel(void) {
    uintptr_t text_lo = (uintptr_t)__text_start;
    uintptr_t text_hi = (uintptr_t)__text_end;
    if (text_hi > text_lo)
        vmm_prot((void *)text_lo, text_hi - text_lo, PROT_READ | PROT_EXEC);

    uintptr_t ro_lo = (uintptr_t)__rodata_start;
    uintptr_t ro_hi = (uintptr_t)__rodata_end;
    if (ro_hi > ro_lo)
        vmm_prot((void *)ro_lo, ro_hi - ro_lo, PROT_READ);
}

void pmm_init(const bootinfo_t *bootinfo) {
    numa_init(bootinfo);
    buddy_init(bootinfo);
    protect_kernel();
}

void *alloc_page(void) {
    return buddy_alloc(0, current_cpu_node(), 0);
}

void free_page(void *page) {
    if (page)
        buddy_free(page, 0, current_cpu_node());
}
