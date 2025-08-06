#include "vnc.h"
#include "../../../kernel/drivers/IO/serial.h"
#include "../../../kernel/Task/thread.h"
#include "../../../kernel/drivers/Net/netstack.h"
#include <string.h>
#include "../../../kernel/IPC/ipc.h"

// Port used for VNC traffic on the loopback stack
#define VNC_PORT 1

static void trim_newline(char *s) {
    char *p = s + strlen(s);
    while (p > s && (p[-1] == '\n' || p[-1] == '\r'))
        *--p = '\0';
}

void vnc_server(ipc_queue_t *q, uint32_t self_id) {
    serial_puts("[vnc] VNC server starting\n");
    // Allow the scheduler to run other threads before performing network
    // setup, preventing potential boot-time hangs if these calls block.
    thread_yield();
    int sock = net_socket_open(VNC_PORT, NET_SOCK_STREAM);
    const char hello[] = "NOS VNC ready\r\n";
    net_socket_send(sock, hello, strlen(hello));
    // Yield once after initialisation so other threads can run even if
    // the networking calls above block or take time to complete.
    thread_yield();
    char buf[64];
    for (;;) {
        // Check for health ping
        if (q) {
            ipc_message_t hmsg, hrep = {0};
            if (ipc_receive(q, self_id, &hmsg) == 0 && hmsg.type == IPC_HEALTH_PING) {
                hrep.type = IPC_HEALTH_PONG;
                ipc_send(q, self_id, &hrep);
                thread_yield();
                continue;
            }
        }

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
