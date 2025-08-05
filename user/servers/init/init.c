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
#include "../../../kernel/drivers/IO/serial.h"

extern ipc_queue_t fs_queue;
extern ipc_queue_t pkg_queue;
extern ipc_queue_t upd_queue;

// --- Service thread wrappers ---
static void login_thread(void)  { login_server(NULL, thread_self()); }
static void vnc_thread(void)    { vnc_server(NULL, thread_self()); }
static void ssh_thread(void)    { ssh_server(NULL, thread_self()); }
static void nitrfs_thread(void) { nitrfs_server(&fs_queue, thread_self()); }
static void ftp_thread(void)    { ftp_server(&fs_queue, thread_self()); }
static void pkg_thread(void)    { pkg_server(&pkg_queue, thread_self()); }
static void update_thread(void) { update_server(&upd_queue, &pkg_queue, thread_self()); }

// Core userspace service/task spawner
void init_main(ipc_queue_t *q, uint32_t self_id) {
    (void)q; (void)self_id;
    thread_t *t;

    // --- Spawn order and dependencies ---
    // nitrfs → provides fs_queue for ftp, login, vnc, ssh
    // pkg    → provides pkg_queue for login, update
    // update → provides upd_queue for login
    // ftp, login, vnc, ssh use fs_queue for storage

    // --- Nitrfs server ---
    t = thread_create(nitrfs_thread);
    if (!t) {
        serial_puts("[init] FATAL: failed to create nitrfs thread\n");
        goto fail;
    }
    ipc_grant(&fs_queue, t->id, IPC_CAP_SEND | IPC_CAP_RECV);
    thread_yield();

    // --- Pkg server ---
    t = thread_create(pkg_thread);
    if (!t) {
        serial_puts("[init] FATAL: failed to create pkg thread\n");
        goto fail;
    }
    ipc_grant(&pkg_queue, t->id, IPC_CAP_SEND | IPC_CAP_RECV);
    thread_yield();

    // --- Update server ---
    t = thread_create(update_thread);
    if (!t) {
        serial_puts("[init] FATAL: failed to create update thread\n");
        goto fail;
    }
    ipc_grant(&upd_queue, t->id, IPC_CAP_SEND | IPC_CAP_RECV);
    ipc_grant(&pkg_queue, t->id, IPC_CAP_SEND | IPC_CAP_RECV);
    thread_yield();

    // --- FTP server ---
    t = thread_create(ftp_thread);
    if (!t) {
        serial_puts("[init] failed to create ftp thread\n");
    } else {
        ipc_grant(&fs_queue, t->id, IPC_CAP_SEND | IPC_CAP_RECV);
    }
    thread_yield();

    // --- Login server ---
    t = thread_create(login_thread);
    if (!t) {
        serial_puts("[init] failed to create login thread\n");
    } else {
        ipc_grant(&fs_queue, t->id, IPC_CAP_SEND | IPC_CAP_RECV);
        ipc_grant(&pkg_queue, t->id, IPC_CAP_SEND | IPC_CAP_RECV);
        ipc_grant(&upd_queue, t->id, IPC_CAP_SEND | IPC_CAP_RECV);
    }
    thread_yield();

    // --- VNC server ---
    t = thread_create(vnc_thread);
    if (!t) {
        serial_puts("[init] failed to create vnc thread\n");
    } else {
        ipc_grant(&fs_queue, t->id, IPC_CAP_SEND | IPC_CAP_RECV);
    }
    thread_yield();

    // --- SSH server ---
    t = thread_create(ssh_thread);
    if (!t) {
        serial_puts("[init] failed to create ssh thread\n");
    } else {
        ipc_grant(&fs_queue, t->id, IPC_CAP_SEND | IPC_CAP_RECV);
    }
    thread_yield();

    serial_puts("[init] all system services launched\n");

    // --- Init task goes idle ---
    for (;;) thread_yield();

fail:
    serial_puts("[init] Halting system. Critical service failed to start.\n");
    for (;;) __asm__ volatile ("hlt");
}
