#include "ipc.h"
#include "../../user/libc/libc.h"

#ifdef IPC_DEBUG
#include "../drivers/IO/serial.h"

// Print uint32_t as decimal string
static void utoa_dec(uint32_t val, char *buf) {
    char tmp[16];
    int i = 0, j = 0;
    if (!val) { buf[0] = '0'; buf[1] = 0; return; }
    while (val && i < (int)sizeof(tmp)) { tmp[i++] = '0' + (val % 10); val /= 10; }
    while (i) buf[j++] = tmp[--i];
    buf[j] = 0;
}

// Weak stub for serial output (linker will override if available)
__attribute__((weak)) void serial_puts(const char *s) { (void)s; }
#endif

// Weak stub for yield (linker will override if available)
__attribute__((weak)) void thread_yield(void) {}

/**
 * Initialize an IPC queue for message passing.
 */
void ipc_init(ipc_queue_t *q) {
    if (!q) return;
    memset(q, 0, sizeof(*q));
}

/**
 * Grant send/receive capabilities to a task for this queue.
 * Returns 0 on success, -1 on invalid args.
 */
int ipc_grant(ipc_queue_t *q, uint32_t task_id, uint32_t caps) {
    if (!q || task_id >= IPC_MAX_TASKS)
        return -1;
    q->caps[task_id] |= caps;
#ifdef IPC_DEBUG
    char buf[16];
    serial_puts("[ipc][grant] task="); utoa_dec(task_id, buf); serial_puts(buf);
    serial_puts(" caps="); utoa_dec(caps, buf); serial_puts(buf);
    serial_puts("\n");
#endif
    return 0;
}

/**
 * Revoke send/receive capabilities from a task for this queue.
 * Returns 0 on success, -1 on invalid args.
 */
int ipc_revoke(ipc_queue_t *q, uint32_t task_id, uint32_t caps) {
    if (!q || task_id >= IPC_MAX_TASKS)
        return -1;
    q->caps[task_id] &= ~caps;
#ifdef IPC_DEBUG
    char buf[16];
    serial_puts("[ipc][revoke] task="); utoa_dec(task_id, buf); serial_puts(buf);
    serial_puts(" caps="); utoa_dec(caps, buf); serial_puts(buf);
    serial_puts("\n");
#endif
    return 0;
}

/**
 * Send a message into the queue (non-blocking).
 * Returns:
 *  0  = Success
 * -1  = Queue full
 * -2  = Unauthorized sender
 * -3  = Message too large
 * -4  = Null pointer argument
 */
int ipc_send(ipc_queue_t *q, uint32_t sender_id, ipc_message_t *msg) {
    if (!q || !msg) {
#ifdef IPC_DEBUG
        serial_puts("[ipc][send] null ptr\n");
#endif
        return -4;
    }
    if (sender_id >= IPC_MAX_TASKS || !(q->caps[sender_id] & IPC_CAP_SEND)) {
#ifdef IPC_DEBUG
        serial_puts("[ipc][send] unauthorized\n");
#endif
        return -2;
    }
    if (msg->len > IPC_MSG_DATA_MAX) {
#ifdef IPC_DEBUG
        serial_puts("[ipc][send] too big\n");
#endif
        return -3;
    }
    size_t next = (q->head + 1) % IPC_QUEUE_SIZE;
    if (next == q->tail) {
#ifdef IPC_DEBUG
        serial_puts("[ipc][send] full\n");
#endif
        return -1;
    }
#ifdef IPC_DEBUG
    char buf[16];
    serial_puts("[ipc][send] id="); utoa_dec(sender_id, buf); serial_puts(buf);
    serial_puts(" type="); utoa_dec(msg->type, buf); serial_puts(buf);
    serial_puts(" len="); utoa_dec(msg->len, buf); serial_puts(buf);
    serial_puts(" head="); utoa_dec(q->head, buf); serial_puts(buf);
    serial_puts(" tail="); utoa_dec(q->tail, buf); serial_puts(buf);
    serial_puts("\n");
#endif
    msg->sender = sender_id;
    q->msgs[q->head] = *msg;
    q->head = next;
    return 0;
}

/**
 * Receive a message from the queue (non-blocking).
 * Returns:
 *  0  = Success
 * -1  = Queue empty
 * -2  = Unauthorized receiver
 * -4  = Null pointer argument
 */
int ipc_receive(ipc_queue_t *q, uint32_t receiver_id, ipc_message_t *msg) {
    if (!q || !msg) {
#ifdef IPC_DEBUG
        serial_puts("[ipc][recv] null ptr\n");
#endif
        return -4;
    }
    if (receiver_id >= IPC_MAX_TASKS || !(q->caps[receiver_id] & IPC_CAP_RECV)) {
#ifdef IPC_DEBUG
        serial_puts("[ipc][recv] unauthorized\n");
#endif
        return -2;
    }
    if (q->tail == q->head) {
#ifdef IPC_DEBUG
        serial_puts("[ipc][recv] empty\n");
#endif
        // Do NOT yield here; caller chooses blocking or polling.
        return -1;
    }
    *msg = q->msgs[q->tail];
    q->tail = (q->tail + 1) % IPC_QUEUE_SIZE;
#ifdef IPC_DEBUG
    char buf[16];
    serial_puts("[ipc][recv] id="); utoa_dec(receiver_id, buf); serial_puts(buf);
    serial_puts(" type="); utoa_dec(msg->type, buf); serial_puts(buf);
    serial_puts(" len="); utoa_dec(msg->len, buf); serial_puts(buf);
    serial_puts(" head="); utoa_dec(q->head, buf); serial_puts(buf);
    serial_puts(" tail="); utoa_dec(q->tail, buf); serial_puts(buf);
    serial_puts("\n");
#endif
    return 0;
}

/**
 * Receive a message from the queue, blocking (with yield) until one is available.
 * Returns 0 on success, <0 on error.
 */
int ipc_receive_blocking(ipc_queue_t *q, uint32_t receiver_id, ipc_message_t *msg) {
    int ret;
    while ((ret = ipc_receive(q, receiver_id, msg)) == -1) {
        thread_yield();
    }
    return ret;
}

/**
 * Helper: Peek at next message type in the queue, non-destructive.
 * Returns message type or -1 if empty.
 */
int ipc_peek_type(ipc_queue_t *q) {
    if (!q || q->tail == q->head) return -1;
    return q->msgs[q->tail].type;
}

/**
 * Helper: Returns the number of messages currently in the queue.
 */
size_t ipc_queue_len(ipc_queue_t *q) {
    if (!q) return 0;
    if (q->head >= q->tail)
        return q->head - q->tail;
    else
        return (IPC_QUEUE_SIZE - q->tail) + q->head;
}
