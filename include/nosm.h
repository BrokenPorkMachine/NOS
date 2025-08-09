#pragma once
#include <stdint.h>
#include "nosm_ipc.h"

typedef struct nosm_env {
    uint32_t mod_id;
    uint64_t caps;     /* granted by nosm agent */
} nosm_env_t;

/* Module entry ABI inside a .nmod */
typedef struct {
    /* Required */
    int  (*init)(const nosm_env_t *env);  /* return 0=OK */
    void (*fini)(void);

    /* Optional control hooks */
    void (*suspend)(void);
    void (*resume)(void);
} nosm_module_ops_t;

/* A compiled module must export this symbol name: */
#define NOSM_MODULE_ENTRY_SYMBOL "nosm_module_ops"

/* Kernel API */
int  nosm_request_verify_and_load(const void *blob, uint32_t len, uint32_t *out_mod_id);
int  nosm_unload(uint32_t mod_id);
int  nosm_cap_check(uint32_t mod_id, uint64_t need_caps);

/* For nosm agent to revoke at runtime via IPC */
void nosm_revoke(uint32_t mod_id);

