#include "pmm.h"
#include "paging.h"
#include "../src/libc.h"

static uint8_t *bitmap = NULL;
static uint64_t total_frames = 0;

static inline void bit_set(uint64_t bit) {
    bitmap[bit / 8] |= (1 << (bit % 8));
}
static inline void bit_clear(uint64_t bit) {
    bitmap[bit / 8] &= ~(1 << (bit % 8));
}
static inline int bit_test(uint64_t bit) {
    return bitmap[bit / 8] & (1 << (bit % 8));
}

void pmm_init(const bootinfo_t *bootinfo) {
    uint64_t max_addr = 0;
    for (uint32_t i = 0; i < bootinfo->mmap_entries; ++i) {
        if (bootinfo->mmap[i].type != 7)
            continue;
        uint64_t end = bootinfo->mmap[i].addr + bootinfo->mmap[i].len;
        if (end > max_addr)
            max_addr = end;
    }
    total_frames = (max_addr + PAGE_SIZE - 1) / PAGE_SIZE;
    uint64_t bitmap_bytes = (total_frames + 7) / 8;
    bitmap = malloc(bitmap_bytes);
    if (!bitmap)
        return;
    memset(bitmap, 0xFF, bitmap_bytes);

    for (uint32_t i = 0; i < bootinfo->mmap_entries; ++i) {
        if (bootinfo->mmap[i].type != 7)
            continue;
        uint64_t start = bootinfo->mmap[i].addr / PAGE_SIZE;
        uint64_t end = (bootinfo->mmap[i].addr + bootinfo->mmap[i].len) / PAGE_SIZE;
        for (uint64_t f = start; f < end; ++f)
            bit_clear(f);
    }
    for (uint64_t f = 0; f < 512; ++f)
        bit_set(f);
}

void *alloc_page(void) {
    if (!bitmap)
        return NULL;
    for (uint64_t f = 0; f < total_frames; ++f) {
        if (!bit_test(f)) {
            bit_set(f);
            return (void *)(f * PAGE_SIZE);
        }
    }
    return NULL;
}

void free_page(void *page) {
    if (!page || !bitmap)
        return;
    uint64_t frame = (uint64_t)page / PAGE_SIZE;
    bit_clear(frame);
}

uint64_t pmm_total_frames(void) {
    return total_frames;
}
