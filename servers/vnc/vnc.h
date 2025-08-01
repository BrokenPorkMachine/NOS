#ifndef VNC_SERVER_H
#define VNC_SERVER_H
#include "../../IPC/ipc.h"
void vnc_server(ipc_queue_t *q, uint32_t self_id);
#endif // VNC_SERVER_H
