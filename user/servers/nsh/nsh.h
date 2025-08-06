#ifndef NSH_H
#define NSH_H

#include <stdint.h>
#include "../../../kernel/IPC/ipc.h"

// Entry point for the NitroShell (nsh).
// Call this from your kernel main thread after IPC init.
void nsh_main(ipc_queue_t *fs_q, ipc_queue_t *pkg_q, ipc_queue_t *upd_q, uint32_t self_id);
#endif // NSH_H
