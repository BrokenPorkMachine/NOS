#include <assert.h>
#include <string.h>
#include "../../kernel/IPC/ipc.h"
#include "../../user/libc/libc.h"

void thread_yield(void) {}

int main(void) {
    ipc_queue_t q;
    ipc_init(&q);
    ipc_grant(&q, 1, IPC_CAP_SEND);
    ipc_grant(&q, 2, IPC_CAP_RECV);

    ipc_message_t msg = { .type = 1, .len = 4 };
    memcpy(msg.data, "test", 4);
    int ret = ipc_send(&q, 1, &msg);
    assert(ret == 0);
    ipc_message_t out;
    ret = ipc_receive(&q, 2, &out);
    assert(ret == 0);
    assert(out.type == 1);
    assert(out.len == 4);
    assert(memcmp(out.data, "test", 4) == 0);

    ret = ipc_send(&q, 2, &msg);
    assert(ret == -2);

    ret = ipc_receive(&q, 2, &out);
    assert(ret == -1);

    return 0;
}
