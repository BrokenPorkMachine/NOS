#include "nosfs_server.h"
#include "nosfs.h"
#include <string.h>
#include <stdatomic.h>

// Optional boot logging from kernel
extern int kprintf(const char *fmt, ...);

// Built-in agent images generated at build time
#include "../../kernel/init_bin.h"
#include "../../kernel/login_bin.h"

// Shared filesystem instance defined in nosfs.c
extern nosfs_fs_t nosfs_root;

// Keep readiness private here; expose via nosfs_is_ready()
static _Atomic int nosfs_ready = 0;
int nosfs_is_ready(void) { return atomic_load(&nosfs_ready); }

// Optional: enumerate files present (nice for early boot-debug)
static void nosfs_debug_list_all(void) {
    char names[32][NOSFS_NAME_LEN];
    int n = nosfs_list(&nosfs_root, names, 32);
    for (int i = 0; i < n; ++i)
        kprintf("[nosfs] file[%d]=%s\n", i, names[i]);
}

// Simple message-driven filesystem server. Each request is handled
// sequentially and the response is sent back on the same queue.
void nosfs_server(ipc_queue_t *q, uint32_t self_id) {
    (void)self_id;

    // Initialise the in-memory FS
    nosfs_init(&nosfs_root);

    // Preload essential agents so the registry can launch them.
    // IMPORTANT: store WITHOUT leading slash so fs_read_all() (which strips one) will match.
    int h = nosfs_create(&nosfs_root, "agents/init.mo2", init_bin_len, 0);
    if (h >= 0) (void)nosfs_write(&nosfs_root, h, 0, init_bin, init_bin_len);

    h = nosfs_create(&nosfs_root, "agents/login.mo2", login_bin_len, 0);
    if (h >= 0) (void)nosfs_write(&nosfs_root, h, 0, login_bin, login_bin_len);

    // Make filesystem contents visible to other threads
    atomic_store(&nosfs_ready, 1);

    // Optional one-time debug listing (uncomment if needed)
    nosfs_debug_list_all();

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

        case NOSFS_MSG_READ: {
            int rc = nosfs_read(&nosfs_root, msg.arg1, msg.arg2,
                                resp.data, msg.len);
            resp.arg1 = rc;
            resp.len  = (rc == 0) ? msg.len : 0;
            break;
        }

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
            resp.len  = (resp.arg1 > 0) ? (resp.arg1 * NOSFS_NAME_LEN) : 0;
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
