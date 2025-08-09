#include "sharedmem.h"
#include "../../user/libc/libc.h"
#include "../VM/cow.h"
#include <stdint.h>

/* Optional: wipe the shared pages before freeing them (defense-in-depth). */
#ifndef SHM_SCRUB_ON_FREE
#define SHM_SCRUB_ON_FREE 1
#endif

/* If your rights bitmasks are per-task capability bits, you likely only care
   about IPC_MAX_TASKS low bits. If not exposed here, keep the masks as-is. */
#ifndef IPC_MAX_TASKS
#define IPC_MAX_TASKS 32
#endif

static inline uint32_t sanitize_rights(uint32_t m) {
    if (IPC_MAX_TASKS >= 32) return m;
    uint32_t mask = (IPC_MAX_TASKS == 0) ? 0u : ((1u << IPC_MAX_TASKS) - 1u);
    return m & mask;
}

int ipc_shared_create(ipc_shared_mem_t *mem, size_t size,
                      uint32_t send_mask, uint32_t recv_mask)
{
    if (!mem || size == 0) return -1;

    /* Align and compute page count with overflow checks. */
    size_t aligned = PAGE_ALIGN_UP(size);
    if (aligned < size) return -1; /* overflow */
    if (PAGE_SIZE == 0) return -1;

    uint64_t pages_u64 = (uint64_t)aligned / (uint64_t)PAGE_SIZE;
    if (pages_u64 == 0 || pages_u64 > UINT32_MAX) return -1;
    uint32_t pages = (uint32_t)pages_u64;

    /* Allocate pages */
    void *base = alloc_pages(pages);
    if (!base) return -1;

    /* Zero the region (even if allocator might already do it; harmless idempotence). */
    memset(base, 0, (size_t)pages * (size_t)PAGE_SIZE);

    /* Initialize descriptor early so destroy() can safely unwind on partial failure. */
    mem->addr         = base;
    mem->size         = aligned;
    mem->pages        = pages;
    mem->rights_send  = sanitize_rights(send_mask);
    mem->rights_recv  = sanitize_rights(recv_mask);

    /* Increase refcount per page; unwind if any inc fails (if your cow_* return an error).
       Current cow_inc_ref signature returns void, so we assume success. If you later
       change it to return int, add error handling and goto unwind. */
    for (uint32_t i = 0; i < pages; ++i) {
        cow_inc_ref((uint64_t)base + (uint64_t)i * (uint64_t)PAGE_SIZE);
    }

    return 0;

    /* Example unwind path if cow_inc_ref can fail in your VM:
    unwind:
        while (i--) cow_dec_ref((uint64_t)base + (uint64_t)i * PAGE_SIZE);
        free_pages(base, pages);
        mem->addr = NULL; mem->size = 0; mem->pages = 0;
        mem->rights_send = mem->rights_recv = 0;
        return -1;
    */
}

void ipc_shared_destroy(ipc_shared_mem_t *mem)
{
    if (!mem || !mem->addr || mem->pages == 0) return;

#if SHM_SCRUB_ON_FREE
    /* Best-effort scrub before dropping refs (prevents stale secrets if pages get reused quickly). */
    memset(mem->addr, 0, (size_t)mem->pages * (size_t)PAGE_SIZE);
#endif

    /* Drop refs per page, then free. */
    for (uint32_t i = 0; i < mem->pages; ++i) {
        cow_dec_ref((uint64_t)mem->addr + (uint64_t)i * (uint64_t)PAGE_SIZE);
    }

    free_pages(mem->addr, mem->pages);

    /* Poison the descriptor. */
    mem->addr = NULL;
    mem->size = 0;
    mem->pages = 0;
    mem->rights_send = 0;
    mem->rights_recv = 0;
}

/* For now, mapping is trivial and returns the kernel VA.
   If/when you have per-process address spaces, this function can perform
   a per-task map with proper permissions derived from rights_* masks. */
void *ipc_shared_map(ipc_shared_mem_t *mem)
{
    if (!mem || !mem->addr) return NULL;
    return mem->addr;
}
