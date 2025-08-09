// kernel/Task/thread.c
#include "thread.h"
#include "../IPC/ipc.h"
#include "../../user/libc/libc.h"
#include <stdint.h>
#include "../arch/CPU/smp.h"

// --- Agents brought up by regx (weak so we build even if not linked yet)
__attribute__((weak)) void nosm_entry(void);
__attribute__((weak)) void nosfs_server(ipc_queue_t *fsq, uint32_t self_tid);
__attribute__((weak)) void init_main(ipc_queue_t *fsq, ipc_queue_t *pkgq, ipc_queue_t *updq, uint32_t self_tid);

#ifndef STACK_SIZE
#define STACK_SIZE 8192
#endif

#define THREAD_MAGIC 0x74687264UL

// Timer readiness: set to 1 in PIT/LAPIC init once the timer ISR is armed and interrupts are enabled.
int timer_ready = 0;

#ifndef SCHED_TRACE
#define SCHED_TRACE 0
#endif

extern void context_switch(uint64_t *prev_rsp, uint64_t next_rsp);

// ---- Saved frame layout restored by context_switch.asm
typedef struct context_frame {
    uint64_t r15, r14, r13, r12, rbx, rbp;
    uint64_t rflags;
    uint64_t rax_dummy;   // consumed by 'pop rax' in context_switch
    uint64_t rip;         // -> thread_entry
    uint64_t arg_rdi;     // consumed by 'pop %rdi' in thread_entry
} context_frame_t;

static inline uintptr_t align16(uintptr_t v) { return v & ~0xFULL; }

// Zombie list (simple SLIST)
static thread_t *zombie_list = NULL;

static thread_t thread_pool[MAX_KERNEL_THREADS];
static char     stack_pool[MAX_KERNEL_THREADS][STACK_SIZE];

// Per-CPU run-queue (circular SLL; tail points to last node)
thread_t *current_cpu[MAX_CPUS] = {0};
static thread_t *tail_cpu[MAX_CPUS] = {0};

static int next_id = 1;
static thread_t main_thread; // CPU0 bootstrap/idle

// IPC queues
ipc_queue_t fs_queue, pkg_queue, upd_queue, init_queue, regx_queue;

// ---- Optional SSE enable (for kernel-side SIMD)
static void enable_sse(void) {
#if defined(__x86_64__)
    uint64_t cr0, cr4;
    __asm__ volatile("mov %%cr0, %0" : "=r"(cr0));
    cr0 &= ~(1ULL << 2);  // EM=0
    cr0 |=  (1ULL << 1);  // MP=1
    __asm__ volatile("mov %0, %%cr0" :: "r"(cr0));
    __asm__ volatile("mov %%cr4, %0" : "=r"(cr4));
    cr4 |= (1ULL << 9) | (1ULL << 10); // OSFXSR|OSXMMEXCPT
    __asm__ volatile("mov %0, %%cr4" :: "r"(cr4));
#endif
}

// ---- IRQ helpers
static inline uint64_t irq_save_disable(void) {
    uint64_t rf;
    __asm__ volatile("pushfq; pop %0; cli" : "=r"(rf) :: "memory");
    return rf;
}
static inline void irq_restore(uint64_t rf) {
    __asm__ volatile("push %0; popfq" :: "r"(rf) : "memory");
}

// ---- Run-queue helpers (call with IF=0)
static inline void rq_insert_tail(int cpu, thread_t *t) {
    if (!current_cpu[cpu]) {
        current_cpu[cpu] = tail_cpu[cpu] = t;
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

// ---- Early bootstrap: install main_thread as RUNNING
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

    current_cpu[0] = tail_cpu[0] = &main_thread;
}

// ---- Query
thread_t *thread_current(void) { return current_cpu[smp_cpu_index()]; }
uint32_t  thread_self(void)    { thread_t *t = thread_current(); return t ? t->id : 0; }

// ---- Zombie management
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
        memset(t, 0, sizeof(thread_t)); // free slot
        t = n;
    }
}

// ---- Choose next
static thread_t *pick_next(int cpu) {
    thread_t *start = current_cpu[cpu];
    if (!start) return NULL;

    thread_t *t = start, *best = NULL;
    do {
        if (t->state == THREAD_READY && (!best || t->priority > best->priority))
            best = t;
        t = t->next;
    } while (t && t != start);

    if (!best) {
        if (start->state == THREAD_READY || start->state == THREAD_RUNNING)
            best = start;
    }
    if (!best) best = &main_thread; // ultimate fallback
    return best;
}

