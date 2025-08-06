#pragma once
#include "../../../kernel/IPC/ipc.h"
#include "update.h"
#include "../pkg/pkg.h"

void update_server(ipc_queue_t *update_q, ipc_queue_t *pkg_q, uint32_t self_id);
