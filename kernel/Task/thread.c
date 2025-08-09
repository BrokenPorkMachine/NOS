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

// Zombie list to track exited threads for reaping (SMP-safe: use atomic pointer if needed)
static thread_t *zombie_list = NULL;

static thread_t thread_pool[MAX_KERNEL_THREADS];
static char     stack_pool[MAX_KERNEL_THREADS][STACK_SIZE];

// Per-CPU run queue pointers
thread_t *current_cpu[MAX_CPUS] = {0};
static thread_t *tail_cpu[MAX_CPUS] = {0};

// Next thread ID (atomic for SMP: use lock or atomic int if race detected)
static int next_id = 1;
static thread_t main_thread;

// IPC queues for servers (exposed globally)
ipc_queue_t fs_queue, pkg_queue, upd_queue, init_queue, regx_queue;

void threads_early_init(void) {
    zombie_list = NULL;
    next_id = 1;
    for (int i = 0; i < MAX_CPUS; ++i) {
        current_cpu[i] = NULL;
        tail_cpu[i] = NULL;
    }
    memset(thread_pool, 0, sizeof(thread_pool));
    memset(stack_pool, 0, sizeof(stack_pool));
}

// --- Stack Alignment Utility ---
static inline uintptr_t align16(uintptr_t val) { return val & ~0xFULL; }

// --- Thread trampoline ---
__attribute__((used)) static void thread_start(void (*f)(void)) {
    f();
    thread_t *cur = thread_current();
    cur->state = THREAD_EXITED;
    thread_yield(); // Should never return
    for (;;) __asm__ volatile("hlt");
}

// Naked entry: pops function pointer into rdi, calls thread_start
static void __attribute__((naked)) thread_entry(void) {
    __asm__ volatile(
        "pop %rdi\n"
        "call thread_start\n"
        "hlt\n"
    );
}

thread_t *thread_current(void) { return current_cpu[smp_cpu_index()]; }
uint32_t thread_self(void)     { thread_t *t = thread_current(); return t ? t->id : 0; }

static void add_to_zombie_list(thread_t *t) {
    t->next = zombie_list;
    zombie_list = t;
}

// --- Thread Creation ---
thread_t *thread_create_with_priority(void (*func)(void), int priority) {
    if (priority < MIN_PRIORITY) priority = MIN_PRIORITY;
    if (priority > MAX_PRIORITY) priority = MAX_PRIORITY;

    thread_t *t = NULL;
    int index = -1;
    for (int i = 0; i < (int)MAX_KERNEL_THREADS; i++) {
        if (thread_pool[i].magic == 0) {
            t = &thread_pool[i];
            index = i;
            break;
        }
    }
    if (!t) return NULL;   // no free slots

    // thread_pool is pre-zeroed during threads_early_init(), but
    // guard the explicit memset so we never touch a bogus low
    // address if the symbol resolves unexpectedly (e.g. to 0x38)
    // which could trigger a #GP fault on early boot.
    if ((uintptr_t)t < 0x1000)
        return NULL;
    memset(t, 0, sizeof(thread_t));
    t->magic = THREAD_MAGIC;
    t->stack = stack_pool[index];

    uintptr_t stack_top = (uintptr_t)t->stack + STACK_SIZE;
    stack_top = align16(stack_top);

    uint64_t *sp = (uint64_t *)stack_top;

    // Layout: [arg][RIP][RAX][RFLAGS][rbp][rbx][r12][r13][r14][r15]
    *--sp = (uint64_t)func;         // arg for thread_entry
    *--sp = (uint64_t)thread_entry; // RIP
    *--sp = 0;                      // RAX placeholder (alignment)
    *--sp = 0x202;                  // RFLAGS (IF=1)
    *--sp = 0; // rbp
    *--sp = 0; // rbx
    *--sp = 0; // r12
    *--sp = 0; // r13
    *--sp = 0; // r14
    *--sp = 0; // r15

    t->rsp = (uint64_t)sp;
    t->func = func;
    t->id = next_id++;
    t->state = THREAD_READY;
    t->started = 0;
    t->priority = priority;

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

    return t;
}

