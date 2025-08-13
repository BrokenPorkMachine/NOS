// user/rt/rt0_agent.c
#include "../rt/agent_abi.h"

const AgentAPI *NOS;
uint32_t        NOS_TID;

extern void agent_main(void);           // or init_main(api, tid)

__attribute__((noreturn))
void _start(const AgentAPI *api, uint32_t tid) {
    __asm__ __volatile__("andq $-16, %%rsp" ::: "rsp");
    NOS     = api;
    NOS_TID = tid;
    agent_main();                       // or: init_main(NOS, NOS_TID);
    for (;;) __asm__ __volatile__("hlt");
}
