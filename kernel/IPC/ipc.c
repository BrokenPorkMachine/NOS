#include "ipc.h"
#include "../../user/libc/libc.h"

/**
 * Initialize an IPC queue for message passing.
 */
void ipc_init(ipc_queue_t *q, uint32_t send_mask, uint32_t recv_mask) {
    if (!q) return;
    memset(q, 0, sizeof(*q));
    q->send_mask = send_mask;
    q->recv_mask = recv_mask;
}

/**
 * Send a message into the queue.
 * Returns:
 *  0  = Success
 * -1  = Queue full
 * -2  = Unauthorized sender
 * -3  = Message too large
 * -4  = Null pointer argument
 */
int ipc_send(ipc_queue_t *q, uint32_t sender_id, ipc_message_t *msg) {
    if (!q || !msg) return -4;
    if (!(q->send_mask & (1u << sender_id)))
        return -2; // unauthorized sender
    if (msg->len > IPC_MSG_DATA_MAX)
        return -3; // invalid message length
    size_t next = (q->head + 1) % IPC_QUEUE_SIZE;
    if (next == q->tail)
        return -1; // queue full
    msg->sender = sender_id;
    q->msgs[q->head] = *msg;
    q->head = next;
    return 0;
}

/**
 * Receive a message from the queue.
 * Returns:
 *  0  = Success
 * -1  = Queue empty
 * -2  = Unauthorized receiver
 * -4  = Null pointer argument
 */
int ipc_receive(ipc_queue_t *q, uint32_t receiver_id, ipc_message_t *msg) {
    if (!q || !msg) return -4;
    if (!(q->recv_mask & (1u << receiver_id)))
        return -2; // unauthorized receiver
    if (q->tail == q->head)
        return -1; // queue empty
    *msg = q->msgs[q->tail];
    q->tail = (q->tail + 1) % IPC_QUEUE_SIZE;
    return 0;
}
