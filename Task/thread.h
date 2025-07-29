#pragma once
#include <stdint.h>

typedef enum {
    THREAD_READY,
    THREAD_RUNNING,
    THREAD_BLOCKED
} thread_state_t;

typedef struct thread {
    uint64_t rsp;
    void (*func)(void);
    char *stack;
    int id;
    thread_state_t state;
    struct thread *next; // run queue link
} thread_t;

extern thread_t *current;

void threads_init(void);
thread_t *thread_create(void (*func)(void));
void thread_block(thread_t *t);
void thread_unblock(thread_t *t);
void context_switch(uint64_t *old_rsp, uint64_t new_rsp);
void enter_user_mode(uint64_t entry, uint64_t user_stack);
void schedule(void);

