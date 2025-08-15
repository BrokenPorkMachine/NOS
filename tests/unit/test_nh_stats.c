#include <assert.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "../../include/nitroheap_shim.h"
#include "../../include/nitroheap_sys.h"
#include "../../include/nitroheap_stats.h"
#include "../../kernel/VM/nitroheap/nitroheap.h"

void smp_stub_set_cpu_index(uint32_t idx);

int main(void) {
    smp_stub_set_cpu_index(0);
    nitroheap_init();

    nh_part_stats_summary stats;
    nh_heapctl_get_stats_args args = {
        .part_id = 0,
        .include_per_cpu = 0,
        .user_buf = &stats,
        .user_buf_len = sizeof(stats),
    };

    int ret = sys_heapctl(NH_HEAPCTL_GET_STATS, &args, sizeof(args));
    assert(ret == 0);
    assert(stats.bytes_inuse == 0);
    assert(stats.allocs == 0);
    assert(stats.frees == 0);

    void* p = mallocx(64, NH_PRESET_BALANCED);
    assert(p);
    ret = sys_heapctl(NH_HEAPCTL_GET_STATS, &args, sizeof(args));
    assert(ret == 0);
    assert(stats.bytes_inuse >= 64);
    assert(stats.allocs >= 1);

    dallocx(p, 0);
    ret = sys_heapctl(NH_HEAPCTL_GET_STATS, &args, sizeof(args));
    assert(ret == 0);
    assert(stats.bytes_inuse == 0);
    assert(stats.frees >= 1);

    printf("nh stats tests passed\n");
    return 0;
}
