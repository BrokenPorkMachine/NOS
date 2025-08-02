#include "thread.h"
#include "../IPC/ipc.h"
#include "../../user/servers/nitrfs/server.h"
#include "../../user/servers/shell/shell.h"
#include "../../user/servers/vnc/vnc.h"
#include "../../user/servers/ssh/ssh.h"
#include "../../user/servers/ftp/ftp.h"
#include "../../user/servers/pkg/server.h"
#include "../../user/servers/update/server.h"
#include "../../user/servers/login/login.h"
#include "../../user/servers/init/init.h"
#include "../../user/libc/libc.h"
#include "../drivers/IO/serial.h"
#include <stdint.h>
#include "../arch/CPU/smp.h"

#define STACK_SIZE 4096

thread_t *current_cpu[MAX_CPUS] = {0};
static thread_t *tail_cpu[MAX_CPUS] = {0};
static int next_id = 1;
static thread_t main_thread; // represents kernel_main for initial switch

ipc_queue_t fs_queue;
ipc_queue_t pkg_queue;
ipc_queue_t upd_queue;

// --- THREAD START HELPERS ---

static void thread_entry(void (*f)(void)) {
    f();
    thread_t *cur = current_cpu[smp_cpu_index()];
    cur->state = THREAD_EXITED;
    thread_yield();
    for (;;) __asm__ volatile("hlt");
}

// --- THREAD FUNCTIONS ---

static void thread_fs_func(void)   { thread_t *c = current_cpu[smp_cpu_index()]; nitrfs_server(&fs_queue, c->id); }
static void thread_init_func(void) { thread_t *c = current_cpu[smp_cpu_index()]; init_main(&fs_queue, c->id); }
static void thread_shell_func(void){ thread_t *c = current_cpu[smp_cpu_index()]; shell_main(&fs_queue, &pkg_queue, &upd_queue, c->id); }
static void thread_pkg_func(void)   { thread_t *c = current_cpu[smp_cpu_index()]; pkg_server(&pkg_queue, c->id); }
static void thread_update_func(void){ thread_t *c = current_cpu[smp_cpu_index()]; update_server(&upd_queue, &pkg_queue, c->id); }
static void thread_vnc_func(void)  { thread_t *c = current_cpu[smp_cpu_index()]; vnc_server(NULL, c->id); }
static void thread_ssh_func(void)  { thread_t *c = current_cpu[smp_cpu_index()]; ssh_server(NULL, c->id); }
static void thread_ftp_func(void)  { thread_t *c = current_cpu[smp_cpu_index()]; ftp_server(&fs_queue, c->id); }
static void thread_login_func(void){ thread_t *c = current_cpu[smp_cpu_index()]; login_server(NULL, c->id); }

// --- THREAD CREATION ---

thread_t *thread_create(void (*func)(void)) {
    thread_t *t = malloc(sizeof(thread_t));
    if (!t) return NULL;
    t->stack = malloc(STACK_SIZE);
    if (!t->stack) { free(t); return NULL; }

    // Setup stack for context_switch: rbp/rbx/r12-r15/ret
    uint64_t *sp = (uint64_t *)(t->stack + STACK_SIZE);

    *--sp = 0; // rbp
    *--sp = 0; // rbx
    *--sp = 0; // r12
    *--sp = 0; // r13
    *--sp = 0; // r14
    *--sp = 0; // r15
    *--sp = (uint64_t)thread_entry; // return address
    *--sp = (uint64_t)func;         // argument for thread_entry

    t->rsp = (uint64_t)sp;
    t->func = func;
    t->id = next_id++;
    t->state = THREAD_READY;

    int cpu = smp_cpu_index();
    if (!current_cpu[cpu]) {
        current_cpu[cpu] = t;
        t->next = t;
        tail_cpu[cpu] = t;
    } else {
        t->next = current_cpu[cpu];
        tail_cpu[cpu]->next = t;
        tail_cpu[cpu] = t;
    }
    char buf[32];
    serial_puts("[thread] created id=");
    buf[0] = '\0';
    {
        int id = t->id;
        int pos = 0;
        char tmp[16];
        if (id == 0) { tmp[pos++] = '0'; }
        while (id > 0 && pos < 15) { tmp[pos++] = '0' + (id % 10); id /= 10; }
        for (int i = pos-1, j=0; i>=0; --i,++j) buf[j]=tmp[i];
        buf[pos] = 0;
    }
    serial_puts(buf);
    serial_puts("\n");
    return t;
}

