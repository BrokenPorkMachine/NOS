#include "regx_ipc.h"
#include <string.h>

void regx_ipc_handle(const regx_ipc_req_t *req, regx_ipc_resp_t *resp) {
    if (!req || !resp) return;  // Defensive

    memset(resp, 0, sizeof(*resp));
    resp->status = -99; // Default to unrecognized/failure

    switch (req->op) {
    case REGX_IPC_ENUM:
        resp->count = regx_enumerate(&req->sel, resp->entries, REGX_MAX_ENTRIES);
        resp->status = 0;
        break;
    case REGX_IPC_QUERY: {
        const regx_entry_t *e = regx_query(req->id);
        if (e) {
            resp->entry = *e;
            resp->status = 0;
        } else {
            resp->status = -1;
        }
        break;
    }
    case REGX_IPC_REGISTER:
        resp->entry.id = regx_register(&req->manifest, req->id);
        resp->status = (resp->entry.id ? 0 : -1);
        break;
    case REGX_IPC_UNREGISTER:
        resp->status = regx_unregister(req->id);
        break;
    case REGX_IPC_TREE: // Full dump
        resp->count = regx_enumerate(NULL, resp->entries, REGX_MAX_ENTRIES);
        resp->status = 0;
        break;
    default:
        // Already set to -99
        break;
    }
}
