#pragma once
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void *kalloc(size_t sz);
void  kfree(void *ptr);
void  kheap_init(void);

#ifdef __cplusplus
}
#endif
