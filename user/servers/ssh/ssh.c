#include "ssh.h"
#include "../../../kernel/drivers/IO/serial.h"
#include "../../../kernel/Task/thread.h"
#include "../../../kernel/drivers/Net/netstack.h"
#include <string.h>

// Port used for SSH traffic on the loopback stack
#define SSH_PORT 2

void ssh_server(ipc_queue_t *q, uint32_t self_id) {
    (void)q; (void)self_id;
    serial_puts("[ssh] SSH server starting\n");
    int sock = net_socket_open(SSH_PORT, NET_SOCK_STREAM);
    const char banner[] = "NOS SSH\r\n";
    net_socket_send(sock, banner, strlen(banner));
    char buf[128];
    for (;;) {
        int n = net_socket_recv(sock, buf, sizeof(buf) - 1);
        if (n > 0) {
            buf[n] = '\0';
            if (!strncmp(buf, "exit", 4)) {
                const char bye[] = "bye\r\n";
                net_socket_send(sock, bye, strlen(bye));
                break;
            }
            net_socket_send(sock, buf, n); // echo
            const char nl[] = "\r\n";
            net_socket_send(sock, nl, strlen(nl));
        }
        thread_yield();
    }
    serial_puts("[ssh] server exiting\n");
}
