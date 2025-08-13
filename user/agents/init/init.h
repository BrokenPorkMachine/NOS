#ifndef INIT_H
#define INIT_H

#include <stdint.h>

// Minimal IPC definitions to avoid depending on kernel headers
#ifndef IPC_MSG_DATA_MAX
#define IPC_MSG_DATA_MAX 64
#endif

typedef struct {
    uint32_t type;
    uint32_t sender;
    uint32_t arg1;
    uint32_t arg2;
    uint32_t len;
    uint8_t  data[IPC_MSG_DATA_MAX];
} ipc_message_t;

typedef struct ipc_queue ipc_queue_t;

#ifdef __cplusplus
extern "C" {
#endif

/**
 * init_main — entrypoint for the init agent/server.
 *
 * Agent (standalone) mode (default):
 *   - Loaded & gated by regx.
 *   - Requests regx to load other agents (pkg, update, login, etc).
 *   - Does not directly reference those servers to avoid link-time deps.
 *
 * Kernel-linked fallback (when KERNEL_BUILD is defined):
 *   - May directly spawn service threads and grant IPC caps.
 *
 * @param q        Optional queue pointer (unused in agent mode; reserved for
 *                 kernel-linked fallback where init might own a control queue).
 * @param self_id  The task/thread id for this init instance (assigned by loader).
 */
void init_main(ipc_queue_t *q, uint32_t self_id);

#ifdef __cplusplus
} // extern "C"
#endif

#endif /* INIT_H */
