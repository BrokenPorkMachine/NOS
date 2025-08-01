#include "vnc.h"
#include "../../IO/serial.h"
#include "../../Task/thread.h"
#include "../../Net/netstack.h"
#include <string.h>

void vnc_server(ipc_queue_t *q, uint32_t self_id) {
    (void)q; (void)self_id;
    serial_puts("[vnc] VNC server starting\n");
    const char msg[] = "NOS VNC not implemented\r\n";
    net_send(msg, strlen(msg));
    while (1)
        thread_yield();
}
