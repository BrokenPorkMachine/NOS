#include "thread.h"
#include "../IPC/ipc.h"
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

// Simple decimal conversion for logging
static void utoa_dec(uint32_t val, char *buf) {
    char tmp[16];
    int i = 0, j = 0;
    if (!val) {
        buf[0] = '0';
        buf[1] = 0;
        return;
    }
    while (val && i < (int)sizeof(tmp)) {
        tmp[i++] = '0' + (val % 10);
        val /= 10;
    }
    while (i)
        buf[j++] = tmp[--i];
    buf[j] = 0;
}

// --- THREAD START HELPERS ---

// Entry trampoline for newly created threads. The scheduler initially
// switches to a stack prepared by thread_create(). At that point the
// return address is thread_entry and the next value on the stack is the
// function pointer the thread should run. We pop that function pointer and
// invoke it. When the function returns the thread marks itself as exited
// and yields back to the scheduler.
__attribute__((used)) static void thread_start(void (*f)(void)) {
    f();
    thread_t *cur = current_cpu[smp_cpu_index()];
    cur->state = THREAD_EXITED;
    thread_yield();
    for (;;) __asm__ volatile("hlt");
}

static void __attribute__((naked)) thread_entry(void) {
    __asm__ volatile(
        "pop %rdi\n"
        "call thread_start\n"
        "hlt\n"
    );
}

// --- THREAD FUNCTIONS ---

// Return the currently running thread
thread_t *thread_current(void) { return current_cpu[smp_cpu_index()]; }

// Return ID of current thread
uint32_t thread_self(void) {
    return thread_current()->id;
}

static void thread_init_func(void) {
    serial_puts("[init] init server started\n");
    init_main(&fs_queue, thread_current()->id);
    /*
     * init_main never returns, but the optimizer may tail-call it and
     * skip pushing a return address.  This leaves the stack misaligned
     * for subsequent function calls and can trigger general protection
     * faults when SSE instructions require 16-byte alignment.  Include
     * a reachable loop after the call to ensure a normal call sequence
     * is generated and the stack remains correctly aligned.
     */
    for (;;) {
        thread_yield();
    }
}

// --- THREAD CREATION ---

thread_t *thread_create(void (*func)(void)) {
    thread_t *t = malloc(sizeof(thread_t));
    if (!t) return NULL;
    t->stack = malloc(STACK_SIZE);
    if (!t->stack) { free(t); return NULL; }

    // Setup stack to match expectations of context_switch.
    // Layout grows downward and ends with r15 on top:
    //   [r15, r14, r13, r12, rbx, rbp, rflags, rip, arg]
    uint64_t *sp = (uint64_t *)(t->stack + STACK_SIZE);

    *--sp = (uint64_t)func;         // argument for thread_entry
    *--sp = (uint64_t)thread_entry; // RIP
    *--sp = 0x202;                  // RFLAGS with IF set
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
    char buf[16];
    serial_puts("[thread] created id=");
    utoa_dec(t->id, buf);
    serial_puts(buf);
    serial_puts("\n");
    return t;
}

// --- THREAD CONTROL ---

void thread_block(thread_t *t) {
    char buf[16];
    serial_puts("[thread] block id=");
    utoa_dec(t->id, buf); serial_puts(buf); serial_puts("\n");
    t->state = THREAD_BLOCKED;
    if (t == current_cpu[smp_cpu_index()])
        schedule();
}

void thread_unblock(thread_t *t) {
    char buf[16];
    serial_puts("[thread] unblock id=");
    utoa_dec(t->id, buf); serial_puts(buf); serial_puts("\n");
    t->state = THREAD_READY;
}

void thread_yield(void) {
    char buf[16];
    thread_t *cur = current_cpu[smp_cpu_index()];
    serial_puts("[thread] yield id=");
    utoa_dec(cur->id, buf); serial_puts(buf); serial_puts("\n");
    schedule();
}

// --- THREAD SYSTEM INIT ---

