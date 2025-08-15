#include "nitroheap_shim.h"
#include "nitroheap_sys.h"    // syscalls

static inline uint16_t pick_partition(nh_flags_t flags) {
  // Simple mapping: (flags → partition key). Kernel can virtualize real partitions per-process.
  // For first cut: hash traits into a small partition index space.
  uint64_t key = (flags & 0x0000FFFFFFFFFFFFull) ^ (flags >> 48);
  return (uint16_t)((key * 11400714819323198485ull) >> 49); // ~15-bit partition id
}

void* malloc(size_t size) {
  return mallocx(size, NH_PRESET_BALANCED);
}

void free(void* p) {
  dallocx(p, 0);
}

void* calloc(size_t n, size_t sz) {
  size_t bytes = n * sz; // TODO: overflow check
  void* p = mallocx(bytes, NH_PRESET_BALANCED | NH_SECURE_STRICT); // zero-on-alloc enforced by trait
  return p;
}

void* realloc(void* p, size_t size) {
  return rallocx(p, size, NH_PRESET_BALANCED);
}

void* mallocx(size_t size, nh_flags_t flags) {
  nh_alloc_req req = {
    .size = size,
    .flags = flags,
    .partition_hint = pick_partition(flags),
  };
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

// MOVABLE
nh_handle_t halloc(size_t size, nh_flags_t flags) {
  nh_halloc_req req = { .size = size, .flags = flags | NH_MOVABLE };
  nh_halloc_resp resp;
  if (sys_nh_halloc(&req, &resp) != 0) return 0;
  return resp.handle;
}

void* hptr(nh_handle_t h) {
  nh_hptr_req req = { .handle = h };
  nh_alloc_resp resp;
  if (sys_nh_hptr(&req, &resp) != 0) return NULL;
  return resp.ptr; // ephemeral; invalidated across epochs or relocations
}

int hfree(nh_handle_t h) {
  nh_hfree_req req = { .handle = h };
  return sys_nh_hfree(&req);
}
