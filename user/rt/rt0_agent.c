#include "agent_abi.h"
#include <stdint.h>

const AgentAPI *NOS = NULL;
uint32_t NOS_TID = 0;

__attribute__((noreturn)) void agent_main(void);

__attribute__((noreturn))
void _start(const AgentAPI *api, uint32_t self_tid) {
    __asm__ __volatile__("andq $-16, %%rsp" ::: "rsp");
    NOS = api;
    NOS_TID = self_tid;
    agent_main();
    for (;;) {
        if (NOS && NOS->yield) {
            NOS->yield();
        } else {
            __asm__ __volatile__("hlt");
        }
    }
}
