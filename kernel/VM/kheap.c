#include "kheap.h"
#include "pmm_buddy.h"
#include <stdint.h>
#include <stddef.h>

#define HEAP_MIN_ORDER   0 // 4K
#define HEAP_MAX_ORDER   PMM_BUDDY_MAX_ORDER

// Very simple heap: every alloc is rounded up to next buddy block.
void *kalloc(size_t sz) {
    uint32_t order = 0;
    size_t x = PAGE_SIZE;
    while (x < sz) { x <<= 1; order++; }
    // Use node 0 for heap by default, can be made NUMA-aware
    return buddy_alloc(order, 0, 0);
}
void kfree(void *ptr) {
    (void)ptr;
    // This requires tracking the order for each allocation (can use shadow table)
    // For simplicity, not shown here
}
void kheap_init(void) {
    // Nothing to do if using buddy directly; for a slab/quicklist heap, initialize here
}
