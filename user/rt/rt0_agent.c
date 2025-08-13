// user/rt/rt0_agent.c
__attribute__((noreturn))
void agent_entry(void);
extern void init_main(void);

__attribute__((noreturn))
void agent_entry(void) {
    // System V ABI: 16-byte alignment before call
    asm volatile ("andq $-16, %%rsp" ::: "rsp");
    init_main();

    // If it returns, park cleanly
    for (;;)
        asm volatile ("hlt");
}
