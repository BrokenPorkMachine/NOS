#pragma once
#include <stdint.h>

typedef struct thread {
    uint64_t rsp;
    void (*func)(void);
    char* stack;
    int id;
    struct thread* next; // For round-robin
} thread_t;
