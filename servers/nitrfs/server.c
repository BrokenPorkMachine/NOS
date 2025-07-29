#include "nitrfs.h"
#include "../../IPC/ipc.h"
#include "../../src/libc.h"
#include "server.h"

void nitrfs_server(ipc_queue_t *q) {
    nitrfs_fs_t fs;
    nitrfs_init(&fs);
    ipc_message_t msg;
    ipc_message_t reply;
    for (;;) {
        if (ipc_receive(q, &msg) != 0)
            continue;
        if (msg.len > IPC_MSG_DATA_MAX)
            continue; /* ignore bogus message */
        int handle;
        int ret;
        switch (msg.type) {
        case NITRFS_MSG_CREATE:
            ret = nitrfs_create(&fs, (const char*)msg.data, msg.arg1, msg.arg2);
            reply.type = msg.type;
            reply.arg1 = ret;
            reply.len  = 0;
            ipc_send(q, &reply);
            break;
        case NITRFS_MSG_WRITE:
            handle = msg.arg1;
            ret = nitrfs_write(&fs, handle, 0, msg.data, msg.arg2);
            reply.type = msg.type;
            reply.arg1 = ret;
            reply.len  = 0;
            ipc_send(q, &reply);
            break;
        case NITRFS_MSG_READ:
            handle = msg.arg1;
            ret = nitrfs_read(&fs, handle, 0, reply.data, msg.arg2);
            reply.type = msg.type;
            reply.arg1 = ret;
            reply.arg2 = msg.arg2;
            reply.len  = (ret==0)? msg.arg2 : 0;
            ipc_send(q, &reply);
            break;
        case NITRFS_MSG_DELETE:
            handle = msg.arg1;
            ret = nitrfs_delete(&fs, handle);
            reply.type = msg.type;
            reply.arg1 = ret;
            reply.len  = 0;
            ipc_send(q, &reply);
            break;
        case NITRFS_MSG_LIST:
            reply.arg1 = nitrfs_list(&fs, (char (*)[NITRFS_NAME_LEN])reply.data,
                                     IPC_MSG_DATA_MAX / NITRFS_NAME_LEN);
            reply.type = msg.type;
            reply.len  = reply.arg1 * NITRFS_NAME_LEN;
            ipc_send(q, &reply);
            break;
        case NITRFS_MSG_CRC:
            handle = msg.arg1;
            ret = nitrfs_compute_crc(&fs, handle);
            reply.type = msg.type;
            reply.arg1 = ret;
            if (ret == 0)
                reply.arg2 = fs.files[handle].crc32;
            reply.len  = 0;
            ipc_send(q, &reply);
            break;
        case NITRFS_MSG_VERIFY:
            handle = msg.arg1;
            ret = nitrfs_verify(&fs, handle);
            reply.type = msg.type;
            reply.arg1 = ret;
            reply.len  = 0;
            ipc_send(q, &reply);
            break;
        default:
            break;
        }
    }
}
