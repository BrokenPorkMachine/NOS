#ifndef NITRFS_SERVER_H
#define NITRFS_SERVER_H
#include "../../IPC/ipc.h"

enum {
    NITRFS_MSG_CREATE = 1,
    NITRFS_MSG_WRITE,
    NITRFS_MSG_READ,
    NITRFS_MSG_DELETE,
    NITRFS_MSG_LIST,
    NITRFS_MSG_CRC,
    NITRFS_MSG_VERIFY
};

void nitrfs_server(ipc_queue_t *q, uint32_t self_id);
#endif
