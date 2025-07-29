#include "ipc.h"
#include "../src/libc.h"

void ipc_init(ipc_queue_t *q) {
    memset(q, 0, sizeof(*q));
}

int ipc_send(ipc_queue_t *q, const ipc_message_t *msg) {
    size_t next = (q->head + 1) % IPC_QUEUE_SIZE;
    if (next == q->tail)
        return -1; // full
    q->msgs[q->head] = *msg;
    q->head = next;
    return 0;
}

int ipc_receive(ipc_queue_t *q, ipc_message_t *msg) {
    if (q->tail == q->head)
        return -1; // empty
    *msg = q->msgs[q->tail];
    q->tail = (q->tail + 1) % IPC_QUEUE_SIZE;
    return 0;
}
