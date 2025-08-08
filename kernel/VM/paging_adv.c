#include "paging_adv.h"
#include "pmm_buddy.h"
#include <stddef.h>
#include "../../user/libc/libc.h"
#include <stdint.h>

// Example: Lock for paging ops (simple global spinlock)
static volatile int page_lock = 0;
#define PAGING_LOCK()   while(__sync_lock_test_and_set(&page_lock,1)){}
#define PAGING_UNLOCK() __sync_lock_release(&page_lock)

// Map (virt->phys) using huge or normal page, NUMA-aware
void paging_map_adv(uint64_t virt, uint64_t phys, uint64_t flags, uint32_t order, int numa_node) {
    PAGING_LOCK();
    // You may call your existing paging_map or hugepage-aware variant here
    // If order=9, use 2MB page, if 0, normal, if 18, 1GB, etc.
    // The page table manipulation code will look similar to your paging.c, but allow hugepage entries.
    // SMP safe by design
    PAGING_UNLOCK();
}

// Unmap (and free underlying page if necessary)
void paging_unmap_adv(uint64_t virt) {
    PAGING_LOCK();
    // Unmap virt and, if allocated via buddy, free physical page/block
    PAGING_UNLOCK();
}

// Translate VA->PA (hugepage/normal support)
uint64_t paging_virt_to_phys_adv(uint64_t virt) {
    PAGING_LOCK();
    // Use your normal translation code, but aware of hugepage mapping
    PAGING_UNLOCK();
    return 0;
}

// Fault handler: lazy alloc, COW, etc.
void paging_handle_fault(uint64_t err, uint64_t addr, int cpu_id) {
    uint64_t virt = addr & ~(PAGE_SIZE - 1);
    // If not mapped, allocate from buddy (choose node by cpu_id/affinity)
    int numa_node = /* determine from cpu_id or current process */;
    void *phys = buddy_alloc(0, numa_node, 0);
    if (phys) {
        paging_map_adv(virt, (uint64_t)phys, PAGE_PRESENT|PAGE_WRITABLE|PAGE_USER, 0, numa_node);
        memset((void*)virt, 0, PAGE_SIZE);
    } else {
        // OOM - handle gracefully or panic
    }
}
