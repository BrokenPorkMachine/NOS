#include "ftp.h"
#include "../../../kernel/drivers/IO/serial.h"
#include "../../../kernel/Task/thread.h"
#include "../../../kernel/drivers/Net/netstack.h"
#include "../nitrfs/server.h"
#include "../nitrfs/nitrfs.h"
#include <string.h>

// Port used for FTP traffic on the loopback stack
#define FTP_PORT 3

static int find_handle(ipc_queue_t *q, uint32_t id, const char *name) {
    ipc_message_t msg = {0}, reply = {0};
    msg.type = NITRFS_MSG_LIST;
    msg.len = 0;
    if (ipc_send(q, id, &msg) != 0 || ipc_receive(q, id, &reply) != 0)
        return -1;
    for (int i = 0; i < (int)reply.arg1; ++i) {
        char *n = (char *)reply.data + i * NITRFS_NAME_LEN;
        if (strncmp(n, name, NITRFS_NAME_LEN) == 0)
            return i;
    }
    return -1;
}

void ftp_server(ipc_queue_t *q, uint32_t self_id) {
    serial_puts("[ftp] FTP server starting\n");
    const char hello[] = "220 NOS FTP\r\n";
    net_send(FTP_PORT, hello, strlen(hello));
    char buf[128];
    for (;;) {
        int n = net_receive(FTP_PORT, buf, sizeof(buf) - 1);
        if (n > 0) {
            buf[n] = '\0';
            if (!strncmp(buf, "QUIT", 4)) {
                const char bye[] = "221 Bye\r\n";
                net_send(FTP_PORT, bye, strlen(bye));
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
                        net_send(FTP_PORT, name, strlen(name));
                        net_send(FTP_PORT, "\r\n", 2);
                    }
                } else {
                    const char err[] = "550 LIST failed\r\n";
                    net_send(FTP_PORT, err, strlen(err));
                }
                continue;
            }
            if (q && !strncmp(buf, "RETR", 4)) {
                char *name = buf + 5;
                int h = find_handle(q, self_id, name);
                if (h < 0) {
                    const char err[] = "550 Not found\r\n";
                    net_send(FTP_PORT, err, strlen(err));
                    continue;
                }
                ipc_message_t msg = {0}, reply = {0};
                msg.type = NITRFS_MSG_READ;
                msg.arg1 = h;
                msg.arg2 = IPC_MSG_DATA_MAX;
                msg.len = 0;
                ipc_send(q, self_id, &msg);
                if (ipc_receive(q, self_id, &reply) == 0 && reply.arg1 == 0) {
                    net_send(FTP_PORT, reply.data, reply.len);
                    net_send(FTP_PORT, "\r\n", 2);
                } else {
                    const char err[] = "550 Read error\r\n";
                    net_send(FTP_PORT, err, strlen(err));
                }
                continue;
            }
            if (q && !strncmp(buf, "STOR", 4)) {
                char *name = buf + 5;
                char *data = strchr(name, ' ');
                if (data) {
                    *data++ = '\0';
                }
                if (!data) {
                    const char err[] = "501 Syntax\r\n";
                    net_send(FTP_PORT, err, strlen(err));
                    continue;
                }
                int h = find_handle(q, self_id, name);
                ipc_message_t msg = {0}, reply = {0};
                if (h < 0) {
                    msg.type = NITRFS_MSG_CREATE;
                    msg.arg1 = IPC_MSG_DATA_MAX;
                    msg.arg2 = NITRFS_PERM_READ | NITRFS_PERM_WRITE;
                    size_t len = strlen(name);
                    if (len > IPC_MSG_DATA_MAX - 1) len = IPC_MSG_DATA_MAX - 1;
                    strncpy((char *)msg.data, name, len);
                    msg.data[len] = '\0';
                    msg.len = len;
                    ipc_send(q, self_id, &msg);
                    if (ipc_receive(q, self_id, &reply) != 0 || (int32_t)reply.arg1 < 0) {
                        const char err[] = "550 STOR fail\r\n";
                        net_send(FTP_PORT, err, strlen(err));
                        continue;
                    }
                    h = reply.arg1;
                }
                msg.type = NITRFS_MSG_WRITE;
                msg.arg1 = h;
                size_t len = strlen(data);
                if (len > IPC_MSG_DATA_MAX) len = IPC_MSG_DATA_MAX;
                msg.arg2 = len;
                memcpy(msg.data, data, len);
                msg.len = len;
                ipc_send(q, self_id, &msg);
                if (ipc_receive(q, self_id, &reply) == 0 && reply.arg1 == 0) {
                    const char ok[] = "200 STOR OK\r\n";
                    net_send(FTP_PORT, ok, strlen(ok));
                } else {
                    const char err[] = "550 STOR fail\r\n";
                    net_send(FTP_PORT, err, strlen(err));
                }
                continue;
            }
            const char ok[] = "200 OK\r\n";
            net_send(FTP_PORT, ok, strlen(ok));
        }
        thread_yield();
    }
    serial_puts("[ftp] server exiting\n");
}
