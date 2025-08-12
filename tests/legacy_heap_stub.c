#include <stdlib.h>
#include <stddef.h>

int legacy_allocs = 0;

void* legacy_kmalloc(size_t sz) {
    legacy_allocs++;
    return malloc(sz);
}

void legacy_kfree(void* ptr) {
    if (ptr) {
        legacy_allocs--;
        free(ptr);
    }
}

void* legacy_krealloc(void* ptr, size_t newsz) {
    if (!ptr) {
        legacy_allocs++;
        return realloc(NULL, newsz);
    }
    if (!newsz) {
        free(ptr);
        legacy_allocs--;
        return NULL;
    }
    return realloc(ptr, newsz);
}

void legacy_kheap_init(void) {}

