#pragma once
#include <regx.h>

enum {
    REGX_IPC_ENUM = 1,     // Enumerate entries
    REGX_IPC_QUERY = 2,    // Query entry by id
    REGX_IPC_REGISTER = 3, // Register new entry
    REGX_IPC_UNREGISTER = 4,// Remove entry
    REGX_IPC_TREE = 5      // Return full tree
};

typedef struct {
    int op;
    regx_selector_t sel;
    uint64_t id;
    regx_manifest_t manifest;
} regx_ipc_req_t;

typedef struct {
    int status;
    regx_entry_t entries[REGX_MAX_ENTRIES];
    size_t count;
    regx_entry_t entry;
} regx_ipc_resp_t;

// Message handler:
void regx_ipc_handle(const regx_ipc_req_t *req, regx_ipc_resp_t *resp);
