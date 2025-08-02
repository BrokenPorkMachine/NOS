#include <assert.h>
#include <stdint.h>
#include "../../kernel/Kernel/syscall.h"

void thread_yield(void) { }

int main(void) {
    assert(syscall_handle(SYS_YIELD, 0, 0, 0) == 0);
    assert(syscall_handle(999, 0, 0, 0) == (uint64_t)-1);
    return 0;
}
