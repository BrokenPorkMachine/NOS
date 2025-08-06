#pragma once
#include <stdint.h>

#define PAGE_SIZE 4096

/**
 * Initialize the COW system with the total number of frames.
 * Must be called at kernel memory init.
 */
void cow_init(uint64_t total_frames);

/**
 * Increment reference count for the given physical address.
 */
void cow_inc_ref(uint64_t phys);

/**
 * Decrement reference count for the given physical address.
 */
void cow_dec_ref(uint64_t phys);

/**
 * Return reference count for a given physical address.
 */
uint16_t cow_refcount(uint64_t phys);

/**
 * Mark a virtual address as COW (removes writable, sets COW flag).
 */
void cow_mark(uint64_t virt);

/**
 * Unmark a virtual address as COW (restores writable, clears flag).
 */
void cow_unmark(uint64_t virt);

/**
 * Returns 1 if the virtual page is marked COW, else 0.
 */
int cow_is_marked(uint64_t virt);

/**
 * (Optional) Called by frame allocator when freeing a page:
 * Returns 1 if the frame is truly free and was released, 0 otherwise.
 */
int cow_free_frame(uint64_t phys);

/**
 * Handle a page fault at the given address with given error code.
 * Will do demand paging and handle COW for write faults.
 * Panics (halts) on unrecoverable error.
 */
void handle_page_fault(uint64_t err, uint64_t addr);
