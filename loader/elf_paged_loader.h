// ============================================================================
// File: loader/elf_paged_loader.h
// Purpose: Page-mapped ELF64 loader interface for NitrOS (drop-in)
// ============================================================================
#ifndef NITROS_ELF_PAGED_LOADER_H
#define NITROS_ELF_PAGED_LOADER_H
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Result of mapping an ELF image. Keep enough info to unmap on failure.
typedef struct elf_map_result_s {
    void*  base_text; size_t text_sz;  // PF_X | PF_R
    void*  base_ro;   size_t ro_sz;    // PF_R
    void*  base_rw;   size_t rw_sz;    // PF_R | PF_W
    void*  entry_va;                  // validated executable VA
} elf_map_result_t;

// Maps PT_LOAD segments a page at a time, copies file bytes, zeros BSS, and
// sets final page protections. Computes a validated entry VA.
// Returns 0 on success, negative errno otherwise (e.g., -12 ENOMEM, -22 EINVAL).
int elf_load_paged(const uint8_t* file, size_t file_sz, elf_map_result_t* out);

// Unmap any mapped segments described by `m` (idempotent).
void elf_unmap(const elf_map_result_t* m);

#ifdef __cplusplus
}
#endif
#endif // NITROS_ELF_PAGED_LOADER_H
