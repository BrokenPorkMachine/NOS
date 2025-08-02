#ifndef IPC_H
#define IPC_H

#include <stdint.h>
#include <stddef.h>

#define IPC_MSG_DATA_MAX 64
#define IPC_QUEUE_SIZE   16

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
 * Simple ring-buffer queue for IPC between threads.
 * send_mask: which thread IDs can send
 * recv_mask: which thread IDs can receive
 */
typedef struct {
    ipc_message_t msgs[IPC_QUEUE_SIZE];
    size_t head;          // Points to oldest element
    size_t tail;          // Points to next write slot
    uint32_t send_mask;   // Bitmask of allowed senders
    uint32_t recv_mask;   // Bitmask of allowed receivers
} ipc_queue_t;

/**
 * Initialize an IPC queue.
 * send_mask/recv_mask: bitmasks of allowed sender/receiver thread IDs
 */
void ipc_init(ipc_queue_t *q, uint32_t send_mask, uint32_t recv_mask);

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
