#include "pmm_buddy.h"
#include "numa.h"
#include <stdint.h>
#include <stddef.h>
#include "../../user/libc/libc.h"

// =====================
//  SMP SPINLOCK PRIMITIVES
// =====================

typedef volatile int spinlock_t;

static inline void spin_lock(spinlock_t *l) {
    while (__sync_lock_test_and_set(l, 1)) while(*l);
}
static inline void spin_unlock(spinlock_t *l) {
    __sync_lock_release(l);
}

// =====================
//  Buddy Allocator Structures
// =====================

typedef struct buddy_block {
    struct buddy_block *next;
} buddy_block_t;

typedef struct {
    buddy_block_t *free_list[PMM_BUDDY_ORDERS];
    uint8_t      *bitmap;      // 1 bit per minimal block
    uint64_t      base, length;
    uint32_t      frames, max_order;
    spinlock_t    lock;
    uint64_t      free_frames;
} buddy_zone_t;

static buddy_zone_t zones[MAX_NUMA_ZONES];
static int zone_count = 0;

// =====================
//  Utility Bit Operations
// =====================

#define BITMAP_SIZE(frames) (((frames)+7)/8)
#define BIT_SET(bm,bit)   ((bm)[(bit)>>3] |= (1<<((bit)&7)))
#define BIT_CLEAR(bm,bit) ((bm)[(bit)>>3] &= ~(1<<((bit)&7)))
#define BIT_TEST(bm,bit)  (((bm)[(bit)>>3] >> ((bit)&7)) & 1)

// =====================
//  Helper: find the order for a size
// =====================
static inline uint32_t order_for_size(uint64_t sz) {
    uint32_t order = 0;
    uint64_t n = PAGE_SIZE;
    while (n < sz) { n <<= 1; order++; }
    return order;
}

// =====================
//  Zone/Node Finding
// =====================

static int addr_in_zone(uint64_t phys, const buddy_zone_t *z) {
    return (phys >= z->base) && (phys < z->base + z->length);
}
int buddy_find_zone(uint64_t phys) {
    for (int n=0; n<zone_count; n++)
        if (addr_in_zone(phys, &zones[n]))
            return n;
    return -1;
}

// =====================
//  Block/Frame Math
// =====================

static uint32_t addr_to_frame(const buddy_zone_t *z, uint64_t addr) {
    return (addr - z->base) / PAGE_SIZE;
}
static uint64_t frame_to_addr(const buddy_zone_t *z, uint32_t frame) {
    return z->base + (frame * PAGE_SIZE);
}

// =====================
//  Buddy Allocator Core
// =====================

// Recursively split blocks to reach required order.
static void split_block(buddy_zone_t *z, uint32_t order) {
    for (uint32_t o=order+1; o<=z->max_order; o++) {
        if (z->free_list[o]) {
            buddy_block_t *block = z->free_list[o];
            z->free_list[o] = block->next;

            uint32_t block_frame = addr_to_frame(z, (uint64_t)block);
            uint32_t split_frame = block_frame + (1U << (o-1));
            buddy_block_t *buddy = (buddy_block_t*)frame_to_addr(z, split_frame);

            block->next = NULL;
            buddy->next = NULL;
            z->free_list[o-1] = block;
            block->next = buddy;

            return;
        }
    }
}

// Allocates a block of order N (2^N pages), NUMA-aware, with fallback.
void *buddy_alloc(uint32_t order, int preferred_node, int strict) {
    for (int tries = 0; tries < (strict ? 1 : zone_count); ++tries) {
        int node = (preferred_node + tries) % zone_count;
        buddy_zone_t *z = &zones[node];

        spin_lock(&z->lock);
        for (uint32_t o=order; o<=z->max_order; ++o) {
            if (z->free_list[o]) {
                // Split down if necessary
                while (o > order)
                    split_block(z, o-1);
                buddy_block_t *block = z->free_list[order];
                if (!block) break; // Defensive (should not happen)
                z->free_list[order] = block->next;

                uint32_t f = addr_to_frame(z, (uint64_t)block);
                for (uint32_t i = 0; i < (1U << order); ++i)
                    BIT_SET(z->bitmap, f+i);
                z->free_frames -= (1U << order);

                spin_unlock(&z->lock);
                return block;
            }
        }
        spin_unlock(&z->lock);
        if (strict) break;
    }
    return NULL; // No memory!
}

