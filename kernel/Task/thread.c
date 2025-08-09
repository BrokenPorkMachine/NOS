// thread.c
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

// --- Context frame expected by your context_switch.asm
// Stack (top -> bottom at t->rsp):
//   r15, r14, r13, r12, rbx, rbp, rflags, rax_dummy, rip(thread_entry), arg_for_thread_entry
typedef struct context_frame {
    uint64_t r15, r14, r13, r12, rbx, rbp;
    uint64_t rflags;
    uint64_t rax_dummy;   // consumed by 'pop rax' in context_switch
    uint64_t rip;         // -> thread_entry
    uint64_t arg_rdi;     // consumed by 'pop %rdi' in thread_entry
} context_frame_t;

static inline uintptr_t align16(uintptr_t v) { return v & ~0xFULL; }

// Zombie list to track exited threads (protected by IF-masked critical sections)
static thread_t *zombie_list = NULL;

static thread_t thread_pool[MAX_KERNEL_THREADS];
static char     stack_pool[MAX_KERNEL_THREADS][STACK_SIZE];

// Per-CPU run queue pointers: circular singly-linked list; tail points to last node
thread_t *current_cpu[MAX_CPUS] = {0};
static thread_t *tail_cpu[MAX_CPUS] = {0};

// Next thread ID (atomic-ish for SMP)
static int next_id = 1;

static thread_t main_thread; // CPU0 bootstrap/idle

// IPC queues for servers (exposed globally)
ipc_queue_t fs_queue, pkg_queue, upd_queue, init_queue, regx_queue;

// --- Optional: enable SSE/FXSR if kernel code may use FP/SIMD
static void enable_sse(void) {
#if defined(__x86_64__)
    uint64_t cr0, cr4;
    __asm__ volatile("mov %%cr0, %0" : "=r"(cr0));
    cr0 &= ~(1ULL << 2);  // clear EM
    cr0 |=  (1ULL << 1);  // set MP
    __asm__ volatile("mov %0, %%cr0" :: "r"(cr0));
    __asm__ volatile("mov %%cr4, %0" : "=r"(cr4));
    cr4 |= (1ULL << 9) | (1ULL << 10); // OSFXSR | OSXMMEXCPT
    __asm__ volatile("mov %0, %%cr4" :: "r"(cr4));
#endif
}

// --- IRQ helpers ---
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
        tail_cpu[cpu]    = t;
        t->next          = t;
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
            if (current_cpu[cpu] == t) current_cpu[cpu] = (t->next == t) ? NULL : t->next;
            t->next = NULL;
            return;
        }
        p = p->next;
    } while (p && p != cur);
}

static inline void rq_requeue_tail(int cpu, thread_t *t) {
    if (!t || !current_cpu[cpu]) return;
    rq_remove(cpu, t);
    rq_insert_tail(cpu, t);
}

// --- Bootstrap early: establish main_thread as the initial RUNNING thread ---
void threads_early_init(void) {
    enable_sse();

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

    uint64_t rsp;
    __asm__ volatile("mov %%rsp, %0" : "=r"(rsp));
    main_thread.rsp = rsp;

    current_cpu[0] = &main_thread;
    tail_cpu[0]    = &main_thread;
}

// --- Query current thread / id ---
thread_t *thread_current(void) { return current_cpu[smp_cpu_index()]; }
uint32_t  thread_self(void)    { thread_t *t = thread_current(); return t ? t->id : 0; }

// --- Zombie management ---
static void add_to_zombie_list(thread_t *t) {
    uint64_t rf = irq_save_disable();
    t->next = zombie_list;
    zombie_list = t;
    irq_restore(rf);
}

static void thread_reap(void) {
    uint64_t rf = irq_save_disable();
    thread_t *list = zombie_list;
    zombie_list = NULL;
    irq_restore(rf);

    for (thread_t *t = list; t;) {
        thread_t *n = t->next;
        memset(t, 0, sizeof(thread_t)); // Wipe slot for reuse
        t = n;
    }
}

