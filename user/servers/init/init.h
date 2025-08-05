#ifndef INIT_H
#define INIT_H

#include <stdint.h>
#include "../../../kernel/IPC/ipc.h"

// Entry point for the user-space init server.  Accepts the filesystem IPC
// queue and the caller's task identifier so the server can communicate and
// identify itself to other components.
void init_main(ipc_queue_t *q, uint32_t self_id);

#endif // INIT_H
