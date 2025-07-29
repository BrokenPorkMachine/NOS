#include "nitrfs.h"
#include "../../IPC/ipc.h"
#include "../../src/libc.h"

enum {
    NITRFS_MSG_CREATE = 1,
    NITRFS_MSG_WRITE,
    NITRFS_MSG_READ,
    NITRFS_MSG_DELETE,
    NITRFS_MSG_LIST
};

void nitrfs_server(ipc_queue_t *q) {
    nitrfs_fs_t fs;
    nitrfs_init(&fs);
    ipc_message_t msg;
    ipc_message_t reply;
    for (;;) {
        if (ipc_receive(q, &msg) != 0)
            continue;
        int handle;
        int ret;
        switch (msg.type) {
        case NITRFS_MSG_CREATE:
            ret = nitrfs_create(&fs, (const char*)msg.data, msg.arg1, msg.arg2);
            reply.type = msg.type;
            reply.arg1 = ret;
            ipc_send(q, &reply);
            break;
        case NITRFS_MSG_WRITE:
            handle = msg.arg1;
            ret = nitrfs_write(&fs, handle, 0, msg.data, msg.arg2);
            reply.type = msg.type;
            reply.arg1 = ret;
            ipc_send(q, &reply);
            break;
        case NITRFS_MSG_READ:
            handle = msg.arg1;
            ret = nitrfs_read(&fs, handle, 0, reply.data, msg.arg2);
            reply.type = msg.type;
            reply.arg1 = ret;
            reply.arg2 = msg.arg2;
            ipc_send(q, &reply);
            break;
        case NITRFS_MSG_DELETE:
            handle = msg.arg1;
            ret = nitrfs_delete(&fs, handle);
            reply.type = msg.type;
            reply.arg1 = ret;
            ipc_send(q, &reply);
            break;
        case NITRFS_MSG_LIST:
            reply.arg1 = nitrfs_list(&fs, (char (*)[NITRFS_NAME_LEN])reply.data,
                                     IPC_MSG_DATA_MAX / NITRFS_NAME_LEN);
            reply.type = msg.type;
            ipc_send(q, &reply);
            break;
        default:
            break;
        }
    }
}
