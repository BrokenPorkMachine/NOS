#ifndef SSH_SERVER_H
#define SSH_SERVER_H
#include "../../../kernel/IPC/ipc.h"
void ssh_server(ipc_queue_t *q, uint32_t self_id);
#endif // SSH_SERVER_H
