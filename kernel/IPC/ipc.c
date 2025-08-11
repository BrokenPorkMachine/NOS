#include "ipc.h"
#include "../../user/libc/libc.h"
#include "../Task/thread.h"

/* schedule() is implemented in the thread subsystem but not declared in a
 * public header. Declare it here to allow ipc_receive_blocking to yield
 * via the scheduler when necessary. */
extern void schedule(void);

#ifdef IPC_DEBUG
#include "../drivers/IO/serial.h"
#endif

#ifdef IPC_DEBUG
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

// --- Internal helpers -------------------------------------------------

static inline size_t idx_next(size_t i) {
    size_t n = i + 1;
    return (n == IPC_QUEUE_SIZE) ? 0 : n;
}

static inline int queue_empty(const ipc_queue_t *q
#ifdef IPC_SMP
                              , size_t head, size_t tail
#endif
) {
#ifdef IPC_SMP
    (void)q;
    return head == tail;
#else
    return q->tail == q->head;
#endif
}

static inline int queue_full(const ipc_queue_t *q
#ifdef IPC_SMP
                             , size_t head, size_t tail
#endif
) {
#ifdef IPC_SMP
    (void)q;
    return idx_next(head) == tail;
#else
    return idx_next(q->head) == q->tail;
#endif
}

#ifdef IPC_SMP
// Atomic wrappers for head/tail with acquire/release semantics
static inline size_t load_head(const ipc_queue_t *q) {
    return __atomic_load_n(&q->head, __ATOMIC_ACQUIRE);
}
static inline size_t load_tail(const ipc_queue_t *q) {
    return __atomic_load_n(&q->tail, __ATOMIC_ACQUIRE);
}
static inline void store_head(ipc_queue_t *q, size_t v) {
    __atomic_store_n(&q->head, v, __ATOMIC_RELEASE);
}
static inline void store_tail(ipc_queue_t *q, size_t v) {
    __atomic_store_n(&q->tail, v, __ATOMIC_RELEASE);
}
#else
static inline size_t load_head(const ipc_queue_t *q) { return q->head; }
static inline size_t load_tail(const ipc_queue_t *q) { return q->tail; }
static inline void store_head(ipc_queue_t *q, size_t v) { q->head = v; }
static inline void store_tail(ipc_queue_t *q, size_t v) { q->tail = v; }
#endif

// --- API --------------------------------------------------------------

void ipc_init(ipc_queue_t *q) {
    if (!q) return;
    memset(q, 0, sizeof(*q));
}

int ipc_grant(ipc_queue_t *q, uint32_t task_id, uint32_t caps) {
    if (!q || task_id >= IPC_MAX_TASKS) return -1;
    q->caps[task_id] |= caps;
#ifdef IPC_DEBUG
    char buf[16];
    serial_puts("[ipc][grant] task="); utoa_dec(task_id, buf); serial_puts(buf);
    serial_puts(" caps="); utoa_dec(caps, buf); serial_puts(buf);
    serial_puts("\n");
#endif
    return 0;
}

int ipc_revoke(ipc_queue_t *q, uint32_t task_id, uint32_t caps) {
    if (!q || task_id >= IPC_MAX_TASKS) return -1;
    q->caps[task_id] &= ~caps;
#ifdef IPC_DEBUG
    char buf[16];
    serial_puts("[ipc][revoke] task="); utoa_dec(task_id, buf); serial_puts(buf);
    serial_puts(" caps="); utoa_dec(caps, buf); serial_puts(buf);
    serial_puts("\n");
#endif
    return 0;
}

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

    // Load indices (atomically if SMP)
    size_t head = load_head(q);
    size_t tail = load_tail(q);

    if (queue_full(q, head, tail)) {
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
    serial_puts(" head="); utoa_dec((uint32_t)head, buf); serial_puts(buf);
    serial_puts(" tail="); utoa_dec((uint32_t)tail, buf); serial_puts(buf);
    serial_puts("\n");
#endif

    // Stamp sender and copy the message into the slot
    ipc_message_t m = *msg;
    m.sender = sender_id;
    q->msgs[head] = m; // POD copy; equivalent to memcpy

    // Advance head
    store_head(q, idx_next(head));

    // If a thread was blocked waiting on this queue, wake it now that a
    // message is available.
    if (q->waiting_thread) {
        thread_unblock((thread_t *)q->waiting_thread);
        q->waiting_thread = NULL;
    }
    return 0;
}

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

    size_t head = load_head(q);
    size_t tail = load_tail(q);

    if (queue_empty(q, head, tail)) {
#ifdef IPC_DEBUG
        serial_puts("[ipc][recv] empty\n");
#endif
        return -1;
    }

    *msg = q->msgs[tail];
    store_tail(q, idx_next(tail));

#ifdef IPC_DEBUG
    char buf[16];
    serial_puts("[ipc][recv] id="); utoa_dec(receiver_id, buf); serial_puts(buf);
    serial_puts(" type="); utoa_dec(msg->type, buf); serial_puts(buf);
    serial_puts(" len="); utoa_dec(msg->len, buf); serial_puts(buf);
    serial_puts(" head="); utoa_dec((uint32_t)load_head(q), buf); serial_puts(buf);
    serial_puts(" tail="); utoa_dec((uint32_t)load_tail(q), buf); serial_puts(buf);
    serial_puts("\n");
#endif

    return 0;
}

int ipc_receive_blocking(ipc_queue_t *q, uint32_t receiver_id, ipc_message_t *msg) {
    int ret;
    while ((ret = ipc_receive(q, receiver_id, msg)) == -1) {
        q->waiting_thread = thread_current();
        thread_block((thread_t *)q->waiting_thread);
        schedule();
    }
    return ret;
}

int ipc_peek_type(ipc_queue_t *q) {
    if (!q) return -1;
#ifdef IPC_SMP
    size_t head = load_head(q), tail = load_tail(q);
    if (head == tail) return -1;
    return (int)q->msgs[tail].type;
#else
    if (q->tail == q->head) return -1;
    return (int)q->msgs[q->tail].type;
#endif
}

size_t ipc_queue_len(ipc_queue_t *q) {
    if (!q) return 0;
#ifdef IPC_SMP
    size_t head = load_head(q), tail = load_tail(q);
    return (head >= tail) ? (head - tail) : (IPC_QUEUE_SIZE - tail + head);
#else
    if (q->head >= q->tail) return q->head - q->tail;
    return (IPC_QUEUE_SIZE - q->tail) + q->head;
#endif
}
