#include "kheap.h"
#include "pmm_buddy.h"
#include "numa.h"
#include <stdint.h>
#include <stddef.h>

#define HEAP_MIN_ORDER   0 // 4K
#define HEAP_MAX_ORDER   PMM_BUDDY_MAX_ORDER

// Simple heap: prefix each allocation with its buddy order for kfree.
void *kalloc(size_t sz) {
    uint32_t order = 0;
    size_t x = PAGE_SIZE;
    size_t total = sz + sizeof(uint32_t);
    while (x < total) { x <<= 1; order++; }

    int node = current_cpu_node();
    uint8_t *block = buddy_alloc(order, node, 0);
    if (!block)
        return NULL;
    *((uint32_t*)block) = order;
    return block + sizeof(uint32_t);
}

void kfree(void *ptr) {
    if (!ptr)
        return;
    uint8_t *block = (uint8_t*)ptr - sizeof(uint32_t);
    uint32_t order = *((uint32_t*)block);
    buddy_free(block, order, current_cpu_node());
}

void kheap_init(void) {
    // Nothing to do if using buddy directly; for a slab/quicklist heap, initialize here
}
