#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include "../../include/nitroheap_shim.h"
#include "../../kernel/VM/nitroheap/nitroheap.h"

extern int buddy_allocs;
void smp_stub_set_cpu_index(uint32_t idx);

int main(void) {
    smp_stub_set_cpu_index(0);
    nitroheap_init();

    void* p = mallocx(64, NH_PRESET_BALANCED);
    assert(p);
    memset(p, 0x5A, 64);
    dallocx(p, 0);

    void* q = mallocx(32, NH_PRESET_BALANCED);
    assert(q);
    memset(q, 0x5A, 32);
    q = rallocx(q, 64, NH_PRESET_BALANCED);
    assert(q);
    dallocx(q, 0);

    printf("nh syscall tests passed\n");
    return 0;
}