// --- THREAD CONTROL ---

void thread_block(thread_t *t) {
    t->state = THREAD_BLOCKED;
    if (t == current_cpu[smp_cpu_index()])
        schedule();
}

void thread_unblock(thread_t *t) {
    t->state = THREAD_READY;
}

void thread_yield(void) {
    schedule();
}

// --- THREAD SYSTEM INIT ---

void threads_init(void) {
    ipc_init(&fs_queue);
    ipc_init(&pkg_queue);
    ipc_init(&upd_queue);
    thread_t *t;

    t = thread_create(thread_fs_func);
    ipc_grant(&fs_queue, t->id, IPC_CAP_SEND | IPC_CAP_RECV);

    t = thread_create(thread_init_func);
    ipc_grant(&fs_queue, t->id, IPC_CAP_SEND | IPC_CAP_RECV);

    t = thread_create(thread_login_func);
    ipc_grant(&fs_queue, t->id, IPC_CAP_SEND | IPC_CAP_RECV);

    t = thread_create(thread_pkg_func);
    ipc_grant(&pkg_queue, t->id, IPC_CAP_SEND | IPC_CAP_RECV);

    t = thread_create(thread_update_func);
    ipc_grant(&upd_queue, t->id, IPC_CAP_SEND | IPC_CAP_RECV);
    ipc_grant(&pkg_queue, t->id, IPC_CAP_SEND | IPC_CAP_RECV);

    t = thread_create(thread_shell_func);
    ipc_grant(&fs_queue, t->id, IPC_CAP_SEND | IPC_CAP_RECV);
    ipc_grant(&pkg_queue, t->id, IPC_CAP_SEND | IPC_CAP_RECV);
    ipc_grant(&upd_queue, t->id, IPC_CAP_SEND | IPC_CAP_RECV);

    t = thread_create(thread_vnc_func);
    ipc_grant(&fs_queue, t->id, IPC_CAP_SEND | IPC_CAP_RECV);

    t = thread_create(thread_ssh_func);
    ipc_grant(&fs_queue, t->id, IPC_CAP_SEND | IPC_CAP_RECV);

    t = thread_create(thread_ftp_func);
    ipc_grant(&fs_queue, t->id, IPC_CAP_SEND | IPC_CAP_RECV);

    // Insert kernel main thread into the run queue so the first
    // schedule() call can switch away safely.
    main_thread.id    = 0;
    main_thread.func  = NULL;
    main_thread.stack = NULL;
    main_thread.state = THREAD_RUNNING;

    int cpu = smp_cpu_index();
    main_thread.next = current_cpu[cpu];
    tail_cpu[cpu]->next = &main_thread;
    tail_cpu[cpu] = &main_thread;
    current_cpu[cpu] = &main_thread;
}

// --- SCHEDULER: simple round-robin, skips non-ready threads ---

void schedule(void) {
    int cpu = smp_cpu_index();
    thread_t *prev = current_cpu[cpu];
    if (prev->state == THREAD_RUNNING)
        prev->state = THREAD_READY;

    int found = 0;
    thread_t *start = current_cpu[cpu];
    do {
        current_cpu[cpu] = current_cpu[cpu]->next;
        if (current_cpu[cpu]->state == THREAD_READY) { found = 1; break; }
    } while (current_cpu[cpu] != start);

    if (!found) {
        for (;;)
            __asm__ volatile("cli; hlt");
    }
    current_cpu[cpu]->state = THREAD_RUNNING;
    context_switch(&prev->rsp, current_cpu[cpu]->rsp);
}

