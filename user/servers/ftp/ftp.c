#include "ftp.h"
#include "../../../kernel/drivers/IO/serial.h"
#include "../../../kernel/Task/thread.h"
#include "../../../kernel/drivers/Net/netstack.h"
#include "../nitrfs/server.h"
#include "../nitrfs/nitrfs.h"
#include "../../../kernel/IPC/ipc.h"
#include <string.h>

// Port used for FTP traffic on the loopback stack
#define FTP_PORT 3

static void trim_newline(char *s) {
    char *p = s + strlen(s);
    while (p > s && (p[-1] == '\n' || p[-1] == '\r'))
        *--p = '\0';
}

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
    int sock = net_socket_open(FTP_PORT, NET_SOCK_STREAM);
    const char hello[] = "220 NOS FTP\r\n";
    net_socket_send(sock, hello, strlen(hello));
    /* Yield once after initialisation so other threads can run even if
     * the networking calls above block or take time to complete. */
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
            if (!strncmp(buf, "QUIT", 4)) {
                const char bye[] = "221 Bye\r\n";
                net_socket_send(sock, bye, strlen(bye));
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
                        net_socket_send(sock, name, strlen(name));
                        net_socket_send(sock, "\r\n", 2);
                    }
                } else {
                    const char err[] = "550 LIST failed\r\n";
                    net_socket_send(sock, err, strlen(err));
                }
                continue;
            }
            if (q && !strncmp(buf, "RETR", 4)) {
                char *name = buf + 5;
                int h = find_handle(q, self_id, name);
                if (h < 0) {
                    const char err[] = "550 Not found\r\n";
                    net_socket_send(sock, err, strlen(err));
                    continue;
                }
                ipc_message_t msg = {0}, reply = {0};
                msg.type = NITRFS_MSG_READ;
                msg.arg1 = h;
                msg.arg2 = 0;
                msg.len = IPC_MSG_DATA_MAX;
                ipc_send(q, self_id, &msg);
                if (ipc_receive(q, self_id, &reply) == 0 && reply.arg1 == 0) {
                    net_socket_send(sock, reply.data, reply.len);
                    net_socket_send(sock, "\r\n", 2);
                } else {
                    const char err[] = "550 Read error\r\n";
                    net_socket_send(sock, err, strlen(err));
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
                    net_socket_send(sock, err, strlen(err));
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
                        net_socket_send(sock, err, strlen(err));
                        continue;
                    }
                    h = reply.arg1;
                }
                msg.type = NITRFS_MSG_WRITE;
                msg.arg1 = h;
                size_t len = strlen(data);
                if (len > IPC_MSG_DATA_MAX) len = IPC_MSG_DATA_MAX;
                msg.arg2 = 0;
                memcpy(msg.data, data, len);
                msg.len = len;
                ipc_send(q, self_id, &msg);
                if (ipc_receive(q, self_id, &reply) == 0 && reply.arg1 == 0) {
                    const char ok[] = "200 STOR OK\r\n";
                    net_socket_send(sock, ok, strlen(ok));
                } else {
                    const char err[] = "550 STOR fail\r\n";
                    net_socket_send(sock, err, strlen(err));
                }
                continue;
            }
            const char ok[] = "200 OK\r\n";
            net_socket_send(sock, ok, strlen(ok));
        }
        thread_yield();
    }
    serial_puts("[ftp] server exiting\n");
}
