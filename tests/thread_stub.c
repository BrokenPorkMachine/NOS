#include <stdint.h>
#include <stddef.h>

/*
 * Minimal thread and scheduler stubs so unit tests can link kernel code
 * that expects threading primitives. These provide no real scheduling
 * behaviour but satisfy symbol dependencies.
 *
 * Updated to expose the scheduler_enqueue/scheduler_pick_and_run
 * interface used by nitros_arch_sched.c while still keeping the old
 * schedule() symbol for compatibility.
 */

struct thread { int dummy; };
typedef struct thread thread_t;

static thread_t dummy_thread;

uint32_t thread_self(void) { return 1; }
thread_t *thread_current(void) { return &dummy_thread; }
void thread_block(thread_t *t) { (void)t; }
void thread_unblock(thread_t *t) { (void)t; }

thread_t *thread_create(void (*func)(void)) {
    if (func) func();
    return &dummy_thread;
}

thread_t *thread_create_with_priority(void (*func)(void), int priority) {
    (void)priority;
    return thread_create(func);
}

void thread_join(thread_t *t) { (void)t; }

/* New scheduler entry points */
void scheduler_enqueue(thread_t *t) { (void)t; }
void scheduler_pick_and_run(void) {}

/* Legacy compatibility */
void schedule(void) { scheduler_pick_and_run(); }
