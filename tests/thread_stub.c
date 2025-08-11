#include <stdint.h>

/* Minimal thread and scheduler stubs so unit tests can link kernel code
   that expects threading primitives. These provide no real scheduling
   behaviour but satisfy symbol dependencies. */

struct thread { int dummy; } dummy_thread;
typedef struct thread thread_t;

uint32_t thread_self(void) { return 1; }
thread_t *thread_current(void) { return &dummy_thread; }
void thread_block(thread_t *t) { (void)t; }
void thread_unblock(thread_t *t) { (void)t; }
void schedule(void) {}
