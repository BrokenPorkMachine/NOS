#include <assert.h>
#include <string.h>
#include <stdio.h>
#include "../../kernel/VM/nitroheap/nitroheap.h"

extern int legacy_allocs;

int main(void) {
    nitroheap_init();

    void* ptrs[64];
    for (int i = 0; i < 64; ++i) {
        ptrs[i] = nitro_kmalloc((i + 1) * 8, 8);
        assert(ptrs[i]);
        memset(ptrs[i], 0xA5, (i + 1) * 8);
    }
    for (int i = 0; i < 64; ++i)
        nitro_kfree(ptrs[i]);

    // Test realloc and large allocation fallback
    void* p = nitro_kmalloc(5000, 8); // larger than any size class
    assert(p);
    p = nitro_krealloc(p, 8000, 8);
    assert(p);
    nitro_kfree(p);

    nitro_kheap_trim();
    assert(legacy_allocs == 0);
    printf("nitroheap unit tests passed\n");
    return 0;
}

