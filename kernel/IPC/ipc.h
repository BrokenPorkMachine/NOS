#ifndef IPC_H
#define IPC_H

#include <stdint.h>
#include <stddef.h>

#define IPC_MSG_DATA_MAX 64
#define IPC_QUEUE_SIZE   16

// Capability flags
#define IPC_CAP_SEND 0x1
#define IPC_CAP_RECV 0x2

// Maximum tasks supported for capability tracking
#define IPC_MAX_TASKS 32

/**
 * Structure of a message for IPC.
 */
typedef struct {
    uint32_t type;
    uint32_t sender;      // Thread ID of sender
    uint32_t arg1;
    uint32_t arg2;
    uint32_t len;         // Number of valid bytes in data[]
    uint8_t  data[IPC_MSG_DATA_MAX];
} ipc_message_t;

/**
 * Simple ring-buffer queue for IPC between threads with
 * capability-based access control. Each task can be granted
 * IPC_CAP_SEND and/or IPC_CAP_RECV capabilities.
 */
typedef struct {
    ipc_message_t msgs[IPC_QUEUE_SIZE];
    size_t head;          // Points to oldest element
    size_t tail;          // Points to next write slot
    uint32_t caps[IPC_MAX_TASKS]; // Capability bits per task ID
} ipc_queue_t;

/**
 * Initialize an IPC queue.
 */
void ipc_init(ipc_queue_t *q);

/**
 * Grant capabilities to a task on this queue.
 * caps: IPC_CAP_SEND and/or IPC_CAP_RECV
 */
int ipc_grant(ipc_queue_t *q, uint32_t task_id, uint32_t caps);

/**
 * Revoke capabilities from a task on this queue.
 * caps: IPC_CAP_SEND and/or IPC_CAP_RECV
 */
int ipc_revoke(ipc_queue_t *q, uint32_t task_id, uint32_t caps);

/**
 * Send a message to the queue.
 * Returns 0 on success, <0 on error/full/not permitted.
 */
int  ipc_send(ipc_queue_t *q, uint32_t sender_id, ipc_message_t *msg);

/**
 * Receive a message from the queue (blocking or polling).
 * Returns 0 on success, <0 on error/empty/not permitted.
 */
int  ipc_receive(ipc_queue_t *q, uint32_t receiver_id, ipc_message_t *msg);

#endif // IPC_H
