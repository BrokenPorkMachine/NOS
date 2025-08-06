#include "mv.h"
#include "../../../kernel/drivers/IO/serial.h"
#include "../../libc/libc.h"
#include "../../../kernel/IPC/ipc.h"

void mv_main(ipc_queue_t *q, uint32_t self_id) {
    (void)q; (void)self_id;
    if (rename("src", "dst") != 0)
        serial_puts("mv: rename failed\n");
    else
        serial_puts("mv: done\n");
}
