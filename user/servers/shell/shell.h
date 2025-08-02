#ifndef SHELL_H
#define SHELL_H

#include <stdint.h>
#include "../../../kernel/IPC/ipc.h"

// Entry point for the NOS kernel shell.
// Call this from your kernel main thread after IPC init.
void shell_main(ipc_queue_t *fs_q, ipc_queue_t *pkg_q, ipc_queue_t *upd_q, uint32_t self_id);
#endif // SHELL_H
