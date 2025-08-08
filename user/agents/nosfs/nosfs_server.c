#include "nosfs_server.h"
#include "nosfs.h"
#include <string.h>
#include <stdio.h>

// Optionally add: #include <unistd.h> for file/manifest validation

void nosfs_server(ipc_queue_t *q, uint32_t self_id) {
    nosfs_fs_t fs;
    if (nosfs_init(&fs) != 0) {
        ipc_message_t fail = {0};
        fail.type = IPC_HEALTH_PONG;
        fail.arg1 = -1;
        ipc_send(q, self_id, &fail);
        return;
    }

    // Optional: Manifest validation (suggested for integrity)
    // if (!nosfs_validate_manifest()) { ... }

    ipc_message_t msg, reply;
    while (1) {
        if (ipc_receive_blocking(q, self_id, &msg) != 0)
            continue;
        if (msg.len > IPC_MSG_DATA_MAX) {
            // Optionally send error/reject message
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
        // Feature extension: Quota management, journaling ops, snapshots
        default:
            reply.arg1 = NOSFS_ERR;
            break;
        }
        ipc_send(q, self_id, &reply);
    }
}
