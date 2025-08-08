#include <assert.h>
#include "../../kernel/VM/pmm_buddy.h"
#include "../../kernel/VM/numa.h"
#include "../../boot/include/bootinfo.h"
#include "../../user/libc/libc.h"
#ifndef PAGE_SIZE
#define PAGE_SIZE 4096
#endif

int main(void) {
    bootinfo_memory_t mmap[1] = {
        { .addr = 512*PAGE_SIZE, .len = 4*PAGE_SIZE, .type = 7, .reserved = 0 }
    };
    bootinfo_t bi = {0};
    bi.mmap = mmap;
    bi.mmap_entries = 1;
    numa_init(&bi);
    buddy_init(&bi);
    assert(buddy_free_frames_total() == 4);
    void *p1 = buddy_alloc(0, 0, 0);
    assert(buddy_free_frames_total() == 3);
    void *p2 = buddy_alloc(0, 0, 0);
    assert(buddy_free_frames_total() == 2);
    assert(p1 && p2 && p1 != p2);
    buddy_free(p1, 0, 0);
    assert(buddy_free_frames_total() == 3);
    void *p3 = buddy_alloc(0, 0, 0);
    assert(buddy_free_frames_total() == 2);
    assert(p3 == p1);
    return 0;
}
