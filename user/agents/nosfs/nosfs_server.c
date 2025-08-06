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
    .manifest = nosfs_manifest_str
};

/* Register with the kernel agent registry at module load time. */
__attribute__((constructor))
static void register_agent(void) {
    n2_agent_register(&nosfs_agent);
}

void nosfs_server(ipc_queue_t *q, uint32_t self_id) {
    nosfs_fs_t fs;
    nosfs_init(&fs);
    ipc_message_t msg, reply;
    int handle, ret;

    while (1) {
        if (ipc_receive_blocking(q, self_id, &msg) != 0)
            continue;
        if (msg.len > IPC_MSG_DATA_MAX)
            continue;

        memset(&reply, 0, sizeof(reply));
        reply.type = msg.type;

        if (msg.type == IPC_HEALTH_PING) {
            reply.type = IPC_HEALTH_PONG;
            ipc_send(q, self_id, &reply);
            continue;
        }

        switch (msg.type) {
        case NOSFS_MSG_CREATE:
            ret = nosfs_create(&fs, (const char*)msg.data, msg.arg1, msg.arg2);
            reply.arg1 = ret;
            break;
        case NOSFS_MSG_WRITE:
            handle = msg.arg1;
            ret = nosfs_write(&fs, handle, msg.arg2, msg.data, msg.len);
            reply.arg1 = ret;
            break;
        case NOSFS_MSG_READ:
            handle = msg.arg1;
            ret = nosfs_read(&fs, handle, msg.arg2, reply.data, msg.len);
            reply.arg1 = ret;
            reply.len  = (ret == 0) ? msg.len : 0;
            break;
        case NOSFS_MSG_DELETE:
            handle = msg.arg1;
            ret = nosfs_delete(&fs, handle);
            reply.arg1 = ret;
            break;
        case NOSFS_MSG_RENAME:
            handle = msg.arg1;
            ret = nosfs_rename(&fs, handle, (const char*)msg.data);
            reply.arg1 = ret;
            break;
        case NOSFS_MSG_LIST:
            reply.arg1 = nosfs_list(&fs, (char (*)[NOSFS_NAME_LEN])reply.data,
                                     IPC_MSG_DATA_MAX / NOSFS_NAME_LEN);
            reply.len  = reply.arg1 * NOSFS_NAME_LEN;
            break;
        case NOSFS_MSG_CRC:
            handle = msg.arg1;
            ret = nosfs_compute_crc(&fs, handle);
            reply.arg1 = ret;
            if (ret == 0)
                reply.arg2 = fs.files[handle].crc32;
            break;
        case NOSFS_MSG_VERIFY:
            handle = msg.arg1;
            ret = nosfs_verify(&fs, handle);
            reply.arg1 = ret;
            break;
        default:
            reply.arg1 = NOSFS_ERR;
            break;
        }
        ipc_send(q, self_id, &reply);
    }
}
