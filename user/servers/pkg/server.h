#pragma once
#include "../../../kernel/IPC/ipc.h"
#include "pkg.h"

void pkg_server(ipc_queue_t *q, uint32_t self_id);
