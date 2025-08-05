#include "init.h"
#include "../../../kernel/Task/thread.h"
#include "../../../kernel/IPC/ipc.h"
#include "../../libc/libc.h"
#include "../login/login.h"
#include "../vnc/vnc.h"
#include "../ssh/ssh.h"
#include "../nitrfs/server.h"
#include "../ftp/ftp.h"
#include "../pkg/server.h"
#include "../update/server.h"

extern ipc_queue_t fs_queue;
extern ipc_queue_t pkg_queue;
extern ipc_queue_t upd_queue;

static void login_thread(void)  { login_server(NULL, thread_self()); }
static void vnc_thread(void)    { vnc_server(NULL, thread_self()); }
static void ssh_thread(void)    { ssh_server(NULL, thread_self()); }
static void nitrfs_thread(void) { nitrfs_server(&fs_queue, thread_self()); }
static void ftp_thread(void)    { ftp_server(&fs_queue, thread_self()); }
static void pkg_thread(void)    { pkg_server(&pkg_queue, thread_self()); }
static void update_thread(void) { update_server(&upd_queue, &pkg_queue, thread_self()); }

// Initial userspace task spawner. Creates core servers and remote access tasks.
void init_main(ipc_queue_t *q, uint32_t self_id) {
    (void)q; (void)self_id;
    thread_t *t;

    // Core system servers
    t = thread_create(nitrfs_thread);
    ipc_grant(&fs_queue, t->id, IPC_CAP_SEND | IPC_CAP_RECV);
    thread_yield();

    t = thread_create(pkg_thread);
    ipc_grant(&pkg_queue, t->id, IPC_CAP_SEND | IPC_CAP_RECV);
    thread_yield();

    t = thread_create(update_thread);
    ipc_grant(&upd_queue, t->id, IPC_CAP_SEND | IPC_CAP_RECV);
    ipc_grant(&pkg_queue, t->id, IPC_CAP_SEND | IPC_CAP_RECV);
    thread_yield();

    t = thread_create(ftp_thread);
    ipc_grant(&fs_queue, t->id, IPC_CAP_SEND | IPC_CAP_RECV);
    thread_yield();

    // Login and remote access servers
    t = thread_create(login_thread);
    ipc_grant(&fs_queue, t->id, IPC_CAP_SEND | IPC_CAP_RECV);
    ipc_grant(&pkg_queue, t->id, IPC_CAP_SEND | IPC_CAP_RECV);
    ipc_grant(&upd_queue, t->id, IPC_CAP_SEND | IPC_CAP_RECV);
    thread_yield();

    t = thread_create(vnc_thread);
    ipc_grant(&fs_queue, t->id, IPC_CAP_SEND | IPC_CAP_RECV);
    thread_yield();

    t = thread_create(ssh_thread);
    ipc_grant(&fs_queue, t->id, IPC_CAP_SEND | IPC_CAP_RECV);
    thread_yield();

    for (;;) {
        thread_yield();
    }
}

