#include <assert.h>
#include "../../kernel/VM/pmm_buddy.h"
#include "../../kernel/VM/numa.h"
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
    numa_init(&bi);
    buddy_init(&bi);
    assert(buddy_free_frames_total() == 128);

    void *p1 = buddy_alloc(6, 0, 0); // 64-page block
    assert(p1);
    assert(buddy_free_frames_total() == 64);

    void *p2 = buddy_alloc(5, 0, 0); // 32-page block
    assert(p2);
    assert(buddy_free_frames_total() == 32);

    buddy_free(p1, 6, 0);
    assert(buddy_free_frames_total() == 96);
    buddy_free(p2, 5, 0);
    assert(buddy_free_frames_total() == 128);

    return 0;
}
