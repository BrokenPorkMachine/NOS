#pragma once
#include <stddef.h>
#include <stdint.h>
#include "nitroheap_flags.h"

#ifdef __cplusplus
extern "C" {
#endif

void* malloc(size_t size);                       // default partition/policy
void  free(void* p);
void* calloc(size_t n, size_t sz);
void* realloc(void* p, size_t size);

// Extended API (flagged)
void* mallocx(size_t size, nh_flags_t flags);
int   dallocx(void* p, nh_flags_t flags);        // flags may carry alignment/lifetime hints for deferred ops
void* rallocx(void* p, size_t size, nh_flags_t flags);

// MOVABLE objects (optional)
typedef uint64_t nh_handle_t;
nh_handle_t halloc(size_t size, nh_flags_t flags);
void*       hptr(nh_handle_t h);                 // temporary direct pointer (epoch-scoped)
int         hfree(nh_handle_t h);

#ifdef __cplusplus
}
#endif
