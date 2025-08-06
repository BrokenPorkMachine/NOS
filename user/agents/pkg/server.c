#include "pkg.h"
#include "server.h"
#include "../../../kernel/IPC/ipc.h"
#include "../../libc/libc.h"
#include <string.h>

void pkg_server(ipc_queue_t *q, uint32_t self_id) {
    pkg_init();
    ipc_message_t msg, reply;
    for (;;) {
        if (ipc_receive(q, self_id, &msg) != 0)
            continue;
        memset(&reply, 0, sizeof(reply));
        reply.type = msg.type;

        // Health check ping handler
        if (msg.type == IPC_HEALTH_PING) {
            reply.type = IPC_HEALTH_PONG;
            ipc_send(q, self_id, &reply);
            continue;
        }
        switch (msg.type) {
        case PKG_MSG_INSTALL:
            msg.data[msg.len] = '\0';
            reply.arg1 = pkg_install((const char*)msg.data);
            break;
        case PKG_MSG_UNINSTALL:
            msg.data[msg.len] = '\0';
            reply.arg1 = pkg_uninstall((const char*)msg.data);
            break;
        case PKG_MSG_LIST:
            reply.arg1 = pkg_list((char (*)[PKG_NAME_MAX])reply.data,
                                  IPC_MSG_DATA_MAX / PKG_NAME_MAX);
            reply.len = reply.arg1 * PKG_NAME_MAX;
            break;
        default:
            reply.arg1 = -1;
            break;
        }
        ipc_send(q, self_id, &reply);
    }
}
