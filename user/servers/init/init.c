#include "init.h"
#include "../../../kernel/drivers/IO/serial.h"
#include "../../../kernel/Task/thread.h"
#include "../../../kernel/IPC/ipc.h"
#include "../../../kernel/arch/CPU/smp.h"
#include "../../libc/libc.h"
#include "../login/login.h"
#include "../vnc/vnc.h"
#include "../ssh/ssh.h"

extern ipc_queue_t fs_queue;
extern ipc_queue_t pkg_queue;
extern ipc_queue_t upd_queue;

static void login_thread(void) { thread_t *c = current_cpu[smp_cpu_index()]; login_server(NULL, c->id); }
static void vnc_thread(void)   { thread_t *c = current_cpu[smp_cpu_index()]; vnc_server(NULL, c->id); }
static void ssh_thread(void)   { thread_t *c = current_cpu[smp_cpu_index()]; ssh_server(NULL, c->id); }

// Simple init/task spawner stub
void init_main(ipc_queue_t *q, uint32_t self_id) {
    (void)q; (void)self_id;
    serial_puts("[init] init server started\n");

    thread_t *t;

    t = thread_create(login_thread);
    ipc_grant(&fs_queue, t->id, IPC_CAP_SEND | IPC_CAP_RECV);
    ipc_grant(&pkg_queue, t->id, IPC_CAP_SEND | IPC_CAP_RECV);
    ipc_grant(&upd_queue, t->id, IPC_CAP_SEND | IPC_CAP_RECV);

    t = thread_create(vnc_thread);
    ipc_grant(&fs_queue, t->id, IPC_CAP_SEND | IPC_CAP_RECV);

    t = thread_create(ssh_thread);
    ipc_grant(&fs_queue, t->id, IPC_CAP_SEND | IPC_CAP_RECV);

    for (;;) {
        thread_yield();
    }
}
