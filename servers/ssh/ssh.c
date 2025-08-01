#include "ssh.h"
#include "../../IO/serial.h"
#include "../../Task/thread.h"
#include "../../Net/netstack.h"
#include <string.h>

void ssh_server(ipc_queue_t *q, uint32_t self_id) {
    (void)q; (void)self_id;
    serial_puts("[ssh] SSH server starting\n");
    const char banner[] = "NOS SSH\r\n";
    net_send(banner, strlen(banner));
    char buf[128];
    for (;;) {
        int n = net_receive(buf, sizeof(buf) - 1);
        if (n > 0) {
            buf[n] = '\0';
            if (!strncmp(buf, "exit", 4)) {
                const char bye[] = "bye\r\n";
                net_send(bye, strlen(bye));
                break;
            }
            net_send(buf, n); // echo
            const char nl[] = "\r\n";
            net_send(nl, strlen(nl));
        }
        thread_yield();
    }
    serial_puts("[ssh] server exiting\n");
}
