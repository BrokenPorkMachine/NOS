#include <assert.h>
#include "../../kernel/VM/pmm.h"
#include "../../kernel/VM/pmm_buddy.h"
#include "../../boot/include/bootinfo.h"
#include "../../user/libc/libc.h"
#ifndef PAGE_SIZE
#define PAGE_SIZE 4096
#endif

int main(void) {
    static uint8_t region[128 * PAGE_SIZE];
    bootinfo_memory_t mmap[1] = {
        { .addr = (uint64_t)(uintptr_t)region, .len = sizeof(region), .type = 7, .reserved = 0 }
    };
    bootinfo_t bi = {0};
    bi.mmap = mmap;
    bi.mmap_entries = 1;
    pmm_init(&bi);
    assert(buddy_free_frames_total() == 128);

    void *p1 = alloc_page();
    assert(p1);
    assert(buddy_free_frames_total() == 127);

    void *p2 = alloc_page();
    assert(p2);
    assert(buddy_free_frames_total() == 126);

    free_page(p1);
    assert(buddy_free_frames_total() == 127);
    free_page(p2);
    assert(buddy_free_frames_total() == 128);

    return 0;
}
