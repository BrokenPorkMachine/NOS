#include "ftp.h"
#include "../../IO/serial.h"
#include "../../Task/thread.h"
#include "../../Net/netstack.h"
#include <string.h>

void ftp_server(ipc_queue_t *q, uint32_t self_id) {
    (void)q; (void)self_id;
    serial_puts("[ftp] FTP server starting\n");
    const char hello[] = "220 NOS FTP\r\n";
    net_send(hello, strlen(hello));
    char buf[128];
    for (;;) {
        int n = net_receive(buf, sizeof(buf) - 1);
        if (n > 0) {
            buf[n] = '\0';
            if (!strncmp(buf, "QUIT", 4)) {
                const char bye[] = "221 Bye\r\n";
                net_send(bye, strlen(bye));
                break;
            }
            const char ok[] = "200 OK\r\n";
            net_send(ok, strlen(ok));
        }
        thread_yield();
    }
    serial_puts("[ftp] server exiting\n");
}
