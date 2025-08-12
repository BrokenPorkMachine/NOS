#include "nitroheap.h"
#include "../legacy_heap.h"

// Temporary stub implementation: delegate to legacy heap.
void nitroheap_init(void) {}

void* nitro_kmalloc(size_t sz, size_t align) {
    (void)align;
    return legacy_kmalloc(sz);
}

void nitro_kfree(void* p) {
    legacy_kfree(p);
}

void* nitro_krealloc(void* p, size_t newsz, size_t align) {
    (void)align;
    return legacy_krealloc(p, newsz);
}

void nitro_kheap_dump_stats(const char* tag) {
    (void)tag;
}

void nitro_kheap_trim(void) {}
