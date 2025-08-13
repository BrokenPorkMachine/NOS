// user/rt/rt0_agent.c
#include <stddef.h>
#include <stdint.h>

// Prototype for the agent's main entry point
extern int init_main(int argc, char **argv, char **envp);

// Optional exit function if you have one in libc
extern void _exit(int code);

// Simple runtime entry point for agents
__attribute__((noreturn))
void _start(void) {
    // Align stack pointer to 16 bytes (SysV ABI requirement)
    uintptr_t sp;
    __asm__ __volatile__("mov %%rsp, %0" : "=r"(sp));
    sp &= ~0xFULL;
    __asm__ __volatile__("mov %0, %%rsp" :: "r"(sp));

    // For now, no argc/argv/envp parsing
    (void) init_main(0, NULL, NULL);

    // If init_main returns, exit
    _exit(0);
    __builtin_unreachable();
}
