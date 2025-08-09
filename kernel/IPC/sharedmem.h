#pragma once
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* -------- Page geometry -------- */
#ifndef PAGE_SIZE
#define PAGE_SIZE 4096u
#endif

/* Compile-time sanity: PAGE_SIZE must be a power of two */
#if defined(__GNUC__) || defined(__clang__)
_Static_assert((PAGE_SIZE & (PAGE_SIZE - 1)) == 0, "PAGE_SIZE must be a power of two");
#endif

/* Align x up to PAGE_SIZE (power-of-two friendly) */
#define PAGE_ALIGN_UP(x)  ( ((size_t)(x) + (PAGE_SIZE - 1u)) & ~(size_t)(PAGE_SIZE - 1u) )

/* -------- IPC shared memory descriptor --------
 * addr:  kernel VA (or physical, if that's your convention) of first page
 * size:  total size in bytes (multiple of PAGE_SIZE)
 * pages: number of pages in the region
 * rights_send/rights_recv: task bitmasks authorizing which tasks may use this region
 */
typedef struct {
    void    *addr;
    size_t   size;
    uint32_t pages;
    uint32_t rights_send;
    uint32_t rights_recv;
} ipc_shared_mem_t;

/* Create a new shared memory region of 'size' bytes (rounded up to PAGE_SIZE).
 * On success returns 0 and fills *mem; on error returns <0 and leaves *mem untouched.
 */
int ipc_shared_create(ipc_shared_mem_t *mem, size_t size,
                      uint32_t send_mask, uint32_t recv_mask);

/* Destroy a region created with ipc_shared_create(): drops refcounts and frees pages. */
void ipc_shared_destroy(ipc_shared_mem_t *mem);

/* Map a shared region into the caller’s address space.
 * Current simple kernels may just return mem->addr (kernel VA).
 * Future per-process kernels can remap with appropriate protections here.
 */
void *ipc_shared_map(ipc_shared_mem_t *mem);

/* -------- Convenience rights checks -------- */
static inline int ipc_shared_can_send(const ipc_shared_mem_t *mem, uint32_t mask) {
    return mem && (mem->rights_send & mask) != 0;
}
static inline int ipc_shared_can_recv(const ipc_shared_mem_t *mem, uint32_t mask) {
    return mem && (mem->rights_recv & mask) != 0;
}

#ifdef __cplusplus
} /* extern "C" */
#endif
