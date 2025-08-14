#include "paging_adv.h"
#include <stddef.h>
#include "../../user/libc/libc.h"
#include <stdint.h>
#include "pmm_buddy.h"
#include "numa.h"

// Simple spinlock for SMP safety
static volatile int page_lock = 0;
#define PAGING_LOCK()   while(__sync_lock_test_and_set(&page_lock,1)){}
#define PAGING_UNLOCK() __sync_lock_release(&page_lock)

// Static page tables (identity map first 4GB as in legacy paging)
// Export the top-level PML4 so the VMM can share kernel mappings with
// per-task page tables.
// Static kernel page tables (identity map first 4GB as in legacy paging)
static uint64_t __attribute__((aligned(PAGE_SIZE))) kernel_pml4[512];
static uint64_t __attribute__((aligned(PAGE_SIZE), unused)) pdpt[512];
static uint64_t __attribute__((aligned(PAGE_SIZE), unused)) pd[4][512];

// Pointer to the page table of the currently running task.  By default we
// operate on the kernel's bootstrap page table until a task switches in a
// private PML4.
static uint64_t *current_pml4 = kernel_pml4;

static uint64_t *alloc_table(int numa_node) {
    void *page = buddy_alloc(0, numa_node, 0);
    if (!page)
        return NULL;
    memset(page, 0, PAGE_SIZE);
    return (uint64_t *)page;
}

static uint64_t *get_or_create(uint64_t *table, uint64_t index, uint64_t flags, int numa_node) {
    if (!(table[index] & PAGE_PRESENT)) {
        uint64_t *new = alloc_table(numa_node);
        if (!new) return NULL;
        table[index] = ((uint64_t)new) | flags | PAGE_PRESENT | PAGE_WRITABLE;
    }
    return (uint64_t *)(table[index] & ~0xFFFULL);
}

// Map (virt->phys) using huge or normal page, NUMA-aware
void paging_map_adv(uint64_t virt, uint64_t phys, uint64_t flags, uint32_t order, int numa_node) {
    PAGING_LOCK();
    uint64_t pml4_i = (virt >> 39) & 0x1FF;
    uint64_t pdpt_i = (virt >> 30) & 0x1FF;
    uint64_t pd_i   = (virt >> 21) & 0x1FF;
    uint64_t pt_i   = (virt >> 12) & 0x1FF;

    uint64_t *pdpt_t = get_or_create(current_pml4, pml4_i, PAGE_USER, numa_node);
    if (!pdpt_t) goto out;
    uint64_t *pd_t = get_or_create(pdpt_t, pdpt_i, PAGE_USER, numa_node);
    if (!pd_t) goto out;

    if (order >= 9 || (flags & PAGE_HUGE_2MB)) {
        pd_t[pd_i] = (phys & ~0x1FFFFFULL) | (flags & ~PAGE_HUGE_2MB) |
                     PAGE_PRESENT | PAGE_WRITABLE | PAGE_SIZE_2MB;
        goto done;
    }

    uint64_t *pt_t = get_or_create(pd_t, pd_i, PAGE_USER, numa_node);
    if (!pt_t) goto out;
    pt_t[pt_i] = (phys & ~0xFFFULL) | flags | PAGE_PRESENT;
    goto done;

out:
    // failed allocation, nothing mapped
done:
    
    PAGING_UNLOCK();
}

void paging_unmap_adv(uint64_t virt) {
    PAGING_LOCK();
    uint64_t pml4_i = (virt >> 39) & 0x1FF;
    uint64_t pdpt_i = (virt >> 30) & 0x1FF;
    uint64_t pd_i   = (virt >> 21) & 0x1FF;
    uint64_t pt_i   = (virt >> 12) & 0x1FF;

    if (!(current_pml4[pml4_i] & PAGE_PRESENT)) goto out;
    uint64_t *pdpt_t = (uint64_t *)(current_pml4[pml4_i] & ~0xFFFULL);
    if (!(pdpt_t[pdpt_i] & PAGE_PRESENT)) goto out;
    uint64_t *pd_t = (uint64_t *)(pdpt_t[pdpt_i] & ~0xFFFULL);
    if (!(pd_t[pd_i] & PAGE_PRESENT)) goto out;

    if (pd_t[pd_i] & PAGE_SIZE_2MB) {
        pd_t[pd_i] = 0;
        goto done;
    }

    uint64_t *pt_t = (uint64_t *)(pd_t[pd_i] & ~0xFFFULL);
    pt_t[pt_i] = 0;

done:
out:
    PAGING_UNLOCK();
}

