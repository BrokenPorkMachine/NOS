#include "agent_abi.h"

/* Globals visible to agents */
const AgentAPI *NOS = 0;
uint32_t        NOS_TID = 0;

/* Every agent provides this. Keep it free of kernel headers. */
extern void agent_main(void) __attribute__((noreturn));

/* Entry called by regx:
 *   rdi -> AgentAPI*
 *   rsi -> self tid
 */
__attribute__((noreturn))
void _start(const AgentAPI *api, uint32_t self_tid)
{
    NOS      = api;
    NOS_TID  = self_tid;

    /* Minimal C runtime could go here if needed (ctors, etc.) */

    agent_main(); /* never returns */
    /* Safety if agent_main ever returns */
    for (;;) {
        if (NOS && NOS->yield) NOS->yield();
    }
}
