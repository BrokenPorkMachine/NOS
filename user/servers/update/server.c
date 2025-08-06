#include "update.h"
#include "server.h"
#include "../../../kernel/IPC/ipc.h"
#include "../../libc/libc.h"
#include <string.h>

void update_server(ipc_queue_t *update_q, ipc_queue_t *pkg_q, uint32_t self_id) {
    ipc_message_t msg, reply;
    for (;;) {
        if (ipc_receive(update_q, self_id, &msg) != 0)
            continue;
        memset(&reply, 0, sizeof(reply));
        reply.type = msg.type;

        // Health check ping handler
        if (msg.type == IPC_HEALTH_PING) {
            reply.type = IPC_HEALTH_PONG;
            ipc_send(update_q, self_id, &reply);
            continue;
        }
        switch (msg.type) {
        case UPDATE_MSG_KERNEL: {
            ipc_message_t pmsg = {0}, prep = {0};
            const char *name = "kernel";
            size_t len = strlen(name);
            memcpy(pmsg.data, name, len);
            pmsg.len = len;
            pmsg.type = PKG_MSG_INSTALL;
            ipc_send(pkg_q, self_id, &pmsg);
            ipc_receive(pkg_q, self_id, &prep);
            reply.arg1 = prep.arg1;
            break;
        }
        case UPDATE_MSG_USERLAND: {
            ipc_message_t pmsg = {0}, prep = {0};
            const char *name = "userland";
            size_t len = strlen(name);
            memcpy(pmsg.data, name, len);
            pmsg.len = len;
            pmsg.type = PKG_MSG_INSTALL;
            ipc_send(pkg_q, self_id, &pmsg);
            ipc_receive(pkg_q, self_id, &prep);
            reply.arg1 = prep.arg1;
            break;
        }
        default:
            reply.arg1 = -1;
            break;
        }
        ipc_send(update_q, self_id, &reply);
    }
}
