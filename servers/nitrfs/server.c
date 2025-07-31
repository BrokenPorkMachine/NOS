#include "nitrfs.h"
#include "../../IPC/ipc.h"
#include "../../src/libc.h"
#include "server.h"

// Optional: status codes for replies (e.g. for error reporting in the shell)
enum {
    NITRFS_OK = 0,
    NITRFS_ERR = -1
};

void nitrfs_server(ipc_queue_t *q, uint32_t self_id) {
    nitrfs_fs_t fs;
    nitrfs_init(&fs);
    ipc_message_t msg, reply;
    int handle, ret;

    while (1) {
        // Block for a message
        if (ipc_receive(q, self_id, &msg) != 0)
            continue;
        if (msg.len > IPC_MSG_DATA_MAX)
            continue; // Ignore bogus message

        // Clear reply
        memset(&reply, 0, sizeof(reply));
        reply.type = msg.type;

        switch (msg.type) {
        case NITRFS_MSG_CREATE:
            ret = nitrfs_create(&fs, (const char*)msg.data, msg.arg1, msg.arg2);
            reply.arg1 = ret; // Handle or error
            break;
        case NITRFS_MSG_WRITE:
            handle = msg.arg1;
            ret = nitrfs_write(&fs, handle, 0, msg.data, msg.arg2);
            reply.arg1 = ret; // 0 = ok
            break;
        case NITRFS_MSG_READ:
            handle = msg.arg1;
            ret = nitrfs_read(&fs, handle, 0, reply.data, msg.arg2);
            reply.arg1 = ret; // 0 = ok
            reply.arg2 = msg.arg2;
            reply.len  = (ret == 0) ? msg.arg2 : 0;
            break;
        case NITRFS_MSG_DELETE:
            handle = msg.arg1;
            ret = nitrfs_delete(&fs, handle);
            reply.arg1 = ret;
            break;
        case NITRFS_MSG_LIST:
            reply.arg1 = nitrfs_list(&fs, (char (*)[NITRFS_NAME_LEN])reply.data,
                                     IPC_MSG_DATA_MAX / NITRFS_NAME_LEN);
            reply.len  = reply.arg1 * NITRFS_NAME_LEN;
            break;
        case NITRFS_MSG_CRC:
            handle = msg.arg1;
            ret = nitrfs_compute_crc(&fs, handle);
            reply.arg1 = ret;
            if (ret == 0)
                reply.arg2 = fs.files[handle].crc32;
            break;
        case NITRFS_MSG_VERIFY:
            handle = msg.arg1;
            ret = nitrfs_verify(&fs, handle);
            reply.arg1 = ret;
            break;
        default:
            reply.arg1 = NITRFS_ERR;
            // Optionally: Fill reply.data with "unknown op" etc.
            break;
        }
        ipc_send(q, self_id, &reply);
    }
}
