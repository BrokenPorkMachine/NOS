#include "thread.h"
#include "../IPC/ipc.h"
#include "../../user/servers/init/init.h"
#include "../../user/libc/libc.h"
#include "../drivers/IO/serial.h"
#include <stdint.h>
#include <stddef.h>
#include <assert.h>
#include "../arch/CPU/smp.h"

#define STACK_SIZE 8192    // Use a bigger, safer stack
#define MAX_PRIORITY 255   // Highest priority value
#define MIN_PRIORITY 0     // Lowest priority value

// --- Thread and scheduler state ---

thread_t *current_cpu[MAX_CPUS] = {0};
static thread_t *tail_cpu[MAX_CPUS] = {0};
static int next_id = 1;
static thread_t main_thread; // represents kernel_main for initial switch

// Zombie (exited thread) list for cleanup:
static thread_t *zombie_list = NULL;

ipc_queue_t fs_queue;
ipc_queue_t pkg_queue;
ipc_queue_t upd_queue;

// --- Utility: decimal integer to string (robust) ---

static void utoa_dec(uint32_t val, char *buf) {
    char tmp[20]; // Large enough for uint32_t + null
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

// --- THREAD START HELPERS ---

__attribute__((used)) static void thread_start(void (*f)(void)) {
    f();
    thread_t *cur = current_cpu[smp_cpu_index()];
    cur->state = THREAD_EXITED;
    thread_yield();
    for (;;) __asm__ volatile("hlt");
}

// naked entry: pops function pointer into rdi, calls thread_start
static void __attribute__((naked)) thread_entry(void) {
    __asm__ volatile(
        "pop %rdi\n"
        "call thread_start\n"
        "hlt\n"
    );
}

// --- THREAD STRUCTURE ---

// Add priority field to thread struct in thread.h:
/*
typedef struct thread {
    uint64_t rsp;
    void *stack;
    void (*func)(void);
    uint32_t id;
    uint8_t state;
    uint8_t started;
    uint8_t priority;      // NEW FIELD
    struct thread *next;
} thread_t;
*/

// --- THREAD FUNCTIONS ---

thread_t *thread_current(void) { return current_cpu[smp_cpu_index()]; }

uint32_t thread_self(void) { return thread_current()->id; }

// --- THREAD CREATION ---

thread_t *thread_create_with_priority(void (*func)(void), uint8_t priority) {
    thread_t *t = malloc(sizeof(thread_t));
    if (!t) return NULL;
    t->stack = malloc(STACK_SIZE);
    if (!t->stack) { free(t); return NULL; }

    uint64_t *sp = (uint64_t *)(t->stack + STACK_SIZE);

    // Ensure 16-byte alignment after all pushes (System V ABI: RSP+8 mod 16 == 0 at call)
    // We'll push: arg, RIP, rflags, rbp, rbx, r12, r13, r14, r15 (9 values = 72 bytes)
    if (((uintptr_t)sp & 0xF) != 0)
        --sp; // Pad for alignment if needed
    assert(((uintptr_t)sp & 0xF) == 0);

    // Optional dummy for alignment
    *--sp = 0;

    // Stack grows downward, so push in reverse order of popping:
    *--sp = (uint64_t)func;         // argument for thread_entry (popped into rdi)
    *--sp = (uint64_t)thread_entry; // RIP
    *--sp = 0x202;                  // RFLAGS (with IF=1)
    *--sp = 0; // rbp
    *--sp = 0; // rbx
    *--sp = 0; // r12
    *--sp = 0; // r13
    *--sp = 0; // r14
    *--sp = 0; // r15

    // Alignment assertion
    assert(((uintptr_t)sp & 0xF) == 0);

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
    utoa_dec(t->id, buf);
    serial_puts(buf);
    serial_puts(" prio=");
    utoa_dec(t->priority, buf);
    serial_puts(buf);
    serial_puts("\n");
    return t;
}

thread_t *thread_create(void (*func)(void)) {
    // Default priority = 128 (mid value)
    return thread_create_with_priority(func, 128);
}

// --- THREAD BLOCK/UNBLOCK ---

void thread_block(thread_t *t) {
    char buf[20];
    serial_puts("[thread] block id=");
    utoa_dec(t->id, buf); serial_puts(buf); serial_puts("\n");
    t->state = THREAD_BLOCKED;
    if (t == current_cpu[smp_cpu_index()])
        schedule();
}

void thread_unblock(thread_t *t) {
    char buf[20];
    serial_puts("[thread] unblock id=");
    utoa_dec(t->id, buf); serial_puts(buf); serial_puts("\n");
    t->state = THREAD_READY;

    // Optional: preempt if the unblocked thread has higher priority than current
    thread_t *cur = current_cpu[smp_cpu_index()];
    if (t->priority > cur->priority && cur->state == THREAD_RUNNING) {
        schedule();
    }
}

void thread_yield(void) {
    char buf[20];
    thread_t *cur = current_cpu[smp_cpu_index()];
    serial_puts("[thread] yield id=");
    utoa_dec(cur->id, buf); serial_puts(buf); serial_puts("\n");
    schedule();
}

// --- ZOMBIE REAPER ---

void thread_reap(void) {
    thread_t *prev = NULL, *t = zombie_list;
    while (t) {
        thread_t *next = t->next;
        char buf[20];
        serial_puts("[thread] free id=");
        utoa_dec(t->id, buf); serial_puts(buf); serial_puts("\n");
        if (t->stack) free(t->stack);
        free(t);
        if (prev)
            prev->next = next;
        else
            zombie_list = next;
        t = next;
    }
}

// --- THREAD SYSTEM INIT ---

static void thread_init_func(void) {
    serial_puts("[init] init server started\n");
    init_main(&fs_queue, thread_current()->id);
    for (;;) {
        thread_yield();
    }
}

void threads_init(void) {
    ipc_init(&fs_queue);
    ipc_init(&pkg_queue);
    ipc_init(&upd_queue);
    thread_t *t;

    t = thread_create_with_priority(thread_init_func, 200);
    if (!t) { serial_puts("[thread] FATAL: cannot create init server\n"); for (;;) __asm__("hlt"); }
    ipc_grant(&fs_queue, t->id, IPC_CAP_SEND | IPC_CAP_RECV);
    ipc_grant(&pkg_queue, t->id, IPC_CAP_SEND | IPC_CAP_RECV);
    ipc_grant(&upd_queue, t->id, IPC_CAP_SEND | IPC_CAP_RECV);

    // Insert kernel main thread into the run queue so first schedule() can switch away safely.
    main_thread.id    = 0;
    main_thread.func  = NULL;
    main_thread.stack = NULL;
    main_thread.state = THREAD_RUNNING;
    main_thread.started = 1;
    main_thread.priority = MAX_PRIORITY; // Kernel main always highest

    int cpu = smp_cpu_index();
    main_thread.next = current_cpu[cpu];
    tail_cpu[cpu]->next = &main_thread;
    tail_cpu[cpu] = &main_thread;
    current_cpu[cpu] = &main_thread;
}

// --- PRIORITY SCHEDULER (picks highest-priority READY thread) ---

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

void schedule(void) {
    uint64_t rflags;
    __asm__ volatile("pushfq; pop %0; cli" : "=r"(rflags) :: "memory");

    int cpu = smp_cpu_index();
    thread_t *prev = current_cpu[cpu];
    if (prev->state == THREAD_RUNNING)
        prev->state = THREAD_READY;

    // Priority: always pick the highest-priority READY thread
    thread_t *next = pick_next(cpu);
    if (!next) {
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

    ((uint64_t*)prev->rsp)[6] = rflags; // restore saved rflags for previous thread

    // If previous thread has exited, move it to zombie list
    if (prev->state == THREAD_EXITED) {
        prev->next = zombie_list;
        zombie_list = prev;
    }

    // Reap any exited threads (cleanup)
    thread_reap();
}

// --- ISR CONTEXT SCHEDULING (for interrupts) ---

uint64_t schedule_from_isr(uint64_t *old_rsp) {
    int cpu = smp_cpu_index();
    thread_t *prev = current_cpu[cpu];

    // Reshape stack to expected layout for context_switch (see your comments)
    uint64_t *frame = old_rsp;
    uint64_t rbp = frame[4];      // preserve rbp before overwriting
    frame[4] = frame[13];         // rbx position
    frame[5] = rbp;               // rbp position
    frame[6] = frame[15];         // rflags
    frame[7] = frame[17];         // rip

    prev->rsp = (uint64_t)frame;
    if (prev->state == THREAD_RUNNING)
        prev->state = THREAD_READY;

    // Pick highest-priority READY thread
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
    // Note: can't do thread_reap() here; do it at next schedule()

    return next->rsp;
}
