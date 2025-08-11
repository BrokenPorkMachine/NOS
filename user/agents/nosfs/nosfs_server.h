#ifndef NOSFS_SERVER_H
#define NOSFS_SERVER_H

#include "../../../kernel/IPC/ipc.h"

// NOSFS IPC message types (match these in your nsh code)
enum {
    NOSFS_MSG_CREATE = 1,   // Create file (msg.data=name, arg1=capacity, arg2=perm)
    NOSFS_MSG_WRITE,        // Write file  (arg1=handle, arg2=offset, msg.len=len, msg.data=buffer)
    NOSFS_MSG_READ,         // Read file   (arg1=handle, arg2=offset, msg.len=len)
    NOSFS_MSG_DELETE,       // Delete file (arg1=handle)
    NOSFS_MSG_RENAME,       // Rename file (arg1=handle, msg.data=new name)
    NOSFS_MSG_LIST,         // List files  (no args)
    NOSFS_MSG_CRC,          // Compute CRC (arg1=handle)
    NOSFS_MSG_VERIFY,       // Verify CRC  (arg1=handle)
    NOSFS_MSG_RESERVED
};

void nosfs_server(ipc_queue_t *q, uint32_t self_id);

// Returns non-zero once nosfs_server has initialized and preloaded files.

#pragma once
int nosfs_is_ready(void);

#endif // NOSFS_SERVER_H
