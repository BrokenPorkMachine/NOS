#ifndef SHELL_H
#define SHELL_H

#include <stdint.h>
#include "../../IPC/ipc.h"

// Entry point for the NOS kernel shell.
// Call this from your kernel main thread after IPC init.
void shell_main(ipc_queue_t *q, uint32_t self_id);
void *__memcpy_chk(void *dest, const void *src, size_t n, size_t destlen);
char *__strncpy_chk(char *dest, const char *src, size_t n, size_t destlen);
#endif // SHELL_H
