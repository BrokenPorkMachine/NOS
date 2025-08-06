#pragma once
#include <stdint.h>
#include "../../../kernel/IPC/ipc.h"

void audio_server(ipc_queue_t *q, uint32_t self_id);
