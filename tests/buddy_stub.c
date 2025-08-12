#include <stdlib.h>
#include <stdint.h>

int buddy_allocs = 0;

void* buddy_alloc(uint32_t order, int preferred_node, int strict) {
    size_t bytes = ((size_t)1 << order) * 4096;
    void* p = malloc(bytes);
    if (p) buddy_allocs++;
    return p;
}

void buddy_free(void* addr, uint32_t order, int node) {
    if (addr) {
        buddy_allocs--;
        free(addr);
    }
}
