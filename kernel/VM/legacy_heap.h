// Minimal kernel heap backed by the buddy allocator.
#pragma once
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void *legacy_kmalloc(size_t sz);
void  legacy_kfree(void *ptr);
void *legacy_krealloc(void *ptr, size_t newsz);
void  legacy_kheap_init(void);

#ifdef __cplusplus
}
#endif
