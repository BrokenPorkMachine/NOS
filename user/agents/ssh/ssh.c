#include "ssh.h"
#include "../../../kernel/drivers/IO/serial.h"
#include "../../../kernel/Task/thread.h"
#include "../../../kernel/drivers/Net/netstack.h"
#include <string.h>
#include "../../../kernel/IPC/ipc.h"

// Port used for SSH traffic on the loopback stack
#define SSH_PORT 2

static void trim_newline(char *s) {
    char *p = s + strlen(s);
    while (p > s && (p[-1] == '\n' || p[-1] == '\r'))
        *--p = '\0';
}

void ssh_server(ipc_queue_t *q, uint32_t self_id) {
    serial_puts("[ssh] SSH server starting\n");
    int sock = net_socket_open(SSH_PORT, NET_SOCK_STREAM);
    const char banner[] = "NOS SSH\r\n";
    net_socket_send(sock, banner, strlen(banner));
    // Yield once after initialisation so other threads can run even if
    // the networking calls above block or take time to complete.
    thread_yield();
    char buf[128];
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
            if (!strncmp(buf, "exit", 4)) {
                const char bye[] = "bye\r\n";
                net_socket_send(sock, bye, strlen(bye));
                break;
            }
            net_socket_send(sock, buf, strlen(buf));
            const char nl[] = "\r\n";
            net_socket_send(sock, nl, strlen(nl));
        }
        thread_yield();
    }
    net_socket_close(sock);
    serial_puts("[ssh] server exiting\n");
}
