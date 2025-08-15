#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include "../../include/nitroheap_shim.h"
#include "../../kernel/VM/nitroheap/nitroheap.h"

void smp_stub_set_cpu_index(uint32_t idx);

int main(void) {
    smp_stub_set_cpu_index(0);
    nitroheap_init();

    nh_handle_t h = halloc(64, NH_PRESET_BALANCED);
    assert(h != 0);
    void* p = hptr(h);
    assert(p);
    memset(p, 0x5A, 64);
    assert(hfree(h) == 0);

    printf("nh handle tests passed\n");
    return 0;
}
