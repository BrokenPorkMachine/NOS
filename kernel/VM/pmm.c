#include "pmm.h"
#include "paging.h"
#include "../../user/libc/libc.h"

// Limit physical memory handling to below 4 GiB. Some platforms report
// large, sparse regions (e.g., at 0xFD00000000) which would otherwise force
// the allocator to reserve an enormous bitmap and exhaust the tiny bootstrap
// heap. We currently only support low memory, so ignore anything above this
// threshold.
#define PMM_MAX_ADDR (1ULL << 32)
#define MAX_FRAMES (PMM_MAX_ADDR / PAGE_SIZE)
#define MAX_BITMAP_BYTES (MAX_FRAMES / 8)

static uint8_t static_bitmap[MAX_BITMAP_BYTES];
static uint8_t *bitmap = NULL;
static uint64_t total_frames = 0;
static uint64_t next_free = 0; // next frame index to start searching from
static uint64_t free_frames = 0; // number of free (unallocated) frames

static inline uint64_t bit_byte(uint64_t bit) { return bit >> 3; }
static inline uint8_t bit_mask(uint64_t bit) { return (uint8_t)(1u << (bit & 7)); }

static inline void bit_set(uint64_t bit) {
    bitmap[bit_byte(bit)] |= bit_mask(bit);
}
static inline void bit_clear(uint64_t bit) {
    bitmap[bit_byte(bit)] &= (uint8_t)~bit_mask(bit);
}
static inline int bit_test(uint64_t bit) {
    return (bitmap[bit_byte(bit)] & bit_mask(bit)) != 0;
}

static void reserve_range(uint64_t start, uint64_t end) {
    uint64_t s = start / PAGE_SIZE;
    uint64_t e = (end + PAGE_SIZE - 1) / PAGE_SIZE;
    for (uint64_t f = s; f < e && f < total_frames; ++f) {
        if (!bit_test(f)) {
            bit_set(f);
            free_frames--;
        }
    }
}

void pmm_init(const bootinfo_t *bootinfo) {
    uint64_t max_addr = 0;
    for (uint32_t i = 0; i < bootinfo->mmap_entries; ++i) {
        if (bootinfo->mmap[i].type != 7)
            continue;
        uint64_t start = bootinfo->mmap[i].addr;
        uint64_t end = start + bootinfo->mmap[i].len;
        if (start >= PMM_MAX_ADDR)
            continue;
        if (end > PMM_MAX_ADDR)
            end = PMM_MAX_ADDR;
        if (end > max_addr)
            max_addr = end;
    }
    total_frames = (max_addr + PAGE_SIZE - 1) / PAGE_SIZE;
    uint64_t bitmap_bytes = (total_frames + 7) / 8;
    if (bitmap_bytes > MAX_BITMAP_BYTES)
        bitmap_bytes = MAX_BITMAP_BYTES;
    bitmap = static_bitmap;
    memset(bitmap, 0xFF, bitmap_bytes);

    free_frames = 0;
    for (uint32_t i = 0; i < bootinfo->mmap_entries; ++i) {
        if (bootinfo->mmap[i].type != 7)
            continue;
        uint64_t start = bootinfo->mmap[i].addr;
        uint64_t end = start + bootinfo->mmap[i].len;
        if (start >= PMM_MAX_ADDR)
            continue;
        if (end > PMM_MAX_ADDR)
            end = PMM_MAX_ADDR;
        uint64_t s = start / PAGE_SIZE;
        uint64_t e = end / PAGE_SIZE;
        for (uint64_t f = s; f < e; ++f)
            bit_clear(f);
        free_frames += (e - s);
    }
    reserve_range(0, bootinfo->kernel_load_base);
    reserve_range(bootinfo->kernel_load_base,
                  bootinfo->kernel_load_base + bootinfo->kernel_load_size);
    for (uint32_t i = 0; i < bootinfo->module_count; ++i)
        reserve_range((uint64_t)bootinfo->modules[i].base,
                      (uint64_t)bootinfo->modules[i].base + bootinfo->modules[i].size);

    next_free = 0;
    while (next_free < total_frames && bit_test(next_free))
        next_free++;
}

void *alloc_page(void) {
    if (!bitmap || free_frames == 0)
        return NULL;
    for (uint64_t off = 0; off < total_frames; ++off) {
        uint64_t f = (next_free + off) % total_frames;
        if (!bit_test(f)) {
            bit_set(f);
            next_free = (f + 1) % total_frames;
            free_frames--;
            return (void *)(f * PAGE_SIZE);
        }
    }
    return NULL;
}

void free_page(void *page) {
    if (!page || !bitmap)
        return;
    uint64_t frame = (uint64_t)page / PAGE_SIZE;
    if (bit_test(frame)) {
        bit_clear(frame);
        free_frames++;
        if (frame < next_free)
            next_free = frame;
    }
}

uint64_t pmm_total_frames(void) {
    return total_frames;
}

uint64_t pmm_free_frames(void) {
    return free_frames;
}
