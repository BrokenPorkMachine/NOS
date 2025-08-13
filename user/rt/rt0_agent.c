#include "agent_abi.h"

/* Globals visible to agents */
const AgentAPI *NOS = 0;
uint32_t        NOS_TID = 0;

/* Every agent provides this. Keep it free of kernel headers. */
extern void agent_main(void);

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

    agent_main(); /* should not return, but guard just in case */
    for (;;) {
        if (NOS && NOS->yield) {
            NOS->yield();
        } else {
            __asm__ __volatile__("pause");
        }
    }
}
