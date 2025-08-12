#pragma once
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif

void kheap_parse_bootarg(const char* cmdline);
void kheap_init(void);
void* kmalloc(size_t sz, size_t align);
void  kfree(void* p);
void* krealloc(void* p, size_t newsz, size_t align);
void  kheap_dump_stats(const char* tag);
void  kheap_trim(void);

#define kalloc(sz) kmalloc((sz), 0)

#ifdef __cplusplus
}
#endif
