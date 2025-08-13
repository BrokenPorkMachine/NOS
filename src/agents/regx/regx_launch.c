// src/agents/regx/regx_launch.c
//
// Consolidated launch helpers for regx. This combines the
// thread/stack adapters and the ELF loader glue so that the
// registry agent's launch logic lives in a single file.

#include <stddef.h>
#include <stdint.h>
#include <stdarg.h>

#include "drivers/IO/serial.h"
#include "../../../kernel/VM/legacy_heap.h"
#include "../../../kernel/Task/thread.h"
#include "../../../loader/elf_paged_loader.h"

// Allocate a simple stack for user threads.
static void* create_user_stack(size_t size) {
    return legacy_kmalloc(size);
}

// Spawn a thread to run at `rip` with the given priority.
static int thread_spawn(uintptr_t rip, void* user_stack_top,
                        uint32_t prio, uint32_t* out_tid) {
    (void)user_stack_top; // current kernel lacks separate user stacks
    thread_t* t = thread_create_with_priority((void(*)(void))rip, prio);
    if (!t) return -1;
    if (out_tid) *out_tid = t->id;
    return 0;
}

// Provide a lightweight log() implementation using the serial driver.
static void log(const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    serial_vprintf(fmt, ap);
    va_end(ap);
}

// Launch an ELF agent using the paged loader.
int regx_launch_elf_paged(const char* name, const uint8_t* file,
                          size_t file_sz, uint32_t* out_tid) {
    (void)name;
    elf_map_result_t m;
    int rc = elf_load_paged(file, file_sz, &m);
    if (rc) {
        log("[regx] elf_load_paged rc=%d", rc);
        return rc;
    }

    void* ustk = create_user_stack(64 * 1024);
    if (!ustk) {
        elf_unmap(&m);
        return -12;
    }

    rc = thread_spawn((uintptr_t)m.entry_va, ustk, 200, out_tid);
    if (rc) {
        log("[regx] thread_spawn rc=%d", rc);
        elf_unmap(&m);
        return rc;
    }

    // Do not unmap here; the process now owns those mappings.
    return 0;
}

