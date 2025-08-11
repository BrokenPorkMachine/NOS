#include "nosfs_server.h"
#include "nosfs.h"
#include <string.h>

// Shared filesystem instance defined in nosfs.c
extern nosfs_fs_t nosfs_root;

// Simple message-driven filesystem server. Each request is handled
// sequentially and the response is sent back on the same queue.
void nosfs_server(ipc_queue_t *q, uint32_t self_id) {
    nosfs_init(&nosfs_root);
    ipc_message_t msg, resp;

    while (1) {
        if (ipc_receive_blocking(q, self_id, &msg) != 0)
            continue;

        memset(&resp, 0, sizeof(resp));
        resp.type   = msg.type;
        resp.sender = self_id;

        switch (msg.type) {
        case NOSFS_MSG_CREATE:
            resp.arg1 = nosfs_create(&nosfs_root, (const char *)msg.data,
                                     msg.arg1, msg.arg2);
            break;
        case NOSFS_MSG_WRITE:
            resp.arg1 = nosfs_write(&nosfs_root, msg.arg1, msg.arg2,
                                    msg.data, msg.len);
            break;
        case NOSFS_MSG_READ:
            resp.arg1 = nosfs_read(&nosfs_root, msg.arg1, msg.arg2,
                                   resp.data, msg.len);
            resp.len  = msg.len;
            break;
        case NOSFS_MSG_DELETE:
            resp.arg1 = nosfs_delete(&nosfs_root, msg.arg1);
            break;
        case NOSFS_MSG_RENAME:
            resp.arg1 = nosfs_rename(&nosfs_root, msg.arg1,
                                     (const char *)msg.data);
            break;
        case NOSFS_MSG_LIST:
            resp.arg1 = nosfs_list(&nosfs_root,
                                   (char (*)[NOSFS_NAME_LEN])resp.data,
                                   IPC_MSG_DATA_MAX / NOSFS_NAME_LEN);
            resp.len  = resp.arg1 * NOSFS_NAME_LEN;
            break;
        case NOSFS_MSG_CRC:
            resp.arg1 = nosfs_compute_crc(&nosfs_root, msg.arg1);
            break;
        case NOSFS_MSG_VERIFY:
            resp.arg1 = nosfs_verify(&nosfs_root, msg.arg1);
            break;
        default:
            resp.type = 0xFFFFFFFF; // unknown request
            break;
        }

        ipc_send(q, self_id, &resp);
    }
}