thread_t *thread_create(void (*func)(void)) {
    return thread_create_with_priority(func, (MAX_PRIORITY + MIN_PRIORITY) / 2);
}

void thread_block(thread_t *t) {
    if (!t || t->magic != THREAD_MAGIC) return;
    t->state = THREAD_BLOCKED;
    if (t == thread_current())
        schedule();
}

void thread_unblock(thread_t *t) {
    if (!t || t->magic != THREAD_MAGIC) return;
    t->state = THREAD_READY;
    thread_t *cur = thread_current();
    if (t->priority > cur->priority && cur->state == THREAD_RUNNING)
        schedule();
}

int thread_is_alive(thread_t *t) {
    return t && t->magic == THREAD_MAGIC && t->state != THREAD_EXITED;
}

void thread_kill(thread_t *t) {
    if (!t || t->magic != THREAD_MAGIC) return;
    t->state = THREAD_EXITED;

    int cpu = smp_cpu_index();
    thread_t *cur = current_cpu[cpu];

    if (t == cur) {
        schedule();
        return;
    }

    thread_t *prev = cur;
    while (prev->next != t && prev->next != cur)
        prev = prev->next;

    if (prev->next == t) {
        prev->next = t->next;
        if (tail_cpu[cpu] == t)
            tail_cpu[cpu] = prev;
    }

    add_to_zombie_list(t);
}

void thread_set_priority(thread_t *t, int priority) {
    if (!t || t->magic != THREAD_MAGIC)
        return;

    if (priority < MIN_PRIORITY) priority = MIN_PRIORITY;
    if (priority > MAX_PRIORITY) priority = MAX_PRIORITY;

    int old = t->priority;
    t->priority = priority;

    thread_t *cur = thread_current();
    /*
     * If we boosted another thread above the current thread's priority,
     * or lowered the current thread, invoke the scheduler to give other
     * runnable threads a chance.
     */
    if ((t != cur && t->priority > cur->priority && t->state == THREAD_READY) ||
        (t == cur && priority < old)) {
        schedule();
    }
}

void thread_join(thread_t *t) {
    if (!t || t->magic != THREAD_MAGIC)
        return;

    while (thread_is_alive(t)) {
        thread_yield();
    }
}

void thread_yield(void) {
    schedule();
}

static void thread_reap(void) {
    thread_t *t = zombie_list;
    zombie_list = NULL;
    while (t) {
        thread_t *next = t->next;
        memset(t, 0, sizeof(thread_t)); // Wipe slot
        t = next;
    }
}

// --- Scheduler core, as in your code, with fixes ---
static thread_t *pick_next(int cpu) {
    thread_t *start = current_cpu[cpu];
    if (!start)
        return NULL;

    thread_t *t = start->next;
    thread_t *best = NULL;

    /*
     * Iterate once over the circular run queue looking for the highest
     * priority thread that is ready to run.  Ties are broken in favour of
     * the thread that appears earliest after the currently running thread
     * (simple round‑robin within a priority level).
     */
    while (t && t != start) {
        if (t->state == THREAD_READY && (!best || t->priority > best->priority))
            best = t;
        t = t->next;
    }

    /*
     * Finally consider the current thread.  It was marked READY by the
     * caller, so include it in the priority comparison.  If no other
     * runnable threads exist, this may return the current thread which will
     * result in a benign self switch.
     */
    if (start->state == THREAD_READY && (!best || start->priority >= best->priority))
        best = start;

    return best;
}

