#ifndef NSH_H
#define NSH_H

#include <stdint.h>
#include "../../../kernel/IPC/ipc.h"

/* Entry point for the NitroShell user interface. */
void nsh_main(ipc_queue_t *registry_q, uint32_t self_id);

#endif /* NSH_H */

