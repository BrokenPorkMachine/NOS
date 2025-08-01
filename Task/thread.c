#include "thread.h"
#include "../IPC/ipc.h"
#include "../servers/nitrfs/server.h"
#include "../servers/shell/shell.h"
#include "../servers/vnc/vnc.h"
#include "../servers/ssh/ssh.h"
#include "../servers/ftp/ftp.h"
#include "../src/libc.h"
#include "../IO/serial.h"
#include <stdint.h>

#define STACK_SIZE 4096

thread_t *current = NULL;
static thread_t *tail = NULL;
static int next_id = 1;
static thread_t main_thread; // represents kernel_main for initial switch

ipc_queue_t fs_queue;

// --- THREAD START HELPERS ---

static void thread_entry(void (*f)(void)) {
    f();
    // Thread returns: mark as exited, yield
    current->state = THREAD_EXITED;
    thread_yield();
    for (;;) __asm__ volatile("hlt");
}

// --- THREAD FUNCTIONS ---

static void thread_fs_func(void)   { nitrfs_server(&fs_queue, current->id); }
static void thread_shell_func(void){ shell_main(&fs_queue, current->id); }
static void thread_vnc_func(void)  { vnc_server(NULL, current->id); }
static void thread_ssh_func(void)  { ssh_server(NULL, current->id); }
static void thread_ftp_func(void)  { ftp_server(&fs_queue, current->id); }

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

    // Insert into round-robin ring
    if (!current) {
        current = t;
        t->next = t;
        tail = t;
    } else {
        t->next = current;
        tail->next = t;
        tail = t;
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
    if (t == current)
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
    uint32_t mask = (1u << 1) | (1u << 2) | (1u << 5);
    ipc_init(&fs_queue, mask, mask);
    thread_create(thread_fs_func);
    thread_create(thread_shell_func);
    thread_create(thread_vnc_func);
    thread_create(thread_ssh_func);
    thread_create(thread_ftp_func);

    // Insert kernel main thread into the run queue so the first
    // schedule() call can switch away safely.
    main_thread.id    = 0;
    main_thread.func  = NULL;
    main_thread.stack = NULL;
    main_thread.state = THREAD_RUNNING;

    main_thread.next = current; // current points to first created thread
    tail->next       = &main_thread;
    tail             = &main_thread;
    current          = &main_thread;
}

// --- SCHEDULER: simple round-robin, skips non-ready threads ---

void schedule(void) {
    thread_t *prev = current;
    if (prev->state == THREAD_RUNNING)
        prev->state = THREAD_READY;

    int found = 0;
    thread_t *start = current;
    do {
        current = current->next;
        if (current->state == THREAD_READY) { found = 1; break; }
    } while (current != start);

    if (!found) {
        // Deadlock! All threads blocked/exited: halt system
        for (;;)
            __asm__ volatile("cli; hlt");
    }
    current->state = THREAD_RUNNING;
    context_switch(&prev->rsp, current->rsp);
}