void schedule(void) {
    uint64_t rflags;
    __asm__ volatile("pushfq; pop %0; cli" : "=r"(rflags) :: "memory");

    int cpu = smp_cpu_index();
    thread_t *prev = current_cpu[cpu];
    if (!prev) {
        __asm__ volatile("push %0; popfq; hlt" :: "r"(rflags) : "memory");
        return;
    }

    if (prev->state == THREAD_RUNNING)
        prev->state = THREAD_READY;

    thread_t *next = pick_next(cpu);

    if (!next) {
        prev->state = THREAD_RUNNING;
        __asm__ volatile("push %0; popfq; hlt" :: "r"(rflags) : "memory");
        return;
    }

    next->state = THREAD_RUNNING;
    next->started = 1;

    current_cpu[cpu] = next;
    context_switch(&prev->rsp, next->rsp);
    current_cpu[cpu] = prev;

    ((uint64_t *)prev->rsp)[6] = rflags;

    if (prev->state == THREAD_EXITED)
        add_to_zombie_list(prev);

    thread_reap();
}

uint64_t schedule_from_isr(uint64_t *old_rsp) {
    int cpu = smp_cpu_index();
    thread_t *prev = current_cpu[cpu];
    if (!prev)
        return (uint64_t)old_rsp;

    prev->rsp = (uint64_t)old_rsp;
    if (prev->state == THREAD_RUNNING)
        prev->state = THREAD_READY;

    thread_t *next = pick_next(cpu);
    if (!next) {
        current_cpu[cpu] = prev;
        prev->state = THREAD_RUNNING;
        return (uint64_t)old_rsp;
    }

    next->state = THREAD_RUNNING;
    next->started = 1;

    current_cpu[cpu] = next;

    return next->rsp;
}

// --- Example Service Threads ---
static void regx_thread_func(void) {
    ipc_message_t msg;
    while (1) {
        if (ipc_receive_blocking(&regx_queue, thread_self(), &msg) == 0) {
            ipc_message_t reply = {0};
            reply.type = msg.type;
            ipc_send(&regx_queue, thread_self(), &reply);
        }
        thread_yield();
    }
}
static void login_thread_func(void) {
    login_server(&fs_queue, thread_self());
}
static void init_thread_func(void) {
    static uint32_t init_tid = 0;
    init_tid = thread_self();
    thread_create_with_priority(login_thread_func, 180);
    ipc_message_t msg;
    while (1) {
        if (ipc_receive_blocking(&init_queue, init_tid, &msg) == 0) {
            ipc_send(&regx_queue, init_tid, &msg);
            ipc_message_t reply;
            if (ipc_receive_blocking(&regx_queue, init_tid, &reply) == 0)
                ipc_send(&init_queue, init_tid, &reply);
        }
        thread_yield();
    }
}

// --- Init threading and launch core services ---
void threads_init(void) {
    ipc_init(&fs_queue); ipc_init(&pkg_queue); ipc_init(&upd_queue); ipc_init(&init_queue); ipc_init(&regx_queue);

    thread_t *regx = thread_create_with_priority(regx_thread_func, 220);
    thread_t *init = thread_create_with_priority(init_thread_func, 200);
    if (!regx || !init) {
        for (;;) __asm__ volatile("hlt");
    }

    ipc_grant(&regx_queue, regx->id, IPC_CAP_SEND | IPC_CAP_RECV);
    ipc_grant(&regx_queue, init->id, IPC_CAP_SEND | IPC_CAP_RECV);
    ipc_grant(&init_queue,  init->id, IPC_CAP_SEND | IPC_CAP_RECV);

    main_thread.magic = THREAD_MAGIC;
    main_thread.id      = 0;
    main_thread.func    = NULL;
    main_thread.stack   = NULL;
    main_thread.state   = THREAD_RUNNING;
    main_thread.started = 1;
    main_thread.priority = MIN_PRIORITY;

    int cpu = smp_cpu_index();
    main_thread.next = current_cpu[cpu];
    if (tail_cpu[cpu])
        tail_cpu[cpu]->next = &main_thread;
    tail_cpu[cpu] = &main_thread;
    current_cpu[cpu] = &main_thread;
}

