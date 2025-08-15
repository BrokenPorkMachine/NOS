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

    void* hold64 = mallocx(64, NH_PRESET_BALANCED);
    void* p = mallocx(64, NH_PRESET_BALANCED);
    assert(hold64 && p);
    ret = sys_heapctl(NH_HEAPCTL_GET_STATS, &args, sizeof(args));
    assert(ret == 0);
    assert(stats.bytes_inuse >= 128);
    assert(stats.allocs >= 2);

    dallocx(p, 0);
    ret = sys_heapctl(NH_HEAPCTL_GET_STATS, &args, sizeof(args));
    assert(ret == 0);
    assert(stats.bytes_inuse >= 64);
    assert(stats.frees >= 1);
    assert(stats.quarantine_backlog >= 1);
    assert(stats.remote_free_backlog == 0);

    void* hold32 = mallocx(32, NH_PRESET_BALANCED);
    void* x = mallocx(32, NH_PRESET_BALANCED);
    assert(hold32 && x);
    smp_stub_set_cpu_index(1);
    dallocx(x, 0); // cross-CPU free
    smp_stub_set_cpu_index(0);
    ret = sys_heapctl(NH_HEAPCTL_GET_STATS, &args, sizeof(args));
    assert(ret == 0);
    assert(stats.bytes_inuse >= 96);
    assert(stats.remote_free_backlog >= 1);

    dallocx(hold32, 0);
    dallocx(hold64, 0);

    printf("nh stats tests passed\n");
    return 0;
}
