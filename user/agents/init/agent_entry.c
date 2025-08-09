// user/agents/init/agent_entry.c
#include "../../rt/agent_abi.h"   // provides AgentAPI, NOS, NOS_TID

// init.c provides this
extern void init_main(const AgentAPI *api, uint32_t self_tid);

__attribute__((noreturn))
void agent_main(void) {
    // Hand-off to the real entry (API + our TID are provided by rt0_agent)
    init_main(NOS, NOS_TID);

    // If init_main ever returned, just yield forever
    for (;;) {
        if (NOS && NOS->yield) NOS->yield();
        else __asm__ __volatile__("pause");
    }
}
