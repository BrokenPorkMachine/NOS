#pragma once
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Public “AgentAPI” that regx passes to every agent at startup.
 *
 * Call ABI (SysV x86-64):
 *   RDI = (const AgentAPI*) api_table
 *   RSI = (uint32_t)        self_tid
 *
 * All function pointers here use the standard SysV calling convention.
 * This header MUST be identical on both the kernel/regx side and the agent side.
 */

_Static_assert(sizeof(void*) == 8, "Agent ABI requires 64-bit");

#pragma pack(push, 1)
typedef struct AgentAPI {
    /* Kernel logging */
    void (*puts)(const char*);

    /* printf-style logging */
    int  (*printf)(const char*, ...);

    /* Load an agent/module by path (optional arg/out) */
    int  (*regx_load)(const char* path, const void* arg, void* out);

    /* Yield the current agent thread */
    void (*yield)(void);

    /* Read entire file into a caller-provided buffer. Returns 0 on success. */
    int  (*fs_read_all)(const char* path, char* buf, size_t cap, size_t* out_sz);
} AgentAPI;
#pragma pack(pop)

/* ABI drift guards — update on both sides if the struct changes */
_Static_assert(offsetof(AgentAPI, puts)        == 0,  "ABI drift: puts");
_Static_assert(offsetof(AgentAPI, printf)      == 8,  "ABI drift: printf");
_Static_assert(offsetof(AgentAPI, regx_load)   == 16, "ABI drift: regx_load");
_Static_assert(offsetof(AgentAPI, yield)       == 24, "ABI drift: yield");
_Static_assert(offsetof(AgentAPI, fs_read_all) == 32, "ABI drift: fs_read_all");

/* Set by the agent runtime (entry/rt0) from RDI/RSI before calling into user code. */
extern const AgentAPI *NOS;
extern uint32_t        NOS_TID;

#ifdef __cplusplus
} // extern "C"
#endif
