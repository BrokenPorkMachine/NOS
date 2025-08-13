#pragma once
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Public “AgentAPI” that regx passes to every agent at startup.
 * regx must jump to the agent's _start with:
 *   RDI = (const AgentAPI*) api_table;
 *   RSI = (uint32_t)        self_tid;
 * All functions use the SysV x86-64 calling convention.
 */

_Static_assert(sizeof(void*) == 8, "Agent ABI requires 64-bit");

#pragma pack(push, 1)
typedef struct AgentAPI {
    void (*puts)(const char*);
    int  (*printf)(const char*, ...);
    int  (*regx_load)(const char*, const void*, void*);
    void (*yield)(void);
} AgentAPI;
#pragma pack(pop)

_Static_assert(offsetof(AgentAPI, puts)      == 0,  "puts off");
_Static_assert(offsetof(AgentAPI, printf)    == 8,  "printf off");
_Static_assert(offsetof(AgentAPI, regx_load) == 16, "regx_load off");
_Static_assert(offsetof(AgentAPI, yield)     == 24, "yield off");

/* Set by the agent runtime (rt0/entry) from RDI/RSI. */
extern const AgentAPI *NOS;
extern uint32_t        NOS_TID;

#ifdef __cplusplus
} // extern "C"
#endif
