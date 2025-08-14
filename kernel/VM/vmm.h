#pragma once
#include <stdint.h>
#include "paging_adv.h"
#ifdef __cplusplus
extern "C" {
#endif

void vmm_init(void);
uint64_t *vmm_create_pml4(void);
void vmm_map_page(uint64_t *pml4, uint64_t virt, uint64_t phys, uint64_t flags, uint32_t order, int numa_node);
void vmm_switch(uint64_t *pml4);

#ifdef __cplusplus
}
#endif
