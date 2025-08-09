#pragma once
#include <stdint.h>
#include "../../boot/include/bootinfo.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MAX_NUMA_NODES 8

typedef struct {
    uint64_t base;
    uint64_t length;
} numa_region_t;

void numa_init(const bootinfo_t *bootinfo);
int  numa_node_count(void);
const numa_region_t *numa_node_region(int node);
// Best-effort NUMA node for the executing CPU.
int  current_cpu_node(void);

#ifdef __cplusplus
}
#endif
