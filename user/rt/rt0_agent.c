// user/rt/rt0_agent.c
// Minimal runtime for agent ELFs: provides _start and calls agent_main() if present.
__attribute__((weak)) void agent_main(void);

void _start(void) {
    if (agent_main) {
        agent_main();
    }
    for (;;) {
        __asm__ __volatile__("hlt");
    }
}
