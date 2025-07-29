#pragma once
#include <stdint.h>

typedef struct thread {
    uint64_t rsp;
    void (*func)(void);
    char* stack;
    int id;
    struct thread* next; // For round-robin
} thread_t;
void context_switch(uint64_t *old_rsp, uint64_t new_rsp);
