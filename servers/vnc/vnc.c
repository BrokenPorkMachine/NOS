#include "vnc.h"
#include "../../IO/serial.h"
#include "../../Task/thread.h"

void vnc_server(ipc_queue_t *q, uint32_t self_id) {
    (void)q; (void)self_id;
    serial_puts("[vnc] VNC server stub - networking not implemented\n");
    while (1) thread_yield();
}