uint64_t paging_virt_to_phys_adv(uint64_t virt) {
    PAGING_LOCK();
    uint64_t pml4_i = (virt >> 39) & 0x1FF;
    uint64_t pdpt_i = (virt >> 30) & 0x1FF;
    uint64_t pd_i   = (virt >> 21) & 0x1FF;
    uint64_t pt_i   = (virt >> 12) & 0x1FF;

  
  if (!(current_pml4[pml4_i] & PAGE_PRESENT)) { PAGING_UNLOCK(); return 0; }
    uint64_t *pdpt_t = (uint64_t *)(current_pml4[pml4_i] & ~0xFFFULL);
  if (!(pdpt_t[pdpt_i] & PAGE_PRESENT)) { PAGING_UNLOCK(); return 0; }
    uint64_t *pd_t = (uint64_t *)(pdpt_t[pdpt_i] & ~0xFFFULL);
    if (!(pd_t[pd_i] & PAGE_PRESENT)) { PAGING_UNLOCK(); return 0; }

    if (pd_t[pd_i] & PAGE_SIZE_2MB) {
        uint64_t phys = (pd_t[pd_i] & ~0x1FFFFFULL) | (virt & 0x1FFFFFULL);
        PAGING_UNLOCK();
        return phys;
    }

    uint64_t *pt_t = (uint64_t *)(pd_t[pd_i] & ~0xFFFULL);
    if (!(pt_t[pt_i] & PAGE_PRESENT)) { PAGING_UNLOCK(); return 0; }
    uint64_t phys = (pt_t[pt_i] & ~0xFFFULL) | (virt & 0xFFFULL);
    PAGING_UNLOCK();
    return phys;
}

/* Lookup mapping for virt: returns 1 if mapped and provides phys+flags. */
int paging_lookup_adv(uint64_t virt, uint64_t *phys, uint64_t *flags) {
    int ret = 0;
    PAGING_LOCK();
    uint64_t pml4_i = (virt >> 39) & 0x1FF;
    uint64_t pdpt_i = (virt >> 30) & 0x1FF;
    uint64_t pd_i   = (virt >> 21) & 0x1FF;
    uint64_t pt_i   = (virt >> 12) & 0x1FF;

    if (!(current_pml4[pml4_i] & PAGE_PRESENT)) goto out;
    uint64_t *pdpt_t = (uint64_t *)(current_pml4[pml4_i] & ~0xFFFULL);
    if (!(pdpt_t[pdpt_i] & PAGE_PRESENT)) goto out;
    uint64_t *pd_t = (uint64_t *)(pdpt_t[pdpt_i] & ~0xFFFULL);
    if (!(pd_t[pd_i] & PAGE_PRESENT)) goto out;

    if (pd_t[pd_i] & PAGE_SIZE_2MB) {
        if (phys) *phys = (pd_t[pd_i] & ~0x1FFFFFULL) | (virt & 0x1FFFFFULL);
        if (flags) *flags = pd_t[pd_i];
        ret = 1;
        goto out;
    }

    uint64_t *pt_t = (uint64_t *)(pd_t[pd_i] & ~0xFFFULL);
    if (!(pt_t[pt_i] & PAGE_PRESENT)) goto out;
    if (phys) *phys = (pt_t[pt_i] & ~0xFFFULL) | (virt & 0xFFFULL);
    if (flags) *flags = pt_t[pt_i];
    ret = 1;

out:
    PAGING_UNLOCK();
    return ret;
}

// Allocate a new PML4 for a task, cloning the kernel's higher-half mappings.
uint64_t *paging_new_context(void) {
    uint64_t *pml4 = alloc_table(current_cpu_node());
    if (!pml4)
        return NULL;
    /* The kernel is currently identity-mapped in the lower half of the
       address space.  Copy the entire bootstrap PML4 so new contexts
       retain mappings for kernel code, data, and stacks. */
    memcpy(pml4, kernel_pml4, 512 * sizeof(uint64_t));
    return pml4;
}

// Switch to a new page table and update the active pointer. Reloading CR3
// implicitly flushes the TLB, so we avoid per-page shootdowns.
void paging_switch(uint64_t *new_pml4) {
    if (!new_pml4)
        new_pml4 = kernel_pml4;
    current_pml4 = new_pml4;
    asm volatile("mov %0, %%cr3" :: "r"(new_pml4) : "memory");
}

// Expose the kernel's bootstrap PML4 for threads that run in kernel space.
uint64_t *paging_kernel_pml4(void) {
    return kernel_pml4;
}


