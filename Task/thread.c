#include "thread.h"
#include "../IPC/ipc.h"
#include "../servers/nitrfs/server.h"
#include "../servers/shell/shell.h"
#include "../src/libc.h"
#include <stdint.h>

#define STACK_SIZE 4096

thread_t *current = NULL;
static thread_t *tail = NULL;
static int next_id = 1;

ipc_queue_t fs_queue;

static void thread_fs_func(void) {
    nitrfs_server(&fs_queue);
}

static void thread_shell_func(void) {
    shell_main(&fs_queue);
}

thread_t *thread_create(void (*func)(void)) {
    thread_t *t = malloc(sizeof(thread_t));
    if (!t)
        return NULL;
    t->stack = malloc(STACK_SIZE);
    if (!t->stack)
        return NULL;
    uint64_t *sp = (uint64_t *)(t->stack + STACK_SIZE);
    *--sp = 0; // rbp
    *--sp = 0; // rbx
    *--sp = 0; // r12
    *--sp = 0; // r13
    *--sp = 0; // r14
    *--sp = 0; // r15
    *--sp = (uint64_t)func; // return address
    t->rsp = (uint64_t)sp;
    t->func = func;
    t->id = next_id++;
    t->state = THREAD_READY;
    if (!current) {
        current = t;
        t->next = t;
        tail = t;
    } else {
        t->next = current;
        tail->next = t;
        tail = t;
    }
    return t;
}

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

void threads_init(void) {
    uint32_t mask = (1u << 1) | (1u << 2);
    ipc_init(&fs_queue, mask, mask);
    thread_create(thread_fs_func);
    thread_create(thread_shell_func);
    current->state = THREAD_RUNNING;
}

void schedule(void) {
    thread_t *prev = current;
    if (prev->state == THREAD_RUNNING)
        prev->state = THREAD_READY;
    do {
        current = current->next;
    } while (current->state != THREAD_READY);
    current->state = THREAD_RUNNING;
    context_switch(&prev->rsp, current->rsp);
}
