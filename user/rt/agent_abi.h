#pragma once
#include <stdint.h>
#include <stddef.h>

/*
 * Public “AgentAPI” that regx passes to every agent at startup.
 * Kernel/regx implements these function pointers; agents only call them.
 *
 * NOTES:
 *  - regx must jump to the agent's _start with:
 *      RDI = (const AgentAPI*) api_table;
 *      RSI = (uint32_t)        self_tid;
 *  - The ABI is stable C: all functions use the standard SysV calling conv.
 */

/* Keep a tight ABI: no implicit padding and fixed field offsets */
#pragma pack(push,1)
typedef struct AgentAPI {
    void (*puts)(const char*);
    int  (*printf)(const char*, ...);
    int  (*regx_load)(const char*, const void*, void*);
    void (*yield)(void);
} AgentAPI;
#pragma pack(pop)

#include <stddef.h>
_Static_assert(offsetof(AgentAPI, puts)      == 0,  "puts off");
_Static_assert(offsetof(AgentAPI, printf)    == 8,  "printf off");
_Static_assert(offsetof(AgentAPI, regx_load) == 16, "regx_load off");
_Static_assert(offsetof(AgentAPI, yield)     == 24, "yield off");

_Static_assert(offsetof(AgentAPI, fs_read_all)== 32, "ABI drift: fs_read_all");
_Static_assert(offsetof(AgentAPI, regx_load)  == 40, "ABI drift: regx_load");
_Static_assert(offsetof(AgentAPI, regx_ping)  == 48, "ABI drift: regx_ping");

/* Set by the agent runtime (rt0_agent.c) from registers (RDI/RSI). */
extern const AgentAPI *NOS;
extern uint32_t        NOS_TID;
