#pragma once
#include <stdint.h>

#ifndef PAGE_SIZE
#define PAGE_SIZE 4096
#endif

/**
 * Initialize the COW system with the total number of page frames.
 * Must be called before any COW/demand paging logic is used.
 */
void cow_init(uint64_t total_frames);

/**
 * Increment reference count for the given physical frame.
 */
void cow_inc_ref(uint64_t phys);

/**
 * Decrement reference count for the given physical frame.
 */
void cow_dec_ref(uint64_t phys);

/**
 * Get reference count for a physical frame.
 */
uint16_t cow_refcount(uint64_t phys);

/**
 * Mark a virtual address as COW (read-only, with COW flag set).
 */
void cow_mark(uint64_t virt);

/**
 * Remove COW marking from a virtual address (restores writable).
 */
void cow_unmark(uint64_t virt);

/**
 * Return 1 if the virtual address is marked COW, 0 otherwise.
 */
int cow_is_marked(uint64_t virt);

// Allocate and free multiple contiguous pages
void *alloc_pages(uint32_t pages);
void free_pages(void *addr, uint32_t pages);

/**
 * (Optional) Called by frame allocator to free a page only if fully unreferenced.
 * Returns 1 if the frame was actually freed, 0 if still referenced.
 */
int cow_free_frame(uint64_t phys);

/**
 * Handle a page fault with the given error code and faulting address.
 * Uses the advanced paging subsystem to allocate or copy pages as needed.
 */
void paging_handle_fault(uint64_t err, uint64_t addr, int cpu_id);
