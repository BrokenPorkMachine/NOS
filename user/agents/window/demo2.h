#pragma once
#include <stdint.h>
#include "../../../kernel/IPC/ipc.h"

void window_demo2(ipc_queue_t *q, uint32_t self_id, uint32_t server);
