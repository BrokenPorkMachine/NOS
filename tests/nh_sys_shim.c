#include "../include/nitroheap_sys.h"
#include <stddef.h>

void* mallocx(size_t size, nh_flags_t flags) {
    nh_alloc_req req = { .size = size, .flags = flags, .partition_hint = 0 };
    nh_alloc_resp resp;
    if (sys_nh_alloc(&req, &resp) != 0) return NULL;
    return resp.ptr;
}

int dallocx(void* p, nh_flags_t flags) {
    nh_free_req req = { .ptr = p, .flags = flags };
    return sys_nh_free(&req);
}

void* rallocx(void* p, size_t size, nh_flags_t flags) {
    nh_realloc_req req = { .ptr = p, .new_size = size, .flags = flags };
    nh_alloc_resp resp;
    if (sys_nh_realloc(&req, &resp) != 0) return NULL;
    return resp.ptr;
}
