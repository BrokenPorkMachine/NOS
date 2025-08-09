#ifndef INIT_H
#define INIT_H

#include <stdint.h>

/*
 * This header is usable in both agent (standalone) and kernel-linked builds.
 * In AGENT builds, we still rely on the same IPC ABI headers that the libc
 * exposes, which map to the kernel’s IPC structures.
 */
#include "../../../kernel/IPC/ipc.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * init_main — entrypoint for the init agent/server.
 *
 * Agent (standalone) mode (default):
 *   - Loaded & gated by regx.
 *   - Requests regx to load other agents (pkg, update, login, etc).
 *   - Does not directly reference those servers to avoid link-time deps.
 *
 * Kernel-linked fallback (when KERNEL_BUILD is defined):
 *   - May directly spawn service threads and grant IPC caps.
 *
 * @param q        Optional queue pointer (unused in agent mode; reserved for
 *                 kernel-linked fallback where init might own a control queue).
 * @param self_id  The task/thread id for this init instance (assigned by loader).
 */
void init_main(ipc_queue_t *q, uint32_t self_id);

#ifdef __cplusplus
} // extern "C"
#endif

#endif /* INIT_H */
