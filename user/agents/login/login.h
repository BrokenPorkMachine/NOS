#ifndef LOGIN_H
#define LOGIN_H

#include <stdint.h>
#include "../../libc/libc.h"

/* Represents the state of the current authenticated session. */
typedef struct {
    uint32_t uid;            /* User identifier */
    uint32_t session_id;     /* Monotonic session counter */
    uint8_t  active;         /* Non-zero when a user is logged in */
    char     username[32];   /* Null-terminated username */
} login_session_t;

/* Entry point for the login agent.  The fs_q parameter is unused in this
 * lightweight implementation and is typed as void* to avoid pulling in
 * kernel IPC headers. */
void login_server(void *fs_q, uint32_t self_id);

/* Global session state exposed for tests and other agents. */
extern volatile login_session_t current_session;

#endif /* LOGIN_H */

