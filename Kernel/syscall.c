#include "syscall.h"
#include "../Task/thread.h"

void syscall_handle(uint64_t num) {
    switch (num) {
        case SYSCALL_YIELD:
            schedule();
            break;
        default:
            break;
    }
}
