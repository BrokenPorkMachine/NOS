#include <assert.h>
#include <stdint.h>
#include "Task/thread.h"

static void dummy(void) {}
extern uintptr_t thread_debug_get_entry_trampoline(void);

int main(void) {
    thread_t *t = thread_create_with_priority(dummy, 100);
    assert(t);
    uint64_t *sp = (uint64_t *)t->rsp;
    assert(sp[6] == 0x202);
    assert(sp[8] == thread_debug_get_entry_trampoline());
    assert(sp[9] == (uint64_t)dummy);
    return 0;
}
