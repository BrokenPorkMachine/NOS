#include "ipc.h"
#include "../src/libc.h"
#include "../Task/thread.h"

void ipc_init(ipc_queue_t *q, uint32_t send_mask, uint32_t recv_mask) {
    memset(q, 0, sizeof(*q));
    q->send_mask = send_mask;
    q->recv_mask = recv_mask;
}

int ipc_send(ipc_queue_t *q, ipc_message_t *msg) {
    if (!(q->send_mask & (1u << current->id)))
        return -2; /* unauthorized */
    if (msg->len > IPC_MSG_DATA_MAX)
        return -3; /* invalid length */
    size_t next = (q->head + 1) % IPC_QUEUE_SIZE;
    if (next == q->tail)
        return -1; /* full */
    msg->sender = current->id;
    q->msgs[q->head] = *msg;
    q->head = next;
    return 0;
}

int ipc_receive(ipc_queue_t *q, ipc_message_t *msg) {
    if (!(q->recv_mask & (1u << current->id)))
        return -2; /* unauthorized */
    if (q->tail == q->head)
        return -1; /* empty */
    *msg = q->msgs[q->tail];
    q->tail = (q->tail + 1) % IPC_QUEUE_SIZE;
    return 0;
}
