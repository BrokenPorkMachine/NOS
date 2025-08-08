/*
 * Buddy Physical Memory Manager
 * -----------------------------
 * Provides NUMA-aware, SMP-safe physical memory allocation using a buddy
 * allocator.  This replaces the older bitmap-based PMM and exposes a more
 * flexible API capable of highmem and multi-node support.
 */

#pragma once
#include <stdint.h>
#include "bootinfo.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef PAGE_SIZE
#define PAGE_SIZE 4096
#endif

// Buddy allocator constants
#define PMM_BUDDY_MIN_ORDER  0               // 4KiB
#define PMM_BUDDY_MAX_ORDER  16              // 256MiB blocks (tunable)
#define PMM_BUDDY_ORDERS     (PMM_BUDDY_MAX_ORDER+1)

#define MAX_NUMA_ZONES       8               // match your NUMA node maximum

typedef struct {
    uint64_t base;
    uint64_t length;
} buddy_zone_info_t;

void buddy_init(const bootinfo_t *bootinfo);

// Allocates 2^order * PAGE_SIZE bytes from NUMA node.
// Will fallback to any node if preferred node is full, unless `strict` is true.
void *buddy_alloc(uint32_t order, int preferred_node, int strict);

// Free a block of 2^order * PAGE_SIZE previously allocated from `buddy_alloc`.
void buddy_free(void *addr, uint32_t order, int node);

// High-memory support: get zone/node by physical address.
int buddy_find_zone(uint64_t phys);

// Runtime NUMA: migrate a page between zones.
int buddy_migrate(void *addr, int from_node, int to_node);

uint64_t buddy_free_frames_total(void);
uint64_t buddy_free_frames_node(int node);
uint64_t buddy_zone_base(int node);

void buddy_debug_print(void);

#ifdef __cplusplus
}
#endif
