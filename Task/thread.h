#pragma once
#include <stdint.h>

typedef struct thread {
    uint64_t rsp;
    void (*func)(void);
    char* stack;
    int id;
    struct thread* next; // For round-robin
} thread_t;

extern thread_t *current;

void threads_init(void);
void context_switch(uint64_t *old_rsp, uint64_t new_rsp);
void enter_user_mode(uint64_t entry, uint64_t user_stack);
void schedule(void);
