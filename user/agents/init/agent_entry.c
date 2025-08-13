// user/rt/rt0_agent.c
#include "../rt/agent_abi.h"
#include <stdint.h>

extern void init_main(const AgentAPI *api, uint32_t self_tid);

__attribute__((noreturn))
void _start(const AgentAPI *api, uint32_t self_tid) {
    __asm__ __volatile__("andq $-16, %%rsp" ::: "rsp"); // keep 16B alignment
    init_main(api, self_tid);
    for (;;) __asm__ __volatile__("hlt");
}
