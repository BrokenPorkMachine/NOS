#include "thread.h"
#define STACK_SIZE 4096

thread_t thread1, thread2;
thread_t *current = &thread1;

// Thread stacks
char stack1[STACK_SIZE], stack2[STACK_SIZE];

void thread_func1(void) {
    while (1) {
        // Print something on the screen, or update a variable
        volatile char *vga = (char*)0xB8000 + 320; // 3rd line
        vga[0] = 'A';
        for (volatile int i = 0; i < 1000000; ++i); // delay
        schedule();
    }
}
void thread_func2(void) {
    while (1) {
        volatile char *vga = (char*)0xB8000 + 324; // 4th line
        vga[0] = 'B';
        for (volatile int i = 0; i < 1000000; ++i);
        schedule();
    }
}

void threads_init(void) {
    // Set up thread1
    thread1.stack = stack1;
    thread1.rsp = (uint64_t)&stack1[STACK_SIZE-16];
    thread1.func = thread_func1;
    thread1.id = 1;
    thread1.next = &thread2;
    *(uint64_t*)(&stack1[STACK_SIZE-16]) = (uint64_t)thread_func1;

    // Set up thread2
    thread2.stack = stack2;
    thread2.rsp = (uint64_t)&stack2[STACK_SIZE-16];
    thread2.func = thread_func2;
    thread2.id = 2;
    thread2.next = &thread1;
    *(uint64_t*)(&stack2[STACK_SIZE-16]) = (uint64_t)thread_func2;
}

void schedule(void) {
    thread_t *prev = current;
    current = current->next;
    context_switch(&prev->rsp, current->rsp);
}
