// src/agents/regx/regx_launch_adapters.c
// Adapts to your thread/stack APIs so regx_launch_elf_paged links cleanly.

#include <stddef.h>
#include <stdint.h>
#include <stdarg.h>

#include "drivers/IO/serial.h"
#include "../../../kernel/VM/legacy_heap.h"
#include "../../../kernel/Task/thread.h"

// Allocate a simple stack for user threads
void* create_user_stack(size_t size) {
    return legacy_kmalloc(size);
}

// Spawn a thread to run at `rip` with given priority
int thread_spawn(uintptr_t rip, void* user_stack_top, uint32_t prio, uint32_t* out_tid) {
    (void)user_stack_top; // current kernel lacks separate user stacks
    thread_t* t = thread_create_with_priority((void(*)(void))rip, prio);
    if (!t) return -1;
    if (out_tid) *out_tid = t->id;
    return 0;
}

// Provide log() using serial_vprintf for regx_launch_elf_paged.c
void   log(const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    serial_vprintf(fmt, ap);
    va_end(ap);
}
