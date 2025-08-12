// ============================================================================
// File: regx/regx_launch_elf_paged.c  (optional glue)
// Purpose: Helper to launch an ELF agent using the page-mapped loader
// ============================================================================
#include <stdint.h>
#include "../loader/elf_paged_loader.h"

// Kernel/threading hooks (provided by your kernel)
extern int   thread_spawn(uintptr_t rip, void* user_stack_top, uint32_t prio, uint32_t* out_tid);
extern void* create_user_stack(size_t sz);
extern void  log(const char* fmt, ...);

int regx_launch_elf_paged(const char* name, const uint8_t* file, size_t file_sz, uint32_t* out_tid)
{
    (void)name;
    elf_map_result_t m; int rc = elf_load_paged(file, file_sz, &m);
    if (rc) { log("[regx] elf_load_paged rc=%d", rc); return rc; }

    void* ustk = create_user_stack(64*1024);
    if (!ustk) { elf_unmap(&m); return -12; }

    rc = thread_spawn((uintptr_t)m.entry_va, ustk, 200, out_tid);
    if (rc) { log("[regx] thread_spawn rc=%d", rc); elf_unmap(&m); return rc; }

    // Note: do not unmap here; the process now owns those mappings.
    return 0;
}
