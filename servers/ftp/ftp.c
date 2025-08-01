#include "ftp.h"
#include "../../IO/serial.h"
#include "../../Task/thread.h"
#include "../../Net/netstack.h"
#include "../nitrfs/server.h"
#include "../nitrfs/nitrfs.h"
#include <string.h>

void ftp_server(ipc_queue_t *q, uint32_t self_id) {
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
            if (q && !strncmp(buf, "LIST", 4)) {
                ipc_message_t msg = {0}, reply = {0};
                msg.type = NITRFS_MSG_LIST;
                msg.len = 0;
                ipc_send(q, self_id, &msg);
                if (ipc_receive(q, self_id, &reply) == 0) {
                    for (int i = 0; i < (int)reply.arg1; i++) {
                        char *name = (char *)reply.data + i * NITRFS_NAME_LEN;
                        net_send(name, strlen(name));
                        net_send("\r\n", 2);
                    }
                } else {
                    const char err[] = "550 LIST failed\r\n";
                    net_send(err, strlen(err));
                }
                continue;
            }
            const char ok[] = "200 OK\r\n";
            net_send(ok, strlen(ok));
        }
        thread_yield();
    }
    serial_puts("[ftp] server exiting\n");
}
