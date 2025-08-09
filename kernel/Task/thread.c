#include "thread.h"
#include "../IPC/ipc.h"
#include "../../user/libc/libc.h"
#include <stdint.h>
#include "../arch/CPU/smp.h"
#include "../../user/agents/login/login.h"

#ifndef STACK_SIZE
#define STACK_SIZE 8192
#endif

#define THREAD_MAGIC 0x74687264UL

// --- Forward decls from low-level context switch asm ---
extern void context_switch(uint64_t *prev_rsp, uint64_t next_rsp);

// --- Minimal context frame saved on a thread's stack.
// Layout matches how the scheduler expects to read/write rflags and resume via RIP.
typedef struct context_frame {
    uint64_t r15, r14, r13, r12, rbx, rbp;
    uint64_t rflags;
    uint64_t rip;           // return address to resume at (thread_entry)
    uint64_t arg_rdi;       // consumed by 'pop %rdi' in thread_entry
} context_frame_t;

static inline uintptr_t align16(uintptr_t v) { return v & ~0xFULL; }

// Zombie list to track exited threads (SMP-ish safe: interrupts-off critical sections)
static thread_t *zombie_list = NULL;

static thread_t thread_pool[MAX_KERNEL_THREADS];
static char     stack_pool[MAX_KERNEL_THREADS][STACK_SIZE];

// Per-CPU run queue pointers: circular singly-linked list, tail points to last node
thread_t *current_cpu[MAX_CPUS] = {0};
static thread_t *tail_cpu[MAX_CPUS] = {0};

// Next thread ID (atomic-ish for SMP)
static int next_id = 1;

static thread_t main_thread; // per-CPU0 bootstrap/idle

// IPC queues for servers (exposed globally)
ipc_queue_t fs_queue, pkg_queue, upd_queue, init_queue, regx_queue;

// --- Tiny helpers to enter/leave per-CPU critical sections using IF masking ---
static inline uint64_t irq_save_disable(void) {
    uint64_t rf;
    __asm__ volatile("pushfq; pop %0; cli" : "=r"(rf) :: "memory");
    return rf;
}
static inline void irq_restore(uint64_t rf) {
    __asm__ volatile("push %0; popfq" :: "r"(rf) : "memory");
}

// --- Run-queue helpers (assume interrupts disabled while mutating) ---
static inline void rq_insert_tail(int cpu, thread_t *t) {
    if (!current_cpu[cpu]) {
        current_cpu[cpu] = t;
        tail_cpu[cpu] = t;
        t->next = t;
        return;
    }
    t->next = current_cpu[cpu];
    tail_cpu[cpu]->next = t;
    tail_cpu[cpu] = t;
}

static inline void rq_remove(int cpu, thread_t *t) {
    thread_t *cur = current_cpu[cpu];
    if (!cur || !t) return;

    thread_t *p = cur;
    do {
        if (p->next == t) {
            p->next = t->next;
            if (tail_cpu[cpu] == t) tail_cpu[cpu] = p;
            if (current_cpu[cpu] == t) current_cpu[cpu] = t->next == t ? NULL : t->next;
            t->next = NULL;
            return;
        }
        p = p->next;
    } while (p && p != cur);
}

static inline void rq_requeue_tail(int cpu, thread_t *t) {
    if (!t || !current_cpu[cpu]) return;
    // unlink then insert at tail
    rq_remove(cpu, t);
    rq_insert_tail(cpu, t);
}

// --- Bootstrap early: establish main_thread as the initial RUNNING thread ---
void threads_early_init(void) {
    zombie_list = NULL;
    next_id = 1;

    for (int i = 0; i < MAX_CPUS; ++i) {
        current_cpu[i] = NULL;
        tail_cpu[i] = NULL;
    }

    memset(thread_pool, 0, sizeof(thread_pool));
    memset(stack_pool, 0, sizeof(stack_pool));

    // CPU0 bootstrap "main" thread doubles as idle; never exits.
    memset(&main_thread, 0, sizeof(main_thread));
    main_thread.magic    = THREAD_MAGIC;
    main_thread.id       = 0;
    main_thread.func     = NULL;
    main_thread.stack    = NULL;
    main_thread.state    = THREAD_RUNNING;
    main_thread.started  = 1;
    main_thread.priority = MIN_PRIORITY;
    main_thread.next     = &main_thread;

    current_cpu[0] = &main_thread;
    tail_cpu[0]    = &main_thread;
}

// --- Thread trampoline chain ---
// Call the user function, mark exited, and yield forever.
__attribute__((noreturn, used))
static void thread_start(void (*f)(void)) {
    f();
    thread_t *cur = thread_current();
    if (cur) cur->state = THREAD_EXITED;
    thread_yield(); // should not return
    for (;;) __asm__ volatile("hlt");
}

// Naked entry: pop arg into %rdi, call thread_start
static void __attribute__((naked, noreturn)) thread_entry(void) {
    __asm__ volatile(
        "pop %rdi\n"
        "call thread_start\n"
        "hlt\n"
    );
}

// --- Query current thread / id ---
thread_t *thread_current(void) { return current_cpu[smp_cpu_index()]; }
uint32_t  thread_self(void)    { thread_t *t = thread_current(); ret_*_
