#ifndef SHELL_H
#define SHELL_H
#include "../../IPC/ipc.h"
void shell_main(ipc_queue_t *q, uint32_t self_id);
#endif
