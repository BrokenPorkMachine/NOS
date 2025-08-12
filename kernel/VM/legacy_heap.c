#include "legacy_heap.h"
#include "pmm_buddy.h"
#include "numa.h"
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#ifndef PMM_PAGE_SHIFT
#define PMM_PAGE_SHIFT 12
#endif

#define HEAP_MIN_ORDER   0 // 4K
#define HEAP_MAX_ORDER   PMM_BUDDY_MAX_ORDER

// Simple heap: prefix each allocation with its buddy order for kfree.
void *legacy_kmalloc(size_t sz) {
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

void legacy_kfree(void *ptr) {
    if (!ptr)
        return;
    uint8_t *block = (uint8_t*)ptr - sizeof(uint32_t);
    uint32_t order = *((uint32_t*)block);
    buddy_free(block, order, current_cpu_node());
}

static size_t legacy_alloc_size(const void *ptr) {
    const uint8_t *block = (const uint8_t*)ptr - sizeof(uint32_t);
    uint32_t order = *((const uint32_t*)block);
    return ((size_t)1 << (order + PMM_PAGE_SHIFT)) - sizeof(uint32_t);
}

void *legacy_krealloc(void *ptr, size_t newsz) {
    if (!ptr)
        return legacy_kmalloc(newsz);
    size_t oldsz = legacy_alloc_size(ptr);
    if (newsz <= oldsz)
        return ptr;
    void *n = legacy_kmalloc(newsz);
    if (!n)
        return NULL;
    memcpy(n, ptr, oldsz < newsz ? oldsz : newsz);
    legacy_kfree(ptr);
    return n;
}

void legacy_kheap_init(void) {
    // Nothing to do if using buddy directly; for a slab/quicklist heap, initialize here
}
