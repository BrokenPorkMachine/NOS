#include "init.h"
#include "../../../kernel/drivers/IO/serial.h"
#include "../../../kernel/Task/thread.h"
#include "../../libc/libc.h"

// Simple init/task spawner stub
void init_main(ipc_queue_t *q, uint32_t self_id) {
    (void)q; (void)self_id;
    serial_puts("[init] init server started\n");
    // In a real system this would spawn other tasks using fork/exec.
    for (;;) {
        thread_yield();
    }
}
