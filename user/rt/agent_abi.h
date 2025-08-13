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
    /* Basic services */
    void     (*yield)(void);
    uint32_t (*self)(void);

    /* Logging */
    int      (*printf)(const char *fmt, ...);
    int      (*puts)(const char *s);

    /* Filesystem (user libc may also provide fopen/fread/etc). These helpers
       are optional; set to NULL if not implemented. */
    int      (*fs_read_all)(const char *path, void *buf, size_t max, size_t *out_len);

    /* regx security gate helpers */
    /* Request regx to load an agent binary (gated/verified).
       Returns 0 on success; *out_tid set to new agent TID (if applicable). */
    int      (*regx_load)(const char *path, const char *args, uint32_t *out_tid);

    /* Optional health ping to regx; returns 1 if regx responds, 0 otherwise. */
    int      (*regx_ping)(void);
    /* add new fields only at the end AND update both kernel and agents */
} AgentAPI;
#pragma pack(pop)

_Static_assert(sizeof(void*) == 8, "64-bit only");
_Static_assert(sizeof(AgentAPI) == 56, "AgentAPI size mismatch");
_Static_assert(offsetof(AgentAPI, yield)      == 0,  "ABI drift: yield");
_Static_assert(offsetof(AgentAPI, self)       == 8,  "ABI drift: self");
_Static_assert(offsetof(AgentAPI, printf)     == 16, "ABI drift: printf");
_Static_assert(offsetof(AgentAPI, puts)       == 24, "ABI drift: puts");
_Static_assert(offsetof(AgentAPI, fs_read_all)== 32, "ABI drift: fs_read_all");
_Static_assert(offsetof(AgentAPI, regx_load)  == 40, "ABI drift: regx_load");
_Static_assert(offsetof(AgentAPI, regx_ping)  == 48, "ABI drift: regx_ping");

/* Set by the agent runtime (rt0_agent.c) from registers (RDI/RSI). */
extern const AgentAPI *NOS;
extern uint32_t        NOS_TID;
