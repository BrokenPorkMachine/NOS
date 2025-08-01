#include "vnc.h"
#include "../../IO/serial.h"
#include "../../Task/thread.h"
#include "../../Net/netstack.h"
#include <string.h>

void vnc_server(ipc_queue_t *q, uint32_t self_id) {
    (void)q; (void)self_id;
    serial_puts("[vnc] VNC server starting\n");
    const char hello[] = "NOS VNC ready\r\n";
    net_send(hello, strlen(hello));
    char buf[64];
    for (;;) {
        int n = net_receive(buf, sizeof(buf) - 1);
        if (n > 0) {
            buf[n] = '\0';
            if (!strncmp(buf, "ping", 4)) {
                const char pong[] = "pong\r\n";
                net_send(pong, strlen(pong));
            } else {
                const char unk[] = "unknown\r\n";
                net_send(unk, strlen(unk));
            }
        }
        thread_yield();
    }
}
