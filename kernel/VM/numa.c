#include "numa.h"
#include "../../user/libc/libc.h"
#include "../arch/CPU/smp.h"

static numa_region_t nodes[MAX_NUMA_NODES];
static int node_cnt = 0;

void numa_init(const bootinfo_t *bootinfo) {
    node_cnt = 0;
    for (uint32_t i = 0; i < bootinfo->mmap_entries && node_cnt < MAX_NUMA_NODES; ++i) {
        if (bootinfo->mmap[i].type != 7)
            continue;
        nodes[node_cnt].base = bootinfo->mmap[i].addr;
        nodes[node_cnt].length = bootinfo->mmap[i].len;
        node_cnt++;
    }
    if (node_cnt == 0) {
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
