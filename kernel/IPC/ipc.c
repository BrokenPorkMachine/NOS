#include "ipc.h"
#include "../../user/libc/libc.h"
#include "../drivers/IO/serial.h"

// thread_yield can be provided by the threading subsystem. Provide a weak

/**
 * Initialize an IPC queue for message passing.
 */
void ipc_init(ipc_queue_t *q) {
    if (!q) return;
    memset(q, 0, sizeof(*q));
}

int ipc_grant(ipc_queue_t *q, uint32_t task_id, uint32_t caps) {
    if (!q || task_id >= IPC_MAX_TASKS)
        return -1;
    q->caps[task_id] |= caps;
    return 0;
}

int ipc_revoke(ipc_queue_t *q, uint32_t task_id, uint32_t caps) {
    if (!q || task_id >= IPC_MAX_TASKS)
        return -1;
    q->caps[task_id] &= ~caps;
    return 0;
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
    if (sender_id >= IPC_MAX_TASKS || !(q->caps[sender_id] & IPC_CAP_SEND))
        return -2; // unauthorized sender
    if (msg->len > IPC_MSG_DATA_MAX)
        return -3; // invalid message length
    size_t next = (q->head + 1) % IPC_QUEUE_SIZE;
    if (next == q->tail)
        return -1; // queue full
    serial_puts("[ipc] send from ");
    {
        char buf[16];
        utoa_dec(sender_id, buf); serial_puts(buf);
        serial_puts(" len=");
        utoa_dec(msg->len, buf); serial_puts(buf);
        serial_puts(" head=");
        utoa_dec(q->head, buf); serial_puts(buf);
        serial_puts(" tail=");
        utoa_dec(q->tail, buf); serial_puts(buf);
        serial_puts("\n");
    }
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
    if (receiver_id >= IPC_MAX_TASKS || !(q->caps[receiver_id] & IPC_CAP_RECV))
        return -2; // unauthorized receiver
    if (q->tail == q->head) {
        thread_yield();
        return -1; // queue empty
    }
    *msg = q->msgs[q->tail];
    serial_puts("[ipc] recv deliver to ");
    {
        char buf[16];
        utoa_dec(receiver_id, buf); serial_puts(buf);
        serial_puts(" from ");
        utoa_dec(msg->sender, buf); serial_puts(buf);
        serial_puts(" idx=");
        utoa_dec(q->tail, buf); serial_puts(buf);
        serial_puts("\n");
    }
    q->tail = (q->tail + 1) % IPC_QUEUE_SIZE;
    return 0;
}
