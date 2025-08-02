#ifndef LOGIN_SERVER_H
#define LOGIN_SERVER_H
#include "../../../kernel/IPC/ipc.h"
#include <stdint.h>

extern volatile int login_done;

typedef struct {
    uint32_t uid;
    char username[32];
    uint32_t session_id;
    int active;
} login_session_t;

extern volatile login_session_t current_session;

void login_server(ipc_queue_t *q, uint32_t self_id);

#endif // LOGIN_SERVER_H
