#ifndef INIT_H
#define INIT_H

#include <stdint.h>
#include "../../../kernel/IPC/ipc.h"

/**
 * Entry point for the user-space init server.
 * 
 * This function is responsible for:
 *   - Spawning system services (filesystem, login, pkg, update, etc)
 *   - Granting IPC capabilities to each service
 *   - Monitoring service health and respawning as needed
 *   - Optionally filtering services based on config or command-line
 *
 * @param q        Pointer to the filesystem IPC queue (or main init queue)
 * @param self_id  Task/thread identifier for this init instance
 */
void init_main(ipc_queue_t *q, uint32_t self_id);

#endif // INIT_H
