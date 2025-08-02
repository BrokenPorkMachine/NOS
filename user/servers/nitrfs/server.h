#ifndef NITRFS_SERVER_H
#define NITRFS_SERVER_H

#include "../../../kernel/IPC/ipc.h"

// NITRFS IPC message types (match these in your shell/server code)
enum {
    NITRFS_MSG_CREATE = 1,   // Create file (msg.data=name, arg1=capacity, arg2=perm)
    NITRFS_MSG_WRITE,        // Write file  (arg1=handle, arg2=offset, msg.len=len, msg.data=buffer)
    NITRFS_MSG_READ,         // Read file   (arg1=handle, arg2=offset, msg.len=len)
    NITRFS_MSG_DELETE,       // Delete file (arg1=handle)
    NITRFS_MSG_RENAME,       // Rename file (arg1=handle, msg.data=new name)
    NITRFS_MSG_LIST,         // List files  (no args)
    NITRFS_MSG_CRC,          // Compute CRC (arg1=handle)
    NITRFS_MSG_VERIFY,       // Verify CRC  (arg1=handle)
    // Add new messages above this line if needed.
    NITRFS_MSG_RESERVED      // Not used: for forward compatibility!
};

// Server entry point: call from main thread/task.
void nitrfs_server(ipc_queue_t *q, uint32_t self_id);

#endif // NITRFS_SERVER_H
