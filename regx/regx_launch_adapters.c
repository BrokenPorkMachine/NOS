// regx/regx_launch_adapters.c
// Adapts to your thread/stack APIs so regx_launch_elf_paged links cleanly.

#include <stddef.h>
#include <stdint.h>
#include <stdarg.h>

#include "drivers/IO/serial.h"

// ====== EDIT THESE externs to match your kernel ======
// Create a user-mode stack, return top-of-stack (or pointer your thread API expects)
extern void* proc_create_user_stack(size_t size);
// Spawn a user thread at RIP with given stack top & prio; returns 0 on success
extern int   thread_spawn_user(uintptr_t rip, void* user_stack_top, uint32_t prio, uint32_t* out_tid);
// No explicit logging extern; we'll bridge to serial_vprintf

// ====== Adapters with the names regx_launch_elf_paged.c expects ======
void*  create_user_stack(size_t sz) { return proc_create_user_stack(sz); }
int    thread_spawn(uintptr_t rip, void* user_stack_top, uint32_t prio, uint32_t* out_tid) {
    return thread_spawn_user(rip, user_stack_top, prio, out_tid);
}
void   log(const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    serial_vprintf(fmt, ap);
    va_end(ap);
}
