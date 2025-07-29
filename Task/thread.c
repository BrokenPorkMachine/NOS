#include "thread.h"
#include "../IPC/ipc.h"
#include "../servers/nitrfs/server.h"
#include "../servers/shell/shell.h"
#include <stdint.h>

// Shell and filesystem server run as cooperative threads

#define STACK_SIZE 4096

thread_t thread_fs, thread_shell;
thread_t *current = &thread_fs;

ipc_queue_t fs_queue;

static char stack_fs[STACK_SIZE];
static char stack_shell[STACK_SIZE];

static void thread_fs_func(void) {
    nitrfs_server(&fs_queue);
}

static void thread_shell_func(void) {
    shell_main(&fs_queue);
}

void threads_init(void) {
    uint32_t mask = (1u << 1) | (1u << 2);
    ipc_init(&fs_queue, mask, mask);

    // Filesystem server thread
    thread_fs.stack = stack_fs;
    uint64_t *sp = (uint64_t *)&stack_fs[STACK_SIZE];
    *--sp = 0; // rbp
    *--sp = 0; // rbx
    *--sp = 0; // r12
    *--sp = 0; // r13
    *--sp = 0; // r14
    *--sp = 0; // r15
    *--sp = (uint64_t)thread_fs_func; // return address
    thread_fs.rsp = (uint64_t)sp;
    thread_fs.func = thread_fs_func;
    thread_fs.id = 1;
    thread_fs.next = &thread_shell;

    // Shell thread
    thread_shell.stack = stack_shell;
    sp = (uint64_t *)&stack_shell[STACK_SIZE];
    *--sp = 0; // rbp
    *--sp = 0; // rbx
    *--sp = 0; // r12
    *--sp = 0; // r13
    *--sp = 0; // r14
    *--sp = 0; // r15
    *--sp = (uint64_t)thread_shell_func;
    thread_shell.rsp = (uint64_t)sp;
    thread_shell.func = thread_shell_func;
    thread_shell.id = 2;
    thread_shell.next = &thread_fs;
}

void schedule(void) {
    thread_t *prev = current;
    current = current->next;
    context_switch(&prev->rsp, current->rsp);
}