// --- Pick next runnable: choose highest-priority READY; fall back to main_thread ---
static thread_t *pick_next(int cpu) {
    thread_t *start = current_cpu[cpu];
    if (!start) return NULL;

    thread_t *t = start;
    thread_t *best = NULL;

    // Single pass over the circular queue
    do {
        if (t->state == THREAD_READY &&
            (!best || t->priority > best->priority)) {
            best = t;
        }
        t = t->next;
    } while (t && t != start);

    // If none READY, keep running the current thread if it's not blocked/exited.
    if (!best) {
        if (start->state == THREAD_READY || start->state == THREAD_RUNNING)
            best = start;
    }

    if (!best) best = &main_thread; // ultimate fallback

    return best;
}

void schedule(void) {
    // Save and disable here to cover the pick/manipulation window
    uint64_t rf;
    __asm__ volatile("pushfq; pop %0; cli" : "=r"(rf) :: "memory");

    int cpu = smp_cpu_index();
    thread_t *prev = current_cpu[cpu];
    if (!prev) {
        // No current thread: don't sleep with IF=0
        __asm__ volatile("push %0; popfq" :: "r"(rf) : "memory");
        __asm__ volatile("hlt");
        return;
    }

    if (prev->state == THREAD_RUNNING)
        prev->state = THREAD_READY;

    thread_t *next = pick_next(cpu);

    if (!next) {
        // Nothing else runnable: keep running prev, restore IF before HLT
        prev->state = THREAD_RUNNING;
        __asm__ volatile("push %0; popfq" :: "r"(rf) : "memory");
        __asm__ volatile("hlt");
        return;
    }

    next->state = THREAD_RUNNING;
    next->started = 1;

    // Switch: asm will restore the next thread's RFLAGS via popfq,
    // so we do NOT restore 'rf' here.
    current_cpu[cpu] = next;
    context_switch(&prev->rsp, next->rsp);

    // ...post-switch bookkeeping...
    if (prev->state == THREAD_EXITED)
        add_to_zombie_list(prev);

    thread_reap();
}

