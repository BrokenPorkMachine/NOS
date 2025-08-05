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
    assert(pmm_free_frames() == 4);
    void *p1 = alloc_page();
    assert(pmm_free_frames() == 3);
    void *p2 = alloc_page();
    assert(pmm_free_frames() == 2);
    assert(p1 && p2 && p1 != p2);
    free_page(p1);
    assert(pmm_free_frames() == 3);
    void *p3 = alloc_page();
    assert(pmm_free_frames() == 2);
    assert(p3 == p1);
    return 0;
}
