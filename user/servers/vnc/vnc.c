#include "vnc.h"
#include "../../../kernel/drivers/IO/serial.h"
#include "../../../kernel/Task/thread.h"
#include "../../../kernel/drivers/Net/netstack.h"
#include <string.h>

// Port used for VNC traffic on the loopback stack
#define VNC_PORT 1

static void trim_newline(char *s) {
    char *p = s + strlen(s);
    while (p > s && (p[-1] == '\n' || p[-1] == '\r'))
        *--p = '\0';
}

void vnc_server(ipc_queue_t *q, uint32_t self_id) {
    (void)q; (void)self_id;
    serial_puts("[vnc] VNC server starting\n");
    int sock = net_socket_open(VNC_PORT, NET_SOCK_STREAM);
    const char hello[] = "NOS VNC ready\r\n";
    net_socket_send(sock, hello, strlen(hello));
    char buf[64];
    for (;;) {
        int n = net_socket_recv(sock, buf, sizeof(buf) - 1);
        if (n > 0) {
            buf[n] = '\0';
            trim_newline(buf);
            if (!strncmp(buf, "ping", 4)) {
                const char pong[] = "pong\r\n";
                net_socket_send(sock, pong, strlen(pong));
            } else {
                const char unk[] = "unknown\r\n";
                net_socket_send(sock, unk, strlen(unk));
            }
        }
        thread_yield();
    }
}