uint64_t schedule_from_isr(uint64_t *old_rsp) {
    // ISR path: called with interrupts masked already.
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

// --- Thread exit (Option B): mark EXITED and reschedule, never returns ---
__attribute__((noreturn))
void thread_exit(void) {
    thread_t *t = thread_current();
    if (t) t->state = THREAD_EXITED;
    schedule();                 // switch away from this stack
    __builtin_unreachable();
}

// --- Thread trampoline chain (Option B) ---
__attribute__((noreturn, used))
static void thread_start(void (*f)(void)) {
    f();
    thread_exit();              // never returns
}

// Naked entry: pop arg into %rdi, call thread_start (never returns)
static void __attribute__((naked, noreturn)) thread_entry(void) {
    __asm__ volatile(
        "pop %rdi\n"          // arg_rdi -> %rdi
        "call thread_start\n" // never returns
        "jmp .\n"             // safety: unreachable
    );
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

    // Paranoia: avoid bogus low pointer
    if ((uintptr_t)t < 0x1000)
        return NULL;

    memset(t, 0, sizeof(thread_t));
    t->magic = THREAD_MAGIC;
    t->stack = stack_pool[index];

    uintptr_t top = align16((uintptr_t)t->stack + STACK_SIZE);

    // Build initial context frame to match your context_switch.asm contract
    context_frame_t *cf = (context_frame_t *)(top - sizeof(*cf));
    *cf = (context_frame_t){
        .r15=0,.r14=0,.r13=0,.r12=0,.rbx=0,.rbp=0,
        .rflags=0x202,             // IF=1 so the new thread starts with interrupts enabled
        .rax_dummy=0,              // consumed by 'pop rax'
        .rip=(uint64_t)thread_entry,
        .arg_rdi=(uint64_t)func
    };

    t->rsp      = (uint64_t)cf;
    t->func     = func;

    int id = __atomic_fetch_add(&next_id, 1, __ATOMIC_RELAXED);
    t->id       = id;
    t->state    = THREAD_READY;
    t->started  = 0;
    t->priority = priority;
    t->next     = NULL;

    uint64_t rf = irq_save_disable();
    int cpu = smp_cpu_index();
    rq_insert_tail(cpu, t);
    irq_restore(rf);

    return t;
}

thread_t *thread_create(void (*func)(void)) {
    return thread_create_with_priority(func, (MAX_PRIORITY + MIN_PRIORITY) / 2);
}

void thread_block(thread_t *t) {
    if (!t || t->magic != THREAD_MAGIC) return;

    uint64_t rf = irq_save_disable();
    t->state = THREAD_BLOCKED;
    irq_restore(rf);

    if (t == thread_current())
        schedule();
}

void thread_unblock(thread_t *t) {
    if (!t || t->magic != THREAD_MAGIC) return;

    uint64_t rf = irq_save_disable();
    t->state = THREAD_READY;
    thread_t *cur = thread_current();
    int should_preempt = (cur && cur->state == THREAD_RUNNING && t->priority > cur->priority);
    irq_restore(rf);

    if (should_preempt)
        schedule();
}

int thread_is_alive(thread_t *t) {
    return t && t->magic == THREAD_MAGIC && t->state != THREAD_EXITED;
}

void thread_kill(thread_t *t) {
    if (!t || t->magic != THREAD_MAGIC) return;

    uint64_t rf = irq_save_disable();
    t->state = THREAD_EXITED;

    int cpu = smp_cpu_index();
    thread_t *cur = current_cpu[cpu];

    if (t == cur) {
        irq_restore(rf);
        schedule(); // will add to zombies on return path
        return;
    }

    rq_remove(cpu, t);
    add_to_zombie_list(t);
    irq_restore(rf);
}

static void rq_on_priority_change(int cpu, thread_t *t) {
    rq_requeue_tail(cpu, t);
}

void thread_set_priority(thread_t *t, int priority) {
    if (!t || t->magic != THREAD_MAGIC) return;

    if (priority < MIN_PRIORITY) priority = MIN_PRIORITY;
    if (priority > MAX_PRIORITY) priority = MAX_PRIORITY;

    uint64_t rf = irq_save_disable();
    int old = t->priority;
    t->priority = priority;

    int cpu = smp_cpu_index();
    rq_on_priority_change(cpu, t);

    thread_t *cur = thread_current();
    int should_yield =
        ((t != cur && t->state == THREAD_READY && t->priority > (cur ? cur->priority : MIN_PRIORITY)) ||
         (t == cur && priority < old));
    irq_restore(rf);

    if (should_yield)
        schedule();
}

void thread_join(thread_t *t) {
    if (!t || t->magic != THREAD_MAGIC) return;

    while (thread_is_alive(t)) {
        __asm__ volatile("pause");
        thread_yield();
    }
}

void thread_yield(void) {
    schedule();
}

// --- Example Service Threads ---
static void regx_thread_func(void) {
    ipc_message_t msg;
    while (1) {
        if (ipc_receive_blocking(&regx_queue, thread_self(), &msg) == 0) {
            ipc_message_t reply = (ipc_message_t){0};
            reply.type = msg.type;
            ipc_send(&regx_queue, thread_self(), &reply);
        }
        thread_yield();
    }
}

static void login_thread_func(void) {
    // NOTE: If login_server needs caps, ensure they are granted before this runs.
    login_server(&fs_queue, thread_self());
}

static void init_thread_func(void) {
    static uint32_t init_tid = 0;
    init_tid = thread_self();
    thread_create_with_priority(login_thread_func, 180);
    /*
     * The init thread originally ran at a higher priority than the login
     * server it launches.  Because the scheduler always selects the highest
     * priority READY thread, the init thread would immediately pre-empt the
     * newly created login thread on every yield, effectively starving it and
     * preventing the login prompt from ever appearing.  Drop our priority to
     * the minimum so other services, like the login server, can run.
     */
    thread_set_priority(thread_current(), MIN_PRIORITY);
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

    // main_thread already installed in threads_early_init
}
