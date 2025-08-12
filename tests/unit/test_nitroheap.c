#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include "../../kernel/VM/nitroheap/nitroheap.h"

extern int buddy_allocs;
void smp_stub_set_cpu_index(uint32_t idx);

int main(void) {
    smp_stub_set_cpu_index(0);
    nitroheap_init();

    void* ptrs[64];
    for (int i = 0; i < 64; ++i) {
        ptrs[i] = nitro_kmalloc((i + 1) * 8, 8);
        assert(ptrs[i]);
        memset(ptrs[i], 0xA5, (i + 1) * 8);
    }
    for (int i = 0; i < 64; ++i)
        nitro_kfree(ptrs[i]);

    // Per-CPU magazine behavior
    void* hold = nitro_kmalloc(32, 8); // keep span alive
    void* a = nitro_kmalloc(32, 8);
    nitro_kfree(a); // stays in CPU 0 magazine
    smp_stub_set_cpu_index(1);
    void* b = nitro_kmalloc(32, 8); // different CPU should not reuse
    assert(b != a);
    nitro_kfree(b);
    smp_stub_set_cpu_index(0);
    void* c = nitro_kmalloc(32, 8); // original CPU reuses
    assert(c == a);
    nitro_kfree(c);
    nitro_kfree(hold);

    // Test realloc and large allocation path
    void* p = nitro_kmalloc(5000, 8); // larger than any size class
    assert(p);
    p = nitro_krealloc(p, 8000, 8);
    assert(p);
    nitro_kfree(p);

    nitro_kheap_trim();
    assert(buddy_allocs == 0);
    printf("nitroheap unit tests passed\n");
    return 0;
}

