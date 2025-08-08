#include "nosfs.h"
#include "../../../kernel/IPC/ipc.h"
#include "../../libc/libc.h"
#include "nosfs_server.h"
#include "agent.h"

enum {
    NOSFS_OK = 0,
    NOSFS_ERR = -1
};

/* Static manifest describing the NOSFS agent.  In a production system this
 * would be a structured JSON/CBOR blob loaded from the module's manifest
 * section. */
static const char nosfs_manifest_str[] =
    "{\"name\":\"NOSFS\",\"version\":\"1.0.0\","
    "\"capabilities\":\"filesystem,snapshot,rollback\"}";

static n2_agent_t nosfs_agent = {
    .name = "NOSFS",
    .version = "1.0.0",
    .entry = nosfs_server,
    .manifest = nosfs_manifest_str,
    .capabilities = "filesystem,snapshot,rollback"
};

/* Register with the kernel agent registry at module load time. */
__attribute__((constructor))
static void register_agent(void) {
    n2_agent_register(&nosfs_agent);
}

void nosfs_server(ipc_queue_t *q, uint32_t self_id) {
    nosfs_fs_t fs;
    if (nosfs_init(&fs) != 0) {
        // Log failure to init, abort server
        ipc_message_t fail;
        memset(&fail, 0, sizeof(fail));
        fail.type = IPC_HEALTH_PONG;
        fail.arg1 = -1;
        ipc_send(q, self_id, &fail);
        return;
    }

    ipc_message_t msg, reply;
    while (1) {
        if (ipc_receive_blocking(q, self_id, &msg) != 0)
            continue;
        if (msg.len > IPC_MSG_DATA_MAX) {
            // Optionally send error/reject message here
            continue;
        }
        memset(&reply, 0, sizeof(reply));
        reply.type = msg.type;

        switch (msg.type) {
        case IPC_HEALTH_PING:
            reply.type = IPC_HEALTH_PONG;
            ipc_send(q, self_id, &reply);
            continue;
        case NOSFS_MSG_CREATE:
            if (!msg.data || msg.len == 0) {
                reply.arg1 = NOSFS_ERR;
                break;
            }
            reply.arg1 = nosfs_create(&fs, (const char*)msg.data, msg.arg1, msg.arg2);
            break;
        case NOSFS_MSG_WRITE:
            reply.arg1 = nosfs_write(&fs, msg.arg1, msg.arg2, msg.data, msg.len);
            break;
        case NOSFS_MSG_READ:
            reply.arg1 = nosfs_read(&fs, msg.arg1, msg.arg2, reply.data, msg.len);
            reply.len  = (reply.arg1 == 0) ? msg.len : 0;
            break;
        case NOSFS_MSG_DELETE:
            reply.arg1 = nosfs_delete(&fs, msg.arg1);
            break;
        case NOSFS_MSG_RENAME:
            if (!msg.data || msg.len == 0) {
                reply.arg1 = NOSFS_ERR;
                break;
            }
            reply.arg1 = nosfs_rename(&fs, msg.arg1, (const char*)msg.data);
            break;
        case NOSFS_MSG_LIST:
            reply.arg1 = nosfs_list(&fs, (char (*)[NOSFS_NAME_LEN])reply.data, IPC_MSG_DATA_MAX / NOSFS_NAME_LEN);
            reply.len  = reply.arg1 * NOSFS_NAME_LEN;
            break;
        case NOSFS_MSG_CRC:
            reply.arg1 = nosfs_compute_crc(&fs, msg.arg1);
            if (reply.arg1 == 0)
                reply.arg2 = fs.files[msg.arg1].crc32;
            break;
        case NOSFS_MSG_VERIFY:
            reply.arg1 = nosfs_verify(&fs, msg.arg1);
            break;
        default:
            reply.arg1 = NOSFS_ERR;
            break;
        }
        ipc_send(q, self_id, &reply);
    }
}
