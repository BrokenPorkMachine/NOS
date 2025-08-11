#include "numa.h"
#include "../../user/libc/libc.h"
#include "../arch/CPU/smp.h"

static numa_region_t nodes[MAX_NUMA_NODES];
static int node_cnt = 0;

void numa_init(const bootinfo_t *bootinfo) {
    node_cnt = 0;

    /*
     * The firmware memory map can contain dozens of small regions marked as
     * EfiConventionalMemory (type 7).  The previous logic recorded at most the
     * first MAX_NUMA_NODES such regions which, on some systems, were tiny
     * fragments below 1MB.  As a result the buddy allocator only managed a
     * handful of pages and early kalloc() calls failed, leading to boot-time
     * crashes when loading agents.
     *
     * Instead, scan the entire map and keep the single largest usable region
     * so the buddy allocator has a contiguous arena big enough for kernel
     * allocations.  Proper NUMA support can add more regions later.
     */
    uint64_t best_len = 0, best_base = 0;
    for (uint32_t i = 0; i < bootinfo->mmap_entries; ++i) {
        if (bootinfo->mmap[i].type != 7)
            continue; /* only EfiConventionalMemory */
        if (bootinfo->mmap[i].len > best_len) {
            best_len = bootinfo->mmap[i].len;
            best_base = bootinfo->mmap[i].addr;
        }
    }

    if (best_len) {
        nodes[0].base = best_base;
        nodes[0].length = best_len;
        node_cnt = 1;
    } else {
        /* Fallback: treat first entry as a single region. */
        nodes[0].base = 0;
        nodes[0].length = bootinfo->mmap[0].len;
        node_cnt = 1;
    }
}

int numa_node_count(void) {
    return node_cnt;
}

const numa_region_t *numa_node_region(int node) {
    if (node < 0 || node >= node_cnt)
        return NULL;
    return &nodes[node];
}

// Return the NUMA node for the current CPU.  If no topology information is
// available, fall back to a simple modulo distribution across detected nodes.
int current_cpu_node(void) {
    if (node_cnt <= 1)
        return 0;
    uint32_t cpu = smp_cpu_id();
    return cpu % node_cnt;
}
