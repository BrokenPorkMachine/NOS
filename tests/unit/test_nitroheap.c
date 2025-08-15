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

    // Cross-CPU free should return to home CPU
    void* hold2 = nitro_kmalloc(32, 8); // keep span alive
    void* x = nitro_kmalloc(32, 8);     // allocate on CPU 0
    smp_stub_set_cpu_index(1);
    nitro_kfree(x);                     // free on CPU 1
    smp_stub_set_cpu_index(0);
    void* y = nitro_kmalloc(32, 8);     // CPU 0 should reclaim
    assert(y == x);
    nitro_kfree(y);
    nitro_kfree(hold2);

    // Test large allocation caching and realloc path
    void* p = nitro_kmalloc(20000, 8); // larger than any size class
    assert(p);
    assert(buddy_allocs == 1);
    nitro_kfree(p);
    assert(buddy_allocs == 1);
    void* q = nitro_kmalloc(20000, 8);
    assert(q == p);
    assert(buddy_allocs == 1);
    void* r = nitro_krealloc(q, 40000, 8);
    assert(r);
    assert(buddy_allocs == 2);
    nitro_kfree(r);

    nitro_kheap_trim();
    assert(buddy_allocs == 0);
    printf("nitroheap unit tests passed\n");
    return 0;
}

