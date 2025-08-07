#include "sharedmem.h"
#include "../../user/libc/libc.h"
#include "../VM/cow.h"

/**
 * Create a shared memory region for IPC.
 * Supports arbitrary size (multi-page).
 * All pages are zeroed.
 * Reference counts are incremented for all pages.
 */
int ipc_shared_create(ipc_shared_mem_t *mem, size_t size, uint32_t send_mask, uint32_t recv_mask) {
    if (!mem || size == 0) return -1;
    size_t aligned = PAGE_ALIGN_UP(size);
    uint32_t pages = aligned / PAGE_SIZE;
    void *base = alloc_pages(pages); // must provide this function in VM/cow
    if (!base) return -1;
    memset(base, 0, pages * PAGE_SIZE);

    mem->addr = base;
    mem->size = aligned;
    mem->pages = pages;
    mem->rights_send = send_mask;
    mem->rights_recv = recv_mask;

    // Increment refcount for each page (for COW/refcounted VM)
    for (uint32_t i = 0; i < pages; ++i)
        cow_inc_ref((uint64_t)base + i * PAGE_SIZE);

    return 0;
}

/**
 * Destroy a shared memory region, decrements refcounts, frees pages, clears struct.
 */
void ipc_shared_destroy(ipc_shared_mem_t *mem) {
    if (!mem || !mem->addr || mem->size == 0) return;
    for (uint32_t i = 0; i < mem->pages; ++i)
        cow_dec_ref((uint64_t)mem->addr + i * PAGE_SIZE);
    free_pages(mem->addr, mem->pages); // must provide this in VM/cow
    mem->addr = NULL;
    mem->size = 0;
    mem->pages = 0;
    mem->rights_send = 0;
    mem->rights_recv = 0;
}

/**
 * Map the shared memory into the caller's address space.
 * For simple systems, returns the pointer. For advanced kernels, could
 * remap per-process. (Here: just return VA.)
 */
void *ipc_shared_map(ipc_shared_mem_t *mem) {
    if (!mem || !mem->addr) return NULL;
    return mem->addr;
}
