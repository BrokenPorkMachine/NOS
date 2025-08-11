#include "nosfs_server.h"
#include "nosfs.h"
#include <string.h>
#include <stdatomic.h>

// Built-in agent images generated at build time
#include "../../kernel/init_bin.h"
#include "../../kernel/login_bin.h"

// Shared filesystem instance defined in nosfs.c
extern nosfs_fs_t nosfs_root;

// Signal to other agents when the filesystem server is ready.
static _Atomic int nosfs_ready = 0;

int nosfs_is_ready(void) {
    return atomic_load(&nosfs_ready);
}

// Global flag indicating when the filesystem is initialized
_Atomic int nosfs_ready = 0;

// Simple message-driven filesystem server. Each request is handled
// sequentially and the response is sent back on the same queue.
void nosfs_server(ipc_queue_t *q, uint32_t self_id) {
    nosfs_init(&nosfs_root);

    // Preload essential agents so the registry can launch them.
    int h = nosfs_create(&nosfs_root, "agents/init.mo2", init_bin_len, 0);
    if (h >= 0)
        nosfs_write(&nosfs_root, h, 0, init_bin, init_bin_len);
    h = nosfs_create(&nosfs_root, "agents/login.bin", login_bin_len, 0);
    if (h >= 0)
        nosfs_write(&nosfs_root, h, 0, login_bin, login_bin_len);

    // Make filesystem contents visible to other threads.
    // Signal readiness so other agents can safely access the filesystem
    atomic_store(&nosfs_ready, 1);

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
