#include "vnc.h"
#include "../../../kernel/drivers/IO/serial.h"
#include "../../../kernel/Task/thread.h"
#include "../../../kernel/drivers/Net/netstack.h"
#include <string.h>

// Port used for VNC traffic on the loopback stack
#define VNC_PORT 1

void vnc_server(ipc_queue_t *q, uint32_t self_id) {
    (void)q; (void)self_id;
    serial_puts("[vnc] VNC server starting\n");
    const char hello[] = "NOS VNC ready\r\n";
    net_send(VNC_PORT, hello, strlen(hello));
    char buf[64];
    for (;;) {
        int n = net_receive(VNC_PORT, buf, sizeof(buf) - 1);
        if (n > 0) {
            buf[n] = '\0';
            if (!strncmp(buf, "ping", 4)) {
                const char pong[] = "pong\r\n";
                net_send(VNC_PORT, pong, strlen(pong));
            } else {
                const char unk[] = "unknown\r\n";
                net_send(VNC_PORT, unk, strlen(unk));
            }
        }
        thread_yield();
    }
}