void threads_init(void) {
    ipc_init(&fs_queue);
    ipc_init(&pkg_queue);
    ipc_init(&upd_queue);
    thread_t *t;

    t = thread_create(thread_init_func);
    ipc_grant(&fs_queue, t->id, IPC_CAP_SEND | IPC_CAP_RECV);
    ipc_grant(&pkg_queue, t->id, IPC_CAP_SEND | IPC_CAP_RECV);
    ipc_grant(&upd_queue, t->id, IPC_CAP_SEND | IPC_CAP_RECV);

    // Insert kernel main thread into the run queue so the first
    // schedule() call can switch away safely.
    main_thread.id    = 0;
    main_thread.func  = NULL;
    main_thread.stack = NULL;
    main_thread.state = THREAD_RUNNING;
    main_thread.started = 1;

    int cpu = smp_cpu_index();
    main_thread.next = current_cpu[cpu];
    tail_cpu[cpu]->next = &main_thread;
    tail_cpu[cpu] = &main_thread;
    current_cpu[cpu] = &main_thread;
}

// --- SCHEDULER: simple round-robin, skips non-ready threads ---
static thread_t *pick_next(int cpu) {
    thread_t *start = current_cpu[cpu];
    char buf[32];
    do {
        current_cpu[cpu] = current_cpu[cpu]->next;
        serial_puts("[sched] consider id=");
        utoa_dec(current_cpu[cpu]->id, buf); serial_puts(buf);
        serial_puts(" state=");
        utoa_dec(current_cpu[cpu]->state, buf); serial_puts(buf);
        serial_puts("\n");
        if (current_cpu[cpu]->state == THREAD_READY)
            return current_cpu[cpu];
    } while (current_cpu[cpu] != start);
    current_cpu[cpu] = start;
    return NULL;
}

void schedule(void) {
    uint64_t rflags;
    __asm__ volatile("pushfq; pop %0; cli" : "=r"(rflags) :: "memory");

    int cpu = smp_cpu_index();
    thread_t *prev = current_cpu[cpu];
    if (prev->state == THREAD_RUNNING)
        prev->state = THREAD_READY;

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
    serial_puts("\n");

    /*
     * pick_next() updates current_cpu[cpu] to point at the thread that will
     * run next. After context_switch() returns we are back on the previous
     * thread, so restore current_cpu to match the thread actually executing.
     * Without this correction a preempted thread may resume with
     * current_cpu still referencing the task that replaced it, confusing
     * subsequent scheduler decisions.
     */
    current_cpu[cpu] = next;
    context_switch(&prev->rsp, next->rsp);
    current_cpu[cpu] = prev;
    ((uint64_t*)prev->rsp)[6] = rflags;
}

uint64_t schedule_from_isr(uint64_t *old_rsp) {
    int cpu = smp_cpu_index();
    thread_t *prev = current_cpu[cpu];
    /*
     * When an interrupt preempts a running thread the stub pushes a much
     * larger register frame onto that thread's stack than the one used by
     * context_switch().  If we saved the raw stack pointer here the next
     * context_switch() would misinterpret the layout and restore garbage
     * registers, crashing the system.  To keep the saved stack compatible
     * with context_switch() we reshape the frame to match the expected
     * [r15,r14,r13,r12,rbx,rbp,rflags,rip] layout before recording it.
     */
    uint64_t *frame = old_rsp;
    uint64_t rbp = frame[4];      // preserve rbp before overwriting
    frame[4] = frame[13];         // rbx position
    frame[5] = rbp;               // rbp position
    frame[6] = frame[15];         // rflags
    frame[7] = frame[17];         // rip

    prev->rsp = (uint64_t)frame;
    if (prev->state == THREAD_RUNNING)
        prev->state = THREAD_READY;

    thread_t *next = pick_next(cpu);
    if (!next) {
        /* No ready threads, continue running the current one */
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
    serial_puts("\n");

    /* pick_next() already advanced current_cpu[cpu] to the next thread. */
    current_cpu[cpu] = next;
    return next->rsp;
}

