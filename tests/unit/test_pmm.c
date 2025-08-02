#include <assert.h>
#include "../../kernel/VM/pmm.h"
#include "../../kernel/VM/paging.h"
#include "../../boot/include/bootinfo.h"
#include "../../user/libc/libc.h"

int main(void) {
    bootinfo_memory_t mmap[1] = {
        { .addr = 512*PAGE_SIZE, .len = 4*PAGE_SIZE, .type = 7, .reserved = 0 }
    };
    bootinfo_t bi = {0};
    bi.mmap = mmap;
    bi.mmap_entries = 1;
    pmm_init(&bi);
    assert(pmm_total_frames() >= 516);
    void *p1 = alloc_page();
    void *p2 = alloc_page();
    assert(p1 && p2 && p1 != p2);
    free_page(p1);
    void *p3 = alloc_page();
    assert(p3 == p1);
    return 0;
}
