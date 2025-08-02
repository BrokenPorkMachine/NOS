#include "window.h"
#include "../../libc/libc.h"
#include "../../../kernel/Task/thread.h"

extern thread_t *current_cpu[MAX_CPUS];
extern uint32_t smp_cpu_index(void);

static inline uint32_t self_id(void) {
    thread_t *t = current_cpu[smp_cpu_index()];
    return t ? (uint32_t)t->id : 0;
}

int window_create(ipc_queue_t *q, uint32_t server,
                  uint32_t x, uint32_t y, uint32_t w, uint32_t h) {
    ipc_message_t msg = {0}, reply = {0};
    uint32_t dims[2] = {w, h};
    msg.type = WINDOW_MSG_CREATE;
    msg.arg1 = x;
    msg.arg2 = y;
    msg.len  = sizeof(dims);
    memcpy(msg.data, dims, sizeof(dims));
    ipc_send(q, server, &msg);
    if (ipc_receive(q, self_id(), &reply) != 0)
        return -1;
    return (int)reply.arg1;
}

int window_draw_rect(ipc_queue_t *q, uint32_t server, uint32_t window,
                     uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t color) {
    ipc_message_t msg = {0};
    uint32_t data[5] = {x, y, w, h, color};
    msg.type = WINDOW_MSG_DRAW_RECT;
    msg.arg1 = window;
    msg.len  = sizeof(data);
    memcpy(msg.data, data, sizeof(data));
    return ipc_send(q, server, &msg);
}

int window_get_event(ipc_queue_t *q, ipc_message_t *ev) {
    return ipc_receive(q, self_id(), ev);
}
