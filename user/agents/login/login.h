#ifndef LOGIN_H
#define LOGIN_H

#include <stdint.h>
#include "../../../kernel/IPC/ipc.h"

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
