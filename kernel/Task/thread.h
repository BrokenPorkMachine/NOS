#pragma once
#include <stdint.h>

// Optional: cacheline alignment helper
#if defined(__GNUC__) || defined(__clang__)
#  define THREAD_ALIGNED(N) __attribute__((aligned(N)))
#else
#  define THREAD_ALIGNED(N)
#endif

#ifdef __cplusplus
extern "C" {
#endif

#define MAX_CPUS      32
#define MIN_PRIORITY   0   // Lowest priority
#define MAX_PRIORITY 255   // Highest priority

// Maximum number of kernel threads that can exist simultaneously.
// Threads are allocated from a static pool to avoid malloc during
// early boot before the heap is fully initialized.
#define MAX_KERNEL_THREADS 64

typedef enum {
    THREAD_READY = 0,
    THREAD_RUNNING,
    THREAD_BLOCKED,
    THREAD_EXITED
} thread_state_t;

/**
 * Kernel thread descriptor. Keep fields hot in scheduling path first.
 * Layout matches thread.c expectations; no extra saved state here because
 * context_switch.asm pushes/pops callee-saved regs & rflags on the stack.
 */
typedef struct THREAD_ALIGNED(64) thread {
    uint64_t       rsp;       // Stack pointer for context switching
    void         (*func)(void); // Entry function
    char          *stack;     // Kernel stack base (from static pool)
    int            id;        // Thread ID (unique)
    thread_state_t state;     // Current state
    int            started;   // Has thread begun execution
    int            priority;  // Priority (0 = lowest, 255 = highest)
    struct thread *next;      // Run queue link (circular per-CPU)
    uint32_t       magic;     // Magic for corruption detection
} thread_t;

// Per-CPU currently running thread (head of that CPU's circular run-queue)
extern thread_t *current_cpu[MAX_CPUS];

/**
 * Reset scheduler bookkeeping before interrupts might fire.
 * Installs the bootstrap "main" thread on CPU0.
 */
void threads_early_init(void);

/**
 * Retrieve pointer to currently running thread on this CPU.
 */
thread_t *thread_current(void);

/**
 * Retrieve ID of currently running thread.
 */
uint32_t thread_self(void);

/**
 * Initialize threading system and create initial threads.
 */
void threads_init(void);
void regx_start(void);

/**
 * Create a new kernel thread with default (mid-level) priority.
 */
thread_t *thread_create(void (*func)(void));

/**
 * Create a new kernel thread with an explicit priority.
 * Priority is clamped to [MIN_PRIORITY, MAX_PRIORITY].
 */
thread_t *thread_create_with_priority(void (*func)(void), int priority);

/**
 * Mark thread as blocked and reschedule.
 */
void thread_block(thread_t *t);

/**
 * Mark thread as ready. If its priority exceeds the current thread’s,
 * it may pre-empt.
 */
void thread_unblock(thread_t *t);

/**
 * Return nonzero if the thread has not exited and is valid.
 */
int thread_is_alive(thread_t *t);

/**
 * Terminate a thread and reclaim its resources.
 */
void thread_kill(thread_t *t);

/**
 * Adjust the priority of a thread. Priority is clamped to the valid range
 * and the scheduler is invoked if the change should cause pre-emption.
 */
void thread_set_priority(thread_t *t, int priority);

/**
 * Block until the supplied thread has exited. This simple join primitive
 * is cooperative and repeatedly yields the processor while waiting.
 */
void thread_join(thread_t *t);

/**
 * Yield CPU to next ready thread (cooperative scheduling).
 */
void thread_yield(void);
int thread_runqueue_length(int cpu);

/**
 * Run the scheduler (internal, also used by yield/block/unblock).
 */
void schedule(void);

/**
 * Scheduler entry from interrupt context.
 * Takes the stack pointer saved by the interrupt stub and returns the
 * stack of the next thread.
 */
uint64_t schedule_from_isr(uint64_t *old_rsp);

/**
 * Switch stack (old_rsp, new_rsp): calls a small asm stub and returns to caller.
 *
 * ABI contract (matches your context_switch.asm):
 *   - Saves caller rflags and disables IF during the switch.
 *   - Saves callee-saved regs: rbp, rbx, r12, r13, r14, r15 (in that order).
 *   - Stores the previous RSP to *old_rsp if old_rsp != NULL.
 *   - Loads new RSP, restores regs in reverse, popfq, pop rax (dummy), ret.
 *   - Verifies all segment selectors are GDT entries both before and after.
 *
 * Thread stack at entry (top -> bottom) must be:
 *   r15, r14, r13, r12, rbx, rbp, rflags, rax_dummy, rip(thread_entry), arg
 */
void context_switch(uint64_t *old_rsp, uint64_t new_rsp);

/**
 * Enter user mode at given entry/stack: implemented in asm.
 * Does not return.
 */
void enter_user_mode(uint64_t entry, uint64_t user_stack) __attribute__((noreturn));

#ifdef __cplusplus
} // extern "C"
#endif
