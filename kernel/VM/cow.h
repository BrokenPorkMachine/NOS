#pragma once
#include <stdint.h>
#include "pmm.h"
#include "paging.h"

#ifdef __cplusplus
extern "C" {
#endif

void cow_init(uint64_t total_frames);
void cow_mark(uint64_t virt);
void cow_unmark(uint64_t virt);
int  cow_is_marked(uint64_t virt);
void cow_inc_ref(uint64_t phys);
void cow_dec_ref(uint64_t phys);
uint16_t cow_refcount(uint64_t phys);

void handle_page_fault(uint64_t error, uint64_t addr);

#ifdef __cplusplus
}
#endif
