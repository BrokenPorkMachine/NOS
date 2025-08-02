#ifndef INIT_H
#define INIT_H

#include <stdint.h>
#include "../../../kernel/IPC/ipc.h"

void init_main(ipc_queue_t *q, uint32_t self_id);

#endif // INIT_H