// ---- Core scheduler
void schedule(void) {
    uint64_t rf;
    __asm__ volatile("pushfq; pop %0; cli" : "=r"(rf) :: "memory");

    int cpu = smp_cpu_index();
    thread_t *prev = current_cpu[cpu];
    if (!prev) {
        __asm__ volatile("push %0; popfq" :: "r"(rf) : "memory");
        __asm__ volatile("hlt");
        return;
    }

    if (prev->state == THREAD_RUNNING)
        prev->state = THREAD_READY;

#if SCHED_TRACE
    uint32_t prev_id_dbg = prev ? prev->id : 0;
#endif

    thread_t *next = pick_next(cpu);

    if (!next) {
        prev->state = THREAD_RUNNING;
        __asm__ volatile("push %0; popfq" :: "r"(rf) : "memory");
        __asm__ volatile("hlt");
        return;
    }

#if SCHED_TRACE
    kprintf("[sched] %u -> %u\n", prev_id_dbg, next ? next->id : 0);
#endif

    next->state = THREAD_RUNNING;
    next->started = 1;

    current_cpu[cpu] = next;
    context_switch(&prev->rsp, next->rsp);

    if (prev->state == THREAD_EXITED)
        add_to_zombie_list(prev);

    thread_reap();
}

uint64_t schedule_from_isr(uint64_t *old_rsp) {
    int cpu = smp_cpu_index();
    thread_t *prev = current_cpu[cpu];
    if (!prev) return (uint64_t)old_rsp;

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

#if SCHED_TRACE
    kprintf("[sched-isr] %u -> %u\n", prev ? prev->id : 0, next ? next->id : 0);
#endif

    return next->rsp;
}

// ---- Thread exit (Option B)
__attribute__((noreturn))
void thread_exit(void) {
    thread_t *t = thread_current();
    if (t) t->state = THREAD_EXITED;
    schedule();
    __builtin_unreachable();
}

// ---- Trampoline chain (Option B)
__attribute__((noreturn, used))
static void thread_start(void (*f)(void)) {
#if SCHED_TRACE
    kprintf("[thread] start %u\n", thread_self());
#endif
    f();
    thread_exit();
}

static void __attribute__((naked, noreturn)) thread_entry(void) {
    __asm__ volatile(
        "pop %rdi\n"          // arg -> %rdi
        "call thread_start\n" // never returns
        "jmp .\n"
    );
}

// ---- Creation
thread_t *thread_create_with_priority(void (*func)(void), int priority) {
    if (priority < MIN_PRIORITY) priority = MIN_PRIORITY;
    if (priority > MAX_PRIORITY) priority = MAX_PRIORITY;

    thread_t *t = NULL; int index = -1;
    for (int i = 0; i < (int)MAX_KERNEL_THREADS; i++) {
        if (thread_pool[i].magic == 0) { t = &thread_pool[i]; index = i; break; }
    }
    if (!t) return NULL;

    if ((uintptr_t)t < 0x1000) return NULL;

    memset(t, 0, sizeof(thread_t));
    t->magic = THREAD_MAGIC;
    t->stack = stack_pool[index];

    uintptr_t top = align16((uintptr_t)t->stack + STACK_SIZE);
    context_frame_t *cf = (context_frame_t *)(top - sizeof(*cf));
    *cf = (context_frame_t){
        .r15=0,.r14=0,.r13=0,.r12=0,.rbx=0,.rbp=0,
        .rflags=0x202,             // IF=1
        .rax_dummy=0,
        .rip=(uint64_t)thread_entry,
        .arg_rdi=(uint64_t)func
    };

    t->rsp      = (uint64_t)cf;
    t->func     = func;
    t->id       = __atomic_fetch_add(&next_id, 1, __ATOMIC_RELAXED);
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
    if (t == thread_current()) schedule();
}

void thread_unblock(thread_t *t) {
    if (!t || t->magic != THREAD_MAGIC) return;
    uint64_t rf = irq_save_disable();
    t->state = THREAD_READY;
    thread_t *cur = thread_current();
    int should_preempt = (cur && cur->state == THREAD_RUNNING && t->priority > cur->priority);
    irq_restore(rf);
    if (should_preempt) schedule();
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
        schedule(); // will add to zombies on return
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

    if (should_yield) schedule();
}

