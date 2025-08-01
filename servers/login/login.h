#ifndef LOGIN_SERVER_H
#define LOGIN_SERVER_H
#include "../../IPC/ipc.h"
#include <stdint.h>

extern volatile int login_done;

void login_server(ipc_queue_t *q, uint32_t self_id);

#endif // LOGIN_SERVER_H
