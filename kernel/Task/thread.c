#include "thread.h"
#include "../IPC/ipc.h"
#include "../../user/servers/init/init.h"
#include "../../user/libc/libc.h"
#include "../drivers/IO/serial.h"
#include <stdint.h>
#include "../arch/CPU/smp.h"

#ifndef STACK_SIZE
#define STACK_SIZE 8192
#endif

#define THREAD_MAGIC 0x74687264UL

// Zombie list to track exited threads for reaping
static thread_t *zombie_list = NULL;

// Per-CPU run queue pointers
thread_t *current_cpu[MAX_CPUS] = {0};
static thread_t *tail_cpu[MAX_CPUS] = {0};

// Next thread ID (atomic in SMP, for now single-core-safe)
static int next_id = 1;

// Main thread stub (represents kernel_main)
static thread_t main_thread;

// IPC queues for servers (exposed globally)
ipc_queue_t fs_queue;
ipc_queue_t pkg_queue;
ipc_queue_t upd_queue;

// Utility: Convert unsigned int to decimal string (for logging)
static void utoa_dec(uint32_t val, char *buf) {
    char tmp[20];
    int i = 0, j = 0;
    if (!val) {
        buf[0] = '0';
        buf[1] = 0;
        return;
    }
    while (val && i < (int)sizeof(tmp) - 1) {
        tmp[i++] = '0' + (val % 10);
        val /= 10;
    }
    while (i)
        buf[j++] = tmp[--i];
    buf[j] = 0;
}

