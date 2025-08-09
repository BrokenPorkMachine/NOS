#ifndef IPC_H
#define IPC_H

#include <stdint.h>
#include <stddef.h>

/* --- IPC compile-time options --- */
/*
 * Define IPC_SMP=1 to enable atomic head/tail updates for
 * safe one-producer/one-consumer usage across multiple CPUs.
 * For multi-producer or multi-consumer SMP usage, you should
 * also add locking around ipc_send/ipc_receive calls.
 */
#ifndef IPC_SMP
#define IPC_SMP 0
#endif

/* Maximum payload size for message data (in bytes) */
#define IPC_MSG_DATA_MAX 64

/* Number of messages in the ring buffer (must be power of two for fast wrap) */
#define IPC_QUEUE_SIZE   16

/* Health check / diagnostics message types */
#define IPC_HEALTH_PING 0x1000
#define IPC_HEALTH_PONG 0x1001

/* Capability flags for per-task queue permissions */
#define IPC_CAP_SEND 0x1  /* Task can send messages */
#define IPC_CAP_RECV 0x2  /* Task can receive messages */

/* Maximum tasks supported for capability tracking */
#define IPC_MAX_TASKS 32

/**
 * IPC message structure
 * - All fields are POD; safe to copy via assignment.
 */
typedef struct {
    uint32_t type;                    /* Application-defined type */
    uint32_t sender;                  /* Task/thread ID of sender */
    uint32_t arg1;                     /* Optional argument */
    uint32_t arg2;                     /* Optional argument */
    uint32_t len;                      /* Number of valid bytes in data[] */
    uint8_t  data[IPC_MSG_DATA_MAX];   /* Payload */
} ipc_message_t;

/**
 * Ring-buffer queue for IPC between threads/tasks.
 * - head: index of next write slot
 * - tail: index of next read slot
 * - caps[]: per-task capabilities
 */
typedef struct {
    ipc_message_t msgs[IPC_QUEUE_SIZE];
#if IPC_SMP
    volatile size_t head;
    volatile size_t tail;
#else
    size_t head;
    size_t tail;
#endif
    uint32_t caps[IPC_MAX_TASKS];
} ipc_queue_t;

/* --- API --- */

/** Initialize an IPC queue to an empty state. */
void ipc_init(ipc_queue_t *q);

/** Grant capabilities to a task on this queue. */
int ipc_grant(ipc_queue_t *q, uint32_t task_id, uint32_t caps);

/** Revoke capabilities from a task on this queue. */
int ipc_revoke(ipc_queue_t *q, uint32_t task_id, uint32_t caps);

/**
 * Send a message to the queue (non-blocking).
 * @return
 *   0  = Success
 *  -1  = Queue full
 *  -2  = Unauthorized sender
 *  -3  = Message too large
 *  -4  = Null pointer
 */
int ipc_send(ipc_queue_t *q, uint32_t sender_id, ipc_message_t *msg);

/**
 * Receive a message from the queue (non-blocking).
 * @return
 *   0  = Success
 *  -1  = Queue empty
 *  -2  = Unauthorized receiver
 *  -4  = Null pointer
 */
int ipc_receive(ipc_queue_t *q, uint32_t receiver_id, ipc_message_t *msg);

/**
 * Receive a message from the queue, blocking until available.
 * - Calls thread_yield() between polls.
 * @return 0 on success, <0 on fatal error.
 */
int ipc_receive_blocking(ipc_queue_t *q, uint32_t receiver_id, ipc_message_t *msg);

/**
 * Peek at type of the next message without removing it.
 * @return Message type, or -1 if queue is empty or q is NULL.
 */
int ipc_peek_type(ipc_queue_t *q);

/** Get the number of messages currently in the queue. */
size_t ipc_queue_len(ipc_queue_t *q);

#endif /* IPC_H */
