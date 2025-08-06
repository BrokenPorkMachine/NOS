#ifndef SHAREDMEM_H
#define SHAREDMEM_H

#include <stdint.h>

#define PAGE_SIZE 4096

/**
 * IPC shared memory descriptor.
 * - addr: physical or kernel VA pointer to the start of the shared region.
 * - size: size in bytes (always a multiple of PAGE_SIZE).
 * - pages: number of pages allocated.
 * - rights_send/rights_recv: task masks for who can send/receive via this region.
 */
typedef struct {
    void    *addr;
    uint32_t size;
    uint32_t pages;
    uint32_t rights_send;
    uint32_t rights_recv;
} ipc_shared_mem_t;

/**
 * Create a new shared memory region of the given size (in bytes).
 * Returns 0 on success, <0 on error.
 * Access is limited by send_mask/recv_mask bitfields.
 */
int ipc_shared_create(ipc_shared_mem_t *mem, uint32_t size, uint32_t send_mask, uint32_t recv_mask);

/**
 * Destroy a shared memory region, releasing all physical pages and rights.
 */
void ipc_shared_destroy(ipc_shared_mem_t *mem);

/**
 * Map a shared memory region into the caller's address space.
 * (For now, just returns pointer; for true per-process mapping, extend here.)
 */
void *ipc_shared_map(ipc_shared_mem_t *mem);

#endif // SHAREDMEM_H