// --- Thread trampoline ---
// Runs each new thread's function, marks exited, yields forever
__attribute__((used)) static void thread_start(void (*f)(void)) {
    f(); // Run user function
    thread_t *cur = thread_current();
    cur->state = THREAD_EXITED;
    thread_yield(); // Never returns
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

// Return pointer to currently running thread
thread_t *thread_current(void) {
    return current_cpu[smp_cpu_index()];
}

// Return ID of currently running thread
uint32_t thread_self(void) {
    thread_t *t = thread_current();
    return t ? t->id : 0;
}

// Add a thread to the zombie list for safe deletion
static void add_to_zombie_list(thread_t *t) {
    t->next = zombie_list;
    zombie_list = t;
}

// Create a new thread with specified priority
thread_t *thread_create_with_priority(void (*func)(void), int priority) {
    if (priority < MIN_PRIORITY) priority = MIN_PRIORITY;
    if (priority > MAX_PRIORITY) priority = MAX_PRIORITY;

    thread_t *t = malloc(sizeof(thread_t));
    if (!t) return NULL;

    t->magic = THREAD_MAGIC;
    t->stack = malloc(STACK_SIZE);
    if (!t->stack) {
        free(t);
        return NULL;
    }

    uint64_t *sp = (uint64_t *)(t->stack + STACK_SIZE);

    // Align to 16-bytes, as required by x86_64 ABI
    if (((uintptr_t)sp & 0xF) != 0)
        --sp;

    // Layout: [func][thread_entry][RFLAGS][rbp][rbx][r12][r13][r14][r15]
    *--sp = 0;                    // (optional dummy for clarity)
    *--sp = (uint64_t)func;       // Function arg for entry
    *--sp = (uint64_t)thread_entry; // RIP
    *--sp = 0x202;                // RFLAGS (IF=1)
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

    char buf[20];
    serial_puts("[thread] created id=");
    utoa_dec(t->id, buf); serial_puts(buf);
    serial_puts(" prio=");
    utoa_dec(t->priority, buf); serial_puts(buf);
    serial_puts("\n");

    return t;
}

// Create a new thread with default (mid-level) priority
thread_t *thread_create(void (*func)(void)) {
    return thread_create_with_priority(func, (MAX_PRIORITY + MIN_PRIORITY) / 2);
}

// Mark thread as blocked, invoke scheduler if it is current
void thread_block(thread_t *t) {
    if (!t || t->magic != THREAD_MAGIC) return;
    char buf[20];
    serial_puts("[thread] block id=");
    utoa_dec(t->id, buf); serial_puts(buf); serial_puts("\n");
    t->state = THREAD_BLOCKED;
    if (t == thread_current())
        schedule();
}

// Unblock a thread and pre-empt if higher prio
void thread_unblock(thread_t *t) {
    if (!t || t->magic != THREAD_MAGIC) return;
    char buf[20];
    serial_puts("[thread] unblock id=");
    utoa_dec(t->id, buf); serial_puts(buf); serial_puts("\n");
    t->state = THREAD_READY;
    thread_t *cur = thread_current();
    if (t->priority > cur->priority && cur->state == THREAD_RUNNING)
        schedule();
}

// Returns 1 if thread is not exited and pointer valid
int thread_is_alive(thread_t *t) {
    return t && t->magic == THREAD_MAGIC && t->state != THREAD_EXITED;
}

// Kill a thread (remove from runqueue, mark as exited)
void thread_kill(thread_t *t) {
    if (!t || t->magic != THREAD_MAGIC) return;
    char buf[20];
    serial_puts("[thread] kill id=");
    utoa_dec(t->id, buf); serial_puts(buf); serial_puts("\n");

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

// Yield CPU to another ready thread
void thread_yield(void) {
    char buf[20];
    thread_t *cur = thread_current();
    serial_puts("[thread] yield id=");
    utoa_dec(cur->id, buf); serial_puts(buf); serial_puts("\n");
    schedule();
}

// Free all zombie threads
static void thread_reap(void) {
    thread_t *t = zombie_list;
    zombie_list = NULL;
    while (t) {
        thread_t *next = t->next;
        char buf[20];
        serial_puts("[thread] free id=");
        utoa_dec(t->id, buf); serial_puts(buf); serial_puts("\n");
        if (t->stack) free(t->stack);
        t->magic = 0;
        free(t);
        t = next;
    }
}

// Pick next ready thread with highest priority (round-robin among equals)
static thread_t *pick_next(int cpu) {
    thread_t *start = current_cpu[cpu];
    thread_t *t = start;
    thread_t *best = NULL;
    char buf[32];
    do {
        t = t->next;
        serial_puts("[sched] consider id=");
        utoa_dec(t->id, buf); serial_puts(buf);
        serial_puts(" state=");
        utoa_dec(t->state, buf); serial_puts(buf);
        serial_puts(" prio=");
        utoa_dec(t->priority, buf); serial_puts(buf);
        serial_puts("\n");
        if (t->state == THREAD_READY) {
            if (!best || t->priority > best->priority)
                best = t;
        }
    } while (t != start);
    return best;
}

// Main scheduler (invoked by yield, block, unblock, etc)
void schedule(void) {
    uint64_t rflags;
    // Save rflags and disable interrupts
    __asm__ volatile("pushfq; pop %0; cli" : "=r"(rflags) :: "memory");

    int cpu = smp_cpu_index();
    thread_t *prev = current_cpu[cpu];

    // Mark current as ready if it was running
    if (prev->state == THREAD_RUNNING)
        prev->state = THREAD_READY;

    // Pick next ready thread
    thread_t *next = pick_next(cpu);

    if (!next) {
        // No ready threads; run idle
        prev->state = THREAD_RUNNING;
        serial_puts("[sched] idle\n");
        __asm__ volatile("push %0; popfq; hlt" :: "r"(rflags) : "memory");
        return;
    }

    next->state = THREAD_RUNNING;
    next->started = 1;

    char buf[32];
    serial_puts("[sched] switch ");
    utoa_dec(prev->id, buf); serial_puts(buf);
    serial_puts(" -> ");
    utoa_dec(next->id, buf); serial_puts(buf);
    serial_puts(" [prio ");
    utoa_dec(next->priority, buf); serial_puts(buf);
    serial_puts("]\n");

    current_cpu[cpu] = next;
    context_switch(&prev->rsp, next->rsp);
    current_cpu[cpu] = prev;

    // Restore saved rflags
    ((uint64_t *)prev->rsp)[6] = rflags;

    // If previous thread exited, add to zombies
    if (prev->state == THREAD_EXITED)
        add_to_zombie_list(prev);

    thread_reap();
}

// Scheduler entry from ISR context (interrupt/IRQ)
uint64_t schedule_from_isr(uint64_t *old_rsp) {
    int cpu = smp_cpu_index();
    thread_t *prev = current_cpu[cpu];
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

    char buf[32];
    serial_puts("[sched] preempt ");
    utoa_dec(prev->id, buf); serial_puts(buf);
    serial_puts(" -> ");
    utoa_dec(next->id, buf); serial_puts(buf);
    serial_puts(" [prio ");
    utoa_dec(next->priority, buf); serial_puts(buf);
    serial_puts("]\n");

    current_cpu[cpu] = next;

    return next->rsp;
}

// System thread startup: create and start init server
static void thread_init_func(void) {
    serial_puts("[init] init server started\n");
    init_main(&fs_queue, thread_current()->id);
    for (;;) thread_yield(); // Should never return
}

// Initialize threading system
void threads_init(void) {
    ipc_init(&fs_queue);
    ipc_init(&pkg_queue);
    ipc_init(&upd_queue);

    // Create init server (userland server thread)
    thread_t *t = thread_create_with_priority(thread_init_func, 200);
    if (!t) {
        serial_puts("[thread] FATAL: cannot create init server\n");
        for (;;) __asm__ volatile("hlt");
    }

    // Grant all capabilities to init
    ipc_grant(&fs_queue, t->id, IPC_CAP_SEND | IPC_CAP_RECV);
    ipc_grant(&pkg_queue, t->id, IPC_CAP_SEND | IPC_CAP_RECV);
    ipc_grant(&upd_queue, t->id, IPC_CAP_SEND | IPC_CAP_RECV);

    // Set up main kernel thread so it can yield safely
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
