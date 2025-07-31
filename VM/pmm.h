#pragma once
#include <stdint.h>
#include "../bootloader/include/bootinfo.h"

#ifdef __cplusplus
extern "C" {
#endif

void pmm_init(const bootinfo_t *bootinfo);
void *alloc_page(void);
void free_page(void *page);
uint64_t pmm_total_frames(void);

#ifdef __cplusplus
}
#endif
