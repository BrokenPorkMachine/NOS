#include <assert.h>
#include <stdint.h>
#include "../../kernel/Kernel/syscall.h"
#include "../../kernel/Task/thread.h"

void thread_yield(void) { }
uint32_t smp_cpu_index(void) { return 0; }
thread_t dummy_thread = {0};
thread_t *current_cpu[MAX_CPUS] = { &dummy_thread };
thread_t *thread_create(void (*func)(void)) { (void)func; return &dummy_thread; }

int main(void) {
    assert(syscall_handle(SYS_YIELD, 0, 0, 0) == 0);
    assert(syscall_handle(999, 0, 0, 0) == (uint64_t)-1);
    return 0;
}
