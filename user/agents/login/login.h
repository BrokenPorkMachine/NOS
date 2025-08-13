#ifndef LOGIN_H
#define LOGIN_H

#include <stdint.h>
#include "../../libc/libc.h"

// Minimal IPC types so this header can be used without kernel internals
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

typedef struct ipc_queue ipc_queue_t; // Opaque to the agent

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint32_t uid;
    uint32_t session_id;
    uint8_t  active;
    char     username[32];
} login_session_t;

/**
 * Entry point for the login server/agent.
 * In agent mode, regx will gate & launch this from the manifest "entry".
 */
void login_server(ipc_queue_t *fs_q, uint32_t self_id);

extern volatile login_session_t current_session;

#ifdef __cplusplus
} // extern "C"
#endif

#endif /* LOGIN_H */
