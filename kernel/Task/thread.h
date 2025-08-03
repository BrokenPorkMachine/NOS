#pragma once
#include <stdint.h>

#define MAX_CPUS 32

typedef enum {
    THREAD_READY,
    THREAD_RUNNING,
    THREAD_BLOCKED,
    THREAD_EXITED
} thread_state_t;

typedef struct thread {
    uint64_t rsp;
    void (*func)(void);
    char *stack;
    int id;
    thread_state_t state;
    struct thread *next; // run queue link
} thread_t;

extern thread_t *current_cpu[MAX_CPUS];

// Initialize threading system and create initial threads
void threads_init(void);

// Create a new kernel thread with function entrypoint
thread_t *thread_create(void (*func)(void));

// Mark thread as blocked and reschedule
void thread_block(thread_t *t);

// Mark thread as ready (for scheduler)
void thread_unblock(thread_t *t);

// Yield CPU to next ready thread (cooperative scheduling)
void thread_yield(void);

// Run the scheduler (internal, also used by yield/block/unblock)
void schedule(void);

// Scheduler entry from interrupt context. Takes the stack pointer saved
// by the interrupt stub and returns the stack of the next thread.
uint64_t schedule_from_isr(uint64_t *old_rsp);

// Switch stack (old_rsp, new_rsp): implemented in asm and returns to caller
void context_switch(uint64_t *old_rsp, uint64_t new_rsp);

// Enter user mode at given entry/stack: implemented in asm
void enter_user_mode(uint64_t entry, uint64_t user_stack) __attribute__((noreturn));

