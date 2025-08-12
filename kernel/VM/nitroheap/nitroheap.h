#pragma once
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
void nitroheap_init(void);
void* nitro_kmalloc(size_t sz, size_t align);
void  nitro_kfree(void* p);
void* nitro_krealloc(void* p, size_t newsz, size_t align);
void  nitro_kheap_dump_stats(const char* tag);
void  nitro_kheap_trim(void);
#ifdef __cplusplus
}
#endif
