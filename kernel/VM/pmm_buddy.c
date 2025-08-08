#include "pmm_buddy.h"
#include "numa.h"
#include <stddef.h>

typedef struct buddy_block {
    struct buddy_block *next;
} buddy_block_t;

typedef struct {
    buddy_block_t *free_list[PMM_BUDDY_MAX_ORDER + 1];
    uint64_t base, length;
} buddy_zone_t;

static buddy_zone_t numa_zones[MAX_NUMA_NODES];

static uint32_t addr_to_frame(buddy_zone_t *zone, uint64_t addr) {
    return (addr - zone->base) / PAGE_SIZE;
}

static void *frame_to_addr(buddy_zone_t *zone, uint32_t frame) {
    return (void *)(zone->base + (frame * PAGE_SIZE));
}

void buddy_init(const bootinfo_t *bootinfo) {
    // For each NUMA node
    for (int n = 0; n < numa_node_count(); n++) {
        const numa_region_t *r = numa_node_region(n);
        buddy_zone_t *z = &numa_zones[n];
        z->base = r->base;
        z->length = r->length;
        // TODO: Initialize all pages as free, set up buddy blocks at max possible order...
        // ... You can do the splitting here or as you first allocate
        for (int o = 0; o <= PMM_BUDDY_MAX_ORDER; o++)
            z->free_list[o] = NULL;
        // Insert initial big block(s) in z->free_list[max_order]
    }
}

void *buddy_alloc(uint32_t order, int numa_node) {
    // Find or split a block in numa_zones[numa_node]->free_list
    // Fallback: try other NUMA zones if needed
    // Omitted: code for block splitting/merging for brevity
    return NULL; // Implement allocation logic
}

void buddy_free(void *addr, uint32_t order, int numa_node) {
    // Coalesce with buddy if possible, insert into free list
    // Omitted: code for brevity
}