// Merge freed block with buddy if possible
static void try_merge(buddy_zone_t *z, uint32_t frame, uint32_t order) {
    if (order >= z->max_order) {
        // Top level, can't merge
        buddy_block_t *blk = (buddy_block_t*)frame_to_addr(z, frame);
        blk->next = z->free_list[order];
        z->free_list[order] = blk;
        return;
    }
    uint32_t buddy_frame = frame ^ (1U << order);
    int buddy_free = 1;
    for (uint32_t i=0; i<(1U<<order); ++i)
        if (BIT_TEST(z->bitmap, buddy_frame+i)) { buddy_free = 0; break; }
    if (buddy_free) {
        // Remove buddy from list
        buddy_block_t **prev = &z->free_list[order];
        while (*prev) {
            if ((uint64_t)*prev == frame_to_addr(z, buddy_frame)) {
                *prev = (*prev)->next;
                break;
            }
            prev = &(*prev)->next;
        }
        // Merge upward
        uint32_t merged_frame = frame & buddy_frame;
        try_merge(z, merged_frame, order+1);
    } else {
        // Just insert this block
        buddy_block_t *blk = (buddy_block_t*)frame_to_addr(z, frame);
        blk->next = z->free_list[order];
        z->free_list[order] = blk;
    }
}

void buddy_free(void *addr, uint32_t order, int node) {
    buddy_zone_t *z = &zones[node];
    spin_lock(&z->lock);
    uint32_t f = addr_to_frame(z, (uint64_t)addr);
    for (uint32_t i=0; i<(1U<<order); ++i)
        BIT_CLEAR(z->bitmap, f+i);
    z->free_frames += (1U<<order);
    try_merge(z, f, order);
    spin_unlock(&z->lock);
}

// Migration: move page from one NUMA node to another.
int buddy_migrate(void *addr, int from_node, int to_node) {
    // Just free on old node, alloc on new node, and copy
    buddy_free(addr, 0, from_node);
    void *new_addr = buddy_alloc(0, to_node, 1);
    if (!new_addr) return 0;
    memcpy(new_addr, addr, PAGE_SIZE);
    return 1;
}

// Return total free frames across all nodes
uint64_t buddy_free_frames_total(void) {
    uint64_t total = 0;
    for (int n=0; n<zone_count; ++n)
        total += zones[n].free_frames;
    return total;
}
uint64_t buddy_free_frames_node(int node) {
    return zones[node].free_frames;
}
uint64_t buddy_zone_base(int node) {
    return zones[node].base;
}

// ========== Debug / Info ==========

void buddy_debug_print(void) {
    serial_puts("[buddy] Zone summary:\n");
    for (int n=0; n<zone_count; n++) {
        char buf[64];
        snprintf(buf, sizeof(buf),
                 "  node=%d base=0x%llx len=0x%llx frames=%llu free=%llu\n",
                 n, (unsigned long long)zones[n].base,
                 (unsigned long long)zones[n].length,
                 (unsigned long long)zones[n].frames,
                 (unsigned long long)zones[n].free_frames);
        serial_puts(buf);
    }
}

// ========== Initialization ==========
void buddy_init(const bootinfo_t *bootinfo) {
    zone_count = numa_node_count();
    for (int n=0; n<zone_count; n++) {
        const numa_region_t *r = numa_node_region(n);
        buddy_zone_t *z = &zones[n];
        z->base = r->base;
        z->length = r->length;
        z->frames = (z->length / PAGE_SIZE);
        z->max_order = PMM_BUDDY_MAX_ORDER;
        z->free_frames = z->frames;
        z->lock = 0;

        size_t bm_bytes = BITMAP_SIZE(z->frames);
        z->bitmap = calloc(bm_bytes, 1);

        // Mark all as free
        for (uint32_t o = 0; o <= z->max_order; ++o)
            z->free_list[o] = NULL;
        // Insert the entire region as a single large free block at max_order
        uint32_t frames_this_order = (1U << z->max_order);
        uint32_t frame = 0;
        while (frame + frames_this_order <= z->frames) {
            buddy_block_t *blk = (buddy_block_t*)frame_to_addr(z, frame);
            blk->next = z->free_list[z->max_order];
            z->free_list[z->max_order] = blk;
            frame += frames_this_order;
        }
        // Any leftover pages: break down to smaller blocks
        for (; frame < z->frames; ++frame) {
            buddy_block_t *blk = (buddy_block_t*)frame_to_addr(z, frame);
            blk->next = z->free_list[0];
            z->free_list[0] = blk;
        }
    }
}

