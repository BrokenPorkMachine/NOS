#pragma once
#include <stdint.h>
#include <stddef.h>

typedef struct {
    int32_t  op;
    int32_t  _pad0;
    uint64_t buffer;
    uint64_t length;
} ipc_msg_t;

_Static_assert(sizeof(ipc_msg_t) == 24, "ipc_msg_t size must be 24 bytes LP64");
