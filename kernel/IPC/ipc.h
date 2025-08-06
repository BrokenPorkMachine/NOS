#ifndef IPC_H
#define IPC_H

#include <stdint.h>
#include <stddef.h>

#define IPC_MSG_DATA_MAX 64
#define IPC_QUEUE_SIZE   16

// Health check message types (standardized)
#define IPC_HEALTH_PING 0x1000
#define IPC_HEALTH_PONG 0x1001

// Capability flags for per-task queue permissions
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
    size_t head;          // Points to next write slot
    size_t tail;          // Points to oldest element
    uint32_t caps[IPC_MAX_TASKS]; // Capability bits per task ID
} ipc_queue_t;

/**
 * Initialize an IPC queue.
 */
void ipc_init(ipc_queue_t *q);

/**
 * Grant capabilities to a task on this queue.
 * @param q      IPC queue
 * @param task_id Task/thread ID
 * @param caps   Bitmask of IPC_CAP_SEND and/or IPC_CAP_RECV
 * @return 0 on success, -1 on error/invalid args.
 */
int ipc_grant(ipc_queue_t *q, uint32_t task_id, uint32_t caps);

/**
 * Revoke capabilities from a task on this queue.
 * @param q      IPC queue
 * @param task_id Task/thread ID
 * @param caps   Bitmask of IPC_CAP_SEND and/or IPC_CAP_RECV
 * @return 0 on success, -1 on error/invalid args.
 */
int ipc_revoke(ipc_queue_t *q, uint32_t task_id, uint32_t caps);

/**
 * Send a message to the queue (non-blocking).
 * @param q         IPC queue
 * @param sender_id Sender task/thread ID
 * @param msg       Pointer to message to send
 * @return 0 on success,
 *        -1 if queue full,
 *        -2 if sender not authorized,
 *        -3 if message too large,
 *        -4 if null pointers.
 */
int ipc_send(ipc_queue_t *q, uint32_t sender_id, ipc_message_t *msg);

/**
 * Receive a message from the queue (non-blocking).
 * @param q           IPC queue
 * @param receiver_id Receiver task/thread ID
 * @param msg         Pointer to output message
 * @return 0 on success,
 *        -1 if queue empty,
 *        -2 if receiver not authorized,
 *        -4 if null pointers.
 */
int ipc_receive(ipc_queue_t *q, uint32_t receiver_id, ipc_message_t *msg);

/**
 * Blocking receive: spin/yield until a message is available.
 * @param q           IPC queue
 * @param receiver_id Receiver task/thread ID
 * @param msg         Pointer to output message
 * @return 0 on success, <0 on fatal error.
 */
int ipc_receive_blocking(ipc_queue_t *q, uint32_t receiver_id, ipc_message_t *msg);

/**
 * Peek at the type of the next message in the queue, non-destructive.
 * @param q IPC queue
 * @return Message type, or -1 if queue is empty or q is NULL.
 */
int ipc_peek_type(ipc_queue_t *q);

/**
 * Return the number of messages currently in the queue.
 * @param q IPC queue
 * @return Message count, or 0 if q is NULL.
 */
size_t ipc_queue_len(ipc_queue_t *q);

#endif // IPC_H