void thread_join(thread_t *t) {
    if (!t || t->magic != THREAD_MAGIC) return;
    while (thread_is_alive(t)) {
        __asm__ volatile("pause");
        thread_yield();
    }
}

void thread_yield(void) { schedule(); }

// ---- Agent threads ----

// regx: responsible for bootstrapping the agent system (nosm, nosfs, init)
static void regx_thread_func(void) {
    // Load/initialize NOSM if linked
    if (nosm_entry) {
        nosm_entry();
    } else {
        kprintf("[regx] NOSM agent not linked; skipping\n");
    }

    // Launch NOSFS server if linked
    thread_t *nosfs_t = NULL;
    if (nosfs_server) {
        nosfs_t = thread_create_with_priority((void(*)(void)) (void*) ( // cast to match trampoline
            ^{ nosfs_server(&fs_queue, thread_self()); } ), 200);
        // NOTE: GCC doesn't support statement-lambdas; we'll wrap below instead.
    }

    // Launch INIT if linked
    thread_t *init_t = NULL;
    if (init_main) {
        init_t = thread_create_with_priority((void(*)(void)) (void*) (
            ^{ init_main(&fs_queue, &pkg_queue, &upd_queue, thread_self()); } ), 200);
    }

    // The above lambda trick isn't C; provide real wrappers:
    // (We define them below and reassign if symbols are present.)
    (void)nosfs_t; (void)init_t;

    // regx main loop can act as a simple router/heartbeat
    ipc_message_t msg;
    while (1) {
        if (ipc_receive_blocking(&regx_queue, thread_self(), &msg) == 0) {
            // echo or route as desired
            ipc_send(&regx_queue, thread_self(), &msg);
        }
        thread_yield();
    }
}

// Real C wrappers for the weak agent entrypoints:
static void nosfs_thread_wrapper(void) {
    if (nosfs_server) nosfs_server(&fs_queue, thread_self());
    thread_exit();
}
static void init_thread_wrapper(void) {
    if (init_main) init_main(&fs_queue, &pkg_queue, &upd_queue, thread_self());
    thread_exit();
}

// init thread that regx would spawn (used by wrapper creation above)

static void spawn_agents_from_regx(void) {
    if (nosfs_server) {
        thread_t *t = thread_create_with_priority(nosfs_thread_wrapper, 200);
        if (t) {
            ipc_grant(&fs_queue, t->id, IPC_CAP_SEND | IPC_CAP_RECV);
        } else {
            kprintf("[regx] failed to spawn nosfs\n");
        }
    } else {
        kprintf("[regx] nosfs_server not linked; skipping\n");
    }

    if (init_main) {
        thread_t *t = thread_create_with_priority(init_thread_wrapper, 200);
        if (t) {
            ipc_grant(&fs_queue, t->id,  IPC_CAP_SEND | IPC_CAP_RECV);
            ipc_grant(&pkg_queue, t->id, IPC_CAP_SEND | IPC_CAP_RECV);
            ipc_grant(&upd_queue, t->id, IPC_CAP_SEND | IPC_CAP_RECV);
        } else {
            kprintf("[regx] failed to spawn init\n");
        }
    } else {
        kprintf("[regx] init_main not linked; skipping\n");
    }
}

// ---- Init threading and launch core services ----
void threads_init(void) {
    // Bring up queues first
    ipc_init(&fs_queue);
    ipc_init(&pkg_queue);
    ipc_init(&upd_queue);
    ipc_init(&init_queue);
    ipc_init(&regx_queue);

    // Spawn regx (it will load nosm, nosfs, init)
    thread_t *regx = thread_create_with_priority(regx_thread_func, 220);
    if (!regx) { for (;;) __asm__ volatile("hlt"); }

    // Give regx capabilities to bootstrap the rest
    ipc_grant(&regx_queue, regx->id, IPC_CAP_SEND | IPC_CAP_RECV);

    // Let regx immediately spin up nosfs/init (if linked)
    spawn_agents_from_regx();

    // Hand off only if your timer is armed (avoid deadlock before preemption)
    if (timer_ready) {
        thread_yield();
    }
}
