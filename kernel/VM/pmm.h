#pragma once
#include <stdint.h>
#include "../../boot/include/bootinfo.h"

#ifdef __cplusplus
extern "C" {
#endif

void pmm_init(const bootinfo_t *bootinfo);
void *alloc_page(void);
void free_page(void *page);

#ifdef __cplusplus
}
#endif
